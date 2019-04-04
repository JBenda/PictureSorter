#pragma once

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL\cl.hpp>
#endif

#include <fstream>
#include <filesystem>
#include <iostream>
#include <string_view>

#define MAX_SOURCE_SIZE (0x100000)

namespace GPUMagic {
	namespace fs = std::filesystem;
	void CalculateSim(const unsigned char* PicData, size_t amt);
	bool LoadCernalCode(const fs::path& path, cl::Program::Sources& srcs);
	bool FindExtenison(const char* pExts, const char* extension);
	void PrintPlatforms(const std::vector<cl::Platform>& platforms);
}