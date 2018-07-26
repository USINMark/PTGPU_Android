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

#ifndef _VEC_H
#define	_VEC_H

#ifndef GPU_KERNEL
#include "../../../cpp/include/CL/cl_platform.h"

typedef cl_float3 Vec;

#define vinit(v, a, b, c) { (v).s[0] = a, (v).s[1] = b, (v).s[2] = c; }
#define vassign(a, b) vinit(a, (b).s[0], (b).s[1], (b).s[2])
#define vclr(v) vinit(v, 0.f, 0.f, 0.f)
#define vadd(v, a, b) vinit(v, (a).s[0] + (b).s[0], (a).s[1] + (b).s[1], (a).s[2] + (b).s[2])
#define vsub(v, a, b) vinit(v, (a).s[0] - (b).s[0], (a).s[1] - (b).s[1], (a).s[2] - (b).s[2])
#define vsadd(v, a, b) { float k = (a); vinit(v, (b).s[0] + k, (b).s[1] + k, (b).s[2] + k) }
#define vssub(v, a, b) { float k = (a); vinit(v, (b).s[0] - k, (b).s[1] - k, (b).s[2] - k) }
#define vmul(v, a, b) vinit(v, (a).s[0] * (b).s[0], (a).s[1] * (b).s[1], (a).s[2] * (b).s[2])
#define vsmul(v, a, b) { float k = (a); vinit(v, k * (b).s[0], k * (b).s[1], k * (b).s[2]) }
#define vdot(a, b) ((a).s[0] * (b).s[0] + (a).s[1] * (b).s[1] + (a).s[2] * (b).s[2])
#define vnorm(v) { float l = 1.f / sqrt(vdot(v, v)); vsmul(v, l, v); }
#define vxcross(v, a, b) vinit(v, (a).s[1] * (b).s[2] - (a).s[2] * (b).s[1], (a).s[2] * (b).s[0] - (a).s[0] * (b).s[2], (a).s[0] * (b).s[1] - (a).s[1] * (b).s[0])
#define vfilter(v) ((v).s[0] > (v).s[1] && (v).s[0] > (v).s[2] ? (v).s[0] : (v).s[1] > (v).s[2] ? (v).s[1] : (v).s[2])
#define viszero(v) (((v).s[0] == 0.f) && ((v).s[1] == 0.f) && ((v).s[2] == 0.f))
#define norm(v) (sqrt((v).s[0]*(v).s[0] + (v).s[1]*(v).s[1] + (v).s[2]*(v).s[2]))
#define dist(a, b) (sqrt( ((a).s[0]-(b).s[0])*((a).s[0]-(b).s[0])  +   ((a).s[1]-(b).s[1])*((a).s[1]-(b).s[1])   +   ((a).s[2]-(b).s[2])*((a).s[2]-(b).s[2])))
#define affine(v, t, a, b) vinit(v, (a).s[0]*t + (b).s[0]*(1-t), (a).s[1]*t + (b).s[1]*(1-t), (a).s[2]*t + (b).s[2]*(1-t))

#define clamp(x, a, b) ((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))
#define sign(x) ((x) > 0 ? 1 : -1)
#ifndef max
#define max(x, y) ( (x) > (y) ? (x) : (y))
#endif
#ifndef min
#define min(x, y) ( (x) < (y) ? (x) : (y))
#endif
#else
typedef float3 Vec;

#define vinit(v, a, b, c) { v = (float3)(a, b, c);  }
#define vassign(a, b) vinit(a, (b).x, (b).y, (b).z)
#define vclr(v) vinit(v, 0.f, 0.f, 0.f)
#define vadd(v, a, b) { v = a + b; }
#define vsub(v, a, b) { v = a - b; }
#define vsadd(v, a, b) { v = a + b; }
#define vssub(v, a, b) { v = a - b; }
#define vmul(v, a, b) { v = a * b; }
#define vsmul(v, a, b) { v = a * b; }
#define vdot(a, b) ( dot(a, b) )
#define vnorm(v) { v = normalize(v); }
#define vxcross(v, a, b) { v = cross(a, b); }
#define vfilter(v) ((v).x > (v).y && (v).x > (v).z ? (v).x : (v).y > (v).z ? (v).y : (v).z)
#define viszero(v) (((v).x == 0.f) && ((v).y == 0.f) && ((v).z == 0.f))
#define norm(v) ( fast_length(v) )
#define dist(a, b) ( fast_distance(a, b) )
#define affine(v, t, a, b) { v = (a * t) + (b * (1-t)); }
#endif

#define toInt(x) ((int)(pow(clamp(x, 0.f, 1.f), 1.f / 2.2f) * 255.f + .5f))

// Rendering flags
#define RFLAGS_DISABLE_DIFFUSE_PATH 1

#endif	/* _VEC_H */

