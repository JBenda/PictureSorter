#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <cassert>
#include <optional>
#include <memory>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
// #include <CL/cl.h>
#endif

#define MAX_SOURCE_SIZE (0x100000)

namespace fs = std::filesystem;

class Image {
	typedef std::size_t size_t;
	typedef int chanel_t;
	typedef unsigned char huff_t;
	enum BLOCK_TYPES { UNDEF = 0x00, SOI = 0xd8 };
	const std::vector<char> data;
	class PictureData;
	std::unique_ptr<PictureData> picture;
	const unsigned char *ptr;
	std::vector<std::string> errorStack;
	size_t resartInterval = 0;
	int dcs[7]; // max chanle id == 7
	class DataItr {
		const unsigned char * const data;
		const unsigned char *itr;
		size_t offset; // bitwise
		void NextByte() {
			if (itr[0] == 0xFF && itr[1] == 0)
				itr += 2;
			else
				++itr;
		}
		void CutOffset() {
			for (; offset >= 8; offset -= 8) NextByte();
		}
	public:
		DataItr(const unsigned char *data) : data{ data }, itr{ data }, offset{ 0 } {}
		/** return number of whole bytes readad/processed
		 */
		size_t GetPosition() const {
			return (itr - data);
		}
		void Align() {
			if (offset) {
				assert(((~(*itr & (0xFF >> offset))) & (0xFF >> offset)) == 0);
				NextByte();
				offset = 0;
			}
		}
		/** attention byte align will be executet
		*/
		const unsigned char* GetPtr() {
			if (offset) {
				Align();
			}
			return itr;
		}
		/** check if next byte(s) a marker and all fill bits
		 * note if offset == 0, than check this byte
		 * retrun 0 no marker
		 * retrun marker if marker
		*/
		unsigned char CheckForMarker() {
			if (itr[offset ? 1 : 0] == 0xFF)
				return itr[offset ? 2 : 1];
			return 0x00;
		}
		DataItr& operator++() {
			++offset;
			if (offset == 8) {
				offset = 0;
				NextByte();
			}
			assert(offset < 8);
			return *this;
		}
		DataItr& operator+=(size_t offset) {
			this->offset += offset;
			while (this->offset >= 8) {
				this->offset -= 8;
				NextByte();
			}
			return *this;
		}
		bool ReadBit() {
			bool ret = (*itr & (0x80 >> offset));
			++*this;
			return ret;
		}
		bool PeekBit() const {
			return (*itr & (0x80 >> offset));
		}
		unsigned char ReadByte() {
			assert(offset == 0);
			unsigned char byte = *itr;
			NextByte();
			return byte;
		}
		unsigned char PeekByte() const {
			assert(offset == 0);
			return *itr;
		}
		bool ReadStream(size_t bits, unsigned char *dest) {
			unsigned char *itrD = dest;
			while (bits >= 8) {
				*itrD = ((0xFF >> offset) & *itr) << offset;
				NextByte();
				assert(!(*itrD & (*itr >> (8 - offset)))); // all zero
				*itrD |= *itr >> (8 - offset);
				++itrD;
				bits -= 8;
			}
			if (bits) {
				if (bits > 8 - offset) {
					*itrD = ((0xFF >> offset) & *itr) << offset;
					NextByte();
					bits -= 8 - offset;
					*itrD |= *itr >> (8 - bits);
					offset = bits;
				} else {
					*itrD = ((0xFF >> (8 - bits)) & *itr) << (8 - bits);
					offset += bits;
					CutOffset();
				}
			}
			assert(offset < 8);
			return true;
		}
	};
	class QuadTables {
		class QuadTable {
			std::vector<unsigned char> data;
			size_t prec;
		public:
			QuadTable(size_t precisson, const unsigned char* table) : data(precisson * 64), prec{ precisson } {
				std::memcpy(data.data(), table, precisson * 64);
			}
			int DeQuad(size_t pos, int val) const {
				if (prec == 1) return data[pos] * val;
				else if (prec == 2) {
					size_t p = pos << 1;
					return (data[p] << 8) + data[++p];
				}
				assert(false);
			}
		};
		std::vector<QuadTable> tables;
	public:
		QuadTables() : tables{} {}
		bool AddTable(size_t id, size_t precission, const unsigned char* data) {
			assert(id == tables.size());
			tables.emplace_back(precission, data);
			return true;
		}
		int DeQuad(size_t id, size_t pos, int val) const {
			assert(id < tables.size());
			return tables[id].DeQuad(pos, val);
		}
	} quadTables;
	class HuffmanTables {
		class HuffmanTabel
		{
			size_t len;
			unsigned char codeLen[17]; ///< offset for code len
			bool bValid;
			class CodePair {
				unsigned char data[3];
			public:
				void Set(size_t code, unsigned char byte) {
					data[0] = (code >> 8) & 0xFF;
					data[1] = code & 0xFF;
					data[2] = byte;
				}
				size_t GetCode() const { return FuseBytes(data); }
				huff_t GatData() const { return data[2]; }
			} codes[256];
			std::optional<huff_t> GetValue(size_t code, size_t len) const {
				if (len > 16)
					std::cerr << "spaq";
				const CodePair *begin = codes + codeLen[len - 1],
					*end = codes + codeLen[len];
				for (const CodePair* itr = begin; itr != end; ++itr) {
					if (itr->GetCode() == code)
						return std::optional<huff_t>(itr->GatData());
				}
				return std::optional<huff_t>();
			}
			size_t encodedSize;
		public:
			HuffmanTabel() : bValid{ false } {};
			size_t GetEncodedSize() { return encodedSize; }
			HuffmanTabel(const unsigned char* data) : bValid{ false }, len{0} {
				for (int i = 0; i < 16; ++i) {
					assert(len < 256);
					codeLen[i] = static_cast<unsigned char>(len);
					len += data[i];
				}
				if (len > 255) {
					assert(false);
				}
				codeLen[16] = static_cast<unsigned char>(len);
				size_t code = 0;
 				const unsigned char *bytes = data + 16;
				size_t lenCode = 1;
				for (size_t i = 0; i < len; ++i) {
					while (i == codeLen[lenCode]) {
						++lenCode;
						code <<= 1;
					}
					codes[i].Set(code, bytes[i]);
					// std::cout << '\t';
					// for (int j = lenCode - 1; j >= 0; --j) {
					// 	std::cout << (code & (0x01 << j) ? '1' : '0');
					// }
					// std::cout << " -> " << static_cast<int>(bytes[i]) << "\n";
					++code;
				}
				encodedSize = len + 16;
				bValid = true;
			}
			~HuffmanTabel() {}
			
