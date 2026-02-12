#pragma once
#include <vector>
#include "GfxDevice.h"

// GLMで設定する値単位をラジアンに
#define GLM_FORCE_RADIANS
// Vulkan ではClip空間 Z:[0,1]のため定義が必要
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/ext.hpp"
#include "Rect.h"

//---------------------------------------------------------------------------
class Application
{
public:
    void Initialize();
    void Shutdown();

    void process();
    bool getIsInitialized() { return mIsInitialized; }
    void surfaceSizeChanged();

private:
    void initializeWindow_();
    void initializeGfxDevice_();

    void beginRender_();
    void endRender_();

    void prepareTriangle_();
    void prepareRenderPass_();
	void createDescriptorSetLayout_();

    bool mIsInitialized = false;

    Rect rect;

    struct UniformBufferObject
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
	};

	VkDescriptorSetLayout mVkDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout mVkPipelineLayout = VK_NULL_HANDLE;
    VkPipeline mVkPipeline = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> mVkFramebuffers;
    VkRenderPass mVkRenderPass;
};
//---------------------------------------------------------------------------