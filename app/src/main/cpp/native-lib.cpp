#include <jni.h>
#include <string>
#include <string.h>

#include "include/displayfunc.h"
#include "include/native-lib.h"
#include "include/geom.h"

#ifdef __cplusplus
//extern "C" {
#endif

extern bool Read(char *fileName);

extern "C" JNIEXPORT jstring
JNICALL Java_ptgpu_kmu_ac_kr_ptgpu_PTGPURenderer_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT void
JNICALL Java_ptgpu_kmu_ac_kr_ptgpu_PTGPURenderer_initSmallPtGPU(JNIEnv* env, jobject /* this */, jint u, jint f, jstring k, jint w, jint h, jstring s)
{
    useGPU = u;
    forceWorkSize = f;
    strcpy(kernelFileName, env->GetStringUTFChars(k, NULL));

    width = w;
    height = h;

    if (Read((char *)env->GetStringUTFChars(s, NULL))) {
        AddWallLight();

#if (ACCELSTR == 0)
        SetUpOpenCL();
#elif (ACCELSTR == 1)
        SetUpOpenCL();
		BuildBVH();
#elif (ACCELSTR == 2)
		BuildKDtree();
		SetUpOpenCL();
#endif
        UpdateCamera();
        ReInit(0);
    }
}

extern "C" JNIEXPORT void
JNICALL Java_ptgpu_kmu_ac_kr_ptgpu_PTGPURenderer_reinitCamera(JNIEnv* env, jobject /* this */, jfloat origx, jfloat origy, jfloat origz, jfloat targx, jfloat targy, jfloat targz)
{
    camera.orig.x = origx,camera.orig.y = origy, camera.orig.z = origz;
    camera.target.x =targx, camera.target.y =targy, camera.target.z = targz;

    ReInit(0);
}

extern "C" JNIEXPORT jintArray
JNICALL Java_ptgpu_kmu_ac_kr_ptgpu_PTGPURenderer_updateRendering(JNIEnv* env, jobject /* this */) {
    unsigned int *pixels = DrawFrame();

    // Get JNI Env for all function calls
    jintArray ji_array = env->NewIntArray(pixelCount);
    env->SetIntArrayRegion(ji_array, 0, pixelCount, (const jint *)pixels);

    return ji_array;
}

extern "C" JNIEXPORT void
JNICALL Java_ptgpu_kmu_ac_kr_ptgpu_PTGPURenderer_finishRendering(JNIEnv* env, jobject /* this */) {
    // Get JNI Env for all function calls
}

#ifdef __cplusplus
//}
#endif