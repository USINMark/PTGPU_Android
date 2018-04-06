/*
Copyright (c) 2009 David Bucciarelli (davibu@interfree.it)

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
 * Based on smallpt, a Path Tracer by Kevin Beason, 2008
 * Modified by David Bucciarelli to show the output via OpenGL/GLUT, ported
 * to C, work with float, fixed RR, ported to OpenCL, etc.
 */

//#include "stdafx.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>

// Jens's patch for MacOS
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include "include/CL/cl.h"
#endif

#include "CLBVH.h"
#include "include/camera.h"
#include "include/scene.h"
#include "include/displayfunc.h"
#include "include/geom.h"
#include "include/native-lib.h"

#ifndef __ANDROID__
#include <GL/glut.h>
//#define M_PI       3.14159265358979323846

bool Read(char *fileName);
#endif

/* Options */
int useGPU = 1;
int forceWorkSize = 0;

/* OpenCL variables */
static cl_context context;
static cl_mem colorBuffer;
static cl_mem pixelBuffer;
static cl_mem seedBuffer;
static cl_mem shapeBuffer;
static cl_mem poiBuffer;
static cl_mem tnBuffer;
static cl_mem tlBuffer;
static cl_mem cameraBuffer;
static cl_command_queue commandQueue;
static cl_program program;
static cl_kernel kernel, kernelRad, kernelBvh, kernelOpt;
unsigned int workGroupSize = 1;

char kernelFileName[MAX_FN] = "/mnt/sdcard/Download/kernels/preprocessed_rendering_kernel.cl";
char bvhFileName[MAX_FN] = "/mnt/sdcard/Download/kernels/BVH.cl";

static Vec *colors;
static unsigned int *seeds;
Camera camera;
static int currentSample = 0;
Shape *shapes;
Poi *pois;
unsigned int shapeCnt = 0, poiCnt = 0;
TreeNode *tn, *tl;
int pixelCount;

void FreeBuffers() {
    cl_int status;

    if (colorBuffer)
    {
        status = clReleaseMemObject(colorBuffer);
        if (status != CL_SUCCESS) {
            LOGE("Failed to release OpenCL color buffer: %d\n", status);
            return;
        }
    }
    if (pixelBuffer)
    {
        status = clReleaseMemObject(pixelBuffer);
        if (status != CL_SUCCESS) {
            LOGE("Failed to release OpenCL pixel buffer: %d\n", status);
            return;
        }
    }
    if (seedBuffer)
    {
        status = clReleaseMemObject(seedBuffer);
        if (status != CL_SUCCESS) {
            LOGE("Failed to release OpenCL seed buffer: %d\n", status);
            return;
        }
    }
    if (seeds) free(seeds);
    if (colors) free(colors);
    if (pixels) free(pixels);
}

void AllocateBuffers() {
    pixelCount = width * height;
    int i;

    colors = (Vec *)malloc(sizeof(Vec) * pixelCount);

    seeds = (unsigned int *)malloc(sizeof(unsigned int) * pixelCount * 2);
    for (i = 0; i < pixelCount * 2; i++) {
        seeds[i] = rand();
        if (seeds[i] < 2)
            seeds[i] = 2;
    }

    pixels = (unsigned int *)malloc(sizeof(unsigned int) * pixelCount);

    cl_int status;
    cl_uint sizeBytes = sizeof(Vec) * width * height;

    colorBuffer = clCreateBuffer(
            context,
            CL_MEM_READ_WRITE,
            sizeBytes,
            NULL,
            &status);
    if (status != CL_SUCCESS) {
        LOGE("Failed to create OpenCL output buffer: %d\n", status);
        return;
    }

    sizeBytes = sizeof(unsigned char[4]) * width * height;
    pixelBuffer = clCreateBuffer(
            context,
            CL_MEM_WRITE_ONLY,
            sizeBytes,
            NULL,
            &status);
    if (status != CL_SUCCESS) {
        LOGE("Failed to create OpenCL pixel buffer: %d\n", status);
        return;
    }

    sizeBytes = sizeof(unsigned int) * width * height * 2;
    seedBuffer = clCreateBuffer(
            context,
            CL_MEM_READ_WRITE,
            sizeBytes,
            NULL,
            &status);
    if (status != CL_SUCCESS) {
        LOGE("Failed to create OpenCL seed buffer: %d\n", status);
        return;
    }
    status = clEnqueueWriteBuffer(
            commandQueue,
            seedBuffer,
            CL_TRUE,
            0,
            sizeof(unsigned int) * width * height * 2,
            seeds,
            0,
            NULL,
            NULL);
    if (status != CL_SUCCESS) {
        LOGE("Failed to write the OpenCL seeds buffer: %d\n", status);
        return;
    }
}

