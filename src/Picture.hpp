#pragma once

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
#include "General.hpp"

namespace fs = std::filesystem;
class Image {
	class MeanType {
		size_t data;
		int amount;
		enum STATE { fill, fin } state = fill;
	public:
		MeanType() : data{ 0 }, amount{ 0 } {}
		void Mean() { assert(state == fill); data /= amount; state = fin; }
		MeanType& operator+= (size_t add) { data += add; ++amount; return *this; }
		operator size_t() const {
			assert(state == fin);
			return data;
		}
	};
	typedef std::size_t size_t;
	typedef unsigned char chanel_t;
	typedef unsigned char huff_t;
	enum BLOCK_TYPES { UNDEF = 0x00, SOI = 0xd8 };
	const std::vector<char> data;
	class PictureData;
	std::unique_ptr<PictureData> picture;
	const unsigned char *ptr;
	std::vector<std::string> errorStack;
	size_t resartInterval = 0;
	int dcs[7]; // max chanle id == 7
	enum STATE { PARSING, FINISH } stata;
	class DataItr {
		const unsigned char * const data;
		const unsigned char *itr;
		size_t offset; // bitwise
		void NextByte();
		void CutOffset();
	public:
		DataItr(const unsigned char *data) : data{ data }, itr{ data }, offset{ 0 } {}
		/** return number of whole bytes readad/processed
		 */
		size_t GetPosition() const {
			return (itr - data);
		}
		void Align();
		/** attention byte align will be executet
		*/
		const unsigned char* GetPtr();
		/** check if next byte(s) a marker and all fill bits
		 * note if offset == 0, than check this byte
		 * retrun 0 no marker
		 * retrun marker if marker
		*/
		unsigned char CheckForMarker();
		DataItr& operator++();
		DataItr& operator+=(size_t offset);
		bool ReadBit();
		bool PeekBit() const;
		unsigned char ReadByte();
		unsigned char PeekByte() const;
		bool ReadStream(size_t bits, unsigned char *dest);
	};
	class QuadTables {
		class QuadTable {
			std::vector<unsigned char> data;
			size_t prec;
		public:
			QuadTable(size_t precisson, const unsigned char* table) : data(precisson * 64), prec{ precisson } {
				std::memcpy(data.data(), table, precisson * 64);
			}
			int DeQuad(size_t pos, int val) const;
		};
		std::vector<QuadTable> tables;
	public:
		QuadTables() : tables{} {}
		bool AddTable(size_t id, size_t precission, const unsigned char* data);
		int DeQuad(size_t id, size_t pos, int val) const;
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
				void Set(size_t code, unsigned char byte);
				size_t GetCode() const { return FuseBytes(data); }
				huff_t GatData() const { return data[2]; }
			} codes[256];
			std::optional<huff_t> GetValue(size_t code, size_t len) const;
			size_t encodedSize;
		public:
			HuffmanTabel() : bValid{ false } {};
			size_t GetEncodedSize() { return encodedSize; }
			HuffmanTabel(const unsigned char* data);
			~HuffmanTabel() {}

