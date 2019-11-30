#include "fJPG.hpp"

#include "HuffTable.hpp"
#include "QuadTable.hpp"
#include "Util.hpp"

#include <istream>

namespace fJPG {

	enum struct Flags : uint8_t {
		DATA = 0xDA,
		END = 0xD9,
		SOI = 0xD8
	};
	

	template<ColorEncoding C>
	class EditablePicture : public Picture<C> {
	public:
		EditablePicture(Dim<std::size_t> size, std::size_t channels)
			: Picture<C>(size, channels){}

		auto GetChanel(std::size_t x) {
			return GetChanel(x);
		}
	};

	template <Flags TYPE>
	struct Block {
		static constexpr uint8_t VALUE = static_cast<uint8_t>(TYPE);
		std::streampos start{0};
	};


	struct ChannelInfo {
		uint8_t sampV{ 0 }, sampH{ 0 }, quadTable{ 0 }, huffAc{ 0 }, huffDc{ 0 };
		int sampN {sampV * sampH};
	}; 
	

	struct JPGDecomposition {
		JPGDecomposition(std::istream& inputStream) : input{inputStream}{}
		static constexpr size_t Max_Channels = 3;
		Dim<std::size_t> size;
		Dim<uint8_t> mcuSize;
		std::size_t nChannel{0};
		ColorEncoding colorEncoding;
		std::array<ChannelInfo, Max_Channels> channelInfos;
		std::array<QuadTable, Max_Channels> quadTables;
		std::array<HuffTable, Max_Channels*2> huffTables;
		Block<Flags::DATA> data;	
		std::istream& input;
	};

	JPGDecomposition Decompose(std::istream&);
	void Decode(EditablePicture<ColorEncoding::YCrCb8>&, const JPGDecomposition&);

	Picture<ColorEncoding::YCrCb8> Convert(std::istream& input) {
		const JPGDecomposition data = Decompose(input);
		
		EditablePicture<ColorEncoding::YCrCb8> 
			picture(data.size, data.nChannel);
		
		Decode(picture, data);

		return std::move(picture);
	}

	class DuIterator {
	public:
		DuIterator(const ChannelInfo& info)
			: _info{info}{}

		void insert(int diff) {
			_diff += diff;

		}
	private:
		Dim<uint8_t> _channelSamp;
		Dim<uint8_t> _mcuSize;
		int _diff{0};
	};

	class BitItr {
		void sync(uint8_t flag) {
#ifdef DEBUG
			assert((flag & 0xF0) == 0xD0);
			assert((flag & 0x0F) == 0 || (flag & 0x0F) == (_block + 1));
#endif
			_block = flag & 0x0F;
		}
		void nextByte() {
			uint8_t byte;
			_window[0] = _window[1];
			_window[1] = _window[2];
top:
			Read(_is, byte);
			if(byte == 0xFF) {
				uint8_t flag;
				Read(_is, flag);
				if(flag == 0x00) {
					_window[2] = 0xFF;
				} else {
					sync(flag);
					goto top;
				}
			} else {
				_window[2] = byte;
			}
			_offset = 0;
		}
	public:
		uint8_t	GetResetCount() { return _block; }
		BitItr& operator++() {
			++_offset;
			if(_offset > 7) nextByte();
		}

		BitItr& operator+=(std::size_t x) {
			for(std::size_t i = 0; i < x; ++i){this->operator++();}	
		}

		uint8_t nextHuff(const HuffTable& huff) {
			uint16_t data;
			data = 
					(_window[0] << (8 + _offset))
				+ (_window[1] << _offset) 
				+ (_window[2] & (0xFF >> (8-_offset)));
			return huff.decode(data, *this);
		}
	private:
		uint8_t _window[3];
		uint8_t _offset;
		uint8_t _block;
		std::istream& _is;
	};

	void Decode(Flags flag, JPGDecomposition& data) {
		switch(flag) {
			default:
				throw std::string("unknown flag: ") + std::to_string((int)flag);
		}
	}

	Flags ReadFlag(std::istream& is) {
		uint8_t flag[2];
		Read(is, flag, 2);
#ifdef DEBUG
		assert(flag[0] == 0xFF);
#endif
		return static_cast<Flags>(flag[1]);
	}

	JPGDecomposition Decompose(std::istream& input) {
		{
			assert(ReadFlag(input) == Flags::SOI);
		}
		JPGDecomposition result(input);
		Flags flag = ReadFlag(input);
		do{
			Decode(flag, result);
		}while ((flag = ReadFlag(input)) != Flags::DATA);
		result.data.start = input.tellg();
		return result;
	}
	
	/**
	 *	@brief read and decode DU.
	*/
	template<typename T>
	void DecodeDu(DuIterator& channel, BitItr& itr, const HuffTable& huffDc, const HuffTable& huffAc, const QuadTable& quad) {
		uint8_t len = itr.nextHuff(huffDc);
		channel.insert(itr.read<int>(len));

		for(std::size_t i = 1; i < 64; ++i) {
			uint8_t byte = itr.nextHuff(huffAc);
			i += (byte & 0xF0) >> 4;
			itr += (byte & 0x0F);
		}
	}

	template<typename T>
	void DecodeMCU(const JPGDecomposition& data, T(&channels)[3]) {
		for(std::size_t c = 0; c < 3; ++c) {
			for(std::size_t i = 0; i < data.channelInfos[c].sampN; ++i) {
				DecodeDU(
						channels[c],
						data.huffTables[data.channelInfos[c].huffDc],
						data.huffTables[data.channelInfos[c].huffAc],
						data.quadTables[data.channelInfos[c].quadTable]);
			}
		}
	}

	template<typename T>
	void DecodeMCU(const JPGDecomposition& data, T(&channels)[1]) {
		DecodeDU(
					channels[0],
					data.huffTables[data.channelInfos[0].huffDc],
					data.huffTables[data.channelInfos[0].huffAC],
					data.quadTables[data.channelInfos[0].quadTable]);
	}

	void Decode(EditablePicture<ColorEncoding::YCrCb8>& picture, const JPGDecomposition& data) {
		data.input.seekg(data.data.start);	
		Dim<std::size_t> pos;
		union Channels {
			DuIterator ycrcb[3];
			DuIterator bw[1];
		} channels;
		if(data.colorEncoding == ColorEncoding::YCrCb8) {
			for(int i = 0; i < 3; ++i){
				channels.ycrcb[i] = DuIterator(data.channelInfos[i], picture.GetChannel(i));
			}
		} else {
			channels.bw[0] = DuIterator(data.channelInfos[0], picture.GetChannel(0));
		}

		for(pos.y = 0; pos.y < data.size.y; pos.y += data.mcuSize.y) {
			for(pos.x = 0; pos.x < data.size.x; pos.x += data.mcuSize.x) {
				if(data.colorEncoding == ColorEncoding::YCrCb8) {
					DecodeMCU(data, picture, channels.ycrcb);	
				} else {
					DecodeMCU(data, picture, channels.bw);
				}
			}
		}
		uint8_t flag[2];
		Read(is, flag, 2);
		assert(flag[0] == 0xFF && flag[1] == 0xd9);
	}

}