char *ReadSources(const char *fileName) {
    FILE *file = fopen(fileName, "r");
    if (!file) {
        LOGE("Failed to open file '%s'\n", fileName);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END)) {
        LOGE("Failed to seek file '%s'\n", fileName);
        return NULL;
    }

    long size = ftell(file);
    if (size == 0) {
        LOGE("Failed to check position on file '%s'\n", fileName);
        return NULL;
    }

    rewind(file);

    char *src = (char *)malloc(sizeof(char) * size + 1);
    if (!src) {
        LOGE("Failed to allocate memory for file '%s'\n", fileName);
        return NULL;
    }

    LOGI("Reading file '%s' (size %ld bytes)\n", fileName, size);
    size_t res = fread(src, 1, sizeof(char) * size, file);
    if (res != sizeof(char) * size) {
        //LOGE("Failed to read file '%s' (read %lu)\n Content: %s\n", fileName, res, src);
        //return NULL;
    }
    src[res] = '\0'; /* NULL terminated */

    fclose(file);

    return src;

}

#define MAX_STYPE 255

void SetUpOpenCL() {
    cl_device_type dType;

    if (useGPU)
        dType = CL_DEVICE_TYPE_GPU;
    else
        dType = CL_DEVICE_TYPE_CPU;

    // Select the platform

    cl_uint numPlatforms;
    cl_platform_id platform = NULL;
    cl_int status = clGetPlatformIDs(0, NULL, &numPlatforms);
    if (status != CL_SUCCESS) {
        LOGE("Failed to get OpenCL platforms\n");
        return ;
    }

    if (numPlatforms > 0) {
        cl_platform_id *platforms = (cl_platform_id *)malloc(sizeof(cl_platform_id) * numPlatforms);
        status = clGetPlatformIDs(numPlatforms, platforms, NULL);
        if (status != CL_SUCCESS) {
            LOGE("Failed to get OpenCL platform IDs\n");
            return ;
        }

        unsigned int i;
        for (i = 0; i < numPlatforms; ++i) {
            char pbuf[100];
            status = clGetPlatformInfo(platforms[i],
                                       CL_PLATFORM_VENDOR,
                                       sizeof(pbuf),
                                       pbuf,
                                       NULL);

            status = clGetPlatformIDs(numPlatforms, platforms, NULL);
            if (status != CL_SUCCESS) {
                LOGE("Failed to get OpenCL platform IDs\n");
                return ;
            }

            LOGI("OpenCL Platform %d: %s\n", i, pbuf);
        }

        platform = platforms[0];
        free(platforms);
    }

    // Select the device

    cl_device_id devices[32];
    cl_uint deviceCount;
    status = clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 32, devices, &deviceCount);
    if (status != CL_SUCCESS) {
        LOGE("Failed to get OpenCL device IDs\n");
        return ;
    }

    int deviceFound = 0;
    cl_device_id selectedDevice;
    unsigned int i;
    for (i = 0; i < deviceCount; ++i) {
        cl_device_type type = 0;
        status = clGetDeviceInfo(devices[i],
                                 CL_DEVICE_TYPE,
                                 sizeof(cl_device_type),
                                 &type,
                                 NULL);
        if (status != CL_SUCCESS) {
            LOGE("Failed to get OpenCL device info: %d\n", status);
            return ;
        }

        char stype[MAX_STYPE];
        switch (type) {
            case CL_DEVICE_TYPE_ALL:
                strcpy(stype, "TYPE_ALL");
                break;
            case CL_DEVICE_TYPE_DEFAULT:
                strcpy(stype, "TYPE_DEFAULT");
                break;
            case CL_DEVICE_TYPE_CPU:
                strcpy(stype, "TYPE_CPU");
                if (!useGPU && !deviceFound) {
                    selectedDevice = devices[i];
                    deviceFound = 1;
                }
                break;
            case CL_DEVICE_TYPE_GPU:
                strcpy(stype, "TYPE_GPU");
                if (useGPU && !deviceFound) {
                    selectedDevice = devices[i];
                    deviceFound = 1;
                }
                break;
            default:
                strcpy(stype, "TYPE_UNKNOWN");
                break;
        }
        LOGI("OpenCL Device %d: Type = %s\n", i, stype);

        char buf[256];
        status = clGetDeviceInfo(devices[i],
                                 CL_DEVICE_NAME,
                                 sizeof(char[256]),
                                 &buf,
                                 NULL);
        if (status != CL_SUCCESS) {
            LOGE("Failed to get OpenCL device info: %d\n", status);
            return ;
        }

        LOGI("OpenCL Device %d: Name = %s\n", i, buf);

        cl_uint units = 0;
        status = clGetDeviceInfo(devices[i],
                                 CL_DEVICE_MAX_COMPUTE_UNITS,
                                 sizeof(cl_uint),
                                 &units,
                                 NULL);
        if (status != CL_SUCCESS) {
            LOGE("Failed to get OpenCL device info: %d\n", status);
            return ;
        }

        LOGI("OpenCL Device %d: Compute units = %u\n", i, units);

        size_t gsize = 0;
        status = clGetDeviceInfo(devices[i],
                                 CL_DEVICE_MAX_WORK_GROUP_SIZE,
                                 sizeof(size_t),
                                 &gsize,
                                 NULL);
        if (status != CL_SUCCESS) {
            LOGE("Failed to get OpenCL device info: %d\n", status);
            return ;
        }

        LOGI("OpenCL Device %d: Max. work group size = %d\n", i, (unsigned int)gsize);
    }

    if (!deviceFound) {
        LOGE("Unable to select an appropriate device\n");
        return ;
    }

    // Create the context

    cl_context_properties cps[3] = {
            CL_CONTEXT_PLATFORM,
            (cl_context_properties) platform,
            0
    };

    cl_context_properties *cprops = (NULL == platform) ? NULL : cps;
    context = clCreateContext(
            cprops,
            1,
            &selectedDevice,
            NULL,
            NULL,
            &status);
    if (status != CL_SUCCESS) {
        LOGE("Failed to open OpenCL context\n");
        return ;
    }

    /* Get the device list data */
    size_t deviceListSize;
    status = clGetContextInfo(
            context,
            CL_CONTEXT_DEVICES,
            32,
            devices,
            &deviceListSize);
    if (status != CL_SUCCESS) {
        LOGE("Failed to get OpenCL context info: %d\n", status);
        return ;
    }

    /* Print devices list */
    for (i = 0; i < deviceListSize / sizeof(cl_device_id); ++i) {
        cl_device_type type = 0;
        status = clGetDeviceInfo(devices[i],
                                 CL_DEVICE_TYPE,
                                 sizeof(cl_device_type),
                                 &type,
                                 NULL);
        if (status != CL_SUCCESS) {
            LOGE("Failed to get OpenCL device info: %d\n", status);
            return ;
        }

        char stype[MAX_STYPE];
        switch (type) {
            case CL_DEVICE_TYPE_ALL:
                strcpy(stype, "TYPE_ALL");
                break;
            case CL_DEVICE_TYPE_DEFAULT:
                strcpy(stype, "TYPE_DEFAULT");
                break;
            case CL_DEVICE_TYPE_CPU:
                strcpy(stype, "TYPE_CPU");
                break;
            case CL_DEVICE_TYPE_GPU:
                strcpy(stype, "TYPE_GPU");
                break;
            default:
                strcpy(stype, "TYPE_UNKNOWN");
                break;
        }
        LOGI("[SELECTED] OpenCL Device %d: Type = %s\n", i, stype);

        char buf[256];
        status = clGetDeviceInfo(devices[i],
                                 CL_DEVICE_NAME,
                                 sizeof(char[256]),
                                 &buf,
                                 NULL);
        if (status != CL_SUCCESS) {
            LOGE("Failed to get OpenCL device info: %d\n", status);
            return ;
        }

        LOGI("[SELECTED] OpenCL Device %d: Name = %s\n", i, buf);

        cl_uint units = 0;
        status = clGetDeviceInfo(devices[i],
                                 CL_DEVICE_MAX_COMPUTE_UNITS,
                                 sizeof(cl_uint),
                                 &units,
                                 NULL);
        if (status != CL_SUCCESS) {
            LOGE("Failed to get OpenCL device info: %d\n", status);
            return ;
        }

        LOGI("[SELECTED] OpenCL Device %d: Compute units = %u\n", i, units);

        size_t gsize = 0;
        status = clGetDeviceInfo(devices[i],
                                 CL_DEVICE_MAX_WORK_GROUP_SIZE,
                                 sizeof(size_t),
                                 &gsize,
                                 NULL);
        if (status != CL_SUCCESS) {
            LOGE("Failed to get OpenCL device info: %d\n", status);
            return ;
        }

        LOGI("[SELECTED] OpenCL Device %d: Max. work group size = %d\n", i, (unsigned int)gsize);
    }

    cl_command_queue_properties prop = 0;
    commandQueue = clCreateCommandQueue(
            context,
            devices[0],
            prop,
            &status);
    if (status != CL_SUCCESS) {
        LOGE("Failed to create OpenCL command queue: %d\n", status);
        return ;
    }

    /*------------------------------------------------------------------------*/
    shapeBuffer = clCreateBuffer(
            context,
            CL_MEM_READ_ONLY,
            sizeof(Shape) * shapeCnt,
            NULL,
            &status);
    if (status != CL_SUCCESS) {
        LOGE("Failed to create OpenCL shape buffer: %d\n", status);
        return ;
    }
    status = clEnqueueWriteBuffer(
            commandQueue,
            shapeBuffer,
            CL_TRUE,
            0,
            sizeof(Shape) * shapeCnt,
            shapes,
            0,
            NULL,
            NULL);
    if (status != CL_SUCCESS) {
        LOGE("Failed to write the OpenCL shape buffer: %d\n", status);
        return ;
    }

    /*------------------------------------------------------------------------*/
    poiBuffer = clCreateBuffer(
            context,
            CL_MEM_READ_ONLY,
            sizeof(Poi) * poiCnt,
            NULL,
            &status);
    if (status != CL_SUCCESS) {
        LOGE("Failed to create OpenCL point buffer: %d\n", status);
        return;
    }
    status = clEnqueueWriteBuffer(
            commandQueue,
            poiBuffer,
            CL_TRUE,
            0,
            sizeof(Poi) * poiCnt,
            pois,
            0,
            NULL,
            NULL);
    if (status != CL_SUCCESS) {
        LOGE("Failed to write the OpenCL point buffer: %d\n", status);
        return;
    }

    /*------------------------------------------------------------------------*/
    tnBuffer = clCreateBuffer(
            context,
            CL_MEM_READ_ONLY,
            sizeof(TreeNode) * (shapeCnt-1),
            NULL,
            &status);
    if (status != CL_SUCCESS) {
        LOGE("Failed to create OpenCL BVH buffer 1: %d\n", status);
        return;
    }
    tlBuffer = clCreateBuffer(
            context,
            CL_MEM_READ_ONLY,
            sizeof(TreeNode) * shapeCnt,
            NULL,
            &status);
    if (status != CL_SUCCESS) {
        LOGE("Failed to create OpenCL BVH buffer 2: %d\n", status);
        return;
    }

    cameraBuffer = clCreateBuffer(
            context,
            CL_MEM_READ_ONLY,
            sizeof(Camera),
            NULL,
            &status);
    if (status != CL_SUCCESS) {
        LOGE("Failed to create OpenCL camera buffer: %d\n", status);
        return ;
    }
    status = clEnqueueWriteBuffer(
            commandQueue,
            cameraBuffer,
            CL_TRUE,
            0,
            sizeof(Camera),
            &camera,
            0,
            NULL,
            NULL);
    if (status != CL_SUCCESS) {
        LOGE("Failed to write the OpenCL camera buffer: %d\n", status);
        return ;
    }

    AllocateBuffers();

    /*------------------------------------------------------------------------*/
    /* Create the kernel program */
    const char *sources = ReadSources(kernelFileName);
    program = clCreateProgramWithSource(
            context,
            1,
            &sources,
            NULL,
            &status);
    if (status != CL_SUCCESS) {
        LOGE("Failed to open OpenCL kernel sources: %d\n", status);
        return ;
    }

    status = clBuildProgram(program, 1, devices, "-I. -I/storage/emulated/0/Download/kernels/", NULL, NULL);
    if (status != CL_SUCCESS) {
        LOGE("Failed to build OpenCL kernel: %d\n", status);

        size_t retValSize;
        status = clGetProgramBuildInfo(
                program,
                devices[0],
                CL_PROGRAM_BUILD_LOG,
                0,
                NULL,
                &retValSize);
        if (status != CL_SUCCESS) {
            LOGE("Failed to get OpenCL kernel info size: %d\n", status);
            return ;
        }

        char *buildLog = (char *)malloc(retValSize + 1);
        status = clGetProgramBuildInfo(
                program,
                devices[0],
                CL_PROGRAM_BUILD_LOG,
                retValSize,
                buildLog,
                NULL);
        if (status != CL_SUCCESS) {
            LOGE("Failed to get OpenCL kernel info: %d\n", status);
            return ;
        }
        buildLog[retValSize] = '\0';

#if 0
        // Query binary (PTX file) size
		size_t bin_sz;
		status = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &bin_sz, NULL);

		// Read binary (PTX file) to memory bufferf
		unsigned char *bin = (unsigned char *)malloc(bin_sz);
		status = clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(unsigned char *), &bin, NULL);

		char ptxFileName[255];
		strcpy(ptxFileName, kernelFileName);
		strcat(ptxFileName, ".ptx");

		// Save PTX to add_vectors_ocl.ptx
		FILE *fp = fopen(ptxFileName, "wb");
		fwrite(bin, sizeof(char), bin_sz, fp);
		fclose(fp);

		free(bin);