			huff_t Decode(DataItr itr) const {
				return DecodeAndMove(itr);
			}
			// TODO: beter name
			/** decode nexthuffman value and adds len to pos
			*/
			huff_t DecodeAndMove(DataItr& itr) const {
				assert(bValid);
				bool bit;
				size_t code = 0;
				size_t len = 0;
				bool sticky = true;
				do {
					bit = itr.ReadBit();
					code = (code << 1) + (bit ? 1 : 0);
					++len;
					if (sticky && !bit) sticky = false;
					if (!sticky) {
						std::optional<huff_t> res = GetValue(code, len);
						if (res) {
							return res.value();
						}
					}
				} while (len < 16);
				std::cerr << "huffman code too long!\n";
				assert(false); // should never reached
			}
		};
		std::vector<HuffmanTabel> tables;
		std::vector<unsigned char> acTabeles; ///< 0xFF == not set
		std::vector<unsigned char> dcTabeles;
	public:
		HuffmanTables() : tables(6) {}
		/** return: bytes readed */
		size_t AddTable(const unsigned char* data) {
			DATA_TYPE type = data[0] & 0xF0 ? AC : DC;
			size_t id = data[0] & 0x0F;

			std::cout << "type: " << ( type == AC ? "ac" : "dc") << "\tid: " << id << "\n";
			tables.push_back(HuffmanTabel(data + 1));
			
			std::vector<unsigned char>& tRef = type == AC ? acTabeles : dcTabeles;
			tRef.resize(id + 1, 0xFF);
			tRef[id] = static_cast<unsigned char>(tables.size() - 1);

			return tables.back().GetEncodedSize() + 2;
		}
		enum DATA_TYPE {AC, DC};
		huff_t DecodeAndMove(size_t id, DATA_TYPE type, DataItr& itr) const {
			if (type == AC) return tables[acTabeles[id]].DecodeAndMove(itr);
			else if (type == DC) return tables[dcTabeles[id]].DecodeAndMove(itr);
			assert(false);
			return 0;
		}
	} huffmanTables;
	struct Infos {
		size_t precission, width, height, numComponents, duPerMCU, ///< data units (a 64 encoded values) per mcu
			pxPerMcu, chanleWithHigestSample;
		struct Components
		{
			unsigned char id, sampV, sampH, qautTable, huffTableAc, huffTableDc;
			bool operator()(const Components& c1, const Components& c2) {
				return c1.id < c2.id;
			}
		} componenst[3];
		friend std::ostream& operator<<(std::ostream& os, const Infos& obj) {
			os << "imgInfo:\n\tpercission: " << obj.precission
				<< "\n\tw: " << obj.width
				<< "\n\th: " << obj.height
				<< "\n\tchannels: " << obj.numComponents;
			for (std::size_t i = 0; i < obj.numComponents; ++i) {
				os << "\n\t\t id:sampleH:sampleV " << static_cast<int>(obj.componenst[i].id) << ':' << static_cast<int>(obj.componenst[i].sampH) << ':' << static_cast<int>(obj.componenst[i].sampV);
			}
			os  << "du per mcu: " << obj.duPerMCU
				<< "\n";
			return os;
		}
	} info;
	struct Density
	{
		size_t x, y;
		enum TYPE { NO, DPER_INCH, DPER_CM} unit;
	} density;
	enum DECODE_DATA {DENSITY, INFO, LAST};
	bool bSet[DECODE_DATA::LAST];
	static size_t FuseBytes(const unsigned char *p) {
		return (static_cast<size_t>(p[0]) << 8) + static_cast<size_t>(p[1]);
	}
	size_t GetLen() const {
		return FuseBytes(ptr + 2);
	}
	bool ParseAPP0(size_t len) {
		const char* jifi = reinterpret_cast<const char*>(ptr + 4);
		if (strcmp(jifi, "JFIF")) {
			errorStack.push_back("app0 don't includes jifi");
			return false;
		}
		if (ptr[9] != 1) {
			errorStack.push_back("Major version nuber don't match to 1 " + std::to_string(ptr[9]));
			return false;
		}
		if (ptr[10] > 2) {
			errorStack.push_back("Minor version number missmatch (assenr <= 2) got: " + std::to_string(ptr[10]));
			return false;
		}

		density.unit = Density::TYPE(ptr[11]);
		density.x = FuseBytes(ptr + 12);
		density.y = FuseBytes(ptr + 14);

		// ignore thumbnail

		return true;
	}
	bool ParseSOF(size_t len) {
		const unsigned char *itr = ptr + 4;
		info = { 0 };
		info.precission = *itr; ++itr;
		std::cout << info.precission << " posit\n";
		info.height = FuseBytes(itr); itr += 2;
		info.width = FuseBytes(itr); itr += 2;
		info.numComponents = static_cast<size_t>(*itr); ++itr;
		info.duPerMCU = 0;
		size_t maxSampH = 0, maxSampV = 0, maxSamp = 0;
		for (size_t i = 0; i < info.numComponents; ++i) {
			info.componenst[i].id = *(itr++);
			info.componenst[i].sampV = (*itr) & 0x0F;
			info.componenst[i].sampH = ((*itr) & 0xF0) >> 4;
			if (info.componenst[i].sampH > maxSampH) maxSampH = info.componenst[i].sampH;
			if (info.componenst[i].sampV > maxSampV) maxSampV = info.componenst[i].sampV;
			if (info.componenst[i].sampH * info.componenst[i].sampV > maxSamp) {
				maxSamp = info.componenst[i].sampH * info.componenst[i].sampV;
				info.chanleWithHigestSample = i;
			}
			info.duPerMCU +=
				info.componenst[i].sampH
				* info.componenst[i].sampV;
			++itr;
			info.componenst[i].qautTable = static_cast<unsigned char>(*(itr++));
		}
		std::sort(info.componenst, info.componenst + 3, Infos::Components());
		info.pxPerMcu = 64 * maxSampV * maxSampH;
		if (itr - ptr - 2 != len) {
			errorStack.push_back("Wrong leng encoded: "
				+ std::to_string(itr - ptr + 2)
				+ " vs " + std::to_string(len));
			return false;
		}
		picture = std::make_unique<PictureData>(PictureData::COLOR_TYPE::COLOR, info.width, info.height, info.componenst[info.chanleWithHigestSample].sampH, info.componenst[info.chanleWithHigestSample].sampV);
		return true;
	}
	
