#include <jni.h>
#include <string>
#include <string.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "include/displayfunc.h"
#include "include/native-lib.h"
#include "../assets/sdcard/include/geom.h"

#ifdef __cplusplus
//extern "C" {
#endif

AAssetManager   *mgr;
char *strResPath;

extern bool Read(char *fileName, bool *walllight);

extern "C" JNIEXPORT jstring
JNICALL Java_gamemobile_kmu_ac_kr_ptgpu_PTGPURenderer_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT void
JNICALL Java_gamemobile_kmu_ac_kr_ptgpu_PTGPURenderer_initSmallPtGPU(JNIEnv* env, jobject /* this */, jint u, jint f, jstring k, jint w, jint h, jstring s, jstring r, jobject assetManager)
{
    bool walllight = true;
    srand(time(NULL));

    useGPU = u;
    forceWorkSize = f;
#ifndef EXP_KERNEL
    strcpy(kernelFileName, env->GetStringUTFChars(k, NULL));
#endif

    width = w;
    height = h;

    mgr = AAssetManager_fromJava(env, assetManager);

    char *strFilePath = (char *)env->GetStringUTFChars(s, NULL);
    strResPath = (char *)env->GetStringUTFChars(r, NULL);
    char *strFullPath = (char *)malloc(strlen(strFilePath) + strlen(strResPath) + 1);

    strcpy(strFullPath, strResPath);
    strcat(strFullPath, "/");
    strcat(strFullPath, strFilePath);

    if (Read(strFullPath, &walllight)) {
        if (walllight) AddWallLight();

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
JNICALL Java_gamemobile_kmu_ac_kr_ptgpu_PTGPURenderer_reinitCamera(JNIEnv* env, jobject /* this */, jfloat origx, jfloat origy, jfloat origz, jfloat targx, jfloat targy, jfloat targz)
{
    camera.orig.s[0] = origx,camera.orig.s[1] = origy, camera.orig.s[2] = origz;
    camera.target.s[0] = targx, camera.target.s[1] = targy, camera.target.s[2] = targz;

    ReInit(0);
}

extern "C" JNIEXPORT void
JNICALL Java_gamemobile_kmu_ac_kr_ptgpu_PTGPURenderer_touchFunc(JNIEnv* env, jobject /* this */, int deltax, int deltay)
{
    touchFunc(deltax, deltay);
}

extern "C" JNIEXPORT jintArray
JNICALL Java_gamemobile_kmu_ac_kr_ptgpu_PTGPURenderer_updateRendering(JNIEnv* env, jobject /* this */) {
    unsigned int *pixels = DrawFrame();

    // Get JNI Env for all function calls
    jintArray ji_array = env->NewIntArray(pixelCount);
    env->SetIntArrayRegion(ji_array, 0, pixelCount, (const jint *)pixels);

    return ji_array;
}

extern "C" JNIEXPORT void
JNICALL Java_gamemobile_kmu_ac_kr_ptgpu_PTGPURenderer_finishRendering(JNIEnv* env, jobject /* this */) {
    // Get JNI Env for all function calls
}

#ifdef __cplusplus
//}
#endif