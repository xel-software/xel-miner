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

extern unsigned char* load_opencl_source(char *work_str) {
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

extern bool init_opencl_kernel(struct opencl_device *gpu, char *ocl_source) {
	cl_int ret;

	// Load OpenCL Source Code
	cl_program program = clCreateProgramWithSource(gpu->context, 1, (const char **)&ocl_source, NULL, &ret);
	if (ret != CL_SUCCESS) {
		applog(LOG_ERR, "Unable to load OpenCL program (Error: %d)", ret);
		return false;
	}

	// Compile OpenCL Source Code
	ret = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
	if (ret != CL_SUCCESS) {
		size_t len;
		char buffer[2048];
		clGetProgramBuildInfo(program, gpu->device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
		applog(LOG_ERR, "Unable to compile OpenCL program (Error: %d) -\n%s", ret, buffer);
		return false;
	}

	// Create OpenCL Kernel
	gpu->kernel_execute = clCreateKernel(program, "execute", &ret);
	if (!gpu->kernel_execute || ret != CL_SUCCESS) {
		applog(LOG_ERR, "Unable to create OpenCL kernel (Error: %d)", ret);
		clReleaseProgram(program);
		return false;
	}

	// Set Argurments For Kernel
	ret  = clSetKernelArg(gpu->kernel_execute, 0, sizeof(cl_mem), (const void*)&gpu->vm_input);
	ret |= clSetKernelArg(gpu->kernel_execute, 1, sizeof(cl_mem), (const void*)&gpu->vm_m);
	ret |= clSetKernelArg(gpu->kernel_execute, 2, sizeof(cl_mem), (const void*)&gpu->vm_i);
	ret |= clSetKernelArg(gpu->kernel_execute, 3, sizeof(cl_mem), (const void*)&gpu->vm_u);
	ret |= clSetKernelArg(gpu->kernel_execute, 4, sizeof(cl_mem), (const void*)&gpu->vm_l);
	ret |= clSetKernelArg(gpu->kernel_execute, 5, sizeof(cl_mem), (const void*)&gpu->vm_ul);
	ret |= clSetKernelArg(gpu->kernel_execute, 6, sizeof(cl_mem), (const void*)&gpu->vm_f);
	ret |= clSetKernelArg(gpu->kernel_execute, 7, sizeof(cl_mem), (const void*)&gpu->vm_d);
	ret |= clSetKernelArg(gpu->kernel_execute, 8, sizeof(cl_mem), (const void*)&gpu->vm_s);
	ret |= clSetKernelArg(gpu->kernel_execute, 9, sizeof(cl_mem), (const void*)&gpu->vm_out);

	if (ret != CL_SUCCESS) {
		applog(LOG_ERR, "Unable to set OpenCL argurments for 'execute' kernel (Error: %d)", ret);
		return false;
	}

	return true;
}

extern int init_opencl_devices() {
	size_t i, j;
	cl_platform_id platforms[100];
	cl_uint ret;
	cl_uint platforms_n = 0;
	cl_uint devices_n = 0;
	char buffer[1024];
	bool found;
	int gpu_cnt = 0;

	CL_CHECK(clGetPlatformIDs(100, platforms, &platforms_n));

	if (platforms_n == 0) {
		applog(LOG_ERR, "ERROR: No OpenCL platforms found!");
		return 0;
	}

	gpu = (struct opencl_device *)malloc(platforms_n * sizeof(struct opencl_device));

	if (!gpu) {
		applog(LOG_ERR, "ERROR: Unable to allocate GPU devices!");
		return 0;
	}

	applog(LOG_DEBUG, "=== %d OpenCL platform(s) found: ===", platforms_n);

	for (i = 0; i < platforms_n; i++) {

		found = false;

		applog(LOG_DEBUG, "  -- %d --", i);
		CL_CHECK(clGetPlatformInfo(platforms[i], CL_PLATFORM_PROFILE, sizeof(buffer), buffer, NULL));
		applog(LOG_DEBUG, "  PROFILE = %s", buffer);
		CL_CHECK(clGetPlatformInfo(platforms[i], CL_PLATFORM_VERSION, sizeof(buffer), buffer, NULL));
		applog(LOG_DEBUG, "  VERSION = %s", buffer);
		CL_CHECK(clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, sizeof(buffer), buffer, NULL));
		applog(LOG_DEBUG, "  NAME = %s", buffer);
		CL_CHECK(clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, sizeof(buffer), buffer, NULL));
		applog(LOG_DEBUG, "  VENDOR = %s", buffer);
		CL_CHECK(clGetPlatformInfo(platforms[i], CL_PLATFORM_EXTENSIONS, sizeof(buffer), buffer, NULL));
		applog(LOG_DEBUG, "  EXTENSIONS = %s", buffer);

		ret = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL, &devices_n);
		if (ret != CL_SUCCESS) {
			applog(LOG_ERR, "Error: Unable to get devices from OpenCL Platform %d (Error: %d)", i, ret);
			return 0;
		}

		if (devices_n) {

			applog(LOG_DEBUG, "  DEVICES:");
			cl_device_id *devices = (cl_device_id *)malloc(devices_n * sizeof(cl_device_id));

			ret = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, devices_n, devices, NULL);

			if (ret != CL_SUCCESS)
				devices_n = 0;

			for (j = 0; j < devices_n; j++) {

				ret = clGetDeviceInfo(devices[j], CL_DEVICE_NAME, sizeof(buffer), buffer, NULL);
				if (ret != CL_SUCCESS)
					break;

				applog(LOG_DEBUG, "    %d - %s", j, buffer);

				ret = clGetDeviceInfo(devices[j], CL_DEVICE_EXTENSIONS, sizeof(buffer), buffer, NULL);
				if (ret != CL_SUCCESS)
					break;

				//if (!strstr(buffer, "cl_khr_fp64")) {
				//	applog(LOG_DEBUG, "        *Device does not support 64bit Floating Point math");
				//	break;
				//}

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
		gpu[gpu_cnt].context = clCreateContext(0, 1, &gpu[gpu_cnt].device_id, NULL, NULL, &ret);
		if (!gpu[gpu_cnt].context) {
			applog(LOG_ERR, "Unable to create OpenCL context (Error: %d)", ret);
			return false;
		}

		// Create Command Queue
		gpu[gpu_cnt].queue = clCreateCommandQueue(gpu[gpu_cnt].context, gpu[gpu_cnt].device_id, 0, &ret);
		if (!gpu[gpu_cnt].queue) {
			applog(LOG_ERR, "Unable to create OpenCL command queue (Error: %d)", ret);
			return false;
		}

		gpu_cnt++;
	}

	return gpu_cnt;
}