	bool ParseHuffmanTable(size_t len) {
		for (size_t l = 4; l < len + 2; l += huffmanTables.AddTable(ptr + l));
		return true;
	}

	/** dcodec integer from bit stream
	 * @param begin first bit
	 * @param end first bit not included int integer
	 * @return decoded integer
	*/
	int ReadValue(DataItr& itr, size_t len) {
		assert(len <= 16);
		bool sign = !itr.PeekBit();
		int res = 0;
		unsigned char *raw = reinterpret_cast<unsigned char*>(&res);
		itr.ReadStream(len, raw);
		if (len <= 8) {
			res >>= 8 - len;
		} else if (len <= 16) {
			unsigned char b = raw[0];
			raw[0] = raw[1];
			raw[1] = b;
			res >>= 16 - len;
		}
		if (sign) {
			res -= (0x01 << len) - 1;
		}
		return res;
	}

	struct DataUnit
	{
		unsigned char chanleId;
		chanel_t value;
	};
	class PictureData {
		std::vector<chanel_t> channles[3];
		struct ImageData {
			const size_t h, w, mSamX, mSamY, sampleBlock;
			ImageData(size_t heigh, size_t width, size_t maxSampX, size_t maxSampY, size_t sampPerBlock)
				: h{ heigh / 8}, w{ width / 8}, mSamX{ maxSampX }, mSamY{ maxSampY }, sampleBlock{sampPerBlock} {}
		} data;
		class QuadItr {
			size_t size, pos, colC, rowC, w;
			const ImageData& data;
		public:
			const size_t& GetPos() { return pos; }
			const size_t& GetSize() { return size; }
			QuadItr(const ImageData& pic) : data{ pic }, size{ 0 }, pos{ 0 }, colC{ 0 }, rowC{ 0 }, w{ 0 } {}
			QuadItr& operator++() {
				++size;
				++pos;
				++colC;
				
				if (colC == data.mSamX) {
					pos += data.w - data.mSamX;
					colC = 0;
					++rowC;
				}
				if (rowC == data.mSamY) {
					pos -= data.w * data.mSamY;
					pos += data.mSamX;
					rowC = 0;
					w += data.mSamX;
				}
				if (w == data.w) {
					pos += data.w * (data.mSamY - 1);
					w = 0;
				}
				return *this;
			}
		};
		std::vector<QuadItr> written;
		QuadItr& GetWritten(size_t id) {
			if (cType == COLOR_TYPE::COLOR) {
				--id;
				assert(id < 3);
				return written[id];
			}
			else {
				id -= 4;
				assert(id < 1);
				return written[id];
			}
		}
	public:
		friend std::ostream& operator<<(std::ostream& os, const PictureData& obj) {
			if (obj.cType == obj.COLOR) {
				os << "ColorImage: "
					<< "\n\tBrigthness cannle:\n\t\t";
				for (const std::size_t& c : obj.channles[0]) os << static_cast<int>(c) << ' ';
				os << "\n\tcr:\n\t\t";
				for (const std::size_t& c : obj.channles[1]) os << static_cast<int>(c) << ' ';
				os << "\n\tcg:\n\t\t";
				for (const std::size_t& c : obj.channles[2]) os << static_cast<int>(c) << ' ';
				os << "\n\t rgb:";
				int *r, *g, *b;
				r = new int[obj.channles[0].size()];
				g = new int[obj.channles[0].size()];
				b = new int[obj.channles[0].size()];
				for (int i = 0; i < obj.channles[0].size(); ++i) {
					r[i] = obj.channles[0][i] + (1.402 * static_cast<float>(obj.channles[2][i] - 128));
					g[i] = obj.channles[0][i] - (0.34414 * static_cast<float>(obj.channles[1][i] - 128)) - (0.71414 * static_cast<float>(obj.channles[2][i] - 128));
					b[i] = obj.channles[0][i] + (1.772 * static_cast<float>(obj.channles[1][i] - 128));
				}
				std::cout << "rgb:\n\ttop left: " << r[0] << ":" << g[0] << ":" << b[0] << "\n";
			} else {
				os << "BW Image: "
					<< "\n\tBrigthness cannle:\n\t\t";
				for (const unsigned char& c : obj.channles[0]) os << static_cast<int>(c) << ' ';
				os << "\n";
			}
			return os;
		}
		const enum COLOR_TYPE { COLOR, BW } cType;
		PictureData(enum COLOR_TYPE t, size_t width, size_t height, size_t maxSampleHor, size_t maxSampleVer)
			: cType{ t }, data{ height, width, maxSampleHor, maxSampleVer, maxSampleHor * maxSampleVer } { // TODO: init stuff
			written.push_back(data);
			written.push_back(data);
			written.push_back(data);
			width >>= 3;
			height >>= 3;
			if (width % 2) ++width;
			if (height % 2) ++height;
			size_t size = width * height;
			if (cType == COLOR) {
				channles[0].resize(size);
				channles[1].resize(size);
				channles[2].resize(size);
			} else if (cType == BW) {
				channles[0].resize(size);
			} else assert(false);
		}
		void AddValues(size_t id, chanel_t val, size_t times = 1) {
			assert((id == 1 && times == 1) || times == 4);
			static size_t count = 0;
			if (id == 1) ++count;
			std::vector<chanel_t>& chanel = GetChanel(id);
			QuadItr& itr = GetWritten(id);
			for (int i = 0; i < times; ++i, ++itr) {
				chanel[itr.GetPos()] = val;
			}
		}
		std::vector<chanel_t>& GetChanel(size_t id) {
			if(cType == COLOR_TYPE::COLOR) {
				--id;
				assert(id < 3);
				return channles[id];
			}
			else {
				id -= 4;
				assert(id < 1);
				return channles[id];
			}
		}
	};
	/** @param id channle id */
	void ParseDU(const Infos::Components& comp, DataItr& itr, int* dcs, DataUnit& du) {
		// encode dc value
		// jump over ac values
		huff_t dcLen = huffmanTables.DecodeAndMove(comp.huffTableDc, HuffmanTables::DC, itr);
		unsigned char acValueCount = 0;
		assert(dcLen <= 16);
		int dcDiff = 0;

		size_t posBuffer = itr.GetPosition();
		if (posBuffer == 369)
			std::cout << "got it\n";
		dcDiff = ReadValue(itr, dcLen);
		
		dcs[comp.id] += dcDiff;
		du.value = (quadTables.DeQuad(comp.qautTable, 0, dcs[comp.id]) / 8) + 128;
		// if (posBuffer > 20000) std::cerr << "4ooo\n";
		if (du.value < 0) {
			// std::cout << "erre: " << std::dec << (int)comp.id << ", d = " << du.value << '\n';
			// if (du.value < -30)
				// std::cout << std::dec << du.value << " offset\n";
			du.value = 0;
		} else if (du.value > 255 ){
			// std::cout << "erre: " << std::dec << (int)comp.id << ", d = " << (du.value - 255) << '\n';
			du.value = 255;
		}
		huff_t sizeDecode;
		while (acValueCount < 63) {
			sizeDecode = huffmanTables.DecodeAndMove(comp.huffTableAc, HuffmanTables::AC, itr);
			if (sizeDecode == 0) break; // eod (flag (0,0) for end of data)
			acValueCount += sizeDecode >> 4; // amount leading zeros
			++acValueCount;
			itr += sizeDecode & 0x0F; // len of ac value
		}
	}
	void ParseMCU(DataItr& itr) {
		int duPos = 0;
		std::vector<DataUnit> dus(info.duPerMCU);
		for (size_t i = 0; i < info.numComponents; ++i) {
			for (int j = 0; j < info.componenst[i].sampH * info.componenst[i].sampV; ++j) {
				dus[duPos].chanleId = info.componenst[i].id;
				ParseDU(info.componenst[i], itr, dcs, dus[duPos]);
				++duPos;
			}
		}
		// TODO: only support 4:1:1!! 
		assert(info.numComponents == 3);
		for (const DataUnit& du : dus) {
			if (du.chanleId == info.componenst[info.chanleWithHigestSample].id) picture->AddValues(du.chanleId, du.value);
			else {
				picture->AddValues(du.chanleId, du.value, 4);
			}
		}
	}

