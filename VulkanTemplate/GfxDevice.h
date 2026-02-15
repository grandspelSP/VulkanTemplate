#pragma once
#include <memory>
#include <vector>
#include <optional>

#define VK_USE_PLATFORM_WIN32_KHR

#include <Volk/volk.h>
#include <vulkan/vulkan.h>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

//---------------------------------------------------------------------------
class GfxDevice;
std::unique_ptr<GfxDevice>& getGfxDevice();

//---------------------------------------------------------------------------
const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};
//---------------------------------------------------------------------------
const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
//---------------------------------------------------------------------------
struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};
//---------------------------------------------------------------------------
class GfxDevice
{
public:
	struct DeviceInitParams
	{
		void* glfwWindow;
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
	inline VkPhysicalDevice getVkPhysicalDevice() const { return mPhysicalDevice; }
	inline VkDevice getVkDevice() const { return mVkDevice; }
	inline VkSurfaceKHR getWindowSurface() const { return mSurface; }
	inline VkSurfaceFormatKHR getSwapchainFormat() const {
		return mSurfaceFormat;
	}
	inline VkQueue getGraphicsQueue() const { return mGraphicsQueue; }
	inline VkQueue getPresentQueue() const { return mPresentQueue; }
	inline uint32_t getGraphicsQueueFamily() const{ return mGraphicsQueueFamily; }
	inline uint32_t getPresentQueueFamily() const{ return mPresentQueueFamily; }

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
	 * GPUのアイドル状態まで待機
	 */
	void waitForIdle();

	inline bool isSupportVulkan13()
	{
		VkPhysicalDeviceProperties p{};
		vkGetPhysicalDeviceProperties(mPhysicalDevice, &p);

		if (p.apiVersion < VK_API_VERSION_1_3)
		{
			return false;
		}
		return true;
	}

	QueueFamilyIndices findQueueFamilies_();
	void setObjectName(uint64_t handle, const char* name, VkObjectType type);

private:
	/*
	 * 各種初期化
	 */
	void initVkInstance_();
	void initPhysicalDevice_();
	void initVkDevice_();
	void initWindowSurface_(const DeviceInitParams& initParams);

	/*
	 * 各種破棄
	 */
	void destroyVkInstance_();
	void destroyVkDevice_();
	void destroyWindowSurface_();

private:
	VkInstance mVkInstance = VK_NULL_HANDLE;
	VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
	VkDevice mVkDevice = VK_NULL_HANDLE;
	VkPhysicalDeviceMemoryProperties mMemoryProperties;

	VkSurfaceKHR mSurface = VK_NULL_HANDLE;
	VkSurfaceFormatKHR mSurfaceFormat{};

	// キューインデックス
	VkQueue  mGraphicsQueue;
	VkQueue  mPresentQueue;
	uint32_t mGraphicsQueueFamily;
	uint32_t mPresentQueueFamily;
};
//---------------------------------------------------------------------------