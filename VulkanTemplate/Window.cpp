#include "Window.h"
#include <cassert>

//---------------------------------------------------------------------------
static std::unique_ptr<Window> window = nullptr;
std::unique_ptr<Window>& getAppWindow() {
    if (window == nullptr)
    {
        window = std::make_unique<Window>();
    }
    return window;
}

//---------------------------------------------------------------------------
void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}
//---------------------------------------------------------------------------
void Window::Initialize(const WindowInitParams& initParams)
{
    glfwSetErrorCallback(error_callback);

    auto result = glfwInit();
    assert(result == GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    mPlatformHandle.window = glfwCreateWindow(initParams.width, initParams.height, initParams.title, nullptr, nullptr);
    glfwSetWindowUserPointer(mPlatformHandle.window, this);
}
//---------------------------------------------------------------------------
void Window::Shutdown()
{
    if (mPlatformHandle.window != nullptr)
    {
        glfwDestroyWindow(mPlatformHandle.window);
    }
    glfwTerminate();
}
//---------------------------------------------------------------------------
void Window::processMessages()
{
    mIsExitRequested = glfwWindowShouldClose(mPlatformHandle.window) == GLFW_TRUE;
    if (mIsExitRequested)
    {
        return;
    }
    glfwPollEvents();
}
//---------------------------------------------------------------------------