	std::optional<const unsigned char*> ParseImage(const unsigned char* data) {
		for (int i = 0; i < 7; ++i) dcs[i] = 0;
		size_t mcus = 0;
		DataItr itr(data);
		unsigned char lastMarker = 0x00;
		for (size_t parsed = 0; parsed < info.width * info.height; /* parsed += info.pxPerMcu */) {
			if (const unsigned char marker = itr.CheckForMarker()) {
				if (marker == 0xd9) {
					std::cout << "mcus: " << std::dec << mcus << "   reached end";
					break;
				}
				assert(resartInterval && mcus > 0 && (mcus % resartInterval) == 0);
				itr.Align();
				// std::cout << std::hex << (int)marker << std::dec << " reset marker: mcus: " << mcus << "\n";
				assert(lastMarker == 0x00 || (lastMarker == 0xd7 && marker == 0xd0) || (lastMarker + 1 == marker));
				lastMarker = marker;
				for (int i = 0; i < 7; ++i) dcs[i] = 0;
				itr += 16;
			}
			ParseMCU(itr);
			++mcus;
		}
		return std::optional<const unsigned char*>(itr.GetPtr());
	}

	bool ParseSOS(size_t len) {
		assert(ptr[4] == 3); // color jpeg
		if (len != (6 + 2 * ptr[4])) return false;
		for (int i = 0; i < 3; ++i) {
			unsigned char id = ptr[5 + 2 * i];
			unsigned char table = ptr[5 + 2 * i + 1];
			assert(info.componenst[i].id == id);
			info.componenst[i].huffTableAc = table & 0x0F;
			info.componenst[i].huffTableDc = table >> 4;
		}
		return true;
	}

