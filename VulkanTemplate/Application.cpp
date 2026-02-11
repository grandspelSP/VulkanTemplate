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

//#define USE_RENDERPASS (1)

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

#ifdef USE_RENDERPASS
    // Dynamic Renderingを使わない場合、VkRenderPassの準備が必要
    PrepareRenderPass();
#endif

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
        .DescriptorPool = gfx_device->getDescriptorPool(),
        .RenderPass = VK_NULL_HANDLE,
        .MinImageCount = uint32_t(gfx_device->sInflightFrames),
        .ImageCount = gfx_device->getSwapchainImageCount(),
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
    vulkanInfo.UseDynamicRendering = false;
    vulkanInfo.RenderPass = m_renderPass;
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

    vkDestroyPipeline(vkDevice, mVkPipeline, nullptr);
    vkDestroyPipelineLayout(vkDevice, mVkPipelineLayout, nullptr);
    mVkPipeline = VK_NULL_HANDLE;
    mVkPipelineLayout = VK_NULL_HANDLE;

#ifdef USE_RENDERPASS
    vkDestroyRenderPass(vkDevice, m_renderPass, nullptr);
    m_renderPass = VK_NULL_HANDLE;

    for (auto& f : m_framebuffers)
    {
        vkDestroyFramebuffer(vkDevice, f, nullptr);
    }
    m_framebuffers.clear();
#endif

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
void Application::surfaceSizeChanged()
{
    const auto& window = getAppWindow();
    int new_width = 0, new_height = 0;
    GLFWwindow* glfw_window = window->getPlatformHandle()->window;
    glfwGetWindowSize(glfw_window, &new_width, &new_height);

    assert(new_width != 0 && new_height != 0);

    auto& gfxDevice = getGfxDevice();
    const auto& swaphain_info = gfxDevice->getSwapchainInfo();
    if (swaphain_info.width != new_width || swaphain_info.height != new_height)
    {
        gfxDevice->recreateSwapchain(GfxDevice::SwapchainInfo{ new_width, new_height });
    }
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

    gfx_device->newFrame();

    auto current_command_buffer = gfx_device->getCurrentCommandBuffer();
    //static bool isFirstFrame = true;
    //if (isFirstFrame)
    //{
    //    isFirstFrame = false;
    //}
    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();

    gfx_device->getSwapchainInfo();
    VkClearValue clearValue = {
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
        .clearValue = clearValue,
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

    VkRenderPassBeginInfo renderPassBegin{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = m_renderPass,
      .framebuffer = m_framebuffers[gfxDevice->GetSwapchainImageIndex()],
      .renderArea = {
        .extent = { uint32_t(width), uint32_t(height) },
      },
      .clearValueCount = 1,
      .pClearValues = &clearValue,
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);
#endif

    vkCmdBindPipeline(current_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipeline);
    //VkDeviceSize vertexOffsets[] = { 0 };
    //vkCmdBindVertexBuffers(current_command_buffer, 0, 1, &mVertexBuffer.buffer, vertexOffsets);
    //vkCmdDraw(current_command_buffer, 3, 1, 0, 0);
	rect.render(current_command_buffer);

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
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), current_command_buffer);

#if !defined(USE_RENDERPASS)
    vkCmdEndRendering(current_command_buffer);
#else
    vkCmdEndRenderPass(commandBuffer);
