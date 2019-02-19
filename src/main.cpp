#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <cassert>
#include <optional>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
// #include <CL/cl.h>
#endif

#define MAX_SOURCE_SIZE (0x100000)

namespace fs = std::filesystem;

class Image {
	typedef std::size_t size_t;
	typedef std::size_t chanel_t;
	enum BLOCK_TYPES { UNDEF = 0x00, SOI = 0xd8 };
	const std::vector<char> data;
	const unsigned char *ptr;
	std::vector<std::string> errorStack;
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
				chanel_t GatData() const { return data[2]; }
			} codes[256];
			std::optional<chanel_t> GetValue(size_t code, size_t len) const {
				const CodePair *begin = codes + codeLen[len - 1],
					*end = codes + codeLen[len];
				for (const CodePair* itr = begin; itr != end; ++itr) {
					if (itr->GetCode() == code)
						return std::optional<chanel_t>(itr->GatData());
				}
				return std::optional<chanel_t>();
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
				std::cout << std::hex;
				for (size_t i = 0; i < len; ++i) {
					while (i == codeLen[lenCode]) {
						++lenCode;
						code <<= 1;
					}
					codes[i].Set(code, bytes[i]);
					std::cout << '\t';
					for (int j = lenCode - 1; j >= 0; --j) {
						std::cout << (code & (0x01 << j) ? '1' : '0');
					}
					std::cout << " -> " << static_cast<int>(bytes[i]) << "\n";
					++code;
				}
				std::cout << std::dec;
				encodedSize = len + 16;
				bValid = true;
			}
			~HuffmanTabel() {}
			
			chanel_t Decode(const unsigned char* data, size_t pos) const {
				assert(bValid);
				bool bit;
				size_t code = 0;
				size_t len = 0;
				bool sticky = true;
				do {
					bit = data[(pos + len) / 8] & (0x80 >> ((pos + len) % 8));
					code = (code << 1) + (bit ? 1 : 0);
					++len;
					if (sticky && !bit) sticky = false;
					if (!sticky) {
						std::optional<chanel_t> res = GetValue(code, len);
						if (res) {
							return res.value();
						}
					}
				} while (len <= 16);
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
		chanel_t Decode(size_t id, DATA_TYPE type, const unsigned char* data, size_t pos) const {
			if (type == AC) return tables[acTabeles[id]].Decode(data, pos);
			else if (type == DC) return tables[dcTabeles[id]].Decode(data, pos);
			assert(false);
			return 0;
		}
	} huffmanTables;
	struct Infos {
		size_t precission, width, height, numComponents, duPerMCU, ///< data units (a 64 encoded values) per mcu
			pxPerMcu;
		struct Components
		{
			unsigned char id, sampV, sampH, qautTable;
			bool operator()(const Components& c1, const Components& c2) {
				return c1.id < c2.id;
			}
		} componenst[3];
		friend std::ostream& operator<<(std::ostream& os, const Infos& obj) {
			os << "imgInfo:\n\tpercission: " << obj.precission
				<< "\n\tw: " << obj.width
				<< "\n\th: " << obj.height
				<< "\n\tchannels: " << obj.numComponents;
			for (int i = 0; i < obj.numComponents; ++i) {
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
		info.height = FuseBytes(itr); itr += 2;
		info.width = FuseBytes(itr); itr += 2;
		info.numComponents = static_cast<size_t>(*itr); ++itr;
		info.duPerMCU = 0;
		size_t maxSampH = 0, maxSampV = 0;
		for (size_t i = 0; i < info.numComponents; ++i) {
			info.componenst[i].id = *(itr++);
			info.componenst[i].sampV = (*itr) & 0x0F;
			info.componenst[i].sampH = ((*itr) & 0xF0) >> 4;
			if (info.componenst[i].sampH > maxSampH) maxSampH = info.componenst[i].sampH;
			if (info.componenst[i].sampV > maxSampV) maxSampV = info.componenst[i].sampV;
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
		return true;
	}
	
	bool ParseHuffmanTable(size_t len) {
		for (size_t l = 4; l < len + 2; l += huffmanTables.AddTable(ptr + l));
		return true;
	}

	struct DataUnit
	{
		unsigned char chanleId;
		size_t value;
	};
	size_t ParseDU(const unsigned char* data, size_t pos, DataUnit& du) {
		// encode dc value
		// jump over ac values
	}
	size_t ParseMCU(const unsigned char* data, size_t pos) {
		int duPos = 0;
		std::vector<DataUnit> dus(info.duPerMCU);
		for (int i = 0; i < info.numComponents; ++i) {
			for (int j = 0; j < info.componenst[i].sampH * info.componenst[i].sampV; ++j) {
				dus[duPos].chanleId = i;
				pos = ParseDU(data, pos, dus[duPos]);
				++duPos;
			}
		}
		// save data in img
		return pos;
	}

	std::optional<const unsigned char*> ParseImage(const unsigned char* data) {
		size_t pos = 0;
		for (size_t parsed = 0; parsed < info.width * info.height; parsed += info.pxPerMcu) {
			pos = ParseMCU(data, pos);
		}
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
			// ParseSOS(len);
			const unsigned char* t = ptr + 2 + len;
			/* std::optional<const unsigned char*> res = ParseImage(t) ///< t reference
			result = res;
			if (result)
				assert(t[0] == 0xff && t[1] ==0xd9);
				t = res.value();*/
			while (t[0] != 0xff && t[1] != 0xd9) ++t;
			len = t - ptr - 2;
		}	break;
		case 0xc4: 
			len = GetLen();
			result = ParseHuffmanTable(len);
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
		std::cout << info;
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
	fs::path p("t1.jpeg");
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