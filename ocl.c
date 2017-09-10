/*
* Copyright 2016 sprocket
* Copyright 2016 Evil-Knievel
*
* This program is free software; you can redistribuSte it and/or modify it
* under the terms of the GNU General Public License as published by the Free
* Software Foundation; either version 2 of the License, or (at your option)
* any later version.
*/

#ifdef USE_OPENCL

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#define _GNU_SOURCE

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include "miner.h"

#define CL_CHECK(x) x
#define MAX_OPENCL_PLATFORMS 100

bool opencl_err_check(int err, char *desc) {
	if (err != CL_SUCCESS) {
		applog(LOG_ERR, "Error: %s (Code: %d)", desc, err);
		return false;
	}
	return true;
}

extern int opencl_init_devices() {
	size_t i, j;
	cl_platform_id platforms[MAX_OPENCL_PLATFORMS];
	cl_uint err;
	cl_uint num_platforms = 0;
	cl_uint num_devices = 0;
	char buffer[1024];
	bool found;
	int gpu_cnt = 0;

	err = clGetPlatformIDs(MAX_OPENCL_PLATFORMS, platforms, &num_platforms);
	if (!opencl_err_check(err, "No OpenCL platforms found")) return 0;

	gpu = (struct opencl_device *)malloc(num_platforms * sizeof(struct opencl_device));

	if (!gpu) {
		applog(LOG_ERR, "ERROR: Unable to allocate GPU devices!");
		return 0;
	}

	applog(LOG_DEBUG, "=== %d OpenCL platform(s) found: ===", num_platforms);

	for (i = 0; i < num_platforms; i++) {

		found = false;

		applog(LOG_DEBUG, "  -- %d --", i);
		clGetPlatformInfo(platforms[i], CL_PLATFORM_PROFILE, sizeof(buffer), buffer, NULL);
		applog(LOG_DEBUG, "  PROFILE = %s", buffer);
		clGetPlatformInfo(platforms[i], CL_PLATFORM_VERSION, sizeof(buffer), buffer, NULL);
		applog(LOG_DEBUG, "  VERSION = %s", buffer);
		clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, sizeof(buffer), buffer, NULL);
		applog(LOG_DEBUG, "  NAME = %s", buffer);
		clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, sizeof(buffer), buffer, NULL);
		applog(LOG_DEBUG, "  VENDOR = %s", buffer);
		clGetPlatformInfo(platforms[i], CL_PLATFORM_EXTENSIONS, sizeof(buffer), buffer, NULL);
		applog(LOG_DEBUG, "  EXTENSIONS = %s", buffer);

		err = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL, &num_devices);
		if (!opencl_err_check(err, "Unable to get devices from OpenCL Platform")) return 0;

		if (num_devices) {

			applog(LOG_DEBUG, "  DEVICES:");
			cl_device_id *devices = (cl_device_id *)malloc(num_devices * sizeof(cl_device_id));

			err = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, num_devices, devices, NULL);
			if (!opencl_err_check(err, "Unable to get GPU devices from device list")) num_devices = 0;

			for (j = 0; j < num_devices; j++) {

				err = clGetDeviceInfo(devices[j], CL_DEVICE_NAME, sizeof(buffer), buffer, NULL);
				if (!opencl_err_check(err, "Unable to Device Name from device list")) break;

				applog(LOG_DEBUG, "    %d - %s", j, buffer);

				err = clGetDeviceInfo(devices[j], CL_DEVICE_EXTENSIONS, sizeof(buffer), buffer, NULL);
				if (!opencl_err_check(err, "Unable to Device Extensions from device list")) break;

				if (!strstr(buffer, "cl_khr_fp64")) {
					applog(LOG_DEBUG, "        *Device does not support 64bit Floating Point math");
					break;
				}

				memcpy(&gpu[gpu_cnt].platform_id, &platforms[i], sizeof(cl_platform_id));
				memcpy(&gpu[gpu_cnt].device_id, &devices[j], sizeof(cl_device_id));
				strncpy(gpu[gpu_cnt].name, buffer, 99);
				found = true;
				break;
			}
			free(devices);
		}

		if (!found)
			continue;

		// Create Context
		gpu[gpu_cnt].context = clCreateContext(NULL, 1, &gpu[gpu_cnt].device_id, NULL, NULL, &err);
		if (!gpu[gpu_cnt].context) {
			applog(LOG_ERR, "ERROR: Unable to create OpenCL context (Error: %d)", err);
			return 0;
		}

		// Create Command Queue
		gpu[gpu_cnt].queue = clCreateCommandQueue(gpu[gpu_cnt].context, gpu[gpu_cnt].device_id, 0, &err);
		if (!gpu[gpu_cnt].queue) {
			applog(LOG_ERR, "ERROR: Unable to create OpenCL command queue (Error: %d)", err);
			return 0;
		}

		gpu_cnt++;
	}

	return gpu_cnt;
}

