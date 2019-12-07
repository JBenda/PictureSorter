#include "fJPG.hpp"

#include "HuffTable.hpp"
#include "QuadTable.hpp"
#include "Util.hpp"
#include "BitItr.hpp"
#include "DataCollection.hpp"
#include "DuIterator.hpp"

#include <istream>
#include <variant>
#include <array>
#include <numeric>
#include <optional>

namespace fJPG {

	enum struct Flags : uint8_t {
		DATA = 0xDA,
		APP0 = 0xE0,
		APP1 = 0xE1,
		DQT = 0xDB,
		SOF0 = 0xC0,
		SOF2 = 0xC2,
		DHT = 0xC4,
		END = 0xD9,
		SOI = 0xD8,
		COM = 0xFE
	};

	JPGDecomposition Decompose( std::istream& );
	void Decode( EditablePicture<ColorEncoding::YCrCb8>&, const JPGDecomposition& );

	Picture<ColorEncoding::YCrCb8> Convert( std::istream& input ) {
		JPGDecomposition data = Decompose( input );
		std::size_t w = data.size.x / 8;
		if ( w * 8 != data.size.x ) ++w;
		if ( w % data.mcuSamp.x != 0 )  w += data.mcuSamp.x - ( w % data.mcuSamp.x );

		std::size_t h = data.size.y / 8;
		if ( h * 8 != data.size.y ) ++h;
		if ( h % data.mcuSamp.y != 0 ) h += data.mcuSamp.y - ( h % data.mcuSamp.y );


		data.nDus = { w, h };
		EditablePicture<ColorEncoding::YCrCb8>
			picture( Dim<std::size_t>( w, h ), data.nChannel );

		Decode( picture, data );

		return picture;
	}

	
	template<Flags F>
	void Parse( JPGDecomposition& /*data*/ ) { throw "not implimented yet"; }

	template<>
	void Parse<Flags::APP0>( JPGDecomposition& data ) {
#ifdef DEBUG
		auto start = data.input.tellg();
#endif
		uint16_t len = ReadLen( data.input );
		constexpr std::size_t headerSize = 4 + 2 + 1 + 2 + 2;
#ifdef DEBUG
		std::array<char, 5> title;
		Read( data.input, title );
		assert( std::string_view( title.data() ) == "JFIF" );
		uint8_t num = 0;
		Read( data.input, num );
		assert( num == 1 );
		Read( data.input, num );
		assert( num >= 0 && num <= 2 ); // weak cretarie
		Skip( data.input, 1 + 2 + 2 ); // density (unit, x, y)

		uint8_t x, y;
		Read( data.input, x );
		Read( data.input, y );
		Skip( data.input, static_cast<std::size_t>( x )* static_cast<std::size_t>( y ) * 3 ); // Skip Thumbnail
		std::size_t diff = data.input.tellg() - start;
		assert( diff == len );
#else
		Skip( data.input, len );
#endif
	}

	template<>
	void Parse <Flags::APP1>( JPGDecomposition& data ) {
		uint16_t len = ReadLen( data.input );
		Skip( data.input, len - 2 );
	}

	template<>
	void Parse<Flags::DQT>( JPGDecomposition& data ) {
		auto start = data.input.tellg();
		std::size_t diff = 0;

		uint16_t len = ReadLen( data.input );
		do {
			QuadTable qt;
			data.input >> qt;
			data.quadTables[qt.id()] = qt;
			diff = data.input.tellg() - start;
		} while ( diff < len );
#ifdef DEBUG
		assert( diff == len );
#endif
	}

	template<>
	void Parse<Flags::SOF0>( JPGDecomposition& data ) {
#ifdef DEBUG
		auto start = data.input.tellg();
#endif
		data.progressive.reset();
		const uint16_t len = ReadLen( data.input );
		uint8_t colorPrcission;
		Read( data.input, colorPrcission );
		data.size = Dim<std::size_t>( ReadLen( data.input ), ReadLen( data.input ) );
		uint8_t colorChannels;
		Read( data.input, colorChannels );
		data.nChannel = colorChannels;
		if ( colorPrcission == 8 && colorChannels == 3 ) {
			data.colorEncoding = ColorEncoding::YCrCb8;
			std::array<uint8_t, 3> meta;
			for ( int i = 0; i < 3; ++i ) {
				Read( data.input, meta );
				ChannelInfo& channel = data.channelInfos[meta[0] - 1];
				channel.sampV = meta[1] & 0x0F;
				channel.sampH = ( meta[1] & 0xF0 ) >> 4;
				channel.quadTable = meta[2];
			}
		} else if ( colorPrcission == 8 && colorChannels == 1 ) {
			data.colorEncoding = ColorEncoding::BW8;
		} else {
			throw "unksupported color encoding";
		}
#ifdef DEBUG
		std::size_t diff = data.input.tellg() - start;
		assert( diff == len );
#endif // DEBUG
	}

