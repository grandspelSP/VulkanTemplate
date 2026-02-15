#include "Application.h"
#include <array>
#include "Window.h"
#include "GfxDevice.h"
#include "FileLoader.h"

#include "imgui.h"
#include "GLFW/glfw3.h"
#include "backends/imgui_impl_glfw.h"

#define IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
#include "backends/imgui_impl_vulkan.h"
#include <stb_image.h>

#define USE_RENDERPASS (1)


//---------------------------------------------------------------------------
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}
//---------------------------------------------------------------------------
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}
//---------------------------------------------------------------------------
void Application::Initialize()
{
    initializeWindow_();
    initializeGfxDevice_();

    mIsInitialized = true;

    // ImGui初期化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    
    {
        const auto& window = getAppWindow();
        GLFWwindow* glfw_window = window->getPlatformHandle()->window;
        ImGui_ImplGlfw_InitForVulkan(glfw_window, true);
    }

    createSwapchain_();
    createImageViews_();
#ifdef USE_RENDERPASS
    // Dynamic Renderingを使わない場合、VkRenderPassの準備が必要
    createRenderPass_();
#endif
    createDescriptorSetLayout_();
    createGraphicsPipeline_();
    createFramebuffers_();
    createCommandPool_();

    auto& gfx_device = getGfxDevice();
    ImGui_ImplVulkan_LoadFunctions(
        [](const char* functionName, void* userArgs) {
            auto& dev = getGfxDevice();
            auto device = dev->getVkDevice();
            auto instance = dev->getVkInstance();
            auto device_proc_addr = vkGetDeviceProcAddr(device, functionName);
            if (device_proc_addr != nullptr)
            {
                return device_proc_addr;
            }
            auto instance_proc_addr = vkGetInstanceProcAddr(instance, functionName);
            return instance_proc_addr;
        });

    ImGui_ImplVulkan_InitInfo impl_vulkakn_init_info{
        .Instance = gfx_device->getVkInstance(),
        .PhysicalDevice = gfx_device->getVkPhysicalDevice(),
        .Device = gfx_device->getVkDevice(),
        .QueueFamily = gfx_device->getGraphicsQueueFamily(),
        .Queue = gfx_device->getGraphicsQueue(),
        .DescriptorPool = mDescriptorPool,
        .RenderPass = VK_NULL_HANDLE,
        .MinImageCount = uint32_t(sInflightFrames),
        .ImageCount = mSwapchainImageCount,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkFormat color_format = gfx_device->getSwapchainFormat().format;
    VkFormat depth_format = VK_FORMAT_UNDEFINED;
    impl_vulkakn_init_info.UseDynamicRendering = true;
    impl_vulkakn_init_info.PipelineRenderingCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &color_format,
      .depthAttachmentFormat = depth_format,
    };
#ifdef USE_RENDERPASS
    impl_vulkakn_init_info.UseDynamicRendering = false;
    impl_vulkakn_init_info.RenderPass = mRenderPass;
#endif

    ImGui_ImplVulkan_Init(&impl_vulkakn_init_info);
    ImGui_ImplVulkan_CreateFontsTexture();

    // アプリケーションコード初期化
    prepareTriangle_();
}
//---------------------------------------------------------------------------
void Application::Shutdown()
{
    auto& gfx_device = getGfxDevice();
    gfx_device->waitForIdle();

    auto vkDevice = gfx_device->getVkDevice();

    // 頂点バッファ破棄
	rect.destroy(gfx_device.get());

    cleanup_();

    // ImGui終了
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    gfx_device->Shutdown();

    auto& window = getAppWindow();
    window->Shutdown();

    mIsInitialized = false;
}
//---------------------------------------------------------------------------
void Application::initializeWindow_()
{
    auto& window = getAppWindow();
    Window::WindowInitParams window_init_params{};
    //init_params.title = "テスト";
    //init_params.width = 1280;
    //init_params.height = 720;
    window->Initialize(window_init_params);
}
//---------------------------------------------------------------------------
void Application::initializeGfxDevice_()
{
    GfxDevice::DeviceInitParams device_init_params{};
    auto& window = getAppWindow();
    device_init_params.glfwWindow = window->getPlatformHandle()->window;

    auto& gfx_device = getGfxDevice();
    gfx_device->Initialize(device_init_params);
}
//---------------------------------------------------------------------------
void Application::process()
{
    auto& gfx_device = getGfxDevice();
    auto device = gfx_device->getVkDevice();

    drawFrame_();
}
//---------------------------------------------------------------------------
void Application::prepareTriangle_()
{
    auto& gfx_device = getGfxDevice();
    auto device = gfx_device->getVkDevice();

    // 頂点バッファの作成
    rect.initialize(gfx_device.get());
}
//---------------------------------------------------------------------------
void Application::createSwapchain_()
{
    SwapChainSupportDetails swap_chain_support = querySwapChainSupport_();

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat_(swap_chain_support.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode_(swap_chain_support.presentModes);
    VkExtent2D extent = chooseSwapExtent_(swap_chain_support.capabilities);

    uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
    if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount) {
        image_count = swap_chain_support.capabilities.maxImageCount;
    }
    mSwapchainImageCount = image_count;

    VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = getGfxDevice()->getWindowSurface(),
        .minImageCount = image_count,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    };

    QueueFamilyIndices indices = findQueueFamilies_();
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swap_chain_support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(getGfxDevice()->getVkDevice(), &createInfo, nullptr, &mSwapchain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(getGfxDevice()->getVkDevice(), mSwapchain, &image_count, nullptr);
    mSwapchainImages.resize(image_count);
    vkGetSwapchainImagesKHR(getGfxDevice()->getVkDevice(), mSwapchain, &image_count, mSwapchainImages.data());

    mSwapchainImageFormat = surfaceFormat.format;
    mSwapchainExtent = extent;
}
//---------------------------------------------------------------------------
SwapChainSupportDetails Application::querySwapChainSupport_()
{
    SwapChainSupportDetails details;
	auto physical_device = getGfxDevice()->getVkPhysicalDevice();
    auto surface = getGfxDevice()->getWindowSurface();

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &details.capabilities);

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);

    if (format_count != 0) {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, details.formats.data());
    }

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);

    if (present_mode_count != 0) {
        details.presentModes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, details.presentModes.data());
    }

    return details;
}
//---------------------------------------------------------------------------
VkSurfaceFormatKHR Application::chooseSwapSurfaceFormat_(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}
//---------------------------------------------------------------------------
VkPresentModeKHR Application::chooseSwapPresentMode_(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}
//---------------------------------------------------------------------------
VkExtent2D Application::chooseSwapExtent_(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    else {
        int width, height;
        glfwGetFramebufferSize(getAppWindow()->getPlatformHandle()->window, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}
//---------------------------------------------------------------------------
void Application::createImageViews_()
{
	mSwapchainImageViews.resize(mSwapchainImages.size());

    for (size_t i = 0; i < mSwapchainImages.size(); i++) {
        mSwapchainImageViews[i] = createImageView_(mSwapchainImages[i], mSwapchainImageFormat);
    }
}
//---------------------------------------------------------------------------
VkImageView Application::createImageView_(VkImage image, VkFormat format)
{
    auto& gfx_device = getGfxDevice();
    auto device = gfx_device->getVkDevice();
    VkImageViewCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {
            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    VkImageView image_view;
    if (vkCreateImageView(device, &create_info, nullptr, &image_view) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image view!");
    }
	return image_view;
}
//---------------------------------------------------------------------------
void Application::createRenderPass_()
{
    auto& gfx_device = getGfxDevice();
    auto device = gfx_device->getVkDevice();

    VkFormat format = gfx_device->getSwapchainFormat().format;
    VkAttachmentDescription color_attachment{
      .format = format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference color_attachment_reference{
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass{
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_reference,
    };

    VkSubpassDependency dependency{
      .srcSubpass = VK_SUBPASS_EXTERNAL,
	  .dstSubpass = 0,
	  .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
	  .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
	  .srcAccessMask = 0,
	  .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo render_pass_create_info{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &color_attachment,
      .subpassCount = 1,
      .pSubpasses = &subpass,
	  .dependencyCount = 1,
	  .pDependencies = &dependency,
    };

    if(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &mRenderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
	}
}
//---------------------------------------------------------------------------
void Application::createDescriptorSetLayout_()
{
    auto& gfx_device = getGfxDevice();
    auto device = gfx_device->getVkDevice();

    VkDescriptorSetLayoutBinding ubo{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
	  .pImmutableSamplers = nullptr,
    };

    VkDescriptorSetLayoutBinding sampler{
      .binding = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = nullptr,
    };

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { ubo, sampler };
    VkDescriptorSetLayoutCreateInfo layout_create_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings = bindings.data(),
	};

    if(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &mDescriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor set layout!");
	}
}
//---------------------------------------------------------------------------
void Application::createGraphicsPipeline_()
{
    auto createShaderModule_ = [this](const std::vector<char>& code) -> const VkShaderModule {
        VkShaderModuleCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = code.size(),
            .pCode = reinterpret_cast<const uint32_t*>(code.data()),
		};

        VkShaderModule shader_module;
        if (vkCreateShaderModule(getGfxDevice()->getVkDevice(), &create_info, nullptr, &shader_module) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create shader module!");
        }
		return shader_module;
    };

    std::vector<char> vert_shader_code, frag_shader_code;
    getFileLoader()->Load("res/shader.vert.spv", vert_shader_code);
    getFileLoader()->Load("res/shader.frag.spv", frag_shader_code);

	auto vert_shader_module = createShaderModule_(vert_shader_code);
    auto frag_shader_module = createShaderModule_(frag_shader_code);
    
    VkPipelineShaderStageCreateInfo vert_shader_stage_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vert_shader_module,
        .pName = "main",
    };
    VkPipelineShaderStageCreateInfo frag_shader_stage_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = frag_shader_module,
        .pName = "main",
    };

    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = { vert_shader_stage_info, frag_shader_stage_info };
    
	auto binding_description = Vertex::getBindingDescription();
	auto attribute_descriptions = Vertex::getAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertex_input_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &binding_description,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size()),
		.pVertexAttributeDescriptions = attribute_descriptions.data(),
	};

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
	};

    VkPipelineViewportStateCreateInfo viewport_state_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
		.lineWidth = 1.0f,
	};

    VkPipelineMultisampleStateCreateInfo multisampling_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment{
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo color_blending_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
        .blendConstants = {0.0f},
    };

    std::vector<VkDynamicState> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
	};
    VkPipelineDynamicStateCreateInfo dynamic_state_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
	};

    VkPipelineLayoutCreateInfo pipeline_layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
		.pSetLayouts = &mDescriptorSetLayout,
	};

    if(vkCreatePipelineLayout(getGfxDevice()->getVkDevice(), &pipeline_layout_info, nullptr, &mPipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipeline_info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = static_cast<uint32_t>(shader_stages.size()),
        .pStages = shader_stages.data(),
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly_info,
        .pViewportState = &viewport_state_info,
        .pRasterizationState = &rasterizer_info,
        .pMultisampleState = &multisampling_info,
        .pColorBlendState = &color_blending_info,
        .layout = mPipelineLayout,
        .renderPass = mRenderPass,
        .subpass = 0,
    };
    if (vkCreateGraphicsPipelines(getGfxDevice()->getVkDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &mPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
    vkDestroyShaderModule(getGfxDevice()->getVkDevice(), vert_shader_module, nullptr);
	vkDestroyShaderModule(getGfxDevice()->getVkDevice(), frag_shader_module, nullptr);
}
//---------------------------------------------------------------------------
void Application::createFramebuffers_()
{
	mSwapchainFramebuffers.resize(mSwapchainImageViews.size());
    
    for (size_t i = 0; i < mSwapchainImageViews.size(); i++) {
        VkImageView attachments[] = {
            mSwapchainImageViews[i]
        };

        VkFramebufferCreateInfo frame_buffer_create_info{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = mRenderPass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = mSwapchainExtent.width,
            .height = mSwapchainExtent.height,
            .layers = 1,
        };

        if (vkCreateFramebuffer(getGfxDevice()->getVkDevice(), &frame_buffer_create_info, nullptr, &mSwapchainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}
//---------------------------------------------------------------------------
void Application::createCommandPool_()
{
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies_();

    VkCommandPoolCreateInfo pool_info {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(getGfxDevice()->getVkDevice(), &pool_info, nullptr, &mCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics command pool!");
    }
}
//---------------------------------------------------------------------------
void Application::createDescriptorPool_()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(sInflightFrames);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(sInflightFrames);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(sInflightFrames);

    if (vkCreateDescriptorPool(getGfxDevice()->getVkDevice(), &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}
//---------------------------------------------------------------------------
void Application::createDescriptorSets_()
{
    std::vector<VkDescriptorSetLayout> layouts(sInflightFrames, mDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = mDescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(sInflightFrames);
    allocInfo.pSetLayouts = layouts.data();

    mDescriptorSets.resize(sInflightFrames);
    if (vkAllocateDescriptorSets(getGfxDevice()->getVkDevice(), &allocInfo, mDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < sInflightFrames; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureImageView;
        imageInfo.sampler = textureSampler;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = mDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = mDescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(getGfxDevice()->getVkDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}
//---------------------------------------------------------------------------
void Application::createCommandBuffer_()
{
    mCommandBuffer.resize(sInflightFrames);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = mCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)mCommandBuffer.size();

    if (vkAllocateCommandBuffers(getGfxDevice()->getVkDevice(), &allocInfo, mCommandBuffer.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}
//---------------------------------------------------------------------------
void Application::recordCommandBuffer_(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    // TODO:mDescriptorSetsはRectに移動
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();

    VkClearValue clear_value = {
    VkClearColorValue{ 0.85f, 0.5f, 0.7f, 0.0f },
    };

#if !defined(USE_RENDERPASS)
    beginRender_();

    VkRenderingAttachmentInfo attachment_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = gfx_device->getCurrentSwapchainImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clear_value,
    };
    VkRenderingInfo rendering_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .extent = {
            static_cast<uint32_t>(gfx_device->getSwapchainInfo().width),
            static_cast<uint32_t>(gfx_device->getSwapchainInfo().height) },
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment_info,
    };

    vkCmdBeginRendering(current_command_buffer, &rendering_info);
#else

    VkRenderPassBeginInfo render_pass_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = mRenderPass,
        .framebuffer = mSwapchainFramebuffers[imageIndex],
        .renderArea{
            .offset = {0, 0},
            .extent = mSwapchainExtent,
        },
        .clearValueCount = 1,
        .pClearValues = &clear_value,
    };

    vkCmdBeginRenderPass(commandBuffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
#endif

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)mSwapchainExtent.width;
    viewport.height = (float)mSwapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = mSwapchainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkBuffer vertexBuffers[] = { vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSets[mCurrentFrame], 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);


    ImGui::Begin("Information");
    ImGui::Text("Hello Triangle");
    ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);

#if defined(USE_RENDERPASS)
    ImGui::Text("USE RenderPass");
#else
    ImGui::Text("USE Dynamic Rendering");
#endif
    ImGui::End();

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

#if !defined(USE_RENDERPASS)
    vkCmdEndRendering(commandBuffer);
#else
    vkCmdEndRenderPass(commandBuffer);
#endif

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}
//---------------------------------------------------------------------------
void Application::createSyncObjects_()
{
    mImageAvailableSemaphores.resize(sInflightFrames);
    mRenderFinishedSemaphores.resize(sInflightFrames);
    mInFlightFences.resize(sInflightFrames);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < sInflightFrames; i++) {
        if (vkCreateSemaphore(getGfxDevice()->getVkDevice(), &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(getGfxDevice()->getVkDevice(), &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(getGfxDevice()->getVkDevice(), &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}
//---------------------------------------------------------------------------
void Application::recreateSwapchain_()
{
    auto window = getAppWindow()->getPlatformHandle()->window;
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(getGfxDevice()->getVkDevice());

    cleanupSwapchain_();

    createSwapchain_();
    createImageViews_();
    createFramebuffers_();
}
//---------------------------------------------------------------------------
void Application::cleanupSwapchain_()
{
	auto device = getGfxDevice()->getVkDevice();

    for (auto framebuffer : mSwapchainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    for (auto imageView : mSwapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(device, mSwapchain, nullptr);
}
//---------------------------------------------------------------------------
void Application::cleanup_()
{
	auto device = getGfxDevice()->getVkDevice();
    cleanupSwapchain_();

    vkDestroyPipeline(device, mPipeline, nullptr);
    vkDestroyPipelineLayout(device, mPipelineLayout, nullptr);
#ifdef USE_RENDERPASS
    vkDestroyRenderPass(device, mRenderPass, nullptr);
#endif

    for (size_t i = 0; i < sInflightFrames; i++) {
        vkDestroyBuffer(device, mUniformBuffers[i], nullptr);
        vkFreeMemory(device, mUniformBuffersMemory[i], nullptr);
    }

    vkDestroyDescriptorPool(device, mDescriptorPool, nullptr);

    vkDestroySampler(device, mTextureSampler, nullptr);
    vkDestroyImageView(device, mTextureImageView, nullptr);

    vkDestroyImage(device, textureImage, nullptr);
    vkFreeMemory(device, textureImageMemory, nullptr);

    vkDestroyDescriptorSetLayout(device, mDescriptorSetLayout, nullptr);

    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkFreeMemory(device, indexBufferMemory, nullptr);

    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);

    for (size_t i = 0; i < sInflightFrames; i++) {
        vkDestroySemaphore(device, mRenderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, mImageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, mInFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(device, mCommandPool, nullptr);
}
//---------------------------------------------------------------------------
void Application::drawFrame_()
{
    auto device = getGfxDevice()->getVkDevice();
    auto graphics_queue = getGfxDevice()->getGraphicsQueue();
    auto present_queue = getGfxDevice()->getPresentQueue();

    vkWaitForFences(device, 1, &mInFlightFences[mCurrentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, mSwapchain, UINT64_MAX, mImageAvailableSemaphores[mCurrentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain_();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    updateUniformBuffer(mCurrentFrame);

    vkResetFences(device, 1, &mInFlightFences[mCurrentFrame]);

    vkResetCommandBuffer(mCommandBuffer[mCurrentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    recordCommandBuffer_(mCommandBuffer[mCurrentFrame], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { mImageAvailableSemaphores[mCurrentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &mCommandBuffer[mCurrentFrame];

    VkSemaphore signalSemaphores[] = { mRenderFinishedSemaphores[mCurrentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphics_queue, 1, &submitInfo, mInFlightFences[mCurrentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkSwapchainKHR swapChains[] = { mSwapchain };
    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signalSemaphores,
        .swapchainCount = 1,
        .pSwapchains = swapChains,
        .pImageIndices = &imageIndex,
    };

    result = vkQueuePresentKHR(present_queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || mFramebufferResized) {
        mFramebufferResized = false;
        recreateSwapchain_();
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    mCurrentFrame = (mCurrentFrame + 1) % sInflightFrames;
}
//---------------------------------------------------------------------------