extern bool opencl_create_buffers(struct opencl_device *gpu) {
	cl_uint err;

	gpu->obj_dat = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY, 24 * sizeof(uint32_t), NULL, &err);
	if (!opencl_err_check(err, "Unable to create OpenCL 'base_data' buffer")) return false;

	gpu->obj_rnd = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY, 1 * sizeof(uint32_t), NULL, &err);
	if (!opencl_err_check(err, "Unable to create OpenCL 'round' buffer")) return false;

	gpu->obj_res = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE, 1 * sizeof(uint32_t), NULL, &err);
	if (!opencl_err_check(err, "Unable to create OpenCL 'result' buffer")) return false;

	gpu->obj_out = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY, 5 * sizeof(uint32_t), NULL, &err);
	if (!opencl_err_check(err, "Unable to create OpenCL 'output' buffer")) return false;

	return true;
}

extern unsigned char* opencl_load_source(char *work_str) {
	unsigned char filename[50], *ocl_source = NULL;
	FILE *fp;
	size_t bytes;

	if (!work_str || (strlen(work_str) > 22)) {
		applog(LOG_ERR, "ERROR: Invalid filename for OpenCL source: %s", work_str);
		return NULL;
	}

	sprintf(filename, "./work/job_%s.cl", work_str);

	// Load The Source Code For The OpenCL Kernels
	fp = fopen(filename, "r");
	if (!fp) {
		applog(LOG_ERR, "ERROR: Failed to load OpenCL source: %s", filename);
		return NULL;
	}

	ocl_source = (char*)malloc(MAX_SOURCE_SIZE);
	bytes = fread(ocl_source, 1, MAX_SOURCE_SIZE, fp);
	ocl_source[bytes] = 0;	// Terminating Zero
	fclose(fp);

	if (bytes <= 0) {
		applog(LOG_ERR, "ERROR: Failed to read OpenCL source: %s", filename);
		free(ocl_source);
		return NULL;
	}

	return ocl_source;
}

