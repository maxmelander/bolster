#pragma once
#include <stdint.h>

#include <vulkan/vulkan.hpp>

#include "glm/mat4x4.hpp"
#include "vk_mem_alloc.h"

struct AllocatedBuffer {
  vk::Buffer _buffer;
  VmaAllocation _allocation;
};

struct AllocatedImage {
  vk::Image _image;
  VmaAllocation _allocation;
};

// UBOs
struct CameraBufferObject {
  float zNear;
  float zFar;
  uint32_t unused1;
  uint32_t unused2;  // Pad to vec4
  glm::vec4 frustum;
  glm::mat4 view;
  glm::mat4 proj;
};

struct SceneBufferObject {
  glm::vec4 fogColor;
  glm::vec4 fogDistance;
  glm::vec4 ambientColor;
};

struct ObjectBufferObject {
  uint32_t materialIndex;
  uint32_t vertexOffset;
  uint32_t indexOffset;
  uint32_t unused1;  // Pad to vec4
  glm::vec4 boundingSphere;
  glm::mat4 transform;
};

struct MaterialBufferObject {
  uint32_t albedoTexture;
  uint32_t normalTexture;
  uint32_t roughnessTexture;
  uint32_t unused1;  // Pad to vec4
};

struct DrawIndexedIndirectCommandBufferObject {
  uint32_t indexCount;
  uint32_t instanceCount;
  uint32_t firstIndex;
  int32_t vertexOffset;
  uint32_t firstInstance;

  uint32_t unused0;  // Pad to vec4
  uint32_t unused1;  // Pad to vec4
  uint32_t unused2;  // Pad to vec4
};

// Material stuff
struct Material {
  uint32_t albedoTexture;
  uint32_t normalTexture;
  uint32_t roughnessTexture;
  uint32_t unused1;  // Pad to vec4
};

struct Texture {
  AllocatedImage image;
  vk::UniqueImageView imageView;
  uint32_t mipLevels;
};

struct MeshPushConstants {
  glm::mat4 model;
};

struct FrameData {
  vk::UniqueCommandBuffer _commandBuffer;
  vk::UniqueSemaphore _imageAvailableSemaphore;
  vk::UniqueSemaphore _renderFinishedSemaphore;
  vk::UniqueFence _inFlightFence;
  vk::UniqueDescriptorSet _globalDescriptorSet;
  vk::UniqueDescriptorSet _objectDescriptorSet;
  vk::UniqueDescriptorSet _computeDescriptorSet;
  AllocatedBuffer _cameraBuffer;
  AllocatedBuffer _objectStorageBuffer;
  AllocatedBuffer _transformStorageBuffer;
  AllocatedBuffer _materialStorageBuffer;
  AllocatedBuffer _indirectCommandBuffer;
};
