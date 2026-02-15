#include "GfxDevice.h"
#include <algorithm>
#include <cassert>
#include "Window.h"

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"
#endif
#include <stdexcept>
#include <set>

//---------------------------------------------------------------------------
static std::unique_ptr<GfxDevice> gfxDevice = nullptr;
std::unique_ptr<GfxDevice>& getGfxDevice()
{
    if (gfxDevice == nullptr)
    {
        gfxDevice = std::make_unique<GfxDevice>();
    }
    return gfxDevice;
}
//---------------------------------------------------------------------------
void CheckVkResult(VkResult res)
{
    assert(res == VK_SUCCESS);
}
//---------------------------------------------------------------------------
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessageUtilsCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    char buf[2048] = { 0 };
    sprintf_s(buf, "%s\n", pCallbackData->pMessage);
    OutputDebugStringA(buf);
    return VK_FALSE;
}
//---------------------------------------------------------------------------
void GfxDevice::Initialize(const DeviceInitParams& initParams)
{
    // Vulkan APIを使用する前にVolkを初期化
    volkInitialize();

    initVkInstance_();
    initPhysicalDevice_();
    initVkDevice_();
    initWindowSurface_(initParams);
}
//---------------------------------------------------------------------------
void GfxDevice::Shutdown()
{
    // デバイスがアイドルになるまで待機
    waitForIdle();

    if (mVkDevice != VK_NULL_HANDLE)
    {
        destroyVkDevice_();

    }
    destroyWindowSurface_();

#if _DEBUG
    if (mDebugMessenger != VK_NULL_HANDLE)
    {
        vkDestroyDebugUtilsMessengerEXT(mVkInstance, mDebugMessenger, nullptr);
        mDebugMessenger = VK_NULL_HANDLE;
    }
#endif
    // VkInstanceの破棄
    destroyVkInstance_();
}
//---------------------------------------------------------------------------
void GfxDevice::waitForIdle()
{
    if (mVkDevice != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(mVkDevice);
    }
}
//---------------------------------------------------------------------------
void GfxDevice::setObjectName(uint64_t handle, const char* name, VkObjectType type)
{
    VkDebugUtilsObjectNameInfoEXT name_info{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = type,
        .objectHandle = uint64_t(handle),
        .pObjectName = name,
    };
    vkSetDebugUtilsObjectNameEXT(mVkDevice, &name_info);
}
//---------------------------------------------------------------------------
void GfxDevice::initVkInstance_()
{
    const char* app_name = "Vulkan Application";
    VkApplicationInfo app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,        // 構造体のタイプを指定
        .pNext = nullptr,                                   // 拡張構造体へのポインタ（今回は使用しないのでnullptr）
        .pApplicationName = app_name,                       // アプリケーション名
//        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),     // アプリケーションのバージョン
        .pEngineName = app_name,                            // エンジン名
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),          // エンジンのバージョン
        .apiVersion = VK_API_VERSION_1_3,                   // 使用するVulkan APIのバージョン
    };

    VkInstanceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,    // 構造体のタイプを指定
        .pNext = nullptr,                                   // 拡張構造体へのポインタ（今回は使用しないのでnullptr）
        .flags = 0,                                         // インスタンス作成のフラグ（今回は使用しないので0）
        .pApplicationInfo = &app_info,                      // アプリケーション情報へのポインタ
        .enabledLayerCount = 0,                             // 有効にするレイヤーの数（今回は使用しないので0）
        .ppEnabledLayerNames = nullptr,                     // 有効にするレイヤー名の配列へのポインタ（今回は使用しないのでnullptr）
        .enabledExtensionCount = 0,                         // 有効にする拡張機能の数（今回は使用しないので0）
        .ppEnabledExtensionNames = nullptr,                 // 有効にする拡張機能名の配列へのポインタ（今回は使用しないのでnullptr）
	};
    // Instance レベルの拡張機能やレイヤーを設定
    std::vector<const char*> layers;
    std::vector<const char*> extensions;


    uint32_t glfwRequiredCount = 0;
    const char** glfwExtensionNames = glfwGetRequiredInstanceExtensions(&glfwRequiredCount);
    std::for_each_n(
        glfwExtensionNames,
        glfwRequiredCount,
        [&](auto v) { extensions.push_back(v); });


    // 検証レイヤーの有効化 (バリデーションレイヤー)
    layers.push_back("VK_LAYER_KHRONOS_validation");

    // いらないかも
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    create_info.enabledExtensionCount = uint32_t(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = uint32_t(layers.size());
    create_info.ppEnabledLayerNames = layers.data();
    CheckVkResult(vkCreateInstance(&create_info, nullptr, &mVkInstance));

    // Volkに生成した VkInstanceを渡して初期化
    volkLoadInstance(mVkInstance);

#if _DEBUG
    VkDebugUtilsMessengerCreateInfoEXT utilMsgCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = DebugMessageUtilsCallback,
        .pUserData = nullptr,
    };
    vkCreateDebugUtilsMessengerEXT(mVkInstance, &utilMsgCreateInfo, nullptr, &mDebugMessenger);
#endif
}
//---------------------------------------------------------------------------
void GfxDevice::initPhysicalDevice_()
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(mVkInstance, &count, nullptr);
    std::vector<VkPhysicalDevice> physical_device(count);
    vkEnumeratePhysicalDevices(mVkInstance, &count, physical_device.data());

    // 最初に見つかったものを使用する
    mPhysicalDevice = physical_device[0];

    // メモリ情報を取得しておく
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mMemoryProperties);
}
//---------------------------------------------------------------------------
void GfxDevice::initVkDevice_()
{
    QueueFamilyIndices indices = findQueueFamilies_();

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };
    mGraphicsQueueFamily = indices.graphicsFamily.value();
    mPresentQueueFamily = indices.presentFamily.value();

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mVkDevice) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(mVkDevice, indices.graphicsFamily.value(), 0, &mGraphicsQueue);
    vkGetDeviceQueue(mVkDevice, indices.presentFamily.value(), 0, &mPresentQueue);
}
//---------------------------------------------------------------------------
void GfxDevice::initWindowSurface_(const DeviceInitParams& initParams)
{
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(initParams.glfwWindow);
    if (glfwCreateWindowSurface(mVkInstance, window, nullptr, &mSurface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}
//---------------------------------------------------------------------------
void GfxDevice::destroyVkDevice_()
{
    vkDestroyDevice(mVkDevice, nullptr);
    mVkDevice = VK_NULL_HANDLE;
}
//---------------------------------------------------------------------------
void GfxDevice::destroyWindowSurface_()
{
    if (mVkInstance != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(mVkInstance, mSurface, nullptr);
    }
    mSurface = VK_NULL_HANDLE;
}
//---------------------------------------------------------------------------
void GfxDevice::destroyVkInstance_()
{
    vkDestroyInstance(mVkInstance, nullptr);
    mVkInstance = VK_NULL_HANDLE;
}
//---------------------------------------------------------------------------
QueueFamilyIndices GfxDevice::findQueueFamilies_()
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, i, mSurface, &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}
//---------------------------------------------------------------------------