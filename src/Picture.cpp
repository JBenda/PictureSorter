#include "Picture.hpp"

void Image::DataItr::NextByte() {
	if (itr[0] == 0xFF && itr[1] == 0)
		itr += 2;
	else
		++itr;
}

void Image::DataItr::CutOffset() {
	for (; offset >= 8; offset -= 8) NextByte();
}

void Image::DataItr::Align() {
	if (offset) {
		assert(((~(*itr & (0xFF >> offset))) & (0xFF >> offset)) == 0);
		NextByte();
		offset = 0;
	}
}

const unsigned char* Image::DataItr::GetPtr() {
	if (offset)
		Align();
	return itr;
}

unsigned char Image::DataItr::CheckForMarker() {
	if (itr[offset ? 1 : 0] == 0xFF)
		return itr[offset ? 2 : 1];
	return 0x00;
}

Image::DataItr& Image::DataItr::operator++() {
	++offset;
	if (offset == 8) {
		offset = 0;
		NextByte();
	}
	assert(offset < 8);
	return *this;
}

Image::DataItr& Image::DataItr::operator+=(Image::size_t offset) {
	this->offset += offset;
	while (this->offset >= 8) {
		this->offset -= 8;
		NextByte();
	}
	return *this;
}

bool Image::DataItr::ReadBit() {
	bool ret = (*itr & (0x80 >> offset));
	++*this;
	return ret;
}

bool Image::DataItr::PeekBit() const {
	return (*itr & (0x80 >> offset));
}

unsigned char Image::DataItr::ReadByte() {
	assert(offset == 0);
	unsigned char byte = *itr;
	NextByte();
	return byte;
}

unsigned char Image::DataItr::PeekByte() const {
	assert(offset == 0);
	return *itr;
}

bool Image::DataItr::ReadStream(size_t bits, unsigned char *dest) {
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
			*itrD |= (*itr >> (8 - bits)) << (offset - bits);
			offset = bits;
		}
		else {
			*itrD = (((0xFF >> offset) & *itr) << offset) & (0xFF << (8 - bits));
			offset += bits;
			CutOffset();
		}
	}
	assert(offset < 8);
	return true;
}

int Image::QuadTables::QuadTable::DeQuad(Image::size_t pos, int val) const {
	if (prec == 1) return data[pos] * val;
	else if (prec == 2) {
		size_t p = pos << 1;
		return (data[p] << 8) + data[++p];
	}
	assert(false);
}

size_t Image::QuadTables::AddTable(size_t id, size_t precission, const unsigned char* data) {
	assert(id == tables.size());
	tables.emplace_back(precission, data);
	return tables.back().GetSize() + 1;
}

int Image::QuadTables::DeQuad(size_t id, size_t pos, int val) const {
	assert(id < tables.size());
	return tables[id].DeQuad(pos, val);
}

void Image::HuffmanTables::HuffmanTabel::CodePair::Set(size_t code, unsigned char byte) {
	data[0] = (code >> 8) & 0xFF;
	data[1] = code & 0xFF;
	data[2] = byte;
}

std::optional<Image::huff_t> Image::HuffmanTables::HuffmanTabel::GetValue(size_t code, size_t len) const {
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

Image::HuffmanTables::HuffmanTabel::HuffmanTabel(const unsigned char* data) : bValid{ false }, len{ 0 } {
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
		++code;
		assert(!((0x01 << lenCode) & code));
	}
	encodedSize = len + 15;
	bValid = true;
}

