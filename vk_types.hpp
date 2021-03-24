#pragma once
#include <vulkan/vulkan.hpp>

#include "glm/mat4x4.hpp"
#include "vk_mem_alloc.h"

struct AllocatedBuffer {
  vk::UniqueBuffer _buffer;
  VmaAllocation _allocation;
};

struct AllocatedImage {
  vk::UniqueImage _image;
  VmaAllocation _allocation;
};

struct UniformBufferObject {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
};