#endif
        LOGE("OpenCL Programm Build Log: %s\n", buildLog);

        FILE *fp = fopen("/storage/emulated/0/Download/kernels/error_renderingkernel.txt", "wt");
        fwrite(buildLog, sizeof(char), retValSize + 1, fp);
        fclose(fp);

        free(buildLog);
        return ;
    }

    kernel = clCreateKernel(program, "RadianceGPU", &status);
    if (status != CL_SUCCESS) {
        LOGE("Failed to create OpenCL RadianceGPU kernel: %d\n", status);
        return ;
    }

    /* Create the kernel program */
    const char *sourcesBvh = ReadSources(bvhFileName);
    program = clCreateProgramWithSource(
            context,
            1,
            &sourcesBvh,
            NULL,
            &status);
    if (status != CL_SUCCESS) {
        LOGE("Failed to open OpenCL kernel sources (BVH): %d\n", status);
        return;
    }

    status = clBuildProgram(program, 1, devices, "-I. -I/storage/emulated/0/Download/kernels/", NULL, NULL);
    if (status != CL_SUCCESS) {
        LOGE("Failed to build OpenCL kernel (BVH): %d\n", status);

        size_t retValSize;
        status = clGetProgramBuildInfo(
                program,
                devices[0],
                CL_PROGRAM_BUILD_LOG,
                0,
                NULL,
                &retValSize);
        if (status != CL_SUCCESS) {
            LOGE("Failed to get OpenCL kernel info size: %d\n", status);
            return;
        }

        char *buildLog = (char *)malloc(retValSize + 1);
        status = clGetProgramBuildInfo(
                program,
                devices[0],
                CL_PROGRAM_BUILD_LOG,
                retValSize,
                buildLog,
                NULL);
        if (status != CL_SUCCESS) {
            LOGE("Failed to get OpenCL kernel info: %d\n", status);
            return;
        }
        buildLog[retValSize] = '\0';
#if 0
        // Query binary (PTX file) size
		size_t bin_sz;
		status = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &bin_sz, NULL);

		// Read binary (PTX file) to memory bufferf
		unsigned char *bin = (unsigned char *)malloc(bin_sz);
		status = clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(unsigned char *), &bin, NULL);

		char ptxFileName[255];
		strcpy(ptxFileName, bvhFileName);
		strcat(ptxFileName, ".ptx");

		// Save PTX to add_vectors_ocl.ptx
		FILE *fp = fopen(ptxFileName, "wb");
		fwrite(bin, sizeof(char), bin_sz, fp);
		fclose(fp);

		free(bin);
