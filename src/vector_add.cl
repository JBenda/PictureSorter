__kernel void vector_add(__global const int *A, __global const int *B, __global int *C) {
	int i = get_global_id(0);
	C[i] = A[i] + B[i];
}

__kernel void mean_var(__global const float *data, __global float* res, unsigned int pxAmt) {
	 const __global float *src = data + (get_global_id(0) * pxAmt);
	 float sum = 0.f;
	for(unsigned int i = 0; i < pxAmt; ++i)
		sum += src[i];
	res[get_global_id(0) * 2] = sum / (float)pxAmt;
	sum = 0.f;
	for (unsigned int i = 0; i < pxAmt; ++i)
		sum += pow(src[i] - res[get_global_id(0) * 2], 2);
	res[get_global_id(0) * 2 + 1] = sum / (float)pxAmt;
}

__kernel void calc_sim(__global const float *data, __global const float *mean,
	__global float *sim, const unsigned int pxAmt, const unsigned int size) {
	const float c1 = pow(0.01 * 255.f, 2), c2 = pow(0.03 * 255.f, 2);
	int id = get_global_id(0);
	int x = id % size;
	int y = id / size;
	if (x <= y) { 
		x = size - 1 - x;
		y = size - 2 - y;
	}
	const __global float *pic1 = data + (x * pxAmt), *pic2 = data + (y * pxAmt);
	const __global float *mean1 = mean + (x * 2), *mean2 = mean + (y * 2);
	float coVar = 0.f;
	for (unsigned int i = 0; i < pxAmt; ++i)
		coVar += (pic1[i] - mean1[0]) * (pic2[i] - mean2[0]);
	coVar /= (float)pxAmt;
	sim[get_global_id(0)] = (2.f * mean1[0] * mean2[0] + c1) * (2.f * coVar + c2)
		/ (mean1[0] * mean1[0] + mean2[0] * mean2[0] + c1)
		/ (mean1[1] + mean2[1] + c2);
}