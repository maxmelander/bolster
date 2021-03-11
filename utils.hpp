#ifndef __UTILS_H_
#define __UTILS_H_

#include <stdint.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vulkan/vulkan.hpp>

#include "GLFW/glfw3.h"
#include "stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

namespace vk {
namespace utils {

// TODO: Utils needs hpp and cpp file
// If not need to do the implementation macro trick
// like other single header libs
std::array<const char *, 1> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#if !defined(NDEBUG)
const std::array<const char *, 1> validationLayers{
    "VK_LAYER_KHRONOS_validation"};
#endif

inline bool checkDeviceExtensionSupport(vk::PhysicalDevice device) {
  auto availableExtensions = device.enumerateDeviceExtensionProperties();

  std::set<std::string> requiredExtensions(std::begin(deviceExtensions),
                                           std::end(deviceExtensions));

  for (const auto &extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  bool isComplete() {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};

inline QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device,
                                            vk::SurfaceKHR &surface) {
  QueueFamilyIndices indices{};

  std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
      device.getQueueFamilyProperties();

  // Find a queue family that support both graphics and present
  int i = 0;
  for (const auto &qfp : queueFamilyProperties) {
    if (qfp.queueFlags & vk::QueueFlagBits::eGraphics &&
        device.getSurfaceSupportKHR(i, surface)) {
      indices.graphicsFamily = i;
      indices.presentFamily = i;

      return indices;
    }

    i++;
  }

  // Find separate queue families for graphics and present
  i = 0;
  for (const auto &qfp : queueFamilyProperties) {
    if (qfp.queueFlags & vk::QueueFlagBits::eGraphics) {
      indices.graphicsFamily = i;
    }

    if (device.getSurfaceSupportKHR(i, surface)) {
      indices.presentFamily = i;
    }

    if (indices.isComplete()) {
      return indices;
    }
  }

  throw std::runtime_error("Could not find the required queue families");
}

struct SwapchainSupportDetails {
  vk::SurfaceCapabilitiesKHR capabilities;
  std::vector<vk::SurfaceFormatKHR> formats;
  std::vector<vk::PresentModeKHR> presentModes;
};

inline SwapchainSupportDetails querySwapchainSupport(vk::PhysicalDevice device,
                                                     vk::SurfaceKHR surface) {
  SwapchainSupportDetails details{device.getSurfaceCapabilitiesKHR(surface),
                                  device.getSurfaceFormatsKHR(surface),
                                  device.getSurfacePresentModesKHR(surface)};

  return details;
}

inline vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
        availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
      return availableFormat;
    }
  }

  return availableFormats[0];
}

inline vk::PresentModeKHR chooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR> &availablePresentModes) {
  for (const auto &availablePresentMode : availablePresentModes) {
    if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
      return availablePresentMode;
    }
  }

  return vk::PresentModeKHR::eFifo;
}

inline vk::Extent2D chooseSwapExtent(
    const vk::SurfaceCapabilitiesKHR &capabilities, GLFWwindow *window) {
  if (capabilities.currentExtent.width != UINT32_MAX) {
    return capabilities.currentExtent;
  } else {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    vk::Extent2D actualExtent{static_cast<uint32_t>(width),
                              static_cast<uint32_t>(height)};

    actualExtent.width =
        max(capabilities.minImageExtent.width,
            min(capabilities.maxImageExtent.width, actualExtent.width));
    actualExtent.height =
        max(capabilities.minImageExtent.height,
            min(capabilities.maxImageExtent.height, actualExtent.height));

    return actualExtent;
  }
}

// NOTE: Can we use a std::File type here that automatically
// closes the file at the end of the scope?
static std::vector<char> readFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file.");
  }

  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);

  file.close();

  return buffer;
}

inline vk::UniqueShaderModule createUniqueShaderModule(
    const vk::UniqueDevice &device, const std::vector<char> &code) {
  vk::ShaderModuleCreateInfo createInfo{
      vk::ShaderModuleCreateFlags{}, code.size(),
      reinterpret_cast<const uint32_t *>(code.data())};

  return device->createShaderModuleUnique(createInfo);
}

inline uint32_t findMemoryType(const vk::PhysicalDevice &device,
                               uint32_t typeFilter,
                               vk::MemoryPropertyFlags properties) {
  auto memoryProperties = device.getMemoryProperties();

  for (uint32_t i{}; i < memoryProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) &&
        (memoryProperties.memoryTypes[i].propertyFlags & properties) ==
            properties) {
      return i;
    }
  }

  throw std::runtime_error("Failed to find a suitable memory type.");
}