#endif
        LOGE("OpenCL Programm Build Log: %s\n", buildLog);

        FILE *fp = fopen("/storage/emulated/0/Download/kernels/error_BVH.txt", "wt");
        fwrite(buildLog, sizeof(char), retValSize + 1, fp);
        fclose(fp);

        free(buildLog);
        return;
    }

    kernelRad = clCreateKernel(program, "kernelConstructRadixTree", &status);
    if (status != CL_SUCCESS) {
        LOGE("Failed to create OpenCL kernelConstructRadixTree kernel: %d\n", status);
        return;
    }

    kernelBvh = clCreateKernel(program, "kernelConstructBVHTree", &status);
    if (status != CL_SUCCESS) {
        LOGE("Failed to create OpenCL kernelConstructBVHTree kernel: %d\n", status);
        return;
    }

    kernelOpt = clCreateKernel(program, "kernelOptimize", &status);
    if (status != CL_SUCCESS) {
        LOGE("Failed to create OpenCL kernelOptimize kernel: %d\n", status);
        return;
    }

    // LordCRC's patch for better workGroupSize
    size_t gsize = 0;
    status = clGetKernelWorkGroupInfo(kernel,
                                      devices[0],
                                      CL_KERNEL_WORK_GROUP_SIZE,
                                      sizeof(size_t),
                                      &gsize,
                                      NULL);
    if (status != CL_SUCCESS) {
        LOGE("Failed to get OpenCL kernel work group size info: %d\n", status);
        return ;
    }

    workGroupSize = (unsigned int) gsize;
    LOGI("OpenCL Device 0: kernel work group size = %d\n", workGroupSize);

    if (forceWorkSize > 0) {
        LOGI("OpenCL Device 0: forced kernel work group size = %d\n", forceWorkSize);
        workGroupSize = forceWorkSize;
    }
}

