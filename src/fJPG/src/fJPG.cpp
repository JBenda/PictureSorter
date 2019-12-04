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
		EditablePicture(Dim<std::size_t> size, std::size_t channels)
			: Picture<C>(size, channels){}

		auto GetData(std::size_t x) {
			return Picture<C>::GetChannel(x);
		}
	};

	template <Flags TYPE>
	struct Block {
		static constexpr uint8_t VALUE = static_cast<uint8_t>(TYPE);
		std::streampos start{0};
	};


	struct ChannelInfo {
		uint8_t sampV{ 0 }, sampH{ 0 }, quadTable{ 0 }, huffAc{ 0 }, huffDc{ 0 };
	}; 
	

	struct JPGDecomposition {
		JPGDecomposition(std::istream& inputStream) : input{inputStream}{}
		static constexpr size_t Max_Channels = 3;
		Dim<std::size_t> size;
		Dim<uint8_t> mcuSize;
		Dim<uint8_t> mcuSamp;
		Dim<std::size_t> nMcus;
		std::size_t nChannel{0};
		ColorEncoding colorEncoding{ColorEncoding::YCrCb8};
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

	JPGDecomposition Decompose(std::istream&);
	void Decode(EditablePicture<ColorEncoding::YCrCb8>&, const JPGDecomposition&);

	Picture<ColorEncoding::YCrCb8> Convert(std::istream& input) {
		JPGDecomposition data = Decompose(input);
		std::size_t w = data.size.x / data.mcuSize.x;
		if (w * data.mcuSize.x != data.size.x) w += data.mcuSamp.x;

		std::size_t h = data.size.y / data.mcuSize.y;
		if (h * data.mcuSize.y != data.size.y) h += data.mcuSamp.y;
		data.nMcus = {w, h};
		EditablePicture<ColorEncoding::YCrCb8> 
			picture(Dim<std::size_t>(w * data.mcuSamp.x, h * data.mcuSamp.y), data.nChannel);
		
		Decode(picture, data);

		return picture;
	}

class DuIterator {
	public:
		DuIterator(const JPGDecomposition& base, std::size_t channelId, Channel<std::vector<color_t>::iterator>& channel)
		:
			_samp{base.channelInfos[channelId].sampH, base.channelInfos[channelId].sampV},
			_mcuSamp{base.mcuSamp},
			_nMcu{base.nMcus},
			_channel{channel},
			_itr{ _channel.begin() }{
#ifdef DEBUG
			_id = channelId;
			assert(_channel.end() - _channel.begin() == base.nMcus.x * base.nMcus.y * base.mcuSamp.x * base.mcuSamp.y);

			uint8_t ratio;
			assert(ratio = _mcuSamp.x / _samp.x, !(ratio == 0) && !(ratio & (ratio - 1)));
			assert(ratio = _mcuSamp.y / _samp.y, !(ratio == 0) && !(ratio & (ratio - 1)));
#endif // DEBUG

		}

		DuIterator() = default;

		void insert(int diff) {
			_diff += diff;

			do {
#ifdef DEBUG
				assert(_itr != _channel.end());
				assert(*_itr == 0);
				if (_id == 0) std::cout << (_itr - _channel.begin()) << '\n';
#endif // DEBUG
				* _itr = _diff / 8 + static_cast<int>(std::numeric_limits<color_t>::max() / 2 + 1);
				++ _itr;
				if (++_local.x == _mcuSamp.x) {
					const std::size_t w = _nMcu.x * _mcuSamp.x;
					_itr += w - 2;
					_local.x = 0;
					if (++_local.y == _mcuSamp.y) {
						_itr -= (w - 1) * 2;
						_local.y = 0;
						if (++_corner.x == _nMcu.x ) {
							_corner.x = 0;
							_itr += w;
#ifdef DEBUG
							if (++_corner.y == _nMcu.y) {
								assert(_itr == _channel.end());
							}
#endif // DEBUG
						}
					}
				}
			} while (_itr != _channel.end() && (_local.x >= _samp.x || _local.y >= _samp.y));

		}
	private:
		Dim<uint8_t> _samp;
		Dim<uint8_t> _mcuSamp;
		Dim<std::size_t> _nMcu;
		Channel<std::vector<color_t>::iterator> _channel;
		std::vector<color_t>::iterator _itr;
		Dim<uint8_t> _local{ 0,0 };
		Dim<std::size_t> _corner{ 0,0 };
		int _diff{ 0 };
#ifdef DEBUG
		std::size_t _id;
#endif // DEBUG

	};

	class BitItr {
		void sync(uint8_t flag) {
#ifdef DEBUG
			// assert((flag & 0xF0) == 0xD0); // only for sequential
			// assert((flag & 0x0F) == 0 || (flag & 0x0F) == (_block + 1));
#endif
			_block = flag & 0x0F;
		}
		void nextByte() {
			uint8_t byte;
			_window[0] = _window[1];
			_window[1] = _window[2];
			_fillByte[0] = _fillByte[1];
			_fillByte[1] = false;
			if (_block > 9) return;
top:
			Read(_is, byte);
			if(byte == 0xFF) {
				uint8_t flag;
				Read(_is, flag);
				if(flag == 0x00) {
					_window[2] = 0xFF;
				} else {
					_fillByte[1] = true;
					if (flag == static_cast<uint8_t>(Flags::END)) {
						_block = flag;
					} else {
						sync(flag);
						goto top;
					}
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
			return *this;
		}

		BitItr& operator+=(std::size_t x) {
			for(std::size_t i = 0; i < x; ++i){this->operator++();}	
			return *this;
		}

		uint8_t nextHuff(const HuffTable& huff) {
			uint16_t data;
			if (_fillByte[0] && (_window[0] ^ (0xFF >> _offset))) {
				nextByte();
			}
			data = 
					(_window[0] << (8 + _offset))
				+ (_window[1] << _offset) 
				+ (_window[2] & (0xFF >> (8-_offset)));
			return huff.decode(data, *this);
		}
		template<typename T>
		T read(std::size_t len) {
			if (len == 0 || len > 32) return 0;
			if (len > 16) {
				throw "to long!";
			}
			T res = (_window[0] << (8 + _offset))
				+ (_window[1] << _offset)
				+ (_window[2] & (0xFF >> (8-_offset)));
			*this += len;
			res = (res >> (16 - len)) & (0xFFFF >> (16 - len));
			if (res >> (len - 1) == 0) { // signed
				res -= (0b1 << len) - 1;
			}
			// std::cout << "read len: " << len << " val: " << res << '\n';
			return res;
		}
		BitItr(std::istream& is, const Block<Flags::DATA>& block)
			: _is{ is } {
			is.seekg(block.start);
			Read(is, _window);
		}
	private:
		std::array<uint8_t, 3> _window{0,0,0};
		std::array<bool, 2> _fillByte{false, false};
		uint8_t _offset{0};
		uint8_t _block{0};
		std::istream& _is;
	};
	template<Flags F>
	void Parse(JPGDecomposition& /*data*/) { throw "not implimented yet"; }

	template<>
	void Parse<Flags::APP0>(JPGDecomposition& data) {
#ifdef DEBUG
		auto start = data.input.tellg();
#endif
		uint16_t len = ReadLen(data.input);
		constexpr std::size_t headerSize = 4 + 2 + 1 + 2 + 2;
#ifdef DEBUG
		std::array<char, 5> title;
		Read(data.input, title);
		assert(std::string_view(title.data()) == "JFIF");
		uint8_t num = 0;
		Read(data.input, num);
		assert(num == 1);
		Read(data.input, num);
		assert(num >= 0 && num <= 2); // weak cretarie
		Skip(data.input, 1 + 2 + 2); // density (unit, x, y)

		uint8_t x, y;
		Read(data.input, x);
		Read(data.input, y);
		Skip(data.input, static_cast<std::size_t>(x) * static_cast<std::size_t>(y) * 3); // Skip Thumbnail
		std::size_t diff = data.input.tellg() - start;
		assert(diff == len);
#else
		Skip(data.input, len);
#endif
	}

	template<>
	void Parse <Flags::APP1>(JPGDecomposition& data) {
		uint16_t len = ReadLen(data.input);
		Skip(data.input, len - 2);
	}

	template<>
	void Parse<Flags::DQT>(JPGDecomposition& data) {
		auto start = data.input.tellg();
		std::size_t diff = 0;

		uint16_t len = ReadLen(data.input);
		do{
			QuadTable qt;
			data.input >> qt;
			data.quadTables[qt.id()] = qt;
			diff = data.input.tellg() - start;
		} while (diff < len);
#ifdef DEBUG
		assert(diff == len);
#endif
	}

	template<>
	void Parse<Flags::SOF0>(JPGDecomposition& data) {
#ifdef DEBUG
		auto start = data.input.tellg();
#endif
		data.progressive.reset();
		const uint16_t len = ReadLen(data.input);
		uint8_t colorPrcission;
		Read(data.input, colorPrcission);
		data.size = Dim<std::size_t>(ReadLen(data.input), ReadLen(data.input));
		uint8_t colorChannels;
		Read(data.input, colorChannels);
		data.nChannel = colorChannels;
		if (colorPrcission == 8 && colorChannels == 3) {
			data.colorEncoding = ColorEncoding::YCrCb8;
			std::array<uint8_t, 3> meta;
			for (int i = 0; i < 3; ++i) {
				Read(data.input, meta);
				ChannelInfo& channel = data.channelInfos[meta[0] - 1];
				channel.sampV = meta[1] & 0x0F;
				channel.sampH = (meta[1] & 0xF0) >> 4;
				channel.quadTable = meta[2];
			}
		} else if (colorPrcission == 8 && colorChannels == 1) {
			data.colorEncoding = ColorEncoding::BW8;
		} else {
			throw "unksupported color encoding";
		}
#ifdef DEBUG
		std::size_t diff = data.input.tellg() - start;
		assert(diff == len);
#endif // DEBUG
	}

	template<>
	void Parse<Flags::SOF2>(JPGDecomposition& data) {
		Parse<Flags::SOF0>(data);
		data.progressive.emplace();
	}

	template<>
	void Parse<Flags::DHT>(JPGDecomposition& data) {
		auto start = data.input.tellg();
		std::size_t diff = data.input.tellg() - start;
		const uint16_t len = ReadLen(data.input);
		do {
			HuffTable::Header header = HuffTable::ParseHeader(data.input);
			if (header.isAC) {
				data.input >> data.huffTablesAc[header.id];
			} else {
				data.input >> data.huffTablesDc[header.id];
			}
			diff = data.input.tellg() - start;
		} while (len < data.input.tellg() - start);

#ifdef DEBUG
		
		assert(diff == len);
#endif
	}

	template<>
	void Parse<Flags::DATA>(JPGDecomposition& data) {
#ifdef DEBUG
		auto start = data.input.tellg();
#endif // DEBUG
		const uint16_t len = ReadLen(data.input);
		uint8_t nC;
		Read(data.input, nC);
#ifdef DEBUG
		if (!data.progressive) {
			assert(nC == data.nChannel);
			assert(len == 6 + 2 * data.nChannel);
		}
#else
		Skip(data.input, 1);
#endif // DEBUG
		if (nC != data.nChannel) { Skip(data.input, nC * 2); } // TODO: more reliable progressive
		else {
			for (unsigned int i = 0; i < data.nChannel; ++i) {
				std::array<uint8_t, 2> channelInfo;
				Read(data.input, channelInfo);
				ChannelInfo& cI = data.channelInfos[channelInfo[0] - 1];
				cI.huffAc = channelInfo[1] & 0x0F;
				cI.huffDc = (channelInfo[1] & 0xF0) >> 4;
#ifdef DEBUG
				assert(cI.huffAc >= 0 && cI.huffAc <= 3);
				assert(cI.huffDc >= 0 && cI.huffDc <= 3);
#endif // DEBUG
			}
		}
		if (!data.progressive) {
			Skip(data.input, 3);
		} else {
			std::array<uint8_t, 3> meta;
			Read(data.input, meta);
			JPGDecomposition::Progressive& p = data.progressive.value();
			p.Ss = meta[0];
			p.Se = meta[1];
			p.Ah = (meta[2] & 0xF0) >> 4;
			p.Al = meta[2] & 0x0F;
		}

#ifdef DEBUG		
		std::size_t diff = data.input.tellg() - start;
		assert(diff == len);
#endif // DEBUG


	}

	template<>
	void Parse<Flags::COM>(JPGDecomposition& data) {
#ifdef DEBUG
		auto start = data.input.tellg();
#endif // DEBUG

		const uint16_t len = ReadLen(data.input);
#ifdef DEBUG
		std::vector<char> msg(len - (2 - 1));
		Read(data.input, msg.data(), len - 2);
		msg[len - 2] = 0;
		std::cout << "Found message: '" << msg.data() << "'\n";
		std::size_t diff = data.input.tellg() - start;
		assert(diff == len);
#else
		Skip(data.input, len - 2);
#endif // DEBUG

	}

	void Decode(Flags flag, JPGDecomposition& data) {
		switch(flag) {
		case Flags::APP0:
			Parse<Flags::APP0>(data);
			break;
		case Flags::APP1:
			Parse<Flags::APP1>(data);
			break;
		case Flags::DQT:
			Parse<Flags::DQT>(data);
			break;
		case Flags::SOF0:
			Parse<Flags::SOF0>(data);
			break;
		case Flags::SOF2:
			Parse<Flags::SOF2>(data);
			break;
		case Flags::DHT:
			Parse<Flags::DHT>(data);
			break;
		case Flags::DATA:
			Parse<Flags::DATA>(data);
			break;
		case Flags::COM:
			Parse<Flags::COM>(data);
			break;
		default:
			uint8_t f = static_cast<uint8_t>(flag);
			if ((f & 0xF0) == 0xC0) { // Start of Frames rather then 0 not interisting
#ifdef DEBUG
				std::cout << "unkonverted SOF\n";
#endif // DEBUG

				Skip(data.input, ReadLen(data.input) - 2);
			} else {
				throw std::string("unknown flag: ") + std::to_string((int)flag);
			}
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
		Flags flag;
		do{
			flag = ReadFlag(input);
			Decode(flag, result);
		}while (flag != Flags::DATA);
		
		result.data.start = input.tellg();
		result.mcuSamp = Dim<uint8_t>(0, 0);
		for (unsigned int i = 0; i < result.nChannel; ++i) {
			if (result.mcuSamp.x < result.channelInfos[i].sampH) {
				result.mcuSamp.x = result.channelInfos[i].sampH;
			}
			if (result.mcuSamp.y < result.channelInfos[i].sampV) {
				result.mcuSamp.y = result.channelInfos[i].sampV;
			}
		}
		result.mcuSize = Dim<uint8_t>(result.mcuSamp.x * 8, result.mcuSamp.y * 8);
		return result;
	}
	
	
	void skipAc(BitItr& itr, const HuffTable& acTable, std::size_t end = 63, std::size_t start = 1) {
		for (std::size_t i = start; i <= end; ++i) {
			uint8_t byte = itr.nextHuff(acTable);
			if (byte == 0) break;
			i += (byte & 0xF0) >> 4;
			itr += (byte & 0x0F);
		}
	}

	void DecodeDu(DuIterator& channel, BitItr& itr, const HuffTable& huffDc, const HuffTable& huffAc, const QuadTable& quad, const std::optional<JPGDecomposition::Progressive>& progressive) {
		if (!progressive) {
			uint8_t len = itr.nextHuff(huffDc);
			channel.insert(itr.read<int>(len) * quad.dc());

			skipAc(itr, huffAc);
		} else {
			const JPGDecomposition::Progressive& prog = progressive.value();
			if (prog.Ss != 0) { throw "only supports DC encoding"; }
			uint8_t len = itr.nextHuff(huffDc);
			channel.insert((itr.read<int>(len - 1) << 1) * quad.dc());

			skipAc(itr, huffAc, prog.Se, prog.Ss + 1);
		}
	}

	template<typename T>
	void DecodeMCU(const JPGDecomposition& data, BitItr& itr, std::array<T,3>& channels) {
		for(std::size_t c = 0; c < 3; ++c) {
			for(unsigned int i = 0; i < static_cast<unsigned int>(data.channelInfos[c].sampV * data.channelInfos[c].sampH); ++i) {
				DecodeDu(
					channels[c],
					itr,
					data.huffTablesDc[data.channelInfos[c].huffDc],
					data.huffTablesAc[data.channelInfos[c].huffAc],
					data.quadTables[data.channelInfos[c].quadTable],
					data.progressive);
			}
		}
	}

	template<typename T>
	void DecodeMCU(const JPGDecomposition& data, BitItr& itr, std::array<T,1>& channels) {

		DecodeDu(
				channels[0],
				itr,
				data.huffTablesDc[data.channelInfos[0].huffDc],
				data.huffTablesAc[data.channelInfos[0].huffAc],
				data.quadTables[data.channelInfos[0].quadTable],
				data.progressive);
	}

	void Decode(EditablePicture<ColorEncoding::YCrCb8>& picture, const JPGDecomposition& data) {
		data.input.seekg(data.data.start);	
		Dim<std::size_t> pos;
		std::variant<std::array<DuIterator, 3>, std::array<DuIterator, 1>> channels{};
		if(data.colorEncoding == ColorEncoding::YCrCb8) {
			for(int i = 0; i < 3; ++i){
				std::get<std::array<DuIterator, 3>>(channels).at(i) = DuIterator(data, i, picture.GetData(i));
			}
		} else {
			std::get<std::array<DuIterator, 1>>(channels).at(0) = DuIterator(data, 0, picture.GetData(0));
		}
		BitItr dataItr(data.input, data.data);
		for(pos.y = 0; pos.y < data.size.y; pos.y += data.mcuSize.y) {
			for(pos.x = 0; pos.x < data.size.x; pos.x += data.mcuSize.x) {
				if(data.colorEncoding == ColorEncoding::YCrCb8) {
					DecodeMCU(data, dataItr, std::get<std::array<DuIterator, 3>>(channels));	
				} else {
					DecodeMCU(data, dataItr, std::get<std::array<DuIterator, 1>>(channels));
				}
			}
		}
		std::cout << "end" << data.input.tellg() << '\n';
		if (!data.progressive) {
			// assert(static_cast<Flags>(dataItr.GetResetCount()) == Flags::END);
		}
	}

}
