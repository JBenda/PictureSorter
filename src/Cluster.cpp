#include "Cluster.hpp"

void GPUMagic::CalculateSim(const unsigned char* PicData, size_t amt) {
	std::vector<cl::Platform> allPlatforms;
	cl::Platform::get(&allPlatforms);

	if (allPlatforms.size() == 0) {
		std::cerr << "No Grphic Platform found.\n";
		exit(1);
	}

	cl::Device device;
	cl::Platform platform;
	size_t maxAllocMem, maxMem, cashLine, comU;
	bool foundDevice = false;
	constexpr char EXTENSION[] = "cl_khr_byte_addressable_store";
	for (const cl::Platform& p : allPlatforms) {
		if (strcmp(p.getInfo<CL_PLATFORM_PROFILE>().c_str(), "FULL_PROFILE")) continue;
		std::vector<cl::Device> allDevices;
		p.getDevices(CL_DEVICE_TYPE_GPU, &allDevices);
		for (const cl::Device& d : allDevices) {
			if (!d.getInfo<CL_DEVICE_COMPILER_AVAILABLE>()
				|| !(d.getInfo<CL_DEVICE_EXECUTION_CAPABILITIES>() & CL_EXEC_KERNEL)
				|| !(d.getInfo<CL_DEVICE_QUEUE_PROPERTIES>() & CL_QUEUE_PROFILING_ENABLE)
				|| !FindExtenison(d.getInfo<CL_DEVICE_EXTENSIONS>().c_str(), EXTENSION))
				continue;
			device = d;
			platform = p;
			maxMem = d.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
			maxAllocMem = d.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>();
			cashLine = d.getInfo<CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE>();
			comU = d.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
			foundDevice = true;
			break;
		}
		if (foundDevice) break;
	}
	if (!foundDevice) {
		std::cerr << "No suitable GPU-Device found on the Platform\n";
		exit(1);
	}

	cl::Context context(device);

	cl::Program::Sources sourceCodes;
	if (!LoadCernalCode("../src/vector_add.cl", sourceCodes)) {
		std::cerr << "Failed to load Code.\n";
		exit(1);
	}

	cl::Program program(context, sourceCodes);
	if (program.build(std::vector<cl::Device>(1, device)) != CL_SUCCESS) {
		std::cerr << "Error building code: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device) << "\n";
		exit(1);
	}	
}

bool GPUMagic::LoadCernalCode(const fs::path& path, cl::Program::Sources& srcs) {
	std::ifstream fp(path);
	size_t source_size;
	if (!fp) {
		fprintf(stderr, "Failed to load kernel.\n");
		exit(1);
	}
	source_size = fs::file_size(path);
	char *sourceStr = new char[source_size];
	fp.read(sourceStr, source_size);
	srcs.push_back(std::pair<const char*, size_t>(sourceStr, source_size));
	fp.close();
}

bool GPUMagic::FindExtenison(const char* pExts, const char* extension) {
	const char* pExt = extension;
	while (*pExts != 0) {
		if (*pExts == ' ') {
			if (*pExt == 0) {

				break;
			}
			pExt = extension;
		}
		else if (*pExt && *pExts == *pExt) {
			++pExt;
		}
		++pExts;
	}
}

void GPUMagic::PrintPlatforms(const std::vector<cl::Platform>& platforms) {
	for (const cl::Platform& p : platforms) {

		std::cout << p.getInfo<CL_PLATFORM_PROFILE>() << "\n";
		std::cout << p.getInfo<CL_PLATFORM_VERSION>() << "\n";
		std::cout << p.getInfo<CL_PLATFORM_NAME>() << "\n";
		std::cout << p.getInfo<CL_PLATFORM_VENDOR>() << "\n";
		std::cout << p.getInfo<CL_PLATFORM_EXTENSIONS>() << "\n";

		std::vector<cl::Device> allDevices;
		p.getDevices(CL_DEVICE_TYPE_GPU, &allDevices);

		for (const cl::Device& d : allDevices) {
			std::cout << "Compile:\t" << d.getInfo<CL_DEVICE_COMPILER_AVAILABLE>() << "\n";
			std::cout << "exe cap:\t" << d.getInfo<CL_DEVICE_EXECUTION_CAPABILITIES>() << "\n"; // CL_EXEC_KERNEL 
			std::cout << "\t" << d.getInfo<CL_DEVICE_EXTENSIONS>() << "\n"; // cl_khr_byte_addressable_store
			std::cout << "GlobMem:\t" << d.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>() << "\n";
			std::cout << "cash line:\t" << d.getInfo<CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE>() << "\n";
			std::cout << "comU:\t" << d.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << "\n";
			std::cout << "maxMemAlloc:\t" << d.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>() << "\n";
			std::cout << "supports queus:\t" << static_cast<bool>(d.getInfo<CL_DEVICE_QUEUE_PROPERTIES>() & CL_QUEUE_PROFILING_ENABLE) << "\n";
		}
		std::cout << "\n\n\n";
	}
}