extern bool calc_opencl_worksize(struct opencl_device *gpu) {
	uint32_t max_threads = 1024;
	cl_ulong global_mem = 0;
	cl_ulong local_mem = 0;
	cl_uint compute_units = 0;
	cl_uint dimensions = 0;
	cl_uint vwidth = 0;
	size_t max_work_size = 0;
	size_t dim1, dim2;

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
	
	// Calculate Max Threads (Must Be A Multiple Of Work Size)
	max_threads = (uint32_t)((uint32_t)((opt_opencl_gthreads ? opt_opencl_gthreads : 1024) / max_work_size) * max_work_size);

	// Calculate Num Threads For This Device
	// GLOB MEM NEEDED: 96 + (x * VM_MEMORY_SIZE * sizeof(int32_t)) + (x * VM_MEMORY_SIZE * sizeof(double)) + (x * sizeof(uint32_t))

	double vm_mem = (((g_work_package[g_work_package_idx].vm_ints + g_work_package[g_work_package_idx].vm_uints + g_work_package[g_work_package_idx].vm_floats + g_work_package[g_work_package_idx].storage_sz) * 4) +
		((g_work_package[g_work_package_idx].vm_longs + g_work_package[g_work_package_idx].vm_ulongs + g_work_package[g_work_package_idx].vm_doubles) * 8));
	double calc = ((double)global_mem - 96.0 - 650 * 1024 * 1024 /*Some 650 M space for who knows what*/) / vm_mem;
	size_t bound = (size_t)calc;

	gpu->threads = (bound < max_threads) ? (int)bound : max_threads;

	if (dimensions == 1 || opt_opencl_vwidth == 1) {
		gpu->work_dim = 1;
		gpu->global_size[0] = gpu->threads;
		gpu->global_size[1] = 1;
		gpu->local_size[0] = max_work_size;
		gpu->local_size[1] = 1;
	}
	else {
		gpu->work_dim = 2;
		dim1 = ((size_t)gpu->threads > max_work_size) ? max_work_size : (size_t)gpu->threads;
		dim2 = (size_t)(gpu->threads / dim1);
		gpu->global_size[0] = dim1;
		gpu->global_size[1] = dim2;

		// Make Sure Local Size Y Value Is A Multiple Of Global Size
		if (opt_opencl_vwidth && (dim2 % opt_opencl_vwidth != 0)) {
				applog(LOG_ERR, "ERROR: Invalid opt_opencl_vwidth = %d.  Must be a multiple of '%d'", opt_opencl_vwidth, dim2);
				return false;
		}

		gpu->local_size[1] = (size_t)(opt_opencl_vwidth ? opt_opencl_vwidth : dim2);

		// Calculate Local Size X Value
		gpu->local_size[0] = max_work_size;
		while ((gpu->local_size[0] * gpu->local_size[1]) > max_work_size) {
			if (max_work_size % (gpu->local_size[0] * gpu->local_size[1]) == 0)
				break;

			gpu->local_size[0] = (size_t)(gpu->local_size[0] / 2);

		}
	}

	applog(LOG_INFO, "Global GPU Memory = %llu, Using %d Threads - G{ %d, %d } L{ %d, %d }", global_mem , gpu->threads, gpu->global_size[0], gpu->global_size[1], gpu->local_size[0], gpu->local_size[1]);

	return true;
}

