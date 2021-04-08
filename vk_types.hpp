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

struct CameraBufferObject {
  glm::mat4 view;
  glm::mat4 proj;
};

struct Material {
  vk::UniquePipeline pipeline;
  vk::UniquePipelineLayout pipelineLayout;
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
  AllocatedBuffer _cameraBuffer;
};
