
#ifndef CLBVH_H
#define CLBVH_H

#include "../../assets/sdcard/include/geom.h"

#if (ACCELSTR == 1)

#include <float.h>
#ifdef WIN32
#include <stdint.h>
#else
#include <string.h>
#endif
#include <stdlib.h>

#include "../../assets/sdcard/include/BVHNodeGPU.h"
#include "CL/cl.h"
#include "native-lib.h"

typedef struct
{
	unsigned int key_mc;
	int value;
} Geometry;

class CLBVH
{
private:
	cl_context m_ctx;
	cl_kernel m_kRad, m_kBvh, m_kOpt;
	cl_mem m_nBuf, m_lBuf, m_shBuf;
	cl_command_queue m_cq;

	BVHNodeGPU *btn, *btl;
	
	unsigned int m_shapeCnt;
	Shape *m_shapes;

	Bound getBound(Sphere s);
	Bound getBound(Triangle t);

public:
	CLBVH(Shape *shapes, int shapeCnt, cl_command_queue cq, cl_context ctx, cl_kernel kRad, cl_kernel kBvh, cl_kernel kOpt);
	~CLBVH();

	void buildRadixTree();
	void buildBVHTree();
	void optimize();

	void getTrees(BVHNodeGPU **ppbtn, BVHNodeGPU **ppbtl);
	int *bubblesort_Geometry(Geometry *geo);
};
#endif //(ACCELSTR == 1)
#endif //CLBVH_H