	template<>
	void Parse<Flags::SOF2>( JPGDecomposition& data ) {
		Parse<Flags::SOF0>( data );
		data.progressive.emplace();
	}

	template<>
	void Parse<Flags::DHT>( JPGDecomposition& data ) {
		auto start = data.input.tellg();
		std::size_t diff = data.input.tellg() - start;
		const uint16_t len = ReadLen( data.input );
		do {
			HuffTable::Header header = HuffTable::ParseHeader( data.input );
			if ( header.isAC ) {
				data.input >> data.huffTablesAc[header.id];
			} else {
				data.input >> data.huffTablesDc[header.id];
			}
			diff = data.input.tellg() - start;
		} while ( len < data.input.tellg() - start );

#ifdef DEBUG

		assert( diff == len );
#endif
	}

	template<>
	void Parse<Flags::DATA>( JPGDecomposition& data ) {
#ifdef DEBUG
		auto start = data.input.tellg();
#endif // DEBUG
		const uint16_t len = ReadLen( data.input );
		uint8_t nC;
		Read( data.input, nC );
#ifdef DEBUG
		if ( !data.progressive ) {
			assert( nC == data.nChannel );
			assert( len == 6 + 2 * data.nChannel );
		}
#else
		Skip( data.input, 1 );
#endif // DEBUG
		if ( nC != data.nChannel ) { Skip( data.input, nC * 2 ); } // TODO: more reliable progressive
		else {
			for ( unsigned int i = 0; i < data.nChannel; ++i ) {
				std::array<uint8_t, 2> channelInfo;
				Read( data.input, channelInfo );
				ChannelInfo& cI = data.channelInfos[channelInfo[0] - 1];
				cI.huffAc = channelInfo[1] & 0x0F;
				cI.huffDc = ( channelInfo[1] & 0xF0 ) >> 4;
#ifdef DEBUG
				assert( cI.huffAc >= 0 && cI.huffAc <= 3 );
				assert( cI.huffDc >= 0 && cI.huffDc <= 3 );
#endif // DEBUG
			}
		}
		if ( !data.progressive ) {
			Skip( data.input, 3 );
		} else {
			std::array<uint8_t, 3> meta;
			Read( data.input, meta );
			JPGDecomposition::Progressive& p = data.progressive.value();
			p.Ss = meta[0];
			p.Se = meta[1];
			p.Ah = ( meta[2] & 0xF0 ) >> 4;
			p.Al = meta[2] & 0x0F;
		}

#ifdef DEBUG		
		std::size_t diff = data.input.tellg() - start;
		assert( diff == len );
#endif // DEBUG


	}

	template<>
	void Parse<Flags::COM>( JPGDecomposition& data ) {
#ifdef DEBUG
		auto start = data.input.tellg();
#endif // DEBUG

		const uint16_t len = ReadLen( data.input );
#ifdef DEBUG
		std::vector<char> msg( len - ( 2 - 1 ) );
		Read( data.input, msg.data(), len - 2 );
		msg[len - 2] = 0;
		std::cout << "Found message: '" << msg.data() << "'\n";
		std::size_t diff = data.input.tellg() - start;
		assert( diff == len );
#else
		Skip( data.input, len - 2 );
#endif // DEBUG

	}

	void Decode( Flags flag, JPGDecomposition& data ) {
		switch ( flag ) {
		case Flags::APP0:
			Parse<Flags::APP0>( data );
			break;
		case Flags::APP1:
			Parse<Flags::APP1>( data );
			break;
		case Flags::DQT:
			Parse<Flags::DQT>( data );
			break;
		case Flags::SOF0:
			Parse<Flags::SOF0>( data );
			break;
		case Flags::SOF2:
			Parse<Flags::SOF2>( data );
			break;
		case Flags::DHT:
			Parse<Flags::DHT>( data );
			break;
		case Flags::DATA:
			Parse<Flags::DATA>( data );
			break;
		case Flags::COM:
			Parse<Flags::COM>( data );
			break;
		default:
			throw std::string( "unknown flag: " ) + std::to_string( (int) flag );
		}
	}

	Flags ReadFlag( std::istream& is ) {
		uint8_t flag[2];
		Read( is, flag, 2 );
#ifdef DEBUG
		assert( flag[0] == 0xFF );
#endif
		return static_cast<Flags>( flag[1] );
	}

	JPGDecomposition Decompose( std::istream& input ) {
		{
			assert( ReadFlag( input ) == Flags::SOI );
		}
		JPGDecomposition result( input );
		Flags flag;
		do {
			flag = ReadFlag( input );
			Decode( flag, result );
		} while ( flag != Flags::DATA );

		result.startData = input.tellg();
		result.mcuSamp = Dim<uint8_t>( 0, 0 );
		for ( unsigned int i = 0; i < result.nChannel; ++i ) {
			if ( result.mcuSamp.x < result.channelInfos[i].sampH ) {
				result.mcuSamp.x = result.channelInfos[i].sampH;
			}
			if ( result.mcuSamp.y < result.channelInfos[i].sampV ) {
				result.mcuSamp.y = result.channelInfos[i].sampV;
			}
		}
		result.mcuSize = Dim<uint8_t>( result.mcuSamp.x * 8, result.mcuSamp.y * 8 );
		return result;
	}