inline vk::UniqueCommandBuffer beginSingleTimeCommands(
    const vk::Device &device, const vk::CommandPool &commandPool) {
  vk::CommandBufferAllocateInfo allocInfo{commandPool,
                                          vk::CommandBufferLevel::ePrimary, 1};
  auto commandBuffers = device.allocateCommandBuffersUnique(allocInfo);

  vk::CommandBufferBeginInfo beginInfo{
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  commandBuffers[0]->begin(beginInfo);

  return std::move(commandBuffers[0]);
}

// NOTE: We want to move the commandbuffer into this function
// so that it will be freed at the end of this scope
inline void endSingleTimeCommands(vk::UniqueCommandBuffer commandBuffer,
                                  const vk::Queue &queue) {
  commandBuffer->end();

  vk::SubmitInfo submitInfo{{}, {}, {}, 1, &commandBuffer.get()};
  queue.submit(std::array{submitInfo});
  queue.waitIdle();
}

inline void copyBuffer(const vk::Device &device,
                       const vk::CommandPool &commandPool,
                       const vk::Queue &queue, const vk::Buffer &srcBuffer,
                       const vk::Buffer &dstBuffer,
                       const vk::DeviceSize &size) {
  auto commandBuffer = beginSingleTimeCommands(device, commandPool);

  vk::BufferCopy copyRegion{0, 0, size};
  commandBuffer->copyBuffer(srcBuffer, dstBuffer, copyRegion);

  endSingleTimeCommands(std::move(commandBuffer), queue);
}

inline void copyBufferToImage(const vk::Device &device,
                              const vk::CommandPool &commandPool,
                              const vk::Queue &queue, const vk::Buffer &buffer,
                              const vk::Image &image, uint32_t width,
                              uint32_t height) {
  auto commandBuffer = beginSingleTimeCommands(device, commandPool);

  vk::BufferImageCopy copyRegion{
      0,
      0,
      0,
      vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
      vk::Offset3D{0, 0, 0},
      vk::Extent3D{width, height, 1}};

  commandBuffer->copyBufferToImage(
      buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &copyRegion);

  endSingleTimeCommands(std::move(commandBuffer), queue);
}

inline void transitionImageLayout(const vk::Device &device,
                                  const vk::CommandPool &commandPool,
                                  const vk::Queue queue, const vk::Image &image,
                                  vk::Format format, vk::ImageLayout oldLayout,
                                  vk::ImageLayout newLayout) {
  auto commandBuffer = beginSingleTimeCommands(device, commandPool);

  vk::ImageMemoryBarrier barrier{
      {},
      {},
      oldLayout,
      newLayout,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      image,
      vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

  vk::PipelineStageFlags srcStage, dstStage;

  if (oldLayout == vk::ImageLayout::eUndefined &&
      newLayout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
    dstStage = vk::PipelineStageFlagBits::eTransfer;
  } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
             newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    srcStage = vk::PipelineStageFlagBits::eTransfer;
    dstStage = vk::PipelineStageFlagBits::eFragmentShader;
  } else {
    throw std::runtime_error("Unsupported layout transition.");
  }

  commandBuffer->pipelineBarrier(srcStage, dstStage, {}, {}, {}, barrier);

  endSingleTimeCommands(std::move(commandBuffer), queue);
}

// NOTE: Does passing by reference or value matter at all
// when inlining the function anyway?
inline vk::UniqueImageView createImageView(
    const vk::Device &device, const vk::Image &image, const vk::Format &format,
    const vk::ImageAspectFlags &aspectFlags) {
  vk::ImageViewCreateInfo createInfo{
      vk::ImageViewCreateFlags{},
      image,
      vk::ImageViewType::e2D,
      format,
      vk::ComponentMapping{},
      vk::ImageSubresourceRange{aspectFlags, 0, 1, 0, 1}};

  return device.createImageViewUnique(createInfo);
}

inline vk::Format findSupportedFormat(const vk::PhysicalDevice &physicalDevice,
                                      const std::vector<vk::Format> &candidates,
                                      vk::ImageTiling tiling,
                                      vk::FormatFeatureFlags features) {
  for (vk::Format format : candidates) {
    auto props = physicalDevice.getFormatProperties(format);
    if (tiling == vk::ImageTiling::eLinear &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    } else if (tiling == vk::ImageTiling::eOptimal &&
               (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  throw std::runtime_error("Failed to find supported format");
}

inline vk::Format findDepthFormat(const vk::PhysicalDevice &physicalDevice) {
  return findSupportedFormat(
      physicalDevice,
      {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
       vk::Format::eD24UnormS8Uint},
      vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

inline bool hasStencilComponent(vk::Format format) {
  return format == vk::Format::eD32SfloatS8Uint ||
         format == vk::Format::eD24UnormS8Uint;
}

}  // namespace utils
}  // namespace vk

#endif  // __UTILS_H_
