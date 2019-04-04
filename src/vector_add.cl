__kernel void vector_add(__global const int *A, __global const int *B, __global int *C) {
	int i = get_global_id(0);
	C[i] = A[i] + B[i];
}

__kernel void mean_var(__global const float *data, __global float* res, __constant unsigned int pxAmt) {
	 const float *src = data + (get_global_id(0) * pxAmt);
	 float sum = 0.f;
	for(unsigned int i = 0; i < pxAmt; ++i)
		sum += src[i];
	mean[get_global_id(0) * 2] = sum / pxAmt;
	sum = 0.f;
	for (unsigned int i = 0; i < pxAmt; ++i)
		sum += pow(src[i] - mean[get_global_id(0)], 2);
	mean[get_global_id(0) * 2 + 1] = sum / pxAmt;
}