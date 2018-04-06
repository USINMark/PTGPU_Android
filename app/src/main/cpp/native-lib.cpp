#include <jni.h>
#include <string>
#include <string.h>
#include "include/displayfunc.h"
#include "include/native-lib.h"

#ifdef __cplusplus
//extern "C" {
#endif

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

    if (ReadFile(env->GetStringUTFChars(s, NULL)))
    {
        AddWallLight();
        UpdateCamera();
        /*------------------------------------------------------------------------*/
        SetUpOpenCL();
        BuildBVH();
    }
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
JNICALL Java_ptgpu_kmu_ac_kr_ptgpu_PTGPURenderer_finishRendering(JNIEnv* env, jobject /* this */)
{
    // Get JNI Env for all function calls
}

#ifdef __cplusplus
//}
#endif