	bool ParseQuadTable(size_t len) {
		return quadTables.AddTable(ptr[4] & 0x0F, (ptr[4] >> 4) + 1, ptr + 5);
	}

	bool ParseDRI(size_t len) {
		assert(len == 4);
		resartInterval = (ptr[4] << 8) + ptr[5];
		std::cout << resartInterval << " restart mcus\n";
		return true;
	}

	bool ParseBlock() {
		if (ptr[0] !=  0xff) {
			errorStack.push_back("no block start found " + ptr[0]);
			return false;
		}
		size_t len = 0;
		bool result = true;
		std::cout << (int)(ptr[1]) << '\n';
		switch (ptr[1])
		{
		case 0xc0:
			len = GetLen();
			result = ParseSOF(len);
			if (result) bSet[DENSITY];
			break;
		case 0xe0:
			len = GetLen();
			result = ParseAPP0(len);
			if (result) bSet[INFO]; 
			break;
		case 0xda: {
			len = GetLen();
			result = ParseSOS(len);
			if (!result) return false;
			const unsigned char* t = ptr + 2 + len;
			std::cout << "ImageInfo: " << resartInterval << " reset\n" << info << std::endl;
			std::optional<const unsigned char*> res = ParseImage(t); ///< t reference
			result = res.has_value();
			if (res) {
				t = res.value();
				PrintInfo();
				assert(t[0] == 0xff && t[1] == 0xd9);
			}
			// while (t[0] != 0xff && t[1] != 0xd9) ++t;
			len = t - ptr - 2;
		}	break;
		case 0xc4: 
			len = GetLen();
			result = ParseHuffmanTable(len);
			break;
		case 0xdb:
			len = GetLen();
			result = ParseQuadTable(len);
			break;
		case 0xdd:
			len = GetLen();
			result = ParseDRI(len);
			break;
		case 0xd0:
		case 0xd1:
		case 0xd2:
		case 0xd3:
		case 0xd4:
		case 0xd5:
		case 0xd6:
		case 0xd7:
			break;
		case 0xd8:
			std::cout << "pic start";
			break;
		case 0xd9:
			std::cout << "reached end of pic\n";
			break;
		default:
			len = GetLen();
			std::cout << "not ignored block: " << std::hex
				<< static_cast<int>(ptr[1]) << '\n';
			break;
		}
		ptr += len + 2;
		return result;
	}
	
public:
	Image(const std::vector<char>&& img) : data{ img } {
		ptr = reinterpret_cast<const unsigned char*>(data.data());
	}
	bool Parse() {
		while (static_cast<std::size_t>(reinterpret_cast<const char*>(ptr) - data.data()) < data.size()) {
			if (!ParseBlock()) {
				errorStack.push_back("can't parse block");
				return false;
			}
		}
		return true;
	}
	void PrintInfo() const {
		std::cout << info << "\n"; // << *picture;
		std::string name;
		std::cout << "FileName: ";
		std::cin >> name;
		std::ofstream file(name + ".pgm");
		const std::vector<int>& chanel = picture->GetChanel(1);
		file << "P2\n" << (info.width / 8) << ' ' << (info.height / 8) << '\n' << ((0x01 << info.precission) - 1) << '\n';
		for (const int& i : chanel) {
			file << i << ' ';
		}
	}
	void PrintError() const {
		std::cerr << "Error trace:\n";
		for (const auto& s : errorStack) {
			std::cerr << '\t' << s << '\n';
		}
	}
};