void ExecuteKernel() {
    /* Enqueue a kernel run call */
    size_t globalThreads[1];

    globalThreads[0] = width * height;

    if (globalThreads[0] % workGroupSize != 0)
        globalThreads[0] = (globalThreads[0] / workGroupSize + 1) * workGroupSize;

    size_t localThreads[1];

    localThreads[0] = workGroupSize;

    cl_int status = clEnqueueNDRangeKernel(
            commandQueue,
            kernel,
            1,
            NULL,
            globalThreads,
            localThreads,
            0,
            NULL,
            NULL);
    if (status != CL_SUCCESS) {
        LOGE("Failed to enqueue OpenCL work: %d\n", status);
        return ;
    }
}

unsigned int *DrawFrame() {
    int len = pixelCount * sizeof(unsigned int);
    double startTime = WallClockTime(), setStartTime, kernelStartTime, readStartTime;
    double setTotalTime = 0.0, kernelTotalTime = 0.0, readTotalTime = 0.0;
    int startSampleCount = currentSample;

    setStartTime = WallClockTime();

    /* Set kernel arguments */
    cl_int status = clSetKernelArg(
            kernel,
            0,
            sizeof(cl_mem),
            (void *) &colorBuffer);
    if (status != CL_SUCCESS) {
        LOGE("Failed to set OpenCL arg. #1: %d\n", status);
        return NULL;
    }

    status = clSetKernelArg(
            kernel,
            1,
            sizeof(cl_mem),
            (void *) &seedBuffer);
    if (status != CL_SUCCESS) {
        LOGE("Failed to set OpenCL arg. #2: %d\n", status);
        return NULL;
    }

    status = clSetKernelArg(
            kernel,
            2,
            sizeof(cl_mem),
            (void *) &shapeBuffer);
    if (status != CL_SUCCESS) {
        LOGE("Failed to set OpenCL arg. #3: %d\n", status);
        return NULL;
    }

    status = clSetKernelArg(
            kernel,
            3,
            sizeof(cl_mem),
            (void *) &cameraBuffer);
    if (status != CL_SUCCESS) {
        LOGE("Failed to set OpenCL arg. #4: %d\n", status);
        return NULL;
    }

    status = clSetKernelArg(
            kernel,
            4,
            sizeof(unsigned int),
            (void *) &shapeCnt);
    if (status != CL_SUCCESS) {
        LOGE("Failed to set OpenCL arg. #5: %d\n", status);
        return NULL;
    }

    status = clSetKernelArg(
            kernel,
            5,
            sizeof(cl_mem),
            (void *)&poiBuffer);
    if (status != CL_SUCCESS) {
        LOGE("Failed to set OpenCL arg. #6: %d\n", status);
        return NULL;
    }

    status = clSetKernelArg(
            kernel,
            6,
            sizeof(unsigned int),
            (void *)&poiCnt);
    if (status != CL_SUCCESS) {
        LOGE("Failed to set OpenCL arg. #7: %d\n", status);
        return NULL;
    }

    status = clSetKernelArg(
            kernel,
            7,
            sizeof(cl_mem),
            (void *)&tnBuffer);
    if (status != CL_SUCCESS) {
        LOGE("Failed to set OpenCL arg. #8: %d\n", status);
        return NULL;
    }

    status = clSetKernelArg(
            kernel,
            8,
            sizeof(cl_mem),
            (void *)&tlBuffer);
    if (status != CL_SUCCESS) {
        LOGE("Failed to set OpenCL arg. #9: %d\n", status);
        return NULL;
    }

    status = clSetKernelArg(
            kernel,
            9,
            sizeof(int),
            (void *) &width);
    if (status != CL_SUCCESS) {
        LOGE("Failed to set OpenCL arg. #10: %d\n", status);
        return NULL;
    }

    status = clSetKernelArg(
            kernel,
            10,
            sizeof(int),
            (void *) &height);
    if (status != CL_SUCCESS) {
        LOGE("Failed to set OpenCL arg. #11: %d\n", status);
        return NULL;
    }

    status = clSetKernelArg(
            kernel,
            11,
            sizeof(int),
            (void *) &currentSample);
    if (status != CL_SUCCESS) {
        LOGE("Failed to set OpenCL arg. #12: %d\n", status);
        return NULL;
    }

    status = clSetKernelArg(
            kernel,
            12,
            sizeof(cl_mem),
            (void *) &pixelBuffer);
    if (status != CL_SUCCESS) {
        LOGE("Failed to set OpenCL arg. #13: %d\n", status);
        return NULL;
    }

    setTotalTime += (WallClockTime() - setStartTime);

    //--------------------------------------------------------------------------
#ifdef CURRENT_SAMPLE
    if (currentSample < 20) {
#endif
    kernelStartTime = WallClockTime();
    ExecuteKernel();
    clFinish(commandQueue);
    kernelTotalTime += (WallClockTime() - kernelStartTime);

    currentSample++;
#ifdef CURRENT_SAMPLE
    }
    else {
        /* After first 20 samples, continue to execute kernels for more and more time */
        const float k = min(currentSample - 20, 100) / 100.f;
        const float tresholdTime = 0.5f * k * 1000.0f;
        for (;;) {
            kernelStartTime = WallClockTime();
            ExecuteKernel();
            //clFinish(commandQueue);
            kernelTotalTime += (WallClockTime() - kernelStartTime);

            currentSample++;
            const float elapsedTime = WallClockTime() - startTime;
            if (elapsedTime > tresholdTime)
                break;
        }
    }
#endif
    readStartTime = WallClockTime();

    //--------------------------------------------------------------------------
    /* Enqueue readBuffer */
    status = clEnqueueReadBuffer(
            commandQueue,
            pixelBuffer,
            CL_TRUE,
            0,
            len,
            pixels,
            0,
            NULL,
            NULL);
    if (status != CL_SUCCESS) {
        LOGE("Failed to read the OpenCL pixel buffer: %d\n", status);
        return NULL;
    }

    clFinish(commandQueue);
    readTotalTime += (WallClockTime()-readStartTime);

    /*------------------------------------------------------------------------*/
    const double elapsedTime = WallClockTime() - startTime;
    const int samples = currentSample - startSampleCount;
    const double sampleSec = samples * height * width / elapsedTime;
    LOGI( "Set time %.5f msec, Kernel time %.5f msec, Read time %.5f msec, Total time %.5f msec (pass %d)  Sample/sec  %.1fK\n",
          setTotalTime, kernelTotalTime, readTotalTime, elapsedTime, currentSample, sampleSec / 1000.f);

    return pixels;
}