Image::huff_t Image::HuffmanTables::HuffmanTabel::DecodeAndMove(Image::DataItr& itr) const {
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

Image::size_t Image::HuffmanTables::AddTable(const unsigned char* data) {
	DATA_TYPE type = data[0] & 0xF0 ? AC : DC;
	size_t id = data[0] & 0x0F;

	tables.push_back(HuffmanTabel(data + 1));

	std::vector<unsigned char>& tRef = type == AC ? acTabeles : dcTabeles;
	tRef.resize(id + 1, 0xFF);
	tRef[id] = static_cast<unsigned char>(tables.size() - 1);

	return tables.back().GetEncodedSize() + 2;
}

Image::huff_t Image::HuffmanTables::DecodeAndMove(size_t id, DATA_TYPE type, Image::DataItr& itr) const {
	if (type == AC) return tables[acTabeles[id]].DecodeAndMove(itr);
	else if (type == DC) return tables[dcTabeles[id]].DecodeAndMove(itr);
	assert(false);
	return 0;
}

bool Image::ParseAPP0(size_t len) {
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

bool Image::ParseSOF(size_t len) {
	const unsigned char *itr = ptr + 4;
	info = { 0 };
	info.precission = *itr; ++itr;
	assert(info.precission == 8);
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

bool Image::ParseHuffmanTable(size_t len) {
	size_t l;
	for (l = 4; l < len + 2; l += huffmanTables.AddTable(ptr + l));
	assert(l == len + 2);
	return true;
}

bool Image::ParseQuadTable(size_t len) {
	size_t l;
	for (l = 4; l < len + 2; l +=quadTables.AddTable(ptr[l] & 0x0F, (ptr[l] >> 4) + 1, ptr + l + 1));
	assert(l == len + 2);
	return true;
}

int Image::ReadValue(DataItr& itr, size_t len) {
	assert(len <= 16);
	bool sign = !itr.PeekBit();
	int res = 0;
	unsigned char *raw = reinterpret_cast<unsigned char*>(&res);
	itr.ReadStream(len, raw);
	if (len <= 8) {
		res >>= 8 - len;
	}
	else if (len <= 16) {
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

Image::PictureData::QuadItr& Image::PictureData::QuadItr::operator++() {
	++size;
	if (size % (data.mSamX * data.mSamY) == 0)
		++w;
	if (pos % Image::dimX < Image::dimX - 1 && w > limW) {
		++pos;
		limW += bPerPxW;
	} else if (w == data.w / data.mSamX) {
		++h;
		pos -= Image::dimX - 1;
		w = 0;
		limW = bPerPxW;
	}
	if (pos / Image::dimY < Image::dimY - 1 && h > limH) {
		limH += bPerPxH;
		pos += Image::dimX;
	}
	if (pos > 10000) {
		std::cout << "test\n";
	}
	return *this;
}

Image::PictureData::QuadItr& Image::PictureData::GetWritten(size_t id) {
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

Image::PictureData::PictureData(enum COLOR_TYPE t, size_t width, size_t height, size_t maxSampleHor, size_t maxSampleVer)
	: cType{ t }, data{ height, width, maxSampleHor, maxSampleVer, maxSampleHor * maxSampleVer } {
	written.push_back(data);
	written.push_back(data);
	written.push_back(data);
	width >>= 3;
	height >>= 3;
	if (width % 2) ++width;
	if (height % 2) ++height;
	size_t size = dimX * dimY;
	if (cType == COLOR) {
		channles[0].resize(size);
		channles[1].resize(size);
		channles[2].resize(size);
	}
	else if (cType == BW) {
		channles[0].resize(size);
	}
	else assert(false);
}

// 
void Image::PictureData::AddValues(size_t id, Image::chanel_t val, size_t times) {
	assert((id == 1 && times == 1) || times == 4);
	static size_t count = 0;
	if (id == 1) ++count;
	std::vector<MeanType>& chanel = GetChanel(id);
	QuadItr& itr = GetWritten(id);
	for (int i = 0; i < times; ++i, ++itr) {
		chanel[itr.GetPos()] += val;
	}
}

const std::vector<Image::MeanType>& Image::PictureData::GetChanel(size_t id) const {
	if (cType == COLOR_TYPE::COLOR) {
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
std::vector<Image::MeanType>& Image::PictureData::GetChanel(size_t id) {
	if (cType == COLOR_TYPE::COLOR) {
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

void Image::ParseDU(const Infos::Components& comp, DataItr& itr, int* dcs, DataUnit& du) {
	// encode dc value
	// jump over ac values
	huff_t dcLen = huffmanTables.DecodeAndMove(comp.huffTableDc, HuffmanTables::DC, itr);
	unsigned char acValueCount = 0;
	assert(dcLen <= 16);
	int dcDiff = 0;

	size_t posBuffer = itr.GetPosition();
	dcDiff = ReadValue(itr, dcLen);

	dcs[comp.id] += dcDiff;
	du.value = (quadTables.DeQuad(comp.qautTable, 0, dcs[comp.id]) / 8) + 128;
	// check if stuff
	if (du.value < 0) {
		du.value = 0;
	}
	else if (du.value > 255) {
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

void Image::ParseMCU(Image::DataItr& itr) {
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

std::optional<const unsigned char*> Image::ParseImage(const unsigned char* data) {
	for (int i = 0; i < 7; ++i) dcs[i] = 0;
	size_t mcus = 0;
	DataItr itr(data);
	unsigned char lastMarker = 0x00;
	for (size_t parsed = 0; parsed < info.width * info.height; /* parsed += info.pxPerMcu */) {
		if (const unsigned char marker = itr.CheckForMarker()) {
			if (marker == 0xd9)
				break;
			assert(resartInterval && mcus > 0 && (mcus % resartInterval) == 0);
			itr.Align();
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

bool Image::ParseSOS(size_t len) {
	assert(ptr[4] == 3);
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

bool Image::ParseBlock() {
	if (ptr[0] != 0xff) {
		errorStack.push_back("no block start found " + ptr[0]);
		return false;
	}
	size_t len = 0;
	bool result = true;
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
		std::optional<const unsigned char*> res = ParseImage(t); ///< t reference
		result = res.has_value();
		if (res) {
			t = res.value();
			assert(t[0] == 0xff && t[1] == 0xd9);
		}
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
		assert(false); // no remark marker in Data block
		break;
	case 0xd8:
		break;
	case 0xd9:
		stata = STATE::FINISH;
		break;
	default:
		len = GetLen();
		break;
	}
	ptr += len + 2;
	return result;
}