extern bool create_opencl_buffers(struct opencl_device *gpu) {
	cl_uint ret;

	gpu->vm_input = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY, 96 * sizeof(char), NULL, &ret);
	if (ret != CL_SUCCESS) {
		applog(LOG_ERR, "ERROR: Unable to create OpenCL 'vm_input' buffer (Error: %d)", ret);
		return false;
	}

	gpu->vm_m = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY, gpu->threads * 12 * sizeof(uint32_t), NULL, &ret);
	if (ret != CL_SUCCESS) {
		applog(LOG_ERR, "ERROR: Unable to create OpenCL 'vm_m' buffer (Error: %d)", ret);
		return false;
	}

	if (g_work_package[g_work_package_idx].vm_ints) {
		gpu->vm_i = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY, gpu->threads * g_work_package[g_work_package_idx].vm_ints * sizeof(int32_t), NULL, &ret);
		if (ret != CL_SUCCESS) {
			applog(LOG_ERR, "ERROR: Unable to create OpenCL 'vm_i' buffer (Error: %d)", ret);
			return false;
		}
	}
	 else
		 gpu->vm_i = NULL;

	if (g_work_package[g_work_package_idx].vm_uints) {
		gpu->vm_u = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY, gpu->threads * g_work_package[g_work_package_idx].vm_uints * sizeof(uint32_t), NULL, &ret);
		if (ret != CL_SUCCESS) {
			applog(LOG_ERR, "ERROR: Unable to create OpenCL 'vm_u' buffer (Error: %d)", ret);
			return false;
		}
	}
	else
		gpu->vm_u = NULL;

	if (g_work_package[g_work_package_idx].vm_longs) {
		gpu->vm_l = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY, gpu->threads * g_work_package[g_work_package_idx].vm_longs * sizeof(int64_t), NULL, &ret);
		if (ret != CL_SUCCESS) {
			applog(LOG_ERR, "ERROR: Unable to create OpenCL 'vm_l' buffer (Error: %d)", ret);
			return false;
		}
	}
	else
		gpu->vm_l = NULL;

	if (g_work_package[g_work_package_idx].vm_ulongs) {
		gpu->vm_ul = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY, gpu->threads * g_work_package[g_work_package_idx].vm_ulongs * sizeof(uint64_t), NULL, &ret);
		if (ret != CL_SUCCESS) {
			applog(LOG_ERR, "ERROR: Unable to create OpenCL 'vm_ul' buffer (Error: %d)", ret);
			return false;
		}
	}
	else
		gpu->vm_ul = NULL;

	if (g_work_package[g_work_package_idx].vm_floats) {
		gpu->vm_f = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY, gpu->threads * g_work_package[g_work_package_idx].vm_floats * sizeof(float), NULL, &ret);
		if (ret != CL_SUCCESS) {
			applog(LOG_ERR, "ERROR: Unable to create OpenCL 'vm_f' buffer (Error: %d)", ret);
			return false;
		}
	}
	else
		gpu->vm_f = NULL;

	if (g_work_package[g_work_package_idx].vm_doubles) {
		gpu->vm_d = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY, gpu->threads * g_work_package[g_work_package_idx].vm_doubles * sizeof(double), NULL, &ret);
		if (ret != CL_SUCCESS) {
			applog(LOG_ERR, "ERROR: Unable to create OpenCL 'vm_d' buffer (Error: %d)", ret);
			return false;
		}
	}
	else
		gpu->vm_d = NULL;

	if (g_work_package[g_work_package_idx].storage_sz) {
		gpu->vm_s = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY, g_work_package[g_work_package_idx].storage_sz * sizeof(uint32_t), NULL, &ret);
		if (ret != CL_SUCCESS) {
			applog(LOG_ERR, "ERROR: Unable to create OpenCL 'vm_s' buffer (Error: %d)", ret);
			return false;
		}
	}
	else
		gpu->vm_s = NULL;

	gpu->vm_out = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY, gpu->threads * sizeof(uint32_t), NULL, &ret);
	if (ret != CL_SUCCESS) {
		applog(LOG_ERR, "ERROR: Unable to create OpenCL 'vm_out' buffer (Error: %d)", ret);
		return false;
	}

	return true;
}

