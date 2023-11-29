/****************************************************************************
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

#include "base/CCIMEDispatcher.h"
#include "base/CCDirector.h"
#include "base/CCEventType.h"
#include "base/CCEventCustom.h"
#include "base/CCEventDispatcher.h"
#include "platform/CCApplication.h"
#include "platform/CCFileUtils.h"
#include "platform/android/jni/JniHelper.h"
#include "platform/catch_and_rethrow_as_platform_exception.h"
#include "renderer/CCTextureCache.h"
#include "renderer/CCRenderer.h"
#include <jni.h>

#include "base/ccUTF8.h"

using namespace cocos2d;

extern "C" {

    extern bool cocos2d_reload_required;
    extern int cocos2d_reload_after_n_frames;

    JNIEXPORT void JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender(JNIEnv* env) {
        CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_BEGIN
            // NOTE: See @Android, @WarmStart.
            if (cocos2d_reload_required) {
                auto director = cocos2d::Director::getInstance();

                if (cocos2d_reload_after_n_frames > 0) {
                    cocos2d_reload_after_n_frames -= 1;
                } else {
                    auto finished = cocos2d::VolatileTextureMgr::reloadAllTexturesIncrementally();

                    if (finished) {
                        cocos2d::EventCustom recreatedEvent(EVENT_RENDERER_RECREATED);
                        director->getEventDispatcher()->dispatchEvent(&recreatedEvent);
                        director->setGLDefaultValues();

                        cocos2d_reload_required = false;
                    }
                }

                director->getRenderer()->clear();
            } else {
                cocos2d::Director::getInstance()->mainLoop();
            }
        CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_END(env)
    }

    JNIEXPORT void JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnPause(JNIEnv* env) {
        CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_BEGIN
            if (Director::getInstance()->getOpenGLView()) {
                    Application::getInstance()->applicationDidEnterBackground();
                    cocos2d::EventCustom backgroundEvent(EVENT_COME_TO_BACKGROUND);
                    cocos2d::Director::getInstance()->getEventDispatcher()->dispatchEvent(&backgroundEvent);
            }
        CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_END(env)
    }

    JNIEXPORT void JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnResume(JNIEnv* env) {
        CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_BEGIN
            static bool firstTime = true;
            if (Director::getInstance()->getOpenGLView()) {
                // don't invoke at first to keep the same logic as iOS
                // can refer to https://github.com/cocos2d/cocos2d-x/issues/14206
                if (!firstTime)
                    Application::getInstance()->applicationWillEnterForeground();

                cocos2d::EventCustom foregroundEvent(EVENT_COME_TO_FOREGROUND);
                cocos2d::Director::getInstance()->getEventDispatcher()->dispatchEvent(&foregroundEvent);

                firstTime = false;
            }
        CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_END(env)
    }

    JNIEXPORT void JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInsertText(JNIEnv* env, jobject thiz, jstring text) {
        CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_BEGIN
            std::string  strValue = cocos2d::StringUtils::getStringUTFCharsJNI(env, text);
            const char* pszText = strValue.c_str();
            cocos2d::IMEDispatcher::sharedDispatcher()->dispatchInsertText(pszText, strlen(pszText));
        CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_END(env)
    }

    JNIEXPORT void JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeDeleteBackward(JNIEnv* env, jobject thiz) {
        CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_BEGIN
            cocos2d::IMEDispatcher::sharedDispatcher()->dispatchDeleteBackward();
        CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_END(env)
    }

    JNIEXPORT jstring JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeGetContentText(JNIEnv* env) {
        CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_BEGIN
            JNIEnv * env = 0;

            if (JniHelper::getJavaVM()->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK || ! env) {
                return 0;
            }
            std::string pszText = cocos2d::IMEDispatcher::sharedDispatcher()->getContentText();
            return cocos2d::StringUtils::newStringUTFJNI(env, pszText);
        CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_END_RET(env, nullptr)
    }
}
