#ifndef __UTILS_H_
#define __UTILS_H_

#include <stdint.h>

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

namespace vk {
namespace utils {

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

inline void copyBuffer(const vk::Device &device,
                       const vk::CommandPool &commandPool,
                       const vk::Queue &queue, const vk::Buffer &srcBuffer,
                       const vk::Buffer &dstBuffer,
                       const vk::DeviceSize &size) {
  vk::CommandBufferAllocateInfo commandInfo{};
  commandInfo.level = vk::CommandBufferLevel::ePrimary;
  commandInfo.commandPool = commandPool;
  commandInfo.commandBufferCount = 1;
  auto commandBuffer = device.allocateCommandBuffersUnique(commandInfo);

  vk::CommandBufferBeginInfo beginInfo{};
  beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
  commandBuffer[0]->begin(beginInfo);
  vk::BufferCopy copyRegion{0, 0, size};
  commandBuffer[0]->copyBuffer(srcBuffer, dstBuffer, copyRegion);
  commandBuffer[0]->end();

  vk::SubmitInfo submitInfo{};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer[0].get();
  queue.submit(std::array{submitInfo});
  queue.waitIdle();
}
}  // namespace utils
}  // namespace vk

#endif  // __UTILS_H_
