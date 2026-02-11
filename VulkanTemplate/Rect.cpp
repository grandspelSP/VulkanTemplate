#include "Rect.h"
#include <stdexcept>

//---------------------------------------------------------------------------
void Rect::initialize(GfxDevice* gfx_device)
{
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
	//vkCmdDraw(commandBuffer, static_cast<uint32_t>(mVertexBufferInfo.vertices.size()), 1, 0, 0);
}
//---------------------------------------------------------------------------
void Rect::destroy(GfxDevice* gfx_device)
{
	auto device = gfx_device->getVkDevice();

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
	// ƒƒ‚ƒŠŠ„‚è“–‚Ä
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

	VkCommandPool command_pool;
	VkCommandPoolCreateInfo command_pool_create_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		.queueFamilyIndex = gfx_device->getGraphicsQueueFamily(),
	};
	vkCreateCommandPool(device, &command_pool_create_info, nullptr, &command_pool);

	VkCommandBufferAllocateInfo allocate_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	VkCommandBuffer command_buffer;
	vkAllocateCommandBuffers(device, &allocate_info, &command_buffer);

	VkCommandBufferBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	vkBeginCommandBuffer(command_buffer, &begin_info);

	VkBufferCopy copy_region{
		.srcOffset = 0,
		.dstOffset = 0,
		.size = size,
	};
	vkCmdCopyBuffer(command_buffer, srcBuffer, dstBuffer, 1, &copy_region);

	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submit_info{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &command_buffer,
	};

	vkQueueSubmit(gfx_device->getGraphicsQueue(), 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(gfx_device->getGraphicsQueue());

	vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}
//---------------------------------------------------------------------------