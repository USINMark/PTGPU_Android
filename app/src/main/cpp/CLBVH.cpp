
//#include "stdafx.h"
#include "CLBVH.h"
#include <memory.h>

#define BLOCK_SIZE 256

#define CODE_OFFSET (1<<10)
#define CODE_LENGTH (10)
//#define DEBUG_TREE 

#define clErrchk(ans) { clAssert((ans), __FILE__, __LINE__); }

inline void clAssert(cl_int code, const char *file, int line)
{
	if (code != CL_SUCCESS)
	{
		LOGI("clAssert: %d %s %d\n", code, file, line);
	}
}

inline int min2(int a, int b)
{
	return (a < b) ? a : b;
}

inline int max2(int a, int b)
{
	return (a > b) ? a : b;
}

inline float min2(float a, float b)
{
	return (a < b) ? a : b;
}

inline float max2(float a, float b)
{
	return (a > b) ? a : b;
}

inline float min3(float a, float b, float c)
{
	return min2(min2(a, b), c);
}

inline float max3(float a, float b, float c)
{
	return max2(max2(a, b), c);
}

inline void swapGeometry(Geometry *a, Geometry *b)
{
	Geometry temp;

	temp = *a;
	*a = *b;
	*b = temp;
}

int *CLBVH::bubblesort_Geometry(Geometry *geo)
{
	int *sorted_geometry_indices = new int[m_shapeCnt];

	for (int level = 0; level < m_shapeCnt - 1; level++)
	{
		for (int i = 0; i < m_shapeCnt - level - 1; i++)
		{
			if (geo[i].key_mc < geo[i + 1].key_mc) swapGeometry(&geo[i], &geo[i + 1]);
		}		
	}

	for (int i = 0; i < m_shapeCnt; i++)
		sorted_geometry_indices[i] = geo[i].value;

	return sorted_geometry_indices;
}

Bound CLBVH::getBound(Sphere s)
{
	Bound b;

	b.min_x = s.p.x - s.rad;
	b.max_x = s.p.x + s.rad;
	b.min_y = s.p.y - s.rad;
	b.max_y = s.p.y + s.rad;
	b.min_z = s.p.z - s.rad;
	b.max_z = s.p.z + s.rad;

	return b;
}

Bound CLBVH::getBound(Triangle t)
{
	Bound b;

	b.min_x = min3(m_pois[t.p1].p.x, m_pois[t.p2].p.x, m_pois[t.p3].p.x);
	b.max_x = max3(m_pois[t.p1].p.x, m_pois[t.p2].p.x, m_pois[t.p3].p.x);
	b.min_y = min3(m_pois[t.p1].p.y, m_pois[t.p2].p.y, m_pois[t.p3].p.y);
	b.max_y = max3(m_pois[t.p1].p.y, m_pois[t.p2].p.y, m_pois[t.p3].p.y);
	b.min_z = min3(m_pois[t.p1].p.z, m_pois[t.p2].p.z, m_pois[t.p3].p.z);
	b.max_z = max3(m_pois[t.p1].p.z, m_pois[t.p2].p.z, m_pois[t.p3].p.z);

	return b;
}