extern bool execute_kernel(struct opencl_device *gpu, const uint32_t *vm_input, const uint32_t *vm_s, uint32_t *vm_out) {
	cl_uint ret;

	// Copy Random Inputs To OpenCL Buffer
	ret = clEnqueueWriteBuffer(gpu->queue, gpu->vm_input, CL_TRUE, 0, 96 * sizeof(char), vm_input, 0, NULL, NULL);
	if (ret != CL_SUCCESS) {
		applog(LOG_ERR, "ERROR: Unable to write to OpenCL 'vm_input' Buffer (Error: %d)", ret);
		return false;
	}

	// Copy Storage To OpenCL Buffer
	if (g_work_package[g_work_package_idx].storage_sz && (g_work_package[g_work_package_idx].iteration_id > 0)) {
			ret = clEnqueueWriteBuffer(gpu->queue, gpu->vm_s, CL_TRUE, 0, g_work_package[g_work_package_idx].storage_sz * sizeof(uint32_t), vm_s, 0, NULL, NULL);
		if (ret != CL_SUCCESS) {
			applog(LOG_ERR, "ERROR: Unable to write to OpenCL 'vm_s' Buffer (Error: %d)", ret);
			return false;
		}
	}

	// Run OpenCL VM
	ret = clEnqueueNDRangeKernel(gpu->queue, gpu->kernel_execute, gpu->work_dim, NULL, &gpu->global_size[0], &gpu->local_size[0], 0, NULL, NULL);
	if (ret) {
		applog(LOG_ERR, "ERROR: Unable to run 'execute' kernel (Error: %d)", ret);
		return false;
	}

	// Get VM Output
	ret = clEnqueueReadBuffer(gpu->queue, gpu->vm_out, CL_TRUE, 0, gpu->threads * sizeof(uint32_t), vm_out, 0, NULL, NULL);
	if (ret != CL_SUCCESS) {
		applog(LOG_ERR, "ERROR: Unable to read from OpenCL 'vm_out' Buffer (Error: %d)", ret);
		return false;
	}

	return true;
}

// Currently Only Dumps Values From u[]
extern bool dump_opencl_kernel_data(struct opencl_device *gpu, uint32_t *data, int idx, int offset, int len) {
	cl_uint ret;

	ret = clEnqueueReadBuffer(gpu->queue, gpu->vm_u, CL_TRUE, (idx * g_work_package[g_work_package_idx].vm_uints * sizeof(uint32_t)) + (offset * sizeof(uint32_t)), len * sizeof(uint32_t), &data[0], 0, NULL, NULL);
	if (ret != CL_SUCCESS) {
		applog(LOG_ERR, "ERROR: Unable to read from OpenCL 'vm_u' Buffer (Error: %d)", ret);
		return false;
	}

	return true;
}

// Currently Dumps Debug Values From A Hacked Up m[] When POW Is Found
extern bool dump_opencl_debug_data(struct opencl_device *gpu, uint32_t *data, int idx, int offset, int len) {
	cl_uint ret;

	if (len > 12)
		return false;

	ret = clEnqueueReadBuffer(gpu->queue, gpu->vm_m, CL_TRUE, (idx * 12 * sizeof(uint32_t)) + (offset * sizeof(uint32_t)), len * sizeof(uint32_t), &data[0], 0, NULL, NULL);
	if (ret != CL_SUCCESS) {
		applog(LOG_ERR, "ERROR: Unable to read from OpenCL 'vm_m' Buffer (Error: %d)", ret);
		return false;
	}

	return true;
}

#endif