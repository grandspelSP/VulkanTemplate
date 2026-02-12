#pragma once

#include <vector>
#include <array>
#include "glm/glm.hpp"
#include <Volk/volk.h>
#include "GfxDevice.h"

//---------------------------------------------------------------------------
struct Vertex
{
    glm::vec2 position;
    glm::vec3 color;
    glm::vec2 texcoord;

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 1;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, position);
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texcoord);
        return attributeDescriptions;
    }
};
//---------------------------------------------------------------------------
class Rect
{
public:
    Rect() {}
    ~Rect() {}

    void initialize(GfxDevice* gfx_device);

	void render(VkCommandBuffer commandBuffer);

	void destroy(GfxDevice* gfx_device);

private:
    void createVertexBuffer_(GfxDevice* gfx_device);
    void createIndexBuffer_(GfxDevice* gfx_device);
	void createBuffer_(GfxDevice* gfx_device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void copyBuffer_(GfxDevice* gfx_device, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    // âÊëúÇÃì«Ç›çûÇ›
	void createTextureImage_(GfxDevice* gfx_device);
	void createImage_(GfxDevice* gfx_device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	void copyBufferToImage_(GfxDevice* gfx_device, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void createTextureImageView_(GfxDevice* gfx_device);
	void createTextureSampler_(GfxDevice* gfx_device);

	VkCommandBuffer beginSingleTimeCommands_(GfxDevice* gfx_device);
	void endSingleTimeCommands_(GfxDevice* gfx_device, VkCommandBuffer commandBuffer);

    struct VertexBufferInfo
    {
        const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
        };
        VkBuffer buffer = nullptr;
        VkDeviceMemory memory = nullptr;
    } mVertexBufferInfo;

    struct IndexBufferInfo
    {
        const std::vector<uint16_t> indices = { 0, 1, 2, 2, 3, 0 };
        VkBuffer buffer = nullptr;
        VkDeviceMemory memory = nullptr;
    } mIndexBufferInfo;

    struct TextureInfo
    {
        VkImage image = nullptr;
        VkDeviceMemory memory = nullptr;
        VkImageView imageView = nullptr;
        VkSampler sampler = nullptr;
	} mTextureInfo;
};
//---------------------------------------------------------------------------