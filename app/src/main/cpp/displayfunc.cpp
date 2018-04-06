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

//#include "stdafx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#define _USE_MATH_DEFINES
#endif
#include <math.h>

#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
#include <sys/time.h>
#elif defined (WIN32)
#include <windows.h>
#else
//        Unsupported Platform !!!
#endif

#include "include/camera.h"
#include "include/geom.h"
#include "include/displayfunc.h"
#include "include/native-lib.h"

extern void ReInit(const int);
extern void ReInitScene();
extern void UpdateRendering();
extern void UpdateCamera();

extern Camera camera;
extern Shape *shapes;
extern Poi *pois;
extern unsigned int shapeCnt, poiCnt;

int amiSmallptCPU;

int width = 640;
int height = 480;
unsigned int *pixels;
char captionBuffer[256];

double WallClockTime()
{
#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
    struct timeval t;
    gettimeofday(&t, NULL);

    return t.tv_sec*1000 + t.tv_usec / 1000.0;
#elif defined (WIN32)
    return GetTickCount();
#else
//	Unsupported Platform !!!
	return GetTickCount();
#endif
}

bool ReadScene(const char *fileName) {
    LOGI("Reading scene: %s\n", fileName);

    FILE *f = fopen(fileName, "r");
    if (!f) {
        LOGE("Failed to open file: %s, Errorno: %d\n", fileName, errno);
        return false;
    }

    /* Read the camera position */
    int c = fscanf(f,"camera %f %f %f %f %f %f\n",
                   &camera.orig.x, &camera.orig.y, &camera.orig.z,
                   &camera.target.x, &camera.target.y, &camera.target.z);
    if (c != 6) {
        LOGE("Failed to read 6 camera parameters: %d\n", c);
        fclose(f);
        return false;
    }

    /* Read the shape count */
    c = fscanf(f,"size %u\n", &shapeCnt);
    if (c != 1) {
        LOGE("Failed to read sphere count: %d\n", c);
        fclose(f);
        return false;
    }
    LOGI("Scene size: %d\n", shapeCnt);

    /* Read all shapes */
    shapes = (Shape *)malloc(sizeof(Shape) * shapeCnt);
    unsigned int i;
    for (i = 0; i < shapeCnt; i++) {
        Shape *s = &shapes[i];
        s->type = SPHERE;

        int mat;
        int c = fscanf(f,"sphere %f  %f %f %f  %f %f %f  %f %f %f  %d\n",
                       &s->s.rad,
                       &s->s.p.x, &s->s.p.y, &s->s.p.z,
                       &s->s.e.x, &s->s.e.y, &s->s.e.z,
                       &s->s.c.x, &s->s.c.y, &s->s.c.z,
                       &mat);
        switch (mat) {
            case 0:
                s->s.refl = DIFF;
                break;
            case 1:
                s->s.refl = SPEC;
                break;
            case 2:
                s->s.refl = REFR;
                break;
            default:
                LOGE("Failed to read material type for sphere #%d: %d\n", i, mat);
                //return false;
                break;
        }
        if (c != 11) {
            LOGE("Failed to read sphere #%d: %d\n", i, c);
            fclose(f);
            return false;
        }
    }

    fclose(f);
    return true;
}

bool ReadPly(const char *fileName) {
    FILE *f;
    int c;
    unsigned int i;
    char buf[255];

    LOGI("Reading ply: %s\n", fileName);

    f = fopen(fileName, "r");
    if (!f) {
        LOGE("Failed to open file: %s\n", fileName);
        return false;
    }

    /* Read the camera position */
    c = fscanf(f,"camera %f %f %f  %f %f %f\n",
               &camera.orig.x, &camera.orig.y, &camera.orig.z,
               &camera.target.x, &camera.target.y, &camera.target.z);
    if (c != 6) {
        LOGE("Failed to read 6 camera parameters: %d\n", c);
        return false;
    }

    /* Read the vertex count */
    c = fscanf(f,"element vertex %u\n", &poiCnt);
    if (c != 1) {
        LOGE("Failed to read vertex count: %d\n", c);
        return false;
    }
    LOGI("Vertex count: %d\n", poiCnt);

    /* Read the face count */
    c = fscanf(f,"element face %u\n", &shapeCnt);
    if (c != 1) {
        LOGE("Failed to read face count: %d\n", c);
        return false;
    }
    LOGI("Face count: %d\n", shapeCnt);

    /* Read all points */
    pois = (Poi *)malloc(sizeof(Poi) * poiCnt);

    for (i = 0; i < poiCnt; i++) {
        char *ptr;
        int j=0;
        Poi *s = &pois[i];
        fgets(buf, 255, f);

        ptr = strtok( buf, " ");
        if (j==0) s->p.x=atof(ptr);
        j++;

        while( ptr = strtok( NULL, " "))
        {
            if (j==1) s->p.y=atof(ptr);
            else if (j==2) s->p.z=atof(ptr);
            /*
            else if (j==3) s->n.x=atof(ptr);
            else if (j==4) s->n.y=atof(ptr);
            else if (j==5) s->n.z=atof(ptr);
            else if (j==6) s->c.x=atof(ptr);
            else if (j==7) s->c.y=atof(ptr);
            else if (j==8) s->c.z=atof(ptr);
            */
            j++;
        }
        j=0;
    }

    /* Read all triangles */
    shapes = (Shape *)malloc(sizeof(Shape) * (shapeCnt + MAX_WALLSUN));

    for (i = 0; i < shapeCnt; i++) {
        char *ptr;
        int j=0;
        Shape *t = &shapes[i];
        fgets(buf, 255, f);

        t->type = TRIANGLE;
        t->t.refl = DIFF;

        ptr = strtok( buf, " ");
        int ntoken = atoi(ptr);

        while(ntoken--)
        {
            ptr = strtok(NULL, " ");

            if (j==0) t->t.p1=atoi(ptr);
            else if (j==1) t->t.p2=atoi(ptr);
            else if (j==2) t->t.p3=atoi(ptr);
            //else if (j==3) t->e.x=atof(ptr);
            //else if (j==4) t->e.y=atof(ptr);
            //else if (j==5) t->e.z=atof(ptr);

            j++;
        }
        j=0;
    }

    fclose(f);
    return true;
}

char* GetFileExt(char * file_name)
{
    int file_name_len = strlen(file_name);
    file_name += file_name_len;

    char *file_ext = NULL;
    for (int i = 0; i <file_name_len; i++)
    {
        if (*file_name == '.')
        {
            file_ext = file_name + 1;
            break;
        }
        file_name--;
    }

    return file_ext;
}

bool ReadFile(const char *fileName) {
    bool ret = true;
    char *fileExt = GetFileExt((char *)fileName);

    if (!strcmp(fileExt, "scn")) ret = ReadScene(fileName);
    else if (!strcmp(fileExt, "ply")) ret = ReadPly(fileName);

    return ret;
}

void UpdateCamera() {
    vsub(camera.dir, camera.target, camera.orig);
    vnorm(camera.dir);

    const Vec up = {0.f, 1.f, 0.f};
    const float fov = (M_PI / 180.f) * 45.f;
    vxcross(camera.x, camera.dir, up);
    vnorm(camera.x);
    vsmul(camera.x, width * fov / height, camera.x);

    vxcross(camera.y, camera.x, camera.dir);
    vnorm(camera.y);
    vsmul(camera.y, fov, camera.y);
}
