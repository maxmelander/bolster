#pragma once
#include <stdint.h>

#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>

#include "glm/mat4x4.hpp"
#include "vk_mem_alloc.h"

// TODO: Unique Buffer
struct AllocatedBuffer {
  vk::Buffer _buffer;
  VmaAllocation _allocation;
};

// TODO: Unique Image
struct AllocatedImage {
  vk::Image _image;
  VmaAllocation _allocation;
};

// UBOs
struct CameraBufferObject {
  glm::vec3 viewPos;
  uint32_t unused1;  // Pad to vec4
  glm::vec4 frustum;
  glm::mat4 view;
  glm::mat4 proj;
  float zNear;
  float zFar;
  uint32_t unused2;  // Pad to vec4
  uint32_t unused3;  // Pad to vec4
};

struct LightData {
  glm::mat4 spaceMatrix;
  glm::vec4 vector;  // w == 1 point light, w == 0 directional
  glm::vec3 color;
  float strength;
};

struct SceneBufferObject {
  glm::vec4 fogColor;
  glm::vec4 fogDistance;
  glm::vec4 ambientColor;
  LightData lights[3];
};

struct ObjectBufferObject {
  glm::mat4 transform;
  glm::vec4 boundingSphere;
  uint32_t materialIndex;
  uint32_t unused1;  // Pad to vec4
  uint32_t unused2;  // Pad to vec4
  uint32_t unused3;  // Pad to vec4
};

struct MaterialBufferObject {
  uint32_t albedoTexture;
  uint32_t armTexture;
  uint32_t emissiveTexture;
  uint32_t normalTexture;
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

struct Texture {
  AllocatedImage image;
  vk::ImageView imageView;  // TODO: Unique
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