extern bool opencl_calc_worksize(struct opencl_device *gpu) {
	uint32_t max_threads = 409600;
	cl_ulong global_mem = 0;
	cl_ulong local_mem = 0;
	cl_uint compute_units = 0;
	cl_uint dimensions = 0;
	cl_uint vwidth = 0;
	size_t max_work_size = 0;

	clGetDeviceInfo(gpu->device_id, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(cl_ulong), &global_mem, NULL);
	applog(LOG_DEBUG, "  CL_DEVICE_GLOBAL_MEM_SIZE = %lu", global_mem);

	clGetDeviceInfo(gpu->device_id, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &local_mem, NULL);
	applog(LOG_DEBUG, "  CL_DEVICE_LOCAL_MEM_SIZE  = %zu", local_mem);

	clGetDeviceInfo(gpu->device_id, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(cl_uint), &dimensions, NULL);
	applog(LOG_DEBUG, "  CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS  = %zu", dimensions);

	clGetDeviceInfo(gpu->device_id, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &max_work_size, NULL);
	applog(LOG_DEBUG, "  CL_DEVICE_MAX_WORK_GROUP_SIZE  = %zu", max_work_size);

	clGetDeviceInfo(gpu->device_id, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &compute_units, NULL);
	applog(LOG_DEBUG, "  CL_DEVICE_MAX_COMPUTE_UNITS  = %zu", compute_units);

	clGetDeviceInfo(gpu->device_id, CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT, sizeof(cl_uint), &vwidth, NULL);
	applog(LOG_DEBUG, "  CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT  = %zu", vwidth);


	///////////////////////////////////////////////////////////////
	// TODO: These Calcs Need To Be Redone
	///////////////////////////////////////////////////////////////


	// Calculate Max Threads (Must Be A Multiple Of Work Size)
	max_threads = (uint32_t)((uint32_t)((opt_opencl_gthreads ? opt_opencl_gthreads : max_threads) / max_work_size) * max_work_size);

	// Calculate Num Threads For This Device
	double vm_mem = (((g_work_package[g_work_package_idx].vm_ints + g_work_package[g_work_package_idx].vm_uints + g_work_package[g_work_package_idx].vm_floats + g_work_package[g_work_package_idx].storage_sz) * 4) +
		((g_work_package[g_work_package_idx].vm_longs + g_work_package[g_work_package_idx].vm_ulongs + g_work_package[g_work_package_idx].vm_doubles) * 8));

	// Calculate Threads (Arbitrarily Subtract 1MB From Mem)
	double threads = ((double)global_mem - (1000000)) / vm_mem;
	size_t bound = (size_t)threads;

	gpu->threads = (bound < max_threads) ? (int)bound : max_threads;
	gpu->work_dim = 1;
	gpu->global_size[0] = gpu->threads;
	gpu->global_size[1] = 1;
	gpu->global_size[2] = 1;
	gpu->local_size[0] = max_work_size;
	gpu->local_size[1] = 1;
	gpu->local_size[2] = 1;

	applog(LOG_INFO, "Global GPU Memory = %llu, Using %d Threads - G{ %d, %d, %d } L{ %d, %d, %d }", global_mem, gpu->threads, gpu->global_size[0], gpu->global_size[1], gpu->global_size[2], gpu->local_size[0], gpu->local_size[1], gpu->local_size[2]);

	return true;
}

extern bool opencl_create_kernel(struct opencl_device *gpu, char *ocl_source, uint32_t storage_sz) {
	cl_int err;
	cl_program program;

	// Load OpenCL Source Code
	program = clCreateProgramWithSource(gpu->context, 1, (const char **)&ocl_source, NULL, &err);
	if (!opencl_err_check(err, "Unable to load OpenCL program")) return false;

	// Compile OpenCL Source Code
	err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
	if (!opencl_err_check(err, "Unable to compile OpenCL program")) {
		size_t len;
		char buffer[10000];
		clGetProgramBuildInfo(program, gpu->device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
		applog(LOG_ERR, "%s", buffer);
		return false;
	}

	// Create OpenCL Kernel
	gpu->kernel = clCreateKernel(program, "execute", &err);
	if (!opencl_err_check(err, "Unable to create OpenCL kernel")) {
		clReleaseProgram(program);
		return false;
	}

	// Create Buffer For Submit / Storage (Depending On Whether ElasticPL Job Uses Storage)
	if (storage_sz) {
		gpu->obj_sub = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY, storage_sz * sizeof(uint32_t), NULL, &err);
		if (!opencl_err_check(err, "Unable to create OpenCL 'submit' buffer")) return false;

		gpu->obj_storage = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY, storage_sz * sizeof(uint32_t), NULL, &err);
		if (!opencl_err_check(err, "Unable to create OpenCL 'storage' buffer")) return false;
	}
	else {
		gpu->obj_sub = NULL;
		gpu->obj_storage = NULL;
	}

	// Set Argurments For Kernel (Storage Argument Is Optional)
	err = clSetKernelArg(gpu->kernel, 0, sizeof(cl_mem), (const void*)&gpu->obj_dat);		// Base Data
	err |= clSetKernelArg(gpu->kernel, 1, sizeof(cl_mem), (const void*)&gpu->obj_rnd);		// Round Number
	err |= clSetKernelArg(gpu->kernel, 2, sizeof(cl_mem), (const void*)&gpu->obj_res);		// Result
	err |= clSetKernelArg(gpu->kernel, 3, sizeof(cl_mem), (const void*)&gpu->obj_out);		// Output
	err |= clSetKernelArg(gpu->kernel, 4, sizeof(cl_mem), (const void*)&gpu->obj_sub);		// Data To Submit
	err |= clSetKernelArg(gpu->kernel, 5, sizeof(cl_mem), (const void*)&gpu->obj_storage);	// Storage
	if (!opencl_err_check(err, "Unable to set OpenCL argurments for 'execute' kernel")) return false;

	return true;
}