void ReInitScene() {
    currentSample = 0;

    // Redownload the scene

    cl_int status = clEnqueueWriteBuffer(
            commandQueue,
            shapeBuffer,
            CL_TRUE,
            0,
            sizeof(Shape) * shapeCnt,
            shapes,
            0,
            NULL,
            NULL);
    if (status != CL_SUCCESS) {
        LOGE("Failed to write the OpenCL scene buffer: %d\n", status);
        return ;
    }
}

void ReInit(const int reallocBuffers) {
    // Check if I have to reallocate buffers
    if (reallocBuffers) {
        FreeBuffers();
        UpdateCamera();
        AllocateBuffers();
    } else {
        UpdateCamera();
    }

    cl_int status = clEnqueueWriteBuffer(
            commandQueue,
            cameraBuffer,
            CL_TRUE,
            0,
            sizeof(Camera),
            &camera,
            0,
            NULL,
            NULL);
    if (status != CL_SUCCESS) {
        LOGE("Failed to write the OpenCL camera buffer: %d\n", status);
        return ;
    }

    status = clEnqueueWriteBuffer(
            commandQueue,
            tnBuffer,
            CL_TRUE,
            0,
            sizeof(TreeNode) * (shapeCnt - 1),
            tn,
            0,
            NULL,
            NULL);
    if (status != CL_SUCCESS) {
        LOGE("Failed to write the OpenCL BVH buffer 1: %d\n", status);
        return;
    }

    status = clEnqueueWriteBuffer(
            commandQueue,
            tlBuffer,
            CL_TRUE,
            0,
            sizeof(TreeNode) * (shapeCnt),
            tl,
            0,
            NULL,
            NULL);
    if (status != CL_SUCCESS) {
        LOGE("Failed to write the OpenCL BVH buffer 2: %d\n", status);
        return;
    }
    currentSample = 0;
}