int main(void) {
	std::cout << "Start open file t1.jpeg\n";
	fs::path p("p1.jpg");
	if (!fs::exists(p)) {
		std::cerr << "file not found t1.jpeg\n";
		return 0;
	}
	std::ifstream picture(p, std::ios::binary);
	if (picture.fail() || !picture.good()) {
		std::cout << "failed to open file\n";
		return 0;
	}
	int len = fs::file_size(p);
	std::cout << "len: " << len << '\n';
	std::vector<char> data(len);
	picture.read(data.data(), len);
	Image img(std::move(data));
	if (!img.Parse()) {
		std::cerr << "Failed to parse\n";
		img.PrintError();
		return 0;
	}
	img.PrintInfo();
	return 0;
	// Create the two input vectors
	/* int i;
	const int LIST_SIZE = 1024;
	int *A = (int*)malloc(sizeof(int)*LIST_SIZE);
	int *B = (int*)malloc(sizeof(int)*LIST_SIZE);
	for (i = 0; i < LIST_SIZE; i++) {
		A[i] = i;
		B[i] = LIST_SIZE - i;
	}

	FILE *fp;
	char *source_str;
	size_t source_size;
	fp = fopen("vector_add.cl", "r");
	if (!fp) {
		fprintf(stderr, "Failed to load kernel.\n");
		exit(1);
	}
	source_str = (char*)malloc(MAX_SOURCE_SIZE);
	source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
	fclose(fp);

	
	cl_platform_id platform_id = NULL;
	cl_device_id device_id = NULL;
	cl_uint ret_num_devices;
	cl_uint ret_num_platforms;
	cl_int ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
	ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, 1,
		&device_id, &ret_num_devices);

	cl_context context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);

	cl_command_queue command_queue = clCreateCommandQueue(context, device_id, 0, &ret);

	cl_mem a_mem_obj = clCreateBuffer(context, CL_MEM_READ_ONLY,
		LIST_SIZE * sizeof(int), NULL, &ret);
	cl_mem b_mem_obj = clCreateBuffer(context, CL_MEM_READ_ONLY,
		LIST_SIZE * sizeof(int), NULL, &ret);
	cl_mem c_mem_obj = clCreateBuffer(context, CL_MEM_WRITE_ONLY,
		LIST_SIZE * sizeof(int), NULL, &ret);

	ret = clEnqueueWriteBuffer(command_queue, a_mem_obj, CL_TRUE, 0,
		LIST_SIZE * sizeof(int), A, 0, NULL, NULL);
	ret = clEnqueueWriteBuffer(command_queue, b_mem_obj, CL_TRUE, 0,
		LIST_SIZE * sizeof(int), B, 0, NULL, NULL);

	cl_program program = clCreateProgramWithSource(context, 1,
		(const char **)&source_str, (const size_t *)&source_size, &ret);

	ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
	cl_kernel kernel = clCreateKernel(program, "vector_add", &ret);

	ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&a_mem_obj);
	ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&b_mem_obj);
	ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&c_mem_obj);

	// Execute the OpenCL kernel on the list
	size_t global_item_size = LIST_SIZE; // Process the entire lists
	size_t local_item_size = 64; // Divide work items into groups of 64
	ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL,
		&global_item_size, &local_item_size, 0, NULL, NULL);

	// Read the memory buffer C on the device to the local variable C
	int *C = (int*)malloc(sizeof(int)*LIST_SIZE);
	ret = clEnqueueReadBuffer(command_queue, c_mem_obj, CL_TRUE, 0,
		LIST_SIZE * sizeof(int), C, 0, NULL, NULL);

	// Display the result to the screen
	for (i = 0; i < LIST_SIZE; i++)
		printf("%d + %d = %d\n", A[i], B[i], C[i]);

	// Clean up
	ret = clFlush(command_queue);
	ret = clFinish(command_queue);
	ret = clReleaseKernel(kernel);
	ret = clReleaseProgram(program);
	ret = clReleaseMemObject(a_mem_obj);
	ret = clReleaseMemObject(b_mem_obj);
	ret = clReleaseMemObject(c_mem_obj);
	ret = clReleaseCommandQueue(command_queue);
	ret = clReleaseContext(context);
	free(A);
	free(B);
	free(C);
	return 0; */
}