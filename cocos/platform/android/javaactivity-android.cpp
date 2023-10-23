/****************************************************************************
Copyright (c) 2013-2016 Chukong Technologies Inc.
Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#include "platform/CCPlatformConfig.h"
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID

#include "platform/android/CCApplication-android.h"
#include "platform/android/CCGLViewImpl-android.h"
#include "base/CCDirector.h"
#include "base/CCEventCustom.h"
#include "base/CCEventType.h"
#include "base/CCEventDispatcher.h"
#include "renderer/CCGLProgramCache.h"
#include "renderer/CCTextureCache.h"
#include "renderer/ccGLStateCache.h"
#include "2d/CCDrawingPrimitives.h"
#include "platform/android/jni/JniHelper.h"
#include "platform/CCDataManager.h"
#include "network/CCDownloader-android.h"
#include <unistd.h>
#include <android/log.h>
#include <android/api-level.h>
#include <jni.h>

#define  LOG_TAG    "main"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)

void cocos_android_app_init(JNIEnv* env) __attribute__((weak));

void cocos_audioengine_focus_change(int focusChange);

using namespace cocos2d;

extern "C"
{

// ndk break compatibility, refer to https://github.com/cocos2d/cocos2d-x/issues/16267 for detail information
// should remove it when using NDK r13 since NDK r13 will add back bsd_signal()
#if __ANDROID_API__ > 19
#include <signal.h>
#include <dlfcn.h>
    typedef __sighandler_t (*bsd_signal_func_t)(int, __sighandler_t);
    bsd_signal_func_t bsd_signal_func = NULL;

    __sighandler_t bsd_signal(int s, __sighandler_t f) {
        if (bsd_signal_func == NULL) {
            // For now (up to Android 7.0) this is always available 
            bsd_signal_func = (bsd_signal_func_t) dlsym(RTLD_DEFAULT, "bsd_signal");

            if (bsd_signal_func == NULL) {
                __android_log_assert("", "bsd_signal_wrapper", "bsd_signal symbol not found!");
            }
        }
        return bsd_signal_func(s, f);
    }
#endif // __ANDROID_API__ > 19

// NOTE: Reload resources after drawing first frame to improve warm start time on Android. @Android, @WarmStart -- mz, 2023-09-06
// One of the measurements of app quality on Google Play Console / Android Vitals
//   is a warm start time.
// Start time is a time measured from an app start
//   to a moment when the first frame is completely drawn and displayed.
// Warm start happens, for example,
//   when the app is brought to front from memory,
//   but app activity needs to be recreated.
// Google expects warm start time to be less than or equal to 2 seconds.
// One way to reproduce it is:
//   1. Enable “Do not keep activities” as described here[1].
//   2. Run the app
//   3. Put it in the background
//   4. Put it in the foreground
// Cocos2d-x reloads all the resources during warm start right before drawing the first frame.
// This results in too long warm start.
//
// This fix moves cocos2d-x resources reloading until after drawing some frames
//   and split resources reloading across several frames.
//
// More info:
// - https://litegames.atlassian.net/browse/RUMMYSP-51
// - https://litegames.atlassian.net/browse/RUMMYSP-76
// - https://litegames.atlassian.net/browse/RUMMYSP-123
// - https://developer.android.com/topic/performance/vitals/launch-time
//
// [1]: https://stackoverflow.com/a/19622671
bool cocos2d_reload_required = false;
int cocos2d_reload_after_n_frames;

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JniHelper::setJavaVM(vm);

    cocos_android_app_init(JniHelper::getEnv());

    return JNI_VERSION_1_4;
}

JNIEXPORT void Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit(JNIEnv*  env, jobject thiz, jint w, jint h)
{
    DataManager::setProcessID(getpid());
    DataManager::setFrameSize(w, h);

    auto director = cocos2d::Director::getInstance();
    auto glview = director->getOpenGLView();
    if (!glview)
    {
        glview = cocos2d::GLViewImpl::create("Android app");
        glview->setFrameSize(w, h);
        director->setOpenGLView(glview);

        cocos2d::Application::getInstance()->run();
    }
    else
    {
        cocos2d::GL::invalidateStateCache();
        cocos2d::GLProgramCache::getInstance()->reloadDefaultGLPrograms();
        cocos2d::DrawPrimitives::init();
        cocos2d_reload_required = true;
        // NOTE: Reload resources after drawing several frames. @Android, @WarmStart -- mz, 2023-09-08
        // For some unknown reason reloading after drawing 1 frame didn't work.
        // Logcat was still reporting very long warm start time:
        //   Displayed com.litegames.rummy_free__aat_google/com.litegames.rummy.AppActivity: +2s287ms
        // Reloading after 2 frames resulted in logcat reporting radically shorter warm start times:
        //   Displayed com.litegames.rummy_free__aat_google/com.litegames.rummy.AppActivity: +263ms
        cocos2d_reload_after_n_frames = 16;
        director->setGLDefaultValues();
    }
    cocos2d::network::_preloadJavaDownloaderClass();
}

JNIEXPORT jintArray Java_org_cocos2dx_lib_Cocos2dxActivity_getGLContextAttrs(JNIEnv*  env, jobject thiz)
{
    cocos2d::Application::getInstance()->initGLContextAttrs(); 
    GLContextAttrs _glContextAttrs = GLView::getGLContextAttrs();
    
    int tmp[7] = {_glContextAttrs.redBits, _glContextAttrs.greenBits, _glContextAttrs.blueBits,
                           _glContextAttrs.alphaBits, _glContextAttrs.depthBits, _glContextAttrs.stencilBits, _glContextAttrs.multisamplingCount};


    jintArray glContextAttrsJava = env->NewIntArray(7);
        env->SetIntArrayRegion(glContextAttrsJava, 0, 7, tmp);
    
    return glContextAttrsJava;
}

JNIEXPORT void Java_org_cocos2dx_lib_Cocos2dxAudioFocusManager_nativeOnAudioFocusChange(JNIEnv* env, jobject thiz, jint focusChange)
{
    cocos_audioengine_focus_change(focusChange);
}

JNIEXPORT void Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnSurfaceChanged(JNIEnv*  env, jobject thiz, jint w, jint h)
{
    cocos2d::Application::getInstance()->applicationScreenSizeChanged(w, h);
}

}

#endif // CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID

