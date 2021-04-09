#pragma once
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
  glm::mat4 view;
  glm::mat4 proj;
};

struct SceneBufferObject {
  glm::vec4 fogColor;
  glm::vec4 fogDistance;
  glm::vec4 ambientColor;
};

struct ObjectBufferObject {
  glm::mat4 model;
};

// Material stuff
struct Material {
  vk::UniqueDescriptorSet textureDescriptorSet;
  vk::Pipeline pipeline;
  vk::PipelineLayout pipelineLayout;
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
  AllocatedBuffer _cameraBuffer;
  AllocatedBuffer _objectStorageBuffer;
};