extern bool opencl_init_buffer_data(struct opencl_device *gpu, uint32_t *base_data, uint32_t *rnd_num, uint32_t *result, uint32_t *storage, uint32_t storage_len) {
	cl_uint err;

	// Copy Base Data To OpenCL Buffer
	err = clEnqueueWriteBuffer(gpu->queue, gpu->obj_dat, CL_FALSE, 0, 24 * sizeof(uint32_t), base_data, 0, NULL, NULL);
	if (!opencl_err_check(err, "Unable to write to OpenCL 'base_data' Buffer")) return false;

	// Reset Round Buffer
	err = clEnqueueWriteBuffer(gpu->queue, gpu->obj_rnd, CL_FALSE, 0, 1 * sizeof(uint32_t), rnd_num, 0, NULL, NULL);
	if (!opencl_err_check(err, "Unable to write to OpenCL 'base_data' Buffer")) return false;

	// Reset Result Buffer
	err = clEnqueueWriteBuffer(gpu->queue, gpu->obj_res, CL_FALSE, 0, 1 * sizeof(uint32_t), result, 0, NULL, NULL);
	if (!opencl_err_check(err, "Unable to write to OpenCL 'base_data' Buffer")) return false;

	// Copy Storage To OpenCL Buffer
	if (gpu->obj_storage && storage_len) {
		err = clEnqueueWriteBuffer(gpu->queue, gpu->obj_storage, CL_FALSE, 0, storage_len * sizeof(uint32_t), storage, 0, NULL, NULL);
		if (!opencl_err_check(err, "Unable to write to OpenCL 'storage' Buffer")) return false;
	}

	clFinish(gpu->queue);

	return true;
}

extern bool opencl_run_kernel(struct opencl_device *gpu, uint32_t *rnd_num, uint32_t *result, uint32_t *output, uint32_t *submit, uint32_t submit_sz) {
	cl_uint err;

	// Update Round Number
	err = clEnqueueWriteBuffer(gpu->queue, gpu->obj_rnd, CL_TRUE, 0, 1 * sizeof(uint32_t), rnd_num, 0, NULL, NULL);
	if (!opencl_err_check(err, "Unable to write to OpenCL 'round' Buffer")) return false;

	// Reset Result Buffer
	if (result[0]) {
		result[0] = 0;
		err = clEnqueueWriteBuffer(gpu->queue, gpu->obj_res, CL_TRUE, 0, 1 * sizeof(uint32_t), result, 0, NULL, NULL);
		if (!opencl_err_check(err, "Unable to write to OpenCL 'result' Buffer")) return false;
	}
//	clFinish(gpu->queue);

	// Run OpenCL Kernel
	err = clEnqueueNDRangeKernel(gpu->queue, gpu->kernel, 1, NULL, gpu->global_size, 0, 0, 0, 0);
	//	err = clEnqueueNDRangeKernel(gpu->queue, gpu->kernel, gpu->work_dim, NULL, &gpu->global_size[0], &gpu->local_size[0], 0, NULL, NULL);
	if (!opencl_err_check(err, "Unable to run 'execute' kernel")) return false;

	// Check For POW / Bounty Solutions
	err = clEnqueueReadBuffer(gpu->queue, gpu->obj_res, CL_TRUE, 0, 1 * sizeof(uint32_t), result, 0, NULL, NULL);
	if (!opencl_err_check(err, "Unable to read from OpenCL 'result' Buffer")) return false;

	// Get Output
	if (result[0]) {
		err = clEnqueueReadBuffer(gpu->queue, gpu->obj_out, CL_FALSE, 0, 5 * sizeof(uint32_t), output, 0, NULL, NULL);
		if (!opencl_err_check(err, "Unable to read from OpenCL 'output' Buffer")) return false;

		if (gpu->obj_sub && submit_sz) {
			err = clEnqueueReadBuffer(gpu->queue, gpu->obj_sub, CL_FALSE, 0, submit_sz * sizeof(uint32_t), submit, 0, NULL, NULL);
			if (!opencl_err_check(err, "Unable to read from OpenCL 'submit' Buffer")) return false;
		}
		clFinish(gpu->queue);
	}

	return true;
}

#endif