/**
* CLBVH constructor
*/
CLBVH::CLBVH(Shape *shapes, int shapeCnt, Poi *pois, int poiCnt, cl_command_queue cq, cl_context ctx, cl_kernel kRad, cl_kernel kBvh, cl_kernel kOpt) : m_shapes(shapes), m_shapeCnt(shapeCnt), m_pois(pois), m_poiCnt(poiCnt), m_cq(cq), m_ctx(ctx), m_kRad(kRad), m_kBvh(kBvh), m_kOpt(kOpt)
{
	// For internal nodes, leaf = false
	tn = (TreeNode *)malloc(sizeof(TreeNode) * (shapeCnt - 1));
	memset(tn, 0, sizeof(TreeNode) * (shapeCnt - 1));

	// For leaves, leaf = true
	tl = (TreeNode *)malloc(sizeof(TreeNode) * shapeCnt);
	memset(tl, ~0, sizeof(TreeNode) * shapeCnt);

	// Initialize morton codes
	float min_x = FLT_MAX;
	float max_x = FLT_MIN;
	float min_y = FLT_MAX;
	float max_y = FLT_MIN;
	float min_z = FLT_MAX;
	float max_z = FLT_MIN;

	for (unsigned int i = 0; i < m_shapeCnt; i++)
	{
		Vec p;
		m_shapes[i].index = i;
		if (m_shapes[i].type == SPHERE)
		{
			m_shapes[i].b = getBound(m_shapes[i].s);
			p = m_shapes[i].s.p;
		}
		else if (m_shapes[i].type == TRIANGLE)
		{
			m_shapes[i].b = getBound(m_shapes[i].t);
			p.x = (m_shapes[i].b.min_x + m_shapes[i].b.max_x) / 2.0f, //(m_pois[m_shapes[i].t.p1].p.x + m_pois[m_shapes[i].t.p2].p.x + m_pois[m_shapes[i].t.p3].p.x) / 3.0f,
			p.y = (m_shapes[i].b.min_y + m_shapes[i].b.max_y) / 2.0f, //(m_pois[m_shapes[i].t.p1].p.y + m_pois[m_shapes[i].t.p2].p.y + m_pois[m_shapes[i].t.p3].p.y) / 3.0f,
			p.z = (m_shapes[i].b.min_z + m_shapes[i].b.max_z) / 2.0f; // (m_pois[m_shapes[i].t.p1].p.z + m_pois[m_shapes[i].t.p2].p.z + m_pois[m_shapes[i].t.p3].p.z) / 3.0f;
		}

		// find min and max coordinates
		if (p.x < min_x) min_x = p.x;
		if (p.x > max_x) max_x = p.x;
		if (p.y < min_y) min_y = p.y;
		if (p.y > max_y) max_y = p.y;
		if (p.z < min_z) min_z = p.z;
		if (p.z > max_z) max_z = p.z;
	}

	for (unsigned int i = 0; i < m_shapeCnt; i++)
	{
		// calculate morton code
		m_shapes[i].index = i;
		m_shapes[i].morton_code = 0;

		Vec p;

		if (m_shapes[i].type == SPHERE)
		{
			p = m_shapes[i].s.p;
		}
		else if (m_shapes[i].type == TRIANGLE)
		{
			float min_x = min3(m_pois[m_shapes[i].t.p1].p.x, m_pois[m_shapes[i].t.p2].p.x, m_pois[m_shapes[i].t.p3].p.x);
			float min_y = min3(m_pois[m_shapes[i].t.p1].p.y, m_pois[m_shapes[i].t.p2].p.y, m_pois[m_shapes[i].t.p3].p.y);
			float min_z = min3(m_pois[m_shapes[i].t.p1].p.z, m_pois[m_shapes[i].t.p2].p.z, m_pois[m_shapes[i].t.p3].p.z);

			float max_x = max3(m_pois[m_shapes[i].t.p1].p.x, m_pois[m_shapes[i].t.p2].p.x, m_pois[m_shapes[i].t.p3].p.x);
			float max_y = max3(m_pois[m_shapes[i].t.p1].p.y, m_pois[m_shapes[i].t.p2].p.y, m_pois[m_shapes[i].t.p3].p.y);
			float max_z = max3(m_pois[m_shapes[i].t.p1].p.z, m_pois[m_shapes[i].t.p2].p.z, m_pois[m_shapes[i].t.p3].p.z);

			p.x = (min_x + max_x) / 2.0f;// (m_pois[m_shapes[i].t.p1].p.x + m_pois[m_shapes[i].t.p2].p.x + m_pois[m_shapes[i].t.p3].p.x) / 3.0f;
			p.y = (min_y + max_y) / 2.0f;//(m_pois[m_shapes[i].t.p1].p.y + m_pois[m_shapes[i].t.p2].p.y + m_pois[m_shapes[i].t.p3].p.y) / 3.0f;
			p.z = (min_z + max_z) / 2.0f;//(m_pois[m_shapes[i].t.p1].p.z + m_pois[m_shapes[i].t.p2].p.z + m_pois[m_shapes[i].t.p3].p.z) / 3.0f;
		}

		// get the first 10 bits of each coordinate 
		unsigned int a = (unsigned int)(((p.x - min_x) / (max_x - min_x)) * CODE_OFFSET);
		unsigned int b = (unsigned int)(((p.y - min_y) / (max_y - min_y)) * CODE_OFFSET);
		unsigned int c = (unsigned int)(((p.z - min_z) / (max_z - min_z)) * CODE_OFFSET);

		// combine into 30 bits morton code
		for (unsigned int j = 0; j < CODE_LENGTH; j++) {
			m_shapes[i].morton_code |=
				(((((a >> (CODE_LENGTH - 1 - j))) & 1) << ((CODE_LENGTH - j) * 3 - 1)) |
				((((b >> (CODE_LENGTH - 1 - j))) & 1) << ((CODE_LENGTH - j) * 3 - 2)) |
				((((c >> (CODE_LENGTH - 1 - j))) & 1) << ((CODE_LENGTH - j) * 3 - 3)));
		}
	}

	cl_int status;
	
	m_nBuf = clCreateBuffer(
		m_ctx,
		CL_MEM_READ_WRITE,
		sizeof(TreeNode) * (shapeCnt - 1),
		NULL,
		&status);
	if (status != CL_SUCCESS) {
		LOGE("Failed to create OpenCL node buffer: %d\n", status);
		return;
	}

	m_lBuf = clCreateBuffer(
		m_ctx,
		CL_MEM_READ_WRITE,
		sizeof(TreeNode) * (shapeCnt),
		NULL,
		&status);
	if (status != CL_SUCCESS) {
		LOGE("Failed to create OpenCL leaf buffer: %d\n", status);
		return;
	}

	m_shBuf = clCreateBuffer(
		m_ctx,
		CL_MEM_READ_ONLY,
		sizeof(Shape) * (shapeCnt),
		NULL,
		&status);
	if (status != CL_SUCCESS) {
		LOGE("Failed to create OpenCL shape buffer: %d\n", status);
		return;
	}

	status = clEnqueueWriteBuffer(
		m_cq,
		m_nBuf,
		CL_TRUE,
		0,
		sizeof(TreeNode) * (shapeCnt - 1),
		tn,
		0,
		NULL,
		NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to write the OpenCL node buffer: %d\n", status);
		return;
	}

	status = clEnqueueWriteBuffer(
		m_cq,
		m_lBuf,
		CL_TRUE,
		0,
		sizeof(TreeNode) * (shapeCnt),
		tl,
		0,
		NULL,
		NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to write the OpenCL leaf buffer: %d\n", status);
		return;
	}

	status = clEnqueueWriteBuffer(
		m_cq,
		m_shBuf,
		CL_TRUE,
		0,
		sizeof(Shape) * (m_shapeCnt),
		m_shapes,
		0,
		NULL,
		NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to write the OpenCL shape buffer: %d\n", status);
		return;
	}
}

