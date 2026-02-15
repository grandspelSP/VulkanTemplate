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
#include <optional>


//---------------------------------------------------------------------------
struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};
//---------------------------------------------------------------------------
constexpr int sInflightFrames = 2; // 同時に処理するフレーム数
//---------------------------------------------------------------------------
class Application
{
public:
    void Initialize();
    void Shutdown();

    void process();
    bool getIsInitialized() { return mIsInitialized; }

private:
    void initializeWindow_();
    void initializeGfxDevice_();

    void prepareTriangle_();

	void createSwapchain_();
	SwapChainSupportDetails querySwapChainSupport_();
	VkSurfaceFormatKHR chooseSwapSurfaceFormat_(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode_(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent_(const VkSurfaceCapabilitiesKHR& capabilities);
	QueueFamilyIndices findQueueFamilies_();
	void createImageViews_();
	VkImageView createImageView_(VkImage image, VkFormat format);
    void createRenderPass_();
	void createDescriptorSetLayout_();
	void createGraphicsPipeline_();
	void createFramebuffers_();
	void createCommandPool_();

	void createDescriptorPool_();
	void createDescriptorSets_();
	void createCommandBuffer_();
	void recordCommandBuffer_(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void createSyncObjects_();

	void recreateSwapchain_();
	void cleanupSwapchain_();
	void cleanup_();

	void drawFrame_();

    bool mIsInitialized = false;

    Rect rect;

    struct UniformBufferObject
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
	};

	VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
	uint32_t mSwapchainImageCount = 0;
	VkExtent2D mSwapchainExtent;
	std::vector<VkImage> mSwapchainImages;
	VkFormat mSwapchainImageFormat;
	std::vector<VkImageView> mSwapchainImageViews;
	std::vector<VkFramebuffer> mSwapchainFramebuffers;

	struct SwapchainState
	{
		VkImage image = VK_NULL_HANDLE;
		VkImageView  view = VK_NULL_HANDLE;
		VkAccessFlags2 accessFlags = VK_ACCESS_2_NONE;
		VkImageLayout  layout = VK_IMAGE_LAYOUT_UNDEFINED;
	};
	std::vector<SwapchainState> mSwapchainState;
	uint32_t mCurrentFrame = 0;
	bool mFramebufferResized = false;

#if _DEBUG
	VkDebugUtilsMessengerEXT mDebugMessenger;
#endif

	VkCommandPool mCommandPool = VK_NULL_HANDLE;
	VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> mDescriptorSets;
	uint32_t mCurrentFrameIndex = 0;
	uint32_t mSwapchainImageIndex = 0;

	std::vector<VkCommandBuffer> mCommandBuffer;
	std::vector<VkSemaphore> mImageAvailableSemaphores;
	std::vector<VkSemaphore> mRenderFinishedSemaphores;
	std::vector<VkFence> mInFlightFences;

	VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
    VkPipeline mPipeline = VK_NULL_HANDLE;

    VkRenderPass mRenderPass;
};
//---------------------------------------------------------------------------