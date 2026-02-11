#pragma once

#include <vector>
#include "glm/glm.hpp"
#include <Volk/volk.h>
#include "GfxDevice.h"

//---------------------------------------------------------------------------
class Rect
{
public:
    Rect() {}
    ~Rect() {}

    void initialize(GfxDevice* gfx_device);

	void render(VkCommandBuffer commandBuffer);

	void destroy(GfxDevice* gfx_device);

    struct Vertex
    {
        glm::vec2 position;
        glm::vec3 color;
        glm::vec2 texcoord;
    };

private:
    void createVertexBuffer_(GfxDevice* gfx_device);
    void createIndexBuffer_(GfxDevice* gfx_device);
	void createBuffer_(GfxDevice* gfx_device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void copyBuffer_(GfxDevice* gfx_device, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    struct VertexBufferInfo
    {
        const std::vector<Vertex> vertices = {
        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        };
        VkBuffer buffer;
        VkDeviceMemory memory;
    } mVertexBufferInfo;

    struct IndexBufferInfo
    {
        const std::vector<uint16_t> indices = { 0, 1, 2, 2, 3, 0 };
        VkBuffer buffer;
        VkDeviceMemory memory;
    } mIndexBufferInfo;
};
//---------------------------------------------------------------------------