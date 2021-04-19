#pragma once

#include <vulkan/vulkan_core.h>

#include <vulkan/vulkan.hpp>

#include "vk_types.hpp"

namespace vkInit {
VkImageCreateInfo getRawImageCreateInfo(const vk::ImageCreateInfo &);
}
