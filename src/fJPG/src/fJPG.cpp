#include "fJPG.hpp"

#include "HuffTable.hpp"
#include "QuadTable.hpp"
#include "Util.hpp"

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


	template<ColorEncoding C>
	class EditablePicture : public Picture<C> {
	public:
		EditablePicture( Dim<std::size_t> size, std::size_t channels )
			: Picture<C>( size, channels ) {}

		auto GetData( std::size_t x ) {
			return Picture<C>::GetChannel( x );
		}
	};

	template <Flags TYPE>
	struct Block {
		static constexpr uint8_t VALUE = static_cast<uint8_t>( TYPE );
		std::streampos start{ 0 };
	};


	struct ChannelInfo {
		uint8_t sampV{ 0 }, sampH{ 0 }, quadTable{ 0 }, huffAc{ 0 }, huffDc{ 0 };
	};


	struct JPGDecomposition {
		JPGDecomposition( std::istream& inputStream ) : input{ inputStream } {}
		static constexpr size_t Max_Channels = 3;
		Dim<std::size_t> size;
		Dim<uint8_t> mcuSize;
		Dim<uint8_t> mcuSamp;
		Dim<std::size_t> nDus;
		std::size_t nChannel{ 0 };
		ColorEncoding colorEncoding{ ColorEncoding::YCrCb8 };
		std::array<ChannelInfo, Max_Channels> channelInfos;
		std::array<QuadTable, Max_Channels> quadTables;
		std::array<HuffTable, Max_Channels> huffTablesAc;
		std::array<HuffTable, Max_Channels> huffTablesDc;
		Block<Flags::DATA> data;
		std::istream& input;
		struct Progressive {
			uint8_t Ss, Se, Ah, Al;
		};
		std::optional<Progressive> progressive;
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

	class DuIterator {
	public:
		DuIterator( const JPGDecomposition& base, std::size_t channelId, Channel<std::vector<color_t>::iterator>& channel )
			:
			_samp{ base.channelInfos[channelId].sampH, base.channelInfos[channelId].sampV },
			_mcuSamp{ base.mcuSamp },
			_nDus{ base.nDus },
			_channel{ channel },
			_itr{ _channel.begin() }{
#ifdef DEBUG
			_id = channelId;
			assert( _channel.end() - _channel.begin() == base.nDus.x * base.nDus.y );

			uint8_t ratio;
			assert( ratio = _mcuSamp.x / _samp.x, !( ratio == 0 ) && !( ratio& ( ratio - 1 ) ) );
			assert( ratio = _mcuSamp.y / _samp.y, !( ratio == 0 ) && !( ratio& ( ratio - 1 ) ) );
#endif // DEBUG

		}

		DuIterator() = default;

		void incriment() {
			++_local.x;
			++_global.x;
			if ( /*_global.x == _nDus.x || */ _local.x == _mcuSamp.x) {
				++_local.y;
				++_global.y;
				if ( _global.x >= _nDus.x && _global.y == _nDus.y && _local.y == _mcuSamp.y) {
					_local.y = 0;
					++_itr;
					_global.x = 0;
				} else if ( _local.y == _mcuSamp.y ) {
					_local.y = 0;
					if ( _global.x >= _nDus.x ) { 
						_global.x = 0; 
						++_itr;
					} else {
						_global.y -= _mcuSamp.y;
						_itr -= ( _mcuSamp.y - 1 ) * _nDus.x - 1;
					}
				} else {
					_global.x -= _local.x;
					_itr += _nDus.x - ( _local.x - 1 );
				}
				_local.x = 0;
			} else {
				++_itr;
			}
		}

		void insert( int diff ) {
			if ( _samp.x == 0 ) {
				// assert(false);
				return;
			}

			_diff += diff;
#ifdef DEBUG
			unsigned int count = 0;
#endif
			do {
#ifdef DEBUG
				++count;
				assert( _itr != _channel.end() );
				if ( _id == 0 ) {
					// std::cout << ( _itr - _channel.begin() ) << '\n';
				}
				assert( *_itr == 0 );
				assert( _diff / 8 + static_cast<int>( ( std::numeric_limits<color_t>::max() + 1 ) / 2 ) < 256 
				&& _diff / 8 + static_cast<int>( ( std::numeric_limits<color_t>::max() + 1 ) / 2 ) >= 0);
#endif // DEBUG
				* _itr = _diff / 8 + static_cast<int>( (std::numeric_limits<color_t>::max() + 1) / 2 );
				std::size_t diff = ( _itr - _channel.begin() );
				// std::cout << "insert: " << ( ( diff % _nDus.x ) * 8 ) << '|' << ( diff / _nDus.x * 8 )  << '\n';
				incriment();
#ifdef DEBUG
				if ( _global.y == 0 && _global.x == 0 ) {
					assert(_itr == _channel.begin() || _itr == _channel.end());
				}
#endif // DEBUG

			} while ( _itr != _channel.end() && ( _local.x >= _samp.x || _local.y >= _samp.y ) );
#ifdef DEBUG
			// std::cout << "executed: " << count << "\n";
#endif // DEBUG

		}
	private:
		Dim<uint8_t> _samp;
		Dim<uint8_t> _mcuSamp;
		Dim<std::size_t> _nDus;
		Channel<std::vector<color_t>::iterator> _channel;
		std::vector<color_t>::iterator _itr;
		Dim<uint8_t> _local{ 0,0 };
		Dim<std::size_t> _global{ 0,0 };
		int _diff{ 0 };
#ifdef DEBUG
		std::size_t _id{255};
#endif // DEBUG

	};

	class BitItr {
		void sync( uint8_t flag ) {
#ifdef DEBUG
			// assert((flag & 0xF0) == 0xD0); // only for sequential
			// assert((flag & 0x0F) == 0 || (flag & 0x0F) == (_block + 1));
#endif
			_block = flag & 0x0F;
		}
		void nextByte() {
			_offset -= 8;
			uint8_t byte;
			_window[0] = _window[1];
			_window[1] = _window[2];
			_fillByte[0] = _fillByte[1];
			_fillByte[1] = false;
			if ( _block > 9 ) return;
top:
			Read( _is, byte );
			if ( byte == 0xFF ) {
				uint8_t flag;
				Read( _is, flag );
				if ( flag == 0x00 ) {
					_window[2] = 0xFF;
				} else {
					_fillByte[1] = true;
					if ( flag == static_cast<uint8_t>( Flags::END ) ) {
						_block = flag;
					} else {
						sync( flag );
						goto top;
					}
				}
			} else {
				_window[2] = byte;
			}
		}
		uint16_t peek16Bit() {
			return  ( _window[0] << ( 8 + _offset ) )
				+ ( _window[1] << _offset )
				+ ( _window[2] >> ( 8 - _offset ) );
		}
	public:
		uint8_t	GetResetCount() { return _block; }
		BitItr& operator++() {
			++_offset;
			if ( _offset > 7 ) nextByte();
			return *this;
		}

		BitItr& operator+=( std::size_t x ) {
			for ( std::size_t i = 0; i < x; ++i ) { this->operator++(); }
			return *this;
		}

		uint8_t nextHuff( const HuffTable& huff ) {
			uint16_t data;
			if ( _fillByte[0] && ( _window[0] ^ ( 0xFF >> _offset ) ) ) {
#ifdef DEBUG
				assert( _offset < 8 );
#endif // DEBUG

				_offset = 8;
				nextByte();
			}
			std::array<uint16_t, 3> between;
			data = peek16Bit();
			std::array<uint8_t, 3> snap = _window;
			std::size_t off = _offset;
			return huff.decode( data, *this );
		}
		template<typename T>
		T read( std::size_t len ) {
			if ( len == 0 || len > 32 ) return 0;
			if ( len > 16 ) {
				throw "to long!";
			}
			T res = peek16Bit();
			*this += len;
			res = ( res >> ( 16 - len ) )& ( 0xFFFF >> ( 16 - len ) );
			if ( res >> ( len - 1 ) == 0 ) { // signed
				res -= ( 0b1 << len ) - 1;
			}
			// std::cout << "read len: " << len << " val: " << res << '\n';
			return res;
		}
		BitItr( std::istream& is, const Block<Flags::DATA>& block )
			: _is{ is } {
			is.seekg( block.start );
			Read( is, _window );
		}
	private:
		std::array<uint8_t, 3> _window{ 0,0,0 };
		std::array<bool, 2> _fillByte{ false, false };
		uint8_t _offset{ 0 };
		uint8_t _block{ 0 };
		std::istream& _is;
	};
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

		result.data.start = input.tellg();
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
			channel.insert( itr.read<int>( len ) * quad.dc() );

			skipAc( itr, huffAc );
		} else {
			const JPGDecomposition::Progressive& prog = progressive.value();
			if ( prog.Ss != 0 ) { throw "only supports DC encoding"; }
			uint8_t len = itr.nextHuff( huffDc );
			channel.insert( ( itr.read<int>( len ) << prog.Al ) * quad.dc() ); // FIXME: add support to ignore wrong SOS's
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
		data.input.seekg( data.data.start );
		Dim<std::size_t> pos;
		std::variant<std::array<DuIterator, 3>, std::array<DuIterator, 1>> channels{};
		if ( data.colorEncoding == ColorEncoding::YCrCb8 ) {
			for ( int i = 0; i < 3; ++i ) {
				std::get<std::array<DuIterator, 3>>( channels ).at( i ) = DuIterator( data, i, picture.GetData( i ) );
			}
		} else {
			std::get<std::array<DuIterator, 1>>( channels ).at( 0 ) = DuIterator( data, 0, picture.GetData( 0 ) );
		}
		BitItr dataItr( data.input, data.data );
		for ( pos.y = 0; pos.y < data.size.y; pos.y += data.mcuSize.y) {
			for ( pos.x = 0; pos.x < data.size.x;) {
				if ( data.colorEncoding == ColorEncoding::YCrCb8 ) {
					DecodeMCU( pos, data, dataItr, std::get<std::array<DuIterator, 3>>( channels ) );
				} else {
					DecodeMCU( pos, data, dataItr, std::get<std::array<DuIterator, 1>>( channels ) );
				}
			}
		}
		std::cout << "start add reading\n";
		std::array<DuIterator, 3> dummys = {DuIterator(), DuIterator(), DuIterator()};
		for ( unsigned int i = 0; i < 400; ++i ) {
			DecodeMCU( pos, data, dataItr, dummys );
			if ( dataItr.GetResetCount() > 9 ) {
				std::cout << "additionals: " << i << '\n';
				break;
			}
		}
		std::cout << "end" << data.input.tellg() << '\n';
		if ( !data.progressive ) {
			assert(static_cast<Flags>(dataItr.GetResetCount()) == Flags::END);
		}
	}

}