#if 1

#define MOVE_STEP 10.0f
#define ROTATE_STEP (2.f * M_PI / 180.f)





void AddWallLight()
{
    shapes[shapeCnt].type = SPHERE;
    shapes[shapeCnt].s.rad = WALL_RAD;
    shapes[shapeCnt].s.p.x = WALL_RAD + 25.0f, shapes[shapeCnt].s.p.y = 0.0f , shapes[shapeCnt].s.p.z = 0.0f;
    shapes[shapeCnt].s.e.x = 0.0f, shapes[shapeCnt].s.e.y = 0.0f , shapes[shapeCnt].s.e.z = 0.0f;
    shapes[shapeCnt].s.c.x = .75f, shapes[shapeCnt].s.c.y = .25f , shapes[shapeCnt].s.c.z = .25f;
    shapes[shapeCnt++].s.refl = DIFF;
    //shapes[shapeCnt++].s = { WALL_RAD,{ WALL_RAD + 25.0f, 0.0f, 0.0f },{ 0.f, 0.f, 0.f },{ .75f, .25f, .25f }, DIFF }; /* Left */

    shapes[shapeCnt].type = SPHERE;
    shapes[shapeCnt].s.rad = WALL_RAD;
    shapes[shapeCnt].s.p.x = -WALL_RAD - 25.0f, shapes[shapeCnt].s.p.y = 0.0f , shapes[shapeCnt].s.p.z = 0.0f;
    shapes[shapeCnt].s.e.x = 0.0f, shapes[shapeCnt].s.e.y = 0.0f , shapes[shapeCnt].s.e.z = 0.0f;
    shapes[shapeCnt].s.c.x = .25f, shapes[shapeCnt].s.c.y = .25f , shapes[shapeCnt].s.c.z = .75f;
    shapes[shapeCnt++].s.refl = DIFF;
    //shapes[shapeCnt++].s = { WALL_RAD,{ -WALL_RAD - 25.0f, 0.0f, 0.0f },{ 0.f, 0.f, 0.f },{ .25f, .25f, .75f }, DIFF }; /* Rght */

    shapes[shapeCnt].type = SPHERE;
    shapes[shapeCnt].s.rad = WALL_RAD;
    shapes[shapeCnt].s.p.x = 0.0f, shapes[shapeCnt].s.p.y = 0.0f , shapes[shapeCnt].s.p.z = WALL_RAD - 25.0f;
    shapes[shapeCnt].s.e.x = 0.0f, shapes[shapeCnt].s.e.y = 0.0f , shapes[shapeCnt].s.e.z = 0.0f;
    shapes[shapeCnt].s.c.x = .75f, shapes[shapeCnt].s.c.y = .75f , shapes[shapeCnt].s.c.z = .75f;
    shapes[shapeCnt++].s.refl = DIFF;
    //shapes[shapeCnt++].s = { WALL_RAD,{ 0.0f, 0.0f, WALL_RAD - 25.0f },{ 0.f, 0.f, 0.f },{ .75f, .75f, .75f }, DIFF }; /* Back */

    shapes[shapeCnt].type = SPHERE;
    shapes[shapeCnt].s.rad = WALL_RAD;
    shapes[shapeCnt].s.p.x = 0.0f, shapes[shapeCnt].s.p.y = 0.0f , shapes[shapeCnt].s.p.z = -WALL_RAD + 100.0f;
    shapes[shapeCnt].s.e.x = 0.0f, shapes[shapeCnt].s.e.y = 0.0f , shapes[shapeCnt].s.e.z = 0.0f;
    shapes[shapeCnt].s.c.x = 0.0f, shapes[shapeCnt].s.c.y = 0.0f , shapes[shapeCnt].s.c.z = 0.0f;
    shapes[shapeCnt++].s.refl = DIFF;
    //shapes[shapeCnt++].s = { WALL_RAD,{ 0.0f, 0.0f, -WALL_RAD + 100.0f },{ 0.f, 0.f, 0.f },{ 0.f, 0.f, 0.f }, DIFF }; /* Frnt */

    shapes[shapeCnt].type = SPHERE;
    shapes[shapeCnt].s.rad = WALL_RAD;
    shapes[shapeCnt].s.p.x = 0.0f, shapes[shapeCnt].s.p.y = WALL_RAD + 25.0f , shapes[shapeCnt].s.p.z = 0.0f;
    shapes[shapeCnt].s.e.x = 0.0f, shapes[shapeCnt].s.e.y = 0.0f , shapes[shapeCnt].s.e.z = 0.0f;
    shapes[shapeCnt].s.c.x = .75f, shapes[shapeCnt].s.c.y = .75f , shapes[shapeCnt].s.c.z = .75f;
    shapes[shapeCnt++].s.refl = DIFF;
    //shapes[shapeCnt++].s = { WALL_RAD,{ 0.0f, WALL_RAD + 25.0f, 0.0f },{ 0.f, 0.f, 0.f },{ .75f, .75f, .75f }, DIFF }; /* Botm */

    shapes[shapeCnt].type = SPHERE;
    shapes[shapeCnt].s.rad = WALL_RAD;
    shapes[shapeCnt].s.p.x = 0.0f, shapes[shapeCnt].s.p.y =  -WALL_RAD - 25.0f , shapes[shapeCnt].s.p.z = 0.0f;
    shapes[shapeCnt].s.e.x = 0.0f, shapes[shapeCnt].s.e.y = 0.0f , shapes[shapeCnt].s.e.z = 0.0f;
    shapes[shapeCnt].s.c.x = .75f, shapes[shapeCnt].s.c.y = .75f , shapes[shapeCnt].s.c.z = .75f;
    shapes[shapeCnt++].s.refl = DIFF;
    //shapes[shapeCnt++].s = { WALL_RAD,{ 0.0f, -WALL_RAD - 25.0f, 0.0f },{ 0.f, 0.f, 0.f },{ .75f, .75f, .75f }, DIFF }; /* Top */

    shapes[shapeCnt].type = SPHERE;
    shapes[shapeCnt].s.rad = 5.0f;
    shapes[shapeCnt].s.p.x = 10.0f, shapes[shapeCnt].s.p.y = -10.0f , shapes[shapeCnt].s.p.z = 0.0f;
    shapes[shapeCnt].s.e.x = 0.0f, shapes[shapeCnt].s.e.y = 0.0f , shapes[shapeCnt].s.e.z = 0.0f;
    shapes[shapeCnt].s.c.x = .9f, shapes[shapeCnt].s.c.y = .9f , shapes[shapeCnt].s.c.z = .9f;
    shapes[shapeCnt++].s.refl = SPEC;
    //shapes[shapeCnt++].s = { 5.0f, { 10.0f, -10.0f, 0.0f }, { 0.f, 0.f, 0.f }, { .9f, .9f, .9f }, SPEC }; /* Mirr */

    shapes[shapeCnt].type = SPHERE;
    shapes[shapeCnt].s.rad = 5.0f;
    shapes[shapeCnt].s.p.x = -5.0f, shapes[shapeCnt].s.p.y = -10.0f , shapes[shapeCnt].s.p.z = 0.0f;
    shapes[shapeCnt].s.e.x = 0.0f, shapes[shapeCnt].s.e.y = 0.0f , shapes[shapeCnt].s.e.z = 0.0f;
    shapes[shapeCnt].s.c.x = .9f, shapes[shapeCnt].s.c.y = .9f , shapes[shapeCnt].s.c.z = .9f;
    shapes[shapeCnt++].s.refl = REFR;
    //shapes[shapeCnt++].s = { 5.0f,{ -5.0f, -10.0f, 0.0f },{ 0.f, 0.f, 0.f },{ .9f, .9f, .9f }, REFR }; /* Glas */

    shapes[shapeCnt].type = SPHERE;
    shapes[shapeCnt].s.rad = 2.f;
    shapes[shapeCnt].s.p.x = 10.0f, shapes[shapeCnt].s.p.y = 15.0f , shapes[shapeCnt].s.p.z = 0.0f;
    shapes[shapeCnt].s.e.x = 12.0f, shapes[shapeCnt].s.e.y = 12.0f , shapes[shapeCnt].s.e.z = 12.0f;
    shapes[shapeCnt].s.c.x = 0.f, shapes[shapeCnt].s.c.y = 0.f , shapes[shapeCnt].s.c.z = 0.f;
    shapes[shapeCnt++].s.refl = DIFF;
    //shapes[shapeCnt++].s = { 2.f, { 10.0f, 15.0f, 0.0f }, { 12.f, 12.f, 12.f }, { 0.f, 0.f, 0.f }, DIFF }; /* Lite */
}

void BuildBVH()
{
    CLBVH *pCB = new CLBVH(shapes, shapeCnt, pois, poiCnt, commandQueue, context, kernelRad, kernelBvh, kernelOpt);

    pCB->buildRadixTree();
    pCB->buildBVHTree();
    //pCB->optimize();

    pCB->getTrees(&tn, &tl);
    /*
    pCB->makeNaiveBVHTree();
    pCB->getTree(&nbtn, &nbtnCnt);
    */
}
#endif