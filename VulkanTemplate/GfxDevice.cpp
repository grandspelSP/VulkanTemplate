#include "GfxDevice.h"
#include <algorithm>
#include <cassert>
#include "Window.h"

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"
#endif

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
    recreateSwapchain(mSwapchainInfo);
    initCommandPool_();
    initDescriptorPool_();
    initSemaphores_();
    initCommandBuffers_();
}
//---------------------------------------------------------------------------
void GfxDevice::Shutdown()
{
    // デバイスがアイドルになるまで待機
    waitForIdle();

    if (mVkDevice != VK_NULL_HANDLE)
    {
        destroyCommandBuffers_();
        destroySemaphores_();
        destroyDescriptorPool_();
        destroyCommandPool_();
        destroySwapchain_();
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
void GfxDevice::newFrame()
{
    const auto& frame_info = mFrameCommandInfos[mCurrentFrameIndex];
    const auto fence = frame_info.fence;
	// fenceの完了を待機し、コマンド処理完了を待つ
    vkWaitForFences(mVkDevice, 1, &fence, VK_TRUE, UINT64_MAX);
    auto res = vkAcquireNextImageKHR(mVkDevice, mVkSwapchain, UINT64_MAX, frame_info.present_completed, VK_NULL_HANDLE, &mSwapchainImageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return;
    }
    vkResetFences(mVkDevice, 1, &fence);

    // コマンドバッファを開始
    vkResetCommandBuffer(frame_info.buffer, 0);
    VkCommandBufferBeginInfo command_begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    vkBeginCommandBuffer(frame_info.buffer, &command_begin_info);
}
//---------------------------------------------------------------------------
void GfxDevice::submit()
{
    const auto& frame_info = mFrameCommandInfos[mCurrentFrameIndex];
    vkEndCommandBuffer(frame_info.buffer);

    // コマンドを発行する.
    VkPipelineStageFlags wait_stage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame_info.present_completed,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &frame_info.buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &frame_info.render_completed,
    };
    vkQueueSubmit(mVkGraphicsQueue, 1, &submit_info, frame_info.fence);

    mCurrentFrameIndex = (++mCurrentFrameIndex) % sInflightFrames;

    // プレゼンテーションの実行
    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame_info.render_completed,
        .swapchainCount = 1,
        .pSwapchains = &mVkSwapchain,
        .pImageIndices = &mSwapchainImageIndex
    };
    vkQueuePresentKHR(mVkGraphicsQueue, &present_info);
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
void GfxDevice::transitionLayoutSwapchainImage(VkCommandBuffer commandBuffer, VkImageLayout newLayout, VkAccessFlags2 newAccessFlags)
{
    // 変更するため非const
    auto& swapchain_state = mSwapchainState[mSwapchainImageIndex];
    VkImageMemoryBarrier2 barrierToRT{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = swapchain_state.accessFlags,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = newAccessFlags,
        .oldLayout = swapchain_state.layout,
        .newLayout = newLayout,
        .image = swapchain_state.image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0, .levelCount = 1,
            .baseArrayLayer = 0, .layerCount = 1,
        }
    };

    VkDependencyInfo dependency_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrierToRT,
    };

    if (vkCmdPipelineBarrier2)
    {
        vkCmdPipelineBarrier2(commandBuffer, &dependency_info);
    }
    else
    {
        vkCmdPipelineBarrier2KHR(commandBuffer, &dependency_info);
    }
    swapchain_state.layout = newLayout;
    swapchain_state.accessFlags = newAccessFlags;
}
//---------------------------------------------------------------------------
void GfxDevice::recreateSwapchain(SwapchainInfo info)
{
	// Windowsの場合、リサイズ時にサーフェスが対応しているか確認
    {
        VkBool32 supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(mVkPhysicalDevice, mGraphicsQueueIndex, mVkSurface, &supported);
        assert(supported == VK_TRUE);
    }

    mSwapchainInfo.width = info.width;
    mSwapchainInfo.height = info.height;

    // 能力情報の取得
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(mVkPhysicalDevice, mVkSurface, &count, nullptr);
    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(mVkPhysicalDevice, mVkSurface, &count, modes.data());

    VkSurfaceCapabilitiesKHR surface_capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mVkPhysicalDevice, mVkSurface, &surface_capabilities);

    const auto present_mode = VK_PRESENT_MODE_FIFO_KHR;
    bool mode_supported = std::any_of(modes.begin(), modes.end(), [=](const auto& v) { return v == present_mode; });
    assert(mode_supported);

    VkExtent2D extent = surface_capabilities.currentExtent;
    if (surface_capabilities.currentExtent.width == UINT32_MAX)
    {
        extent.width = info.width;
        extent.height = info.height;
    }

    // ダブルバッファリング想定
    uint32_t swapchain_image_count = 2;
    if (swapchain_image_count < surface_capabilities.minImageCount)
    {
        swapchain_image_count = surface_capabilities.minImageCount;
    }

    // デバイス状態の待機
    waitForIdle();

    VkSwapchainKHR old_swapchain = mVkSwapchain;
    // スワップチェインの初期化
    VkSwapchainCreateInfoKHR swapchain_create_info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = mVkSurface,
        .minImageCount = swapchain_image_count,
        .imageFormat = mVkSurfaceFormat.format,
        .imageColorSpace = mVkSurfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = old_swapchain,
    };

    CheckVkResult(vkCreateSwapchainKHR(mVkDevice, &swapchain_create_info, nullptr, &mVkSwapchain));

    // スワップチェインのイメージ取得
    count = 0;
    vkGetSwapchainImagesKHR(mVkDevice, mVkSwapchain, &count, nullptr);
    std::vector<VkImage> images(count);
    vkGetSwapchainImagesKHR(mVkDevice, mVkSwapchain, &count, images.data());

    // スワップチェインのイメージビューの準備
    std::vector<SwapchainState> swapchain_state(count);
    for (auto i = 0; auto& state : swapchain_state)
    {
        auto image = images[i];
        VkImageViewCreateInfo image_view_create_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = mVkSurfaceFormat.format,
            .components = {
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            }
        };
        vkCreateImageView(mVkDevice, &image_view_create_info, nullptr, &state.view);
        state.image = image;
        state.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        state.accessFlags = VK_ACCESS_2_NONE;
        ++i;
    }
    mSwapchainState.swap(swapchain_state);

    // 古い方を破棄
    if (old_swapchain != VK_NULL_HANDLE)
    {
        for (auto& state : swapchain_state)
        {
            vkDestroyImageView(mVkDevice, state.view, nullptr);
        }
        vkDestroySwapchainKHR(mVkDevice, old_swapchain, nullptr);
    }

    // Present で使用できるデバイスキューの準備
    count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(mVkPhysicalDevice, &count, nullptr);
    std::vector<VkBool32> support_present(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        vkGetPhysicalDeviceSurfaceSupportKHR(mVkPhysicalDevice, i, mVkSurface, &support_present[i]);
    }
    // 今回はグラフィックスキューでPresentを実行するため確認
    assert(support_present[mGraphicsQueueIndex] == VK_TRUE);

}
//---------------------------------------------------------------------------
VkShaderModule GfxDevice::createShaderModule(const void* code, size_t length)
{
    VkShaderModuleCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = length,
        .pCode = reinterpret_cast<const uint32_t*>(code),
    };
    VkShaderModule module = VK_NULL_HANDLE;
    vkCreateShaderModule(mVkDevice, &create_info, nullptr, &module);
    return module;
}
//---------------------------------------------------------------------------
void GfxDevice::destroyShaderModule(VkShaderModule shaderModule)
{
    vkDestroyShaderModule(mVkDevice, shaderModule, nullptr);
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
    mVkPhysicalDevice = physical_device[0];

    // メモリ情報を取得しておく
    vkGetPhysicalDeviceMemoryProperties(mVkPhysicalDevice, &mMemoryProperties);

    // グラフィックス用のキューインデックスを調査
    vkGetPhysicalDeviceQueueFamilyProperties(mVkPhysicalDevice, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_family_properties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(mVkPhysicalDevice, &count, queue_family_properties.data());

    uint32_t queue_index = ~0U;
    for (uint32_t i = 0; const auto& props : queue_family_properties)
    {
        if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            queue_index = i;
            break;
        }
        ++i;
    }
    assert(queue_index != ~0U);
    mGraphicsQueueIndex = queue_index;
}
//---------------------------------------------------------------------------
void GfxDevice::initVkDevice_()
{
    std::vector<const char*> extensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,          // スワップチェインは常に使用
      VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,  // Vulkan 1.3 使えない環境向け
    };

    // 対象のGPUが使える機能を取得し、使えるものは有効状態でデバイスを生成
    // ここでは各バージョンレベルの機能について取得するため各構造体をリンク
    VkPhysicalDeviceFeatures2 device_features2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceVulkan11Features vulkan11_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES
    };
    VkPhysicalDeviceVulkan12Features vulkan12_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
    };
    VkPhysicalDeviceVulkan13Features vulkan13_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
    };
    device_features2.pNext = &vulkan11_features;
    vulkan11_features.pNext = &vulkan12_features;
    vulkan12_features.pNext = &vulkan13_features;
    vkGetPhysicalDeviceFeatures2(mVkPhysicalDevice, &device_features2);

    // 有効にする機能を明示的にセット
    vulkan13_features.dynamicRendering = VK_TRUE;
    vulkan13_features.synchronization2 = VK_TRUE;
    vulkan13_features.maintenance4 = VK_TRUE;

    if (!isSupportVulkan13())
    {
        vulkan13_features.dynamicRendering = VK_FALSE;
    }
    // VkDeviceの生成
    const float queue_priorities[] = { 1.0f };
    VkDeviceQueueCreateInfo deviceQueueCI{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = mGraphicsQueueIndex,
        .queueCount = 1,
        .pQueuePriorities = queue_priorities,
    };
    VkDeviceCreateInfo device_create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &deviceQueueCI,
        .enabledExtensionCount = uint32_t(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };
    // VkPhysicalDeviceFeatures2に有効にする機能情報を入れているので、pNextに設定
    device_create_info.pNext = &device_features2;

    CheckVkResult(vkCreateDevice(mVkPhysicalDevice, &device_create_info, nullptr, &mVkDevice));

    // Volk に VkDevice を渡して初期化
    volkLoadDevice(mVkDevice);

    // デバイスキューを取得
    vkGetDeviceQueue(mVkDevice, mGraphicsQueueIndex, 0, &mVkGraphicsQueue);
}
//---------------------------------------------------------------------------
void GfxDevice::initWindowSurface_(const DeviceInitParams& initParams)
{
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(initParams.glfwWindow);

    HINSTANCE inst = GetModuleHandleW(NULL);
    VkWin32SurfaceCreateInfoKHR surface_create_info{
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = inst,
        .hwnd = glfwGetWin32Window(window),
    };
    CheckVkResult(vkCreateWin32SurfaceKHR(mVkInstance, &surface_create_info, nullptr, &mVkSurface));

    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        mSwapchainInfo.width = width;
        mSwapchainInfo.height = height;
    }

    getAppWindow()->getWindowSize(mSwapchainInfo.width, mSwapchainInfo.height);

    // フォーマットの選択
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(mVkPhysicalDevice, mVkSurface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> surface_formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(mVkPhysicalDevice, mVkSurface, &count, surface_formats.data());

    const VkFormat formats[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };
    mVkSurfaceFormat.format = VK_FORMAT_UNDEFINED;
    bool found = false;
    for (int i = 0; i < std::size(formats) && !found; ++i)
    {
        auto format = formats[i];
        for (const auto& f : surface_formats)
        {
            if (f.format == format && f.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
            {
                mVkSurfaceFormat = f;
                found = true;
                break;
            }
        }
    }
    assert(found);
}
//---------------------------------------------------------------------------
void GfxDevice::initCommandPool_()
{
    VkCommandPoolCreateInfo command_pool_create_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = mGraphicsQueueIndex,
    };
    CheckVkResult(vkCreateCommandPool(mVkDevice, &command_pool_create_info, nullptr, &mCommandPool));
}
//---------------------------------------------------------------------------
void GfxDevice::initSemaphores_()
{
    VkSemaphoreCreateInfo semaphore_create_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    for (auto& frame : mFrameCommandInfos)
    {
        vkCreateSemaphore(mVkDevice, &semaphore_create_info, nullptr, &frame.render_completed);
        vkCreateSemaphore(mVkDevice, &semaphore_create_info, nullptr, &frame.present_completed);
    }
}
//---------------------------------------------------------------------------
void GfxDevice::initCommandBuffers_()
{
    VkFenceCreateInfo fence_create_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    for (auto& frame : mFrameCommandInfos)
    {
        vkCreateFence(mVkDevice, &fence_create_info, nullptr, &frame.fence);
    }

    VkCommandBufferAllocateInfo command_allocate_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = mCommandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    };
    for (auto& frame : mFrameCommandInfos)
    {
        vkAllocateCommandBuffers(mVkDevice, &command_allocate_info, &frame.buffer);
    }
}
//---------------------------------------------------------------------------
void GfxDevice::initDescriptorPool_()
{
    const uint32_t count = 10000;
    std::vector<VkDescriptorPoolSize> descriptor_pool_size = { {
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = count,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = count,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = count,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = count,
        },
    } };
    VkDescriptorPoolCreateInfo descriptor_pool_create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = count,
        .poolSizeCount = uint32_t(descriptor_pool_size.size()),
        .pPoolSizes = descriptor_pool_size.data(),
    };
    vkCreateDescriptorPool(mVkDevice, &descriptor_pool_create_info, nullptr, &mDescriptorPool);
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
        vkDestroySurfaceKHR(mVkInstance, mVkSurface, nullptr);
    }
    mVkSurface = VK_NULL_HANDLE;
}
//---------------------------------------------------------------------------
void GfxDevice::destroySwapchain_()
{
    for (auto& state : mSwapchainState)
    {
        vkDestroyImageView(mVkDevice, state.view, nullptr);
    }
    // スワップチェインから取得したイメージについては廃棄処理は不要
    mSwapchainState.clear();
    vkDestroySwapchainKHR(mVkDevice, mVkSwapchain, nullptr);
    mVkSwapchain = VK_NULL_HANDLE;
}
//---------------------------------------------------------------------------
void GfxDevice::destroyCommandPool_()
{
    vkDestroyCommandPool(mVkDevice, mCommandPool, nullptr);
    mCommandPool = VK_NULL_HANDLE;
}
//---------------------------------------------------------------------------
void GfxDevice::destroySemaphores_()
{
    for (auto& frame : mFrameCommandInfos)
    {
        vkDestroySemaphore(mVkDevice, frame.render_completed, nullptr);
        vkDestroySemaphore(mVkDevice, frame.present_completed, nullptr);
        frame.render_completed = VK_NULL_HANDLE;
        frame.present_completed = VK_NULL_HANDLE;
    }
}
//---------------------------------------------------------------------------
void GfxDevice::destroyCommandBuffers_()
{
    for (auto& f : mFrameCommandInfos)
    {
        vkDestroyFence(mVkDevice, f.fence, nullptr);
        vkFreeCommandBuffers(mVkDevice, mCommandPool, 1, &f.buffer);
        f.fence = VK_NULL_HANDLE;
        f.buffer = VK_NULL_HANDLE;
    }

}
//---------------------------------------------------------------------------
void GfxDevice::destroyDescriptorPool_()
{
    vkDestroyDescriptorPool(mVkDevice, mDescriptorPool, nullptr);
    mDescriptorPool = VK_NULL_HANDLE;
}
//---------------------------------------------------------------------------
void GfxDevice::destroyVkInstance_()
{
    vkDestroyInstance(mVkInstance, nullptr);
    mVkInstance = VK_NULL_HANDLE;
}
//---------------------------------------------------------------------------