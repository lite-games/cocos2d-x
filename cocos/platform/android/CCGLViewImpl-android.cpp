/****************************************************************************
Copyright (c) 2010-2012 cocos2d-x.org
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

#include "platform/android/CCGLViewImpl-android.h"
#include "base/CCDirector.h"
#include "base/ccMacros.h"
#include "platform/android/jni/JniHelper.h"
#include "CCGL.h"

#include <stdlib.h>
#include <android/log.h>

// <EGL/egl.h> exists since android 2.3
#include <EGL/egl.h>
PFNGLGENVERTEXARRAYSOESPROC glGenVertexArraysOESEXT = 0;
PFNGLBINDVERTEXARRAYOESPROC glBindVertexArrayOESEXT = 0;
PFNGLDELETEVERTEXARRAYSOESPROC glDeleteVertexArraysOESEXT = 0;

#define DEFAULT_MARGIN_ANDROID				30.0f
#define WIDE_SCREEN_ASPECT_RATIO_ANDROID	2.0f

void initExtensions() {
     glGenVertexArraysOESEXT = (PFNGLGENVERTEXARRAYSOESPROC)eglGetProcAddress("glGenVertexArraysOES");
     glBindVertexArrayOESEXT = (PFNGLBINDVERTEXARRAYOESPROC)eglGetProcAddress("glBindVertexArrayOES");
     glDeleteVertexArraysOESEXT = (PFNGLDELETEVERTEXARRAYSOESPROC)eglGetProcAddress("glDeleteVertexArraysOES");
}

NS_CC_BEGIN

GLViewImpl* GLViewImpl::createWithRect(const std::string& viewName, Rect rect, float frameZoomFactor)
{
    auto ret = new GLViewImpl;
    if(ret && ret->initWithRect(viewName, rect, frameZoomFactor)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

GLViewImpl* GLViewImpl::create(const std::string& viewName)
{
    auto ret = new GLViewImpl;
    if(ret && ret->initWithFullScreen(viewName)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

GLViewImpl* GLViewImpl::createWithFullScreen(const std::string& viewName)
{
    auto ret = new GLViewImpl();
    if(ret && ret->initWithFullScreen(viewName)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

GLViewImpl::GLViewImpl()
{
    initExtensions();
}

GLViewImpl::~GLViewImpl()
{

}

bool GLViewImpl::initWithRect(const std::string& viewName, Rect rect, float frameZoomFactor)
{
    return true;
}

bool GLViewImpl::initWithFullScreen(const std::string& viewName)
{
    return true;
}


bool GLViewImpl::isOpenGLReady()
{
    return (_screenSize.width != 0 && _screenSize.height != 0);
}

void GLViewImpl::end()
{
    JniHelper::callStaticVoidMethod("org.cocos2dx.lib.Cocos2dxHelper", "terminateProcess");
}

void GLViewImpl::swapBuffers()
{
}

void GLViewImpl::setIMEKeyboardState(bool bOpen)
{
    if (bOpen) {
        JniHelper::callStaticVoidMethod("org.cocos2dx.lib.Cocos2dxGLSurfaceView", "openIMEKeyboard");
    } else {
        JniHelper::callStaticVoidMethod("org.cocos2dx.lib.Cocos2dxGLSurfaceView", "closeIMEKeyboard");
    }
}

Rect GLViewImpl::getSafeAreaRect() const {
    auto safeAreaRect = GLView::getSafeAreaRect();

    auto safeAreaInsetsInPixels = JniHelper::callStaticIntArrayMethod(
        "org/cocos2dx/lib/Cocos2dxHelper",
        "getSafeAreaInsetsArray"
    );
    if (safeAreaInsetsInPixels != nullptr) {
        auto leftInsetInDesignPoints =
            static_cast<float>(safeAreaInsetsInPixels[0]) / _scaleX;
        auto rightInsetInDesignPoints =
            static_cast<float>(safeAreaInsetsInPixels[1]) / _scaleX;
        auto topInsetInDesignPoints =
            static_cast<float>(safeAreaInsetsInPixels[2]) / _scaleY;
        auto bottomInsetInDesignPoints =
            static_cast<float>(safeAreaInsetsInPixels[3]) / _scaleY;

        // Apply insets to safe area rect
        if (leftInsetInDesignPoints > 0) {
            safeAreaRect.origin.x += leftInsetInDesignPoints;
            safeAreaRect.size.width -= leftInsetInDesignPoints;
        }
        if (rightInsetInDesignPoints > 0) {
            safeAreaRect.size.width -= rightInsetInDesignPoints;
        }
        if (topInsetInDesignPoints > 0) {
            safeAreaRect.size.height -= topInsetInDesignPoints;
        }
        if (bottomInsetInDesignPoints > 0) {
            safeAreaRect.origin.y += bottomInsetInDesignPoints;
            safeAreaRect.size.height -= bottomInsetInDesignPoints;
        }
    }

    return safeAreaRect;
}

NS_CC_END

#endif // CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
