#pragma once
#include <memory>
#include "GLFW/glfw3.h"

#define PLATFORM_WINDOWS 1

struct GLFWwindow;

//---------------------------------------------------------------------------
class Window;
std::unique_ptr<Window>& getAppWindow();

//---------------------------------------------------------------------------
class Window
{
public:
    struct WindowInitParams
    {
        int width = 1280;
        int height = 720;
        const char* title = "VulkanProject";
    };
    struct PlatformHandle
    {
        GLFWwindow* window;
    };

    void Initialize(const WindowInitParams& initParams);
    void Shutdown();

    inline void getWindowSize(int& width, int& height) {
        auto window = mPlatformHandle.window;
        glfwGetWindowSize(window, &width, &height);
    }
    inline bool getIsExitRequired() const { return mIsExitRequested; }
    inline const PlatformHandle* getPlatformHandle() { return &mPlatformHandle; }

    void processMessages();

private:
    bool mIsExitRequested;
    PlatformHandle mPlatformHandle;
};
//---------------------------------------------------------------------------