	void skipAc( BitItr& itr, const HuffTable& acTable, std::size_t end = 63, std::size_t start = 1 ) {
		for ( std::size_t i = start; i <= end; ++i ) {
			uint8_t byte = itr.nextHuff( acTable );
			if ( byte == 0 ) {
				break;
			}
			i += ( byte & 0xF0 ) >> 4;
			itr += ( byte & 0x0F );
#ifdef DEBUG
			// assert( i <= end + 1 );
#endif // DEBUG

		}

	}

	void DecodeDu( DuIterator& channel, BitItr& itr, const HuffTable& huffDc, const HuffTable& huffAc, const QuadTable& quad, const std::optional<JPGDecomposition::Progressive>& progressive ) {
		if ( !progressive ) {
			uint8_t len = itr.nextHuff( huffDc );
			channel.insert( itr.read( len ) * quad.dc() );

			skipAc( itr, huffAc );
		} else {
			const JPGDecomposition::Progressive& prog = progressive.value();
			if ( prog.Ss != 0 ) { throw "only supports DC encoding"; }
			uint8_t len = itr.nextHuff( huffDc );
			channel.insert( ( itr.read( len ) << prog.Al ) * quad.dc() ); // FIXME: add support to ignore wrong SOS's
			skipAc( itr, huffAc, prog.Se, prog.Ss +  1 );
		}
	}

	template<typename T>
	void DecodeMCU( Dim<std::size_t>& pos, const JPGDecomposition& data, BitItr& itr, std::array<T, 3>& channels ) {
		std::size_t x = pos.x;
		std::size_t max = 0; // FIXME: later
		// std::cout << "pos: " << pos.x << '|' << pos.y << '\n';
		for ( std::size_t c = 0; c < 3; ++c ) {
			x = pos.x;
			for ( unsigned int i = 0; i < static_cast<unsigned int>( data.channelInfos[c].sampV * data.channelInfos[c].sampH ); ++i ) { 
				DecodeDu(
					channels[c],
					itr,
					data.huffTablesDc[data.channelInfos[c].huffDc],
					data.huffTablesAc[data.channelInfos[c].huffAc],
					data.quadTables[data.channelInfos[c].quadTable],
					data.progressive );
				x += 8;
				if ( x > max ) { max = x; }
				if ( i % data.channelInfos[c].sampH == data.channelInfos[c].sampH - 1/*x > data.size.x*/ ) {
					x = pos.x;
					// i = ( i / data.mcuSamp.x + 1 ) * data.channelInfos[c].sampV;
				}
			}
		}
		pos.x = max;
	}

	template<typename T>
	void DecodeMCU( Dim<std::size_t>& pos, const JPGDecomposition& data, BitItr& itr, std::array<T, 1>& channels ) {
		pos.x += data.mcuSize.x;
		std::cout << "pos: " << pos.x << '|' << pos.y << '\n';
		DecodeDu(
			channels[0],
			itr,
			data.huffTablesDc[data.channelInfos[0].huffDc],
			data.huffTablesAc[data.channelInfos[0].huffAc],
			data.quadTables[data.channelInfos[0].quadTable],
			data.progressive );
	}
	void Decode( EditablePicture<ColorEncoding::YCrCb8>& picture, const JPGDecomposition& data ) {
		data.input.seekg( data.startData );
		Dim<std::size_t> pos;
		std::variant<std::array<DuIterator, 3>, std::array<DuIterator, 1>> channels{};
		if ( data.colorEncoding == ColorEncoding::YCrCb8 ) {
			for ( int i = 0; i < 3; ++i ) {
				std::get<std::array<DuIterator, 3>>( channels ).at( i ) = DuIterator( data, i, picture.GetData( i ) );
			}
		} else {
			std::get<std::array<DuIterator, 1>>( channels ).at( 0 ) = DuIterator( data, 0, picture.GetData( 0 ) );
		}
		BitItr dataItr( data.input, data.startData );
		for ( pos.y = 0; pos.y < data.size.y; pos.y += data.mcuSize.y) {
			for ( pos.x = 0; pos.x < data.size.x;) {
				if ( data.colorEncoding == ColorEncoding::YCrCb8 ) {
					DecodeMCU( pos, data, dataItr, std::get<std::array<DuIterator, 3>>( channels ) );
				} else {
					DecodeMCU( pos, data, dataItr, std::get<std::array<DuIterator, 1>>( channels ) );
				}
			}
		}
		std::cout << "end" << data.input.tellg() << '\n';
		if ( !data.progressive ) {
			assert(static_cast<Flags>(dataItr.GetResetCount()) == Flags::END);
		}
	}

}