/**
* CLBVH destructor
*/
CLBVH::~CLBVH()
{
	if (m_shBuf)
		clReleaseMemObject(m_shBuf);
	if (m_lBuf)
		clReleaseMemObject(m_lBuf);
	if (m_nBuf)
		clReleaseMemObject(m_nBuf);
	if (tn)
		free(tn);
	if (tl)
		free(tl);
}
/**
* CLBVH::buildRadixTree
* Make a radix tree. Required for the construction of BVH tree.
* Described in karras2012 paper.
*/
void CLBVH::buildRadixTree()
{
	size_t globalThreads[1];
	size_t localThreads[1];

	// Setup number of iterations for each thread
	int internalNodes = m_shapeCnt - 1;

	globalThreads[0] = ((internalNodes + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
	localThreads[0] = BLOCK_SIZE;

	/* Set kernel arguments */
	cl_int status = clSetKernelArg(m_kRad, 0, sizeof(int), &internalNodes);
	if (status != CL_SUCCESS) {
		LOGE("Failed to set OpenCL arg. #0: %d\n", status);
		return ;
	}

	status = clSetKernelArg(
		m_kRad,
		1,
		sizeof(cl_mem),
		(void *)&m_nBuf);
	if (status != CL_SUCCESS) {
		LOGE("Failed to set OpenCL arg. #1: %d\n", status);
		return ;
	}

	status = clSetKernelArg(
		m_kRad,
		2,
		sizeof(cl_mem),
		(void *)&m_lBuf);
	if (status != CL_SUCCESS) {
		LOGE("Failed to set OpenCL arg. #2: %d\n", status);
		return ;
	}

	status = clEnqueueNDRangeKernel(
		m_cq,
		m_kRad,
		1,
		NULL,
		globalThreads,
		localThreads,
		0,
		NULL,
		NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to enqueue OpenCL work: %d\n", status);
		return;
	}
	
#ifdef DEBUG_TREE
	clFinish(m_cq);

	status = clEnqueueReadBuffer(m_cq, m_nBuf, CL_TRUE, 0, sizeof(TreeNode) * (m_shapeCnt - 1), dtn, 0, NULL, NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to read the OpenCL node buffer: %d\n", status);
		return;
	}
	status = clEnqueueReadBuffer(m_cq, m_lBuf, CL_TRUE, 0, sizeof(TreeNode) * (m_shapeCnt), dtl, 0, NULL, NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to read the OpenCL leaf buffer: %d\n", status);
		return;
	}
#endif
}

/**
* CLBVH::buildBVHTree
* Make the BVH tree to be used for ray tracing later.
* Required that a radix tree is generated
*/
void CLBVH::buildBVHTree()
{
	// initialize host variables
	size_t i;
	Geometry *geo = new Geometry[m_shapeCnt];

	// place data into host variables
	for (i = 0; i < m_shapeCnt; i++) {
		geo[i].key_mc = m_shapes[i].morton_code;
		geo[i].value = i;
	}

	int *sorted_geometry_indices = bubblesort_Geometry(geo);
	int *nodeCounter = (int *)malloc(sizeof(int) * m_shapeCnt);
	memset(nodeCounter, 0, sizeof(int) * m_shapeCnt);

	cl_int status;

	cl_mem ncBuf = clCreateBuffer(
		m_ctx,
		CL_MEM_READ_WRITE,
		sizeof(int) * (m_shapeCnt),
		NULL,
		&status);
	if (status != CL_SUCCESS) {
		LOGE("Failed to create OpenCL nodeCounter buffer: %d\n", status);
		return;
	}

	status = clEnqueueWriteBuffer(
		m_cq,
		ncBuf,
		CL_TRUE,
		0,
		sizeof(int) * (m_shapeCnt),
		nodeCounter,
		0,
		NULL,
		NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to write the OpenCL nodeCounter buffer: %d\n", status);
		return;
	}

	cl_mem sgBuf = clCreateBuffer(
		m_ctx,
		CL_MEM_READ_ONLY,
		sizeof(int) * (m_shapeCnt),
		NULL,
		&status);
	if (status != CL_SUCCESS) {
		LOGE("Failed to create OpenCL sorted_geometry_indices buffer: %d\n", status);
		return;
	}

	status = clEnqueueWriteBuffer(
		m_cq,
		sgBuf,
		CL_TRUE,
		0,
		sizeof(int) * (m_shapeCnt),
		sorted_geometry_indices,
		0,
		NULL,
		NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to write the OpenCL sorted_geometry_indices buffer: %d\n", status);
		return;
	}

	size_t globalThreads[1];
	size_t localThreads[1];

	// Setup number of iterations for each thread
	globalThreads[0] = ((m_shapeCnt + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
	localThreads[0] = BLOCK_SIZE;

	/* Set kernel arguments */
	status = clSetKernelArg(m_kBvh, 0, sizeof(int), &m_shapeCnt);
	if (status != CL_SUCCESS) {
		LOGE("Failed to set OpenCL arg. #0: %d\n", status);
		return;
	}

	status = clSetKernelArg(
		m_kBvh,
		1,
		sizeof(cl_mem),
		(void *)&m_nBuf);
	if (status != CL_SUCCESS) {
		LOGE("Failed to set OpenCL arg. #1: %d\n", status);
		return;
	}

	status = clSetKernelArg(
		m_kBvh,
		2,
		sizeof(cl_mem),
		(void *)&m_lBuf);
	if (status != CL_SUCCESS) {
		LOGE("Failed to set OpenCL arg. #2: %d\n", status);
		return;
	}

	status = clSetKernelArg(
		m_kBvh,
		3,
		sizeof(cl_mem),
		(void *)&ncBuf);
	if (status != CL_SUCCESS) {
		LOGE("Failed to set OpenCL arg. #3: %d\n", status);
		return;
	}

	status = clSetKernelArg(
		m_kBvh,
		4,
		sizeof(cl_mem),
		(void *)&sgBuf);
	if (status != CL_SUCCESS) {
		LOGE("Failed to set OpenCL arg. #4: %d\n", status);
		return;
	}

	status = clSetKernelArg(
		m_kBvh,
		5,
		sizeof(cl_mem),
		(void *)&m_shBuf);
	if (status != CL_SUCCESS) {
		LOGE("Failed to set OpenCL arg. #5: %d\n", status);
		return;
	}

	status = clEnqueueNDRangeKernel(
		m_cq,
		m_kBvh,
		1,
		NULL,
		globalThreads,
		localThreads,
		0,
		NULL,
		NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to enqueue OpenCL work: %d\n", status);
		return;
	}

	clFinish(m_cq);
#ifdef DEBUG_TREE
	clFinish(m_cq);

	status = clEnqueueReadBuffer(m_cq, m_nBuf, CL_TRUE, 0, sizeof(TreeNode) * (m_shapeCnt - 1), dtn, 0, NULL, NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to read the OpenCL node buffer: %d\n", status);
		return;
	}
	status = clEnqueueReadBuffer(m_cq, m_lBuf, CL_TRUE, 0, sizeof(TreeNode) * (m_shapeCnt), dtl, 0, NULL, NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to read the OpenCL leaf buffer: %d\n", status);
		return;
	}
	status = clEnqueueReadBuffer(m_cq, ncBuf, CL_TRUE, 0, sizeof(int) * (m_shapeCnt), nodeCounter, 0, NULL, NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to read the OpenCL nodeCounter buffer: %d\n", status);
		return;
	}
#endif
	clReleaseMemObject(sgBuf);
	clReleaseMemObject(ncBuf);

	free(nodeCounter);

	delete[] sorted_geometry_indices;
	delete[] geo;
}

/**
* CLBVH::optimize
* Use treelet reconstruction algorithm described in karras2013hpg_paper.pdf
* to optimize BVH tree
*/
void CLBVH::optimize()
{
	int *nodeCounter = (int *)malloc(sizeof(int) * m_shapeCnt);
	memset(nodeCounter, 0, sizeof(int) * m_shapeCnt);

	cl_int status;

	cl_mem ncBuf = clCreateBuffer(
		m_ctx,
		CL_MEM_READ_WRITE,
		sizeof(int) * (m_shapeCnt),
		NULL,
		&status);
	if (status != CL_SUCCESS) {
		LOGE("Failed to create OpenCL nodeCounter buffer: %d\n", status);
		return;
	}

	status = clEnqueueWriteBuffer(
		m_cq,
		ncBuf,
		CL_TRUE,
		0,
		sizeof(int) * (m_shapeCnt),
		nodeCounter,
		0,
		NULL,
		NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to write the OpenCL nodeCounter buffer: %d\n", status);
		return;
	}
	
	size_t globalThreads[1];
	size_t localThreads[1];

	// Setup number of iterations for each thread
	globalThreads[0] = ((m_shapeCnt + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
	localThreads[0] = BLOCK_SIZE;

	/* Set kernel arguments */
	status = clSetKernelArg(m_kOpt, 0, sizeof(int), &m_shapeCnt);
	if (status != CL_SUCCESS) {
		LOGE("Failed to set OpenCL arg. #0: %d\n", status);
		return;
	}

	status = clSetKernelArg(
		m_kOpt,
		1,
		sizeof(cl_mem),
		(void *)&ncBuf);
	if (status != CL_SUCCESS) {
		LOGE("Failed to set OpenCL arg. #1: %d\n", status);
		return;
	}

	status = clSetKernelArg(
		m_kOpt,
		2,
		sizeof(cl_mem),
		(void *)&m_nBuf);
	if (status != CL_SUCCESS) {
		LOGE("Failed to set OpenCL arg. #2: %d\n", status);
		return;
	}

	status = clSetKernelArg(
		m_kOpt,
		3,
		sizeof(cl_mem),
		(void *)&m_lBuf);
	if (status != CL_SUCCESS) {
		LOGE("Failed to set OpenCL arg. #3: %d\n", status);
		return;
	}

	status = clEnqueueNDRangeKernel(
		m_cq,
		m_kOpt,
		1,
		NULL,
		globalThreads,
		localThreads,
		0,
		NULL,
		NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to enqueue OpenCL work: %d\n", status);
		return;
	}

#ifdef DEBUG_TREE
	clFinish(m_cq);

	status = clEnqueueReadBuffer(m_cq, m_nBuf, CL_TRUE, 0, sizeof(TreeNode) * (m_shapeCnt - 1), dtn, 0, NULL, NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to read the OpenCL node buffer: %d\n", status);
		return;
	}
	status = clEnqueueReadBuffer(m_cq, m_lBuf, CL_TRUE, 0, sizeof(TreeNode) * (m_shapeCnt), dtl, 0, NULL, NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to read the OpenCL leaf buffer: %d\n", status);
		return;
	}
	status = clEnqueueReadBuffer(m_cq, ncBuf, CL_TRUE, 0, sizeof(int) * (m_shapeCnt), nodeCounter, 0, NULL, NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to read the OpenCL nodeCounter buffer: %d\n", status);
		return;
	}
#endif
	clReleaseMemObject(ncBuf);

	free(nodeCounter);
}

void CLBVH::getTrees(TreeNode **pptn, TreeNode **pptl)
{
	cl_int status;

	status = clEnqueueReadBuffer(m_cq, m_nBuf, CL_TRUE, 0, sizeof(TreeNode) * (m_shapeCnt - 1), tn, 0, NULL, NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to read the OpenCL node buffer: %d\n", status);
		return;
	}
	status = clEnqueueReadBuffer(m_cq, m_lBuf, CL_TRUE, 0, sizeof(TreeNode) * (m_shapeCnt), tl, 0, NULL, NULL);
	if (status != CL_SUCCESS) {
		LOGE("Failed to read the OpenCL leaf buffer: %d\n", status);
		return;
	}

	*pptn = tn;
	*pptl = tl;
}