			huff_t Decode(DataItr itr) const {
				return DecodeAndMove(itr);
			}
			// TODO: beter name
			/** decode nexthuffman value and adds len to pos
			*/
			huff_t DecodeAndMove(DataItr& itr) const;
		};
		std::vector<HuffmanTabel> tables;
		std::vector<unsigned char> acTabeles; ///< 0xFF == not set
		std::vector<unsigned char> dcTabeles;
	public:
		HuffmanTables() : tables(6) {}
		/** return: bytes readed */
		size_t AddTable(const unsigned char* data);
		enum DATA_TYPE { AC, DC };
		huff_t DecodeAndMove(size_t id, DATA_TYPE type, DataItr& itr) const;
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
	} info;
	struct Density
	{
		size_t x, y;
		enum TYPE { NO, DPER_INCH, DPER_CM } unit;
	} density;
	enum DECODE_DATA { DENSITY, INFO, LAST };
	bool bSet[DECODE_DATA::LAST];
	static size_t FuseBytes(const unsigned char *p) {
		return (static_cast<size_t>(p[0]) << 8) + static_cast<size_t>(p[1]);
	}
	size_t GetLen() const {
		return FuseBytes(ptr + 2);
	}
	bool ParseAPP0(size_t len);

	bool ParseSOF(size_t len);

	bool ParseHuffmanTable(size_t len);

	/** dcodec integer from bit stream
	 * @param begin first bit
	 * @param end first bit not included int integer
	 * @return decoded integer
	*/
	int ReadValue(DataItr& itr, size_t len);

	struct DataUnit
	{
		unsigned char chanleId;
		chanel_t value;
	};
	class PictureData {
		std::vector<MeanType> channles[3];
		struct ImageData {
			const size_t h, w, mSamX, mSamY, sampleBlock;
			ImageData(size_t heigh, size_t width, size_t maxSampX, size_t maxSampY, size_t sampPerBlock)
				: h{ heigh / 8 }, w{ width / 8 }, mSamX{ maxSampX }, mSamY{ maxSampY }, sampleBlock{ sampPerBlock } {}
		} data;
		class QuadItr {
			size_t size, pos, w, h;
			const float bPerPxH, bPerPxW;
			float limW, limH;
			const ImageData& data;
		public:
			const size_t& GetPos() { return pos; }
			const size_t& GetSize() { return size; }
			QuadItr(const ImageData& pic) : data{ pic }, size{ 0 }, pos{ 0 }, w{ 0 }, h{ 0 },
				bPerPxH{ static_cast<float>(pic.h / pic.mSamY) / static_cast<float>(Image::dimY) },
				bPerPxW{ static_cast<float>(pic.w / pic.mSamX) / static_cast<float>(Image::dimX) }
					{
				limW = bPerPxW;
				limH = bPerPxH;
			}
			QuadItr& operator++();
		};
		std::vector<QuadItr> written;
		QuadItr& GetWritten(size_t id);
	public:
		const enum COLOR_TYPE { COLOR, BW } cType;
		PictureData(enum COLOR_TYPE t, size_t width, size_t height, size_t maxSampleHor, size_t maxSampleVer);
		void AddValues(size_t id, chanel_t val, size_t times = 1);
		void CalcMean() { for (std::vector<MeanType>& mts : channles) for (MeanType& mt : mts) mt.Mean(); };
		std::vector<MeanType>& GetChanel(size_t id);
	};

	/** @param id channle id */
	void ParseDU(const Infos::Components& comp, DataItr& itr, int* dcs, DataUnit& du);

	void ParseMCU(DataItr& itr);

	std::optional<const unsigned char*> ParseImage(const unsigned char* data);

	bool ParseSOS(size_t len);

	bool ParseQuadTable(size_t len) {
		return quadTables.AddTable(ptr[4] & 0x0F, (ptr[4] >> 4) + 1, ptr + 5);
	}

	bool ParseDRI(size_t len) {
		assert(len == 4);
		resartInterval = (ptr[4] << 8) + ptr[5];
		return true;
	}

	bool ParseBlock();

public:
	constexpr static size_t dimX = 100;
	constexpr static size_t dimY = 100;
	constexpr static size_t memorySize =  dimX * dimY;
	Image(const std::vector<char>&& img) : data{ img }, stata{ STATE::PARSING } {
		ptr = reinterpret_cast<const unsigned char*>(data.data());
	}
	const std::vector<MeanType> GetGrayChanel() {
		return picture->GetChanel(1);
	}
	bool Parse() {
		while (stata != STATE::FINISH) {
			if (!ParseBlock()) {
				errorStack.push_back("can't parse block");
				return false;
			}
		}
		return true;
	}
	void PrintInfo() const {
		std::string name;
		std::cout << "FileName: ";
		std::cin >> name;
		std::ofstream file(name + ".pgm");
		const std::vector<MeanType>& chanel = picture->GetChanel(1);
		file << "P2\n" << Image::dimX << ' ' << Image::dimY << '\n' << ((0x01 << info.precission) - 1) << '\n';
		for (const int& i : chanel) {
			file << i << ' ';
		}
		file.close();
	}
	void PrintError() const {
		std::cerr << "Error trace:\n";
		for (const auto& s : errorStack) {
			std::cerr << '\t' << s << '\n';
		}
	}
	friend std::ofstream& operator<<(std::ofstream& ofs, Image& img) {
		for (const chanel_t& data : img.picture->GetChanel(1)) {
			ofs.write(reinterpret_cast<const char*>(&data), sizeof(chanel_t));
		}
		return ofs;
	}
};