#endif

    endRender_();

    gfx_device->submit();
}
//---------------------------------------------------------------------------
void Application::beginRender_()
{
    auto& gfx_device = getGfxDevice();
    auto current_command_buffer = gfx_device->getCurrentCommandBuffer();

    gfx_device->transitionLayoutSwapchainImage(current_command_buffer,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
}
//---------------------------------------------------------------------------
void Application::endRender_()
{
    auto& gfx_device = getGfxDevice();
    auto current_command_buffer = gfx_device->getCurrentCommandBuffer();

    gfx_device->transitionLayoutSwapchainImage(current_command_buffer,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_2_NONE);
}
//---------------------------------------------------------------------------
void Application::prepareTriangle_()
{
    auto& gfx_device = getGfxDevice();
    auto device = gfx_device->getVkDevice();

    // 頂点バッファの作成
    rect.initialize(gfx_device.get());
 //   Vertex vertices[] = {
 //   Vertex{ glm::vec2( -0.5f, -0.5f ), glm::vec3(1.0f,0.0f,0.0f), glm::vec2(1.0f, 0.0f) },
 //   Vertex{ glm::vec2(  0.5f, -0.5f ), glm::vec3(0.0f,1.0f,0.0f), glm::vec2(0.0f, 0.0f) },
 //   Vertex{ glm::vec2(  0.5f,  0.5f ), glm::vec3(0.0f,0.0f,1.0f), glm::vec2(0.0f, 1.0f) },
 //   Vertex{ glm::vec2( -0.5f,  0.5f),  glm::vec3(1.0f,1.0f,1.0f), glm::vec2(1.0f, 1.0f) },
 //   };
 //   const std::vector<uint16_t> indices = {
 //       0, 1, 2, 2, 3, 0
 //   };
 //   VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	//// テスト
 ////   int tex_width, tex_height, tex_channels;
	////stbi_uc* pixels = stbi_load("C:/Users/user/Desktop/go/VulkanTemplate/texture/ENDFIELD_SHARE_1769687062.png", &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
 ////   if (!pixels) {
 ////       throw std::runtime_error("failed to load texture image!");
 ////   }
 ////   VkDeviceSize image_size = tex_width * tex_height * 4;
 //   VkBufferCreateInfo buffer_create_info{
 //       .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
 //       .size = uint32_t(sizeof(bufferSize)),
 //       .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
 //   };
 //   vkCreateBuffer(device, &buffer_create_info, nullptr, &mVertexBuffer.buffer);

 //   // メモリ要件を求める
 //   VkMemoryRequirements memory_requirements;
 //   vkGetBufferMemoryRequirements(device, mVertexBuffer.buffer, &memory_requirements);
 //   // メモリを確保する
 //   VkMemoryAllocateInfo memory_allocate_info{
 //       .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
 //       .allocationSize = memory_requirements.size,
 //       .memoryTypeIndex = gfx_device->getMemoryTypeIndex(memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
 //   };
 //   vkAllocateMemory(device, &memory_allocate_info, nullptr, &mVertexBuffer.memory);
 //   vkBindBufferMemory(device, mVertexBuffer.buffer, mVertexBuffer.memory, 0);

 //   // 頂点データを書き込む
 //   void* mapped;
 //   vkMapMemory(device, mVertexBuffer.memory, 0, bufferSize, 0, &mapped);
 //   memcpy(mapped, indices.data(), sizeof(bufferSize));
 //   vkUnmapMemory(device, mVertexBuffer.memory);
 //   buffer_create_info = {
 //       .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
 //       .size = uint32_t(sizeof(bufferSize)),
 //       .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
 //   };
 //   vkCreateBuffer(device, &buffer_create_info, nullptr, &mVertexBuffer.buffer);

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &mVkPipelineLayout);

    VkVertexInputBindingDescription binding_desc{
        .binding = 0,
        .stride = uint32_t(sizeof(Rect::Vertex)),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription attribute_desc[] = {
      {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 },
      {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = sizeof(glm::vec2) },
      {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = sizeof(glm::vec2) + sizeof(glm::vec3) },
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_desc,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = attribute_desc,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
    };

    // ビューポート/シザーの指定
    VkViewport viewport{
        .x = 0, .y = 0,
        .width = float(gfx_device->getSwapchainInfo().width), .height = float(gfx_device->getSwapchainInfo().height),
        .minDepth = 0.0f, .maxDepth = 1.0f,
    };
    VkRect2D scissor{
        .offset = { 0, 0 },
        .extent = {
            .width = uint32_t(gfx_device->getSwapchainInfo().width),
            .height = uint32_t(gfx_device->getSwapchainInfo().height)
        },
    };

    VkPipelineViewportStateCreateInfo viewport_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };


    VkPipelineRasterizationStateCreateInfo rasterize_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.cullMode = VK_CULL_MODE_NONE,  // カリング無効
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisample_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineColorBlendAttachmentState blend_attachment_state{
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo blend_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment_state
    };


    VkPipelineDepthStencilStateCreateInfo depthStencilState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
    };


    // シェーダー(SPIR-V) 読み込み
    std::vector<char> vertexShaderSpv, fragmentShaderSpv;
    getFileLoader()->Load("res/shader.vert.spv", vertexShaderSpv);
    getFileLoader()->Load("res/shader.frag.spv", fragmentShaderSpv);

    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stage_create_info{ {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = gfx_device->createShaderModule(vertexShaderSpv.data(), vertexShaderSpv.size()),
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = gfx_device->createShaderModule(fragmentShaderSpv.data(), fragmentShaderSpv.size()),
            .pName = "main",
        }
    } };

    VkGraphicsPipelineCreateInfo pipelineCreateInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = uint32_t(shader_stage_create_info.size()),
      .pStages = shader_stage_create_info.data(),
      .pVertexInputState = &vertex_input_create_info,
      .pInputAssemblyState = &input_assembly_state_create_info,
      .pViewportState = &viewport_state_create_info,
      .pRasterizationState = &rasterize_state_create_info,
      .pMultisampleState = &multisample_state_create_info,
      .pDepthStencilState = &depthStencilState,
      .pColorBlendState = &blend_state_create_info,
      .layout = mVkPipelineLayout,
      .renderPass = VK_NULL_HANDLE,
    };
#ifndef USE_RENDERPASS
    VkFormat colorFormats[] = {
        gfx_device->getSwapchainFormat().format,
    };
    VkPipelineRenderingCreateInfo rendering_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = colorFormats,
    };
    pipelineCreateInfo.pNext = &rendering_create_info;
#else
    pipelineCreateInfo.renderPass = m_renderPass;
#endif

    auto res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &mVkPipeline);
    if (res != VK_SUCCESS)
    {

    }
    for (auto& m : shader_stage_create_info)
    {
        gfx_device->destroyShaderModule(m.module);
    }

}
//---------------------------------------------------------------------------
void Application::prepareRenderPass_()
{
    auto& gfx_device = getGfxDevice();
    auto device = gfx_device->getVkDevice();

    VkFormat format = gfx_device->getSwapchainFormat().format;
    VkAttachmentDescription colorAttachment{
      .format = format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference colorRefs{
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass{
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorRefs,
    };

    VkRenderPassCreateInfo renderPassCI{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &colorAttachment,
      .subpassCount = 1,
      .pSubpasses = &subpass,
    };
    vkCreateRenderPass(device, &renderPassCI, nullptr, &mVkRenderPass);

    int viewCount = gfx_device->getSwapchainImageCount();
    mVkFramebuffers.resize(viewCount);
    for (int i = 0; i < viewCount; ++i)
    {
        auto view = gfx_device->getSwapchainImageView(i);

        VkFramebufferCreateInfo fbCI{
          .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
          .renderPass = mVkRenderPass,
          .attachmentCount = 1,
          .pAttachments = &view,
          .width = uint32_t(gfx_device->getSwapchainInfo().width),
          .height = uint32_t(gfx_device->getSwapchainInfo().height),
          .layers = 1,
        };
        vkCreateFramebuffer(device, &fbCI, nullptr, &mVkFramebuffers[i]);
    }
}
//---------------------------------------------------------------------------