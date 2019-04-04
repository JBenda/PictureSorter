#include "Cluster.hpp"

void GPUMagic::CalculateSim(const float* pictureData, size_t px, size_t amtImgs) {
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
				/*|| !FindExtenison(d.getInfo<CL_DEVICE_EXTENSIONS>().c_str(), EXTENSION)*/)
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

	cl_int err;
	cl::Buffer gColorData(context, CL_MEM_READ_ONLY, sizeof(float) * px * amtImgs, NULL, &err);
	float *cMeta = new float[amtImgs * 2];
	cl::Buffer gMeta(context, CL_MEM_READ_WRITE, sizeof(float) * 2 * amtImgs, NULL, &err);
	unsigned int simAmt = (amtImgs * amtImgs - amtImgs) / 2;
	float *cSims = new float[simAmt];
	cl::Buffer gSims(context, CL_MEM_WRITE_ONLY, sizeof(float) * simAmt, NULL, &err);
	if (err) {
		std::cerr << "failed to allocate Buffer on GPU\n";
		exit(1);
	}

	cl::CommandQueue queue(context, device, NULL, &err);
	if (err) {
		std::cerr << "failed to create queue\n";
		exit(1);
	}

	queue.enqueueWriteBuffer(gColorData, CL_TRUE, 0, sizeof(float) * px * amtImgs, pictureData);

	cl::Event eCalcMetaData;
	cl::Kernel mean_var(program, "mean_var");
	mean_var.setArg(0, gColorData);
	mean_var.setArg(1, gMeta);
	mean_var.setArg(2, px);

	cl::Event eCalcSim;
	cl::Kernel calc_sim(program, "calc_sim");
	calc_sim.setArg(0, gColorData);
	calc_sim.setArg(1, gMeta);
	calc_sim.setArg(2, gSims);
	calc_sim.setArg(3, px);
	calc_sim.setArg(4, amtImgs);
	queue.enqueueNDRangeKernel(mean_var, cl::NullRange, cl::NDRange(amtImgs), cl::NullRange);

	std::vector<cl::Event> waitList{eCalcMetaData};
	queue.enqueueNDRangeKernel(calc_sim, cl::NullRange, cl::NDRange(simAmt), cl::NullRange);

	waitList[0] = eCalcSim;
	queue.enqueueReadBuffer(gMeta, CL_TRUE, 0, sizeof(float) *amtImgs * 2, cMeta);
	queue.enqueueReadBuffer(gSims, CL_TRUE, 0, sizeof(float) * simAmt, cSims);
	
	for (int i = 0; i < amtImgs; ++i) {
		float mean = 0;
		for (int j = 0; j < px; ++j)
			mean += pictureData[i * px + j];
		mean /= px;
		std::cout << cMeta[i * 2] << " : " << cMeta[i * 2 + 1] << " vs " << mean << "\n";
	}
	std::cout << std::setprecision(3);
	for (int i = 0; i < amtImgs; ++i) {
		std::cout << i << ":\t";
		for (int j = 0; j < amtImgs; ++j) {
			if (j <= i) { std::cout << "    \t"; }
			else if (i * amtImgs + j < simAmt) { std::cout << cSims[i * amtImgs + j] << "\t"; }
			else {
				int x = amtImgs -j - 1;
				int y = amtImgs - i - 2;
				std::cout << cSims[y * amtImgs + x] << '\t';
			}
		}
		std::cout << "\n";
	}
}

bool GPUMagic::LoadCernalCode(const fs::path& path, cl::Program::Sources& srcs) {
	std::ifstream fp(path, std::ios::binary);
	size_t source_size;
	if (!fp) {
		fprintf(stderr, "Failed to load kernel.\n");
		exit(1);
	}
	source_size = fs::file_size(path);
	char *sourceStr = new char[source_size + 1];
	fp.read(sourceStr, source_size);
	sourceStr[source_size] = 0;
	srcs.push_back(std::pair<const char*, size_t>(sourceStr, source_size));
	fp.close();
	return true;
}

bool GPUMagic::FindExtenison(const char* pExts, const char* extension) {
	const char* pExt = extension;
	while (*pExts != 0) {
		if (*pExts == ' ') {
			if (*pExt == 0) {
				true;
				break;
			}
			pExt = extension;
		}
		else if (*pExt && *pExts == *pExt) {
			++pExt;
		}
		++pExts;
	}
	return false;
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