#pragma once
#include <memory>
#include <vector>

#define VK_USE_PLATFORM_WIN32_KHR

#include <Volk/volk.h>
#include <vulkan/vulkan.h>

//---------------------------------------------------------------------------
class GfxDevice;
std::unique_ptr<GfxDevice>& getGfxDevice();

//---------------------------------------------------------------------------
class GfxDevice
{
public:
	struct DeviceInitParams
	{
		void* glfwWindow;
	};

	struct SwapchainInfo
	{
		int32_t width, height;
	};

public:
	/*
	 * グラフィックスデバイスの初期化
	 */
	void Initialize(const DeviceInitParams& initParams);
	/*
	 * グラフィックスデバイスの終了
	 */
	void Shutdown();

	/*
	 * ゲッター群
	 */
	inline VkInstance getVkInstance() const { return mVkInstance; }
	inline VkPhysicalDevice getVkPhysicalDevice() const { return mVkPhysicalDevice; }
	inline VkDevice getVkDevice() const { return mVkDevice; }
	inline uint32_t getFrameIndex() const { return mCurrentFrameIndex; }
	inline VkCommandBuffer getCurrentCommandBuffer() {
		return mFrameCommandInfos[mCurrentFrameIndex].buffer;
	}
	inline SwapchainInfo getSwapchainInfo() const { return mSwapchainInfo; }
	inline VkImage getCurrentSwapchainImage() {
		return mSwapchainState[mSwapchainImageIndex].image;
	}
	inline VkImageView getCurrentSwapchainImageView() {
		return mSwapchainState[mSwapchainImageIndex].view;
	}
	inline VkSurfaceFormatKHR getSwapchainFormat() const {
		return mVkSurfaceFormat;
	}
	inline uint32_t getSwapchainImageCount() const {
		return uint32_t(mSwapchainState.size());
	}
	inline VkImageView getSwapchainImageView(int i) const {
		return mSwapchainState[i].view;
	}
	inline uint32_t getSwapchainImageIndex() const { return mSwapchainImageIndex; }
	inline uint32_t getGraphicsQueueFamily() const {
		return mGraphicsQueueIndex;
	}
	inline VkQueue getGraphicsQueue() const {
		return mVkGraphicsQueue;
	}
	inline VkDescriptorPool getDescriptorPool() const {
		return mDescriptorPool;
	}
	inline uint32_t getMemoryTypeIndex(VkMemoryRequirements reqs, VkMemoryPropertyFlags memoryPropFlags) {
		auto requestBits = reqs.memoryTypeBits;
		for (uint32_t i = 0; i < mMemoryProperties.memoryTypeCount; ++i)
		{
			if (requestBits & 1)
			{
				// 要求されたメモリプロパティと一致するものを見つける
				const auto types = mMemoryProperties.memoryTypes[i];
				if ((types.propertyFlags & memoryPropFlags) == memoryPropFlags)
				{
					return i;
				}
			}
			requestBits >>= 1;
		}
		return UINT32_MAX;
	}

	/*
	 * 描画フレーム開始処理
	 */
	void newFrame();
	/*
	 * 描画コマンドの実行と描画結果の画面表示要求
	 */
	void submit();
	/*
	 * GPUのアイドル状態まで待機
	 */
	void waitForIdle();
	/*
	 * スワップチェインイメージのレイアウト変更
	 */
	void transitionLayoutSwapchainImage(VkCommandBuffer commandBuffer, VkImageLayout newLayout, VkAccessFlags2 newAccessFlag);
	/*
	 * スワップチェインの再作成
	 */
	void recreateSwapchain(SwapchainInfo info);
	/*
	 * シェーダ構築
	 */
	VkShaderModule createShaderModule(const void* code, size_t length);
	/*
	 * シェーダ破棄
	 */
	void destroyShaderModule(VkShaderModule shaderModule);

	inline bool isSupportVulkan13()
	{
		VkPhysicalDeviceProperties p{};
		vkGetPhysicalDeviceProperties(mVkPhysicalDevice, &p);

		if (p.apiVersion < VK_API_VERSION_1_3)
		{
			return false;
		}
		return true;
	}

	void setObjectName(uint64_t handle, const char* name, VkObjectType type);

public:
	static constexpr int sInflightFrames = 2;

private:
	/*
	 * 各種初期化
	 */
	void initVkInstance_();
	void initPhysicalDevice_();
	void initVkDevice_();
	void initWindowSurface_(const DeviceInitParams& initParams);
	void initCommandPool_();
	void initSemaphores_();
	void initCommandBuffers_();
	void initDescriptorPool_();

	/*
	 * 各種破棄
	 */
	void destroyVkInstance_();
	void destroyVkDevice_();
	void destroyWindowSurface_();
	void destroySwapchain_();
	void destroyCommandPool_();
	void destroySemaphores_();
	void destroyCommandBuffers_();
	void destroyDescriptorPool_();

private:
	VkInstance mVkInstance = VK_NULL_HANDLE;
	VkPhysicalDevice mVkPhysicalDevice = VK_NULL_HANDLE;
	VkDevice mVkDevice = VK_NULL_HANDLE;
	VkPhysicalDeviceMemoryProperties mMemoryProperties;

	VkSurfaceKHR mVkSurface = VK_NULL_HANDLE;
	VkSurfaceFormatKHR mVkSurfaceFormat{};

	SwapchainInfo mSwapchainInfo;
	struct SwapchainState
	{
		VkImage image = VK_NULL_HANDLE;
		VkImageView  view = VK_NULL_HANDLE;
		VkAccessFlags2 accessFlags = VK_ACCESS_2_NONE;
		VkImageLayout  layout = VK_IMAGE_LAYOUT_UNDEFINED;
	};
	VkSwapchainKHR mVkSwapchain = VK_NULL_HANDLE;
	std::vector<SwapchainState> mSwapchainState;

	// キューインデックス.
	uint32_t mGraphicsQueueIndex;
	VkQueue  mVkGraphicsQueue;

#if _DEBUG
	VkDebugUtilsMessengerEXT mDebugMessenger;
#endif

	VkCommandPool mCommandPool = VK_NULL_HANDLE;
	VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
	uint32_t mCurrentFrameIndex = 0;
	uint32_t mSwapchainImageIndex = 0;

	struct FrameInfo
	{
		VkFence fence = VK_NULL_HANDLE;
		VkCommandBuffer buffer = VK_NULL_HANDLE;

		// 描画完了・Present完了待機のためのセマフォ
		VkSemaphore render_completed = VK_NULL_HANDLE;
		VkSemaphore present_completed = VK_NULL_HANDLE;
	};

	FrameInfo  mFrameCommandInfos[sInflightFrames];
};
//---------------------------------------------------------------------------