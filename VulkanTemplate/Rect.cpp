#include "Rect.h"
#include <stdexcept>
#include <stb_image.h>

//---------------------------------------------------------------------------
void Rect::initialize(GfxDevice* gfx_device)
{
	createTextureImage_(gfx_device);
	createTextureImageView_(gfx_device);
	createTextureSampler_(gfx_device);
	createVertexBuffer_(gfx_device);
	createIndexBuffer_(gfx_device);
}
//---------------------------------------------------------------------------
void Rect::render(VkCommandBuffer commandBuffer)
{
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mVertexBufferInfo.buffer, offsets);
	vkCmdBindIndexBuffer(commandBuffer, mIndexBufferInfo.buffer, 0, VK_INDEX_TYPE_UINT16);

	vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mIndexBufferInfo.indices.size()), 1, 0, 0, 0);
}
//---------------------------------------------------------------------------
void Rect::destroy(GfxDevice* gfx_device)
{
	auto device = gfx_device->getVkDevice();

	vkDestroyImage(device, mTextureInfo.image, nullptr);
	vkFreeMemory(device, mTextureInfo.memory, nullptr);
	vkDestroyImageView(device, mTextureInfo.imageView, nullptr);
	vkDestroySampler(device, mTextureInfo.sampler, nullptr);
	mTextureInfo.image = VK_NULL_HANDLE;
	mTextureInfo.memory = VK_NULL_HANDLE;
	mTextureInfo.imageView = VK_NULL_HANDLE;
	mTextureInfo.sampler = VK_NULL_HANDLE;

	vkDestroyBuffer(device, mIndexBufferInfo.buffer, nullptr);
	vkFreeMemory(device, mIndexBufferInfo.memory, nullptr);
	mIndexBufferInfo.buffer = VK_NULL_HANDLE;
	mIndexBufferInfo.memory = VK_NULL_HANDLE;

	vkDestroyBuffer(device, mVertexBufferInfo.buffer, nullptr);
	vkFreeMemory(device, mVertexBufferInfo.memory, nullptr);
	mVertexBufferInfo.buffer = VK_NULL_HANDLE;
	mVertexBufferInfo.memory = VK_NULL_HANDLE;
}
//---------------------------------------------------------------------------
void Rect::createTextureImage_(GfxDevice* gfx_device)
{
	int tex_width, tex_height, tex_channels;
	stbi_uc* pixels = stbi_load("C:/Users/user/Desktop/Project/VulkanTemplate/texture/ENDFIELD_SHARE_1769687062.png", &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
	VkDeviceSize image_size = tex_width * tex_height * 4;
	if (!pixels) {
		throw std::runtime_error("failed to load texture image!");
	}

	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;
	createBuffer_(
		gfx_device,
		image_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		staging_buffer,
		staging_buffer_memory);

	void* data;
	vkMapMemory(gfx_device->getVkDevice(), staging_buffer_memory, 0, image_size, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(image_size));
	vkUnmapMemory(gfx_device->getVkDevice(), staging_buffer_memory);
	stbi_image_free(pixels);

	createImage_(
		gfx_device,
		static_cast<uint32_t>(tex_width),
		static_cast<uint32_t>(tex_height),
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mTextureInfo.image,
		mTextureInfo.memory);

	copyBufferToImage_(
		gfx_device,
		staging_buffer,
		mTextureInfo.image,
		static_cast<uint32_t>(tex_width),
		static_cast<uint32_t>(tex_height));
}
//---------------------------------------------------------------------------
void Rect::createImage_(GfxDevice* gfx_device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
	VkImageCreateInfo image_info{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = {
			.width = width,
			.height = height,
			.depth = 1,
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = tiling,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	if (vkCreateImage(gfx_device->getVkDevice(), &image_info, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements requirements;
	vkGetImageMemoryRequirements(gfx_device->getVkDevice(), image, &requirements);

	VkMemoryAllocateInfo allocate_info{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = requirements.size,
		.memoryTypeIndex = gfx_device->getMemoryTypeIndex(requirements, properties),
	};

	if (vkAllocateMemory(gfx_device->getVkDevice(), &allocate_info, nullptr, &imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(gfx_device->getVkDevice(), image, imageMemory, 0);
}
//---------------------------------------------------------------------------
void Rect::copyBufferToImage_(GfxDevice* gfx_device, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer command_buffer = beginSingleTimeCommands_(gfx_device);

	VkBufferImageCopy copy_region{
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.imageOffset = { 0, 0, 0 },
		.imageExtent = {
			.width = width,
			.height = height,
			.depth = 1,
		},
	};

	vkCmdCopyBufferToImage(
		command_buffer,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&copy_region);

	endSingleTimeCommands_(gfx_device, command_buffer);
}
//---------------------------------------------------------------------------
void Rect::createTextureImageView_(GfxDevice* gfx_device)
{
	VkImageViewCreateInfo view_info{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = mTextureInfo.image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_SRGB,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	if (vkCreateImageView(gfx_device->getVkDevice(), &view_info, nullptr, &mTextureInfo.imageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture image view!");
	}
}
//---------------------------------------------------------------------------
void Rect::createTextureSampler_(GfxDevice* gfx_device)
{
	VkSamplerCreateInfo sampler_info{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias = 0.0f,
		.anisotropyEnable = VK_TRUE,
		.maxAnisotropy = 256,		// 最終的な色の計算に使用できるテクセルサンプル数の制限を指定します
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		.unnormalizedCoordinates = VK_FALSE,
	};

	vkCreateSampler(gfx_device->getVkDevice(), &sampler_info, nullptr, &mTextureInfo.sampler);
}
//---------------------------------------------------------------------------
void Rect::createVertexBuffer_(GfxDevice* gfx_device)
{
	auto device = gfx_device->getVkDevice();

	VkDeviceSize buffer_size = sizeof(mVertexBufferInfo.vertices[0]) * mVertexBufferInfo.vertices.size();

	createBuffer_(
		gfx_device,
		buffer_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		mVertexBufferInfo.buffer,
		mVertexBufferInfo.memory);

	void* data;
	vkMapMemory(device, mVertexBufferInfo.memory, 0, buffer_size, 0, &data);
	memcpy(data, mVertexBufferInfo.vertices.data(), buffer_size);
	vkUnmapMemory(device, mVertexBufferInfo.memory);
}
//---------------------------------------------------------------------------
void Rect::createIndexBuffer_(GfxDevice* gfx_device)
{
	auto device = gfx_device->getVkDevice();

	VkDeviceSize buffer_size = sizeof(mIndexBufferInfo.indices[0]) * mIndexBufferInfo.indices.size();

	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;
	createBuffer_(
		gfx_device,
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		staging_buffer,
		staging_buffer_memory);

	void* data;
	vkMapMemory(device, staging_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, mIndexBufferInfo.indices.data(), buffer_size);
	vkUnmapMemory(device, staging_buffer_memory);

	createBuffer_(
		gfx_device,
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mIndexBufferInfo.buffer,
		mIndexBufferInfo.memory);

	copyBuffer_(gfx_device, staging_buffer, mIndexBufferInfo.buffer, buffer_size);

	vkDestroyBuffer(device, staging_buffer, nullptr);
	vkFreeMemory(device, staging_buffer_memory, nullptr);
}
//---------------------------------------------------------------------------
void Rect::createBuffer_(GfxDevice* gfx_device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	auto device = gfx_device->getVkDevice();
	VkBufferCreateInfo buffer_create_info{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};
	if (vkCreateBuffer(device, &buffer_create_info, nullptr, &buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create vertex buffer!");
	}
	// メモリ割り当て
	{
		VkMemoryRequirements requirements;
		vkGetBufferMemoryRequirements(device, buffer, &requirements);
		VkMemoryAllocateInfo allocate_info{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = requirements.size,
			.memoryTypeIndex = gfx_device->getMemoryTypeIndex(requirements, properties),
		};
		if (vkAllocateMemory(device, &allocate_info, nullptr, &bufferMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate buffer memory!");
		}
		if (vkBindBufferMemory(device, buffer, bufferMemory, 0) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate buffer memory!");
		}
	}
}
//---------------------------------------------------------------------------
void Rect::copyBuffer_(GfxDevice* gfx_device, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	auto device = gfx_device->getVkDevice();

	VkCommandBuffer command_buffer = beginSingleTimeCommands_(gfx_device);

	VkBufferCopy copy_region{
		.srcOffset = 0,
		.dstOffset = 0,
		.size = size,
	};
	vkCmdCopyBuffer(command_buffer, srcBuffer, dstBuffer, 1, &copy_region);

	endSingleTimeCommands_(gfx_device, command_buffer);
}
//---------------------------------------------------------------------------
VkCommandBuffer Rect::beginSingleTimeCommands_(GfxDevice* gfx_device)
{
	VkCommandBufferAllocateInfo allocate_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = gfx_device->getCommandPool(),
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	VkCommandBuffer command_buffer;
	vkAllocateCommandBuffers(gfx_device->getVkDevice(), &allocate_info, &command_buffer);

	VkCommandBufferBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	vkBeginCommandBuffer(command_buffer, &begin_info);

	return command_buffer;
}
//---------------------------------------------------------------------------
void Rect::endSingleTimeCommands_(GfxDevice* gfx_device, VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submit_info{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffer,
	};

	vkQueueSubmit(gfx_device->getGraphicsQueue(), 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(gfx_device->getGraphicsQueue());

	vkFreeCommandBuffers(gfx_device->getVkDevice(), gfx_device->getCommandPool(), 1, &commandBuffer);
}
//---------------------------------------------------------------------------