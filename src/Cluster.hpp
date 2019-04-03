#pragma once

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
// #include <CL/cl.h>
#endif

#define MAX_SOURCE_SIZE (0x100000)

namespace GPUMagic {
	void CalculateSim();
}