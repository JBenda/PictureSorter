#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#define MAX_SOURCE_SIZE (0x100000)

namespace fs = std::filesystem;

class Image {
	typedef std::size_t size_t;
	enum BLOCK_TYPES { UNDEF = 0x00, SOI = 0xd8 };
	const std::vector<char>& data;
	const char *ptr;
	std::vector<std::string> errorStack;
	struct Infos {
		size_t precission, width, height, numComponents;
		struct Components
		{
			unsigned char id, sampV, sampH, qautTable;
		} componenst[3];
		friend std::ostream& operator<<(std::ostream& os, const Infos& obj) {
			os << "imgInfo:\n\tpercission: " << obj.precission
				<< "\n\tw: " << obj.width
				<< "\n\th: " << obj.height
				<< "\n\tchannels: " << obj.numComponents
				<< "\n";
			return os;
		}
	} info;
	size_t FuseBytes(const char *p) const {
		return (static_cast<size_t>(p[2]) << 8) + static_cast<size_t>(p[3]);
	}
	size_t GetLen() const {
		return FuseBytes(ptr);
	}
	bool ParseSOF(size_t len) {
		const char *itr = ptr + 4;
		info = { 0 };
		info.precission = *itr; ++itr;
		info.height = FuseBytes(itr); itr += 2;
		info.width = FuseBytes(itr); itr += 2;
		info.numComponents = static_cast<size_t>(*itr); ++itr;
		for (size_t i = 0; i < info.numComponents; ++i) {
			info.componenst[i].id = static_cast<unsigned char>(*(itr++));
			info.componenst[i].sampV = static_cast<unsigned char>((*itr) & 0x0F);
			info.componenst[i].sampH = static_cast<unsigned char>((*itr) & 0xF0);
			++itr;
			info.componenst[i].qautTable = static_cast<unsigned char>(*(itr++));
		}
		if (itr - ptr + 2 != len) {
			errorStack.push_back("Wrong leng encoded: "
				+ std::to_string(itr - ptr + 2)
				+ " vs " + std::to_string(len));
			return false;
		}
		return true;
	}
	bool ParseBlock() {
		if (ptr[0] != 0xff) {
			errorStack.push_back("no block start found " + ptr[0]);
			return false;
		}
		size_t len = 0;
		bool result = true;
		switch (ptr[1])
		{
		case 0xd8:
			len = GetLen();
			result = ParseSOF(len);
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
	Image(const std::vector<char>& img) : data{ img } {
		ptr = data.data();
	}
	bool Parse() {
		while (static_cast<std::size_t>(ptr - data.data()) < data.size()) {
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
	std::ifstream picture(p, std::ios::binary | std::ios::app);
	if (picture.fail() || !picture.good()) {
		std::cout << "failed to open file\n";
		return 0;
	}
	picture.seekg(std::ios::end);
	unsigned int len = static_cast<unsigned int>(picture.tellg());
	picture.seekg(std::ios::beg);
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
	int i;
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
	return 0;
}