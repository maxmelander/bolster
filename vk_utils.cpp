#include <vulkan/vulkan_core.h>
#define NOMINMAX

#include <algorithm>
#include <set>

#include "vk_utils.hpp"

namespace vkutils {
QueueFamilyIndices findQueueFamilies(const vk::PhysicalDevice &device,
                                     const vk::SurfaceKHR &surface) {
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

  abort();
}

SwapchainSupportDetails querySwapchainSupport(const vk::PhysicalDevice &device,
                                              const vk::SurfaceKHR &surface) {
  SwapchainSupportDetails details{device.getSurfaceCapabilitiesKHR(surface),
                                  device.getSurfaceFormatsKHR(surface),
                                  device.getSurfacePresentModesKHR(surface)};

  return details;
}

bool checkDeviceExtensionSupport(const vk::PhysicalDevice &device) {
  auto availableExtensions = device.enumerateDeviceExtensionProperties();

  std::set<std::string> requiredExtensions(std::begin(deviceExtensions),
                                           std::end(deviceExtensions));

  for (const auto &extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
        availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
      return availableFormat;
    }
  }

  return availableFormats[0];
}

vk::PresentModeKHR chooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR> &availablePresentModes) {
  for (const auto &availablePresentMode : availablePresentModes) {
    if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
      return availablePresentMode;
    }
  }

  return vk::PresentModeKHR::eFifo;
}

vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities,
                              GLFWwindow *window) {
  if (capabilities.currentExtent.width != UINT32_MAX) {
    return capabilities.currentExtent;
  } else {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    vk::Extent2D actualExtent{static_cast<uint32_t>(width),
                              static_cast<uint32_t>(height)};

    actualExtent.width = std::max(
        capabilities.minImageExtent.width,
        std::min(capabilities.maxImageExtent.width, actualExtent.width));
    actualExtent.height = std::max(
        capabilities.minImageExtent.height,
        std::min(capabilities.maxImageExtent.height, actualExtent.height));

    return actualExtent;
  }
}

vk::Format _findSupportedFormat(const vk::PhysicalDevice &physicalDevice,
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

  abort();
}

vk::Format findDepthFormat(const vk::PhysicalDevice &physicalDevice) {
  return _findSupportedFormat(
      physicalDevice,
      {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
       vk::Format::eD24UnormS8Uint},
      vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

void allocateImage(const VmaAllocator &allocator,
                   const vk::ImageCreateInfo &createInfo,
                   VmaMemoryUsage memoryUsage, AllocatedImage &toImage) {
  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = memoryUsage;
  VkImageCreateInfo ici = static_cast<VkImageCreateInfo>(createInfo);
  VkImage tempImage;
  vmaCreateImage(allocator, &ici, &allocInfo, &tempImage, &toImage._allocation,
                 nullptr);

  toImage._image = vk::UniqueImage{tempImage};
}

void allocateBuffer(const VmaAllocator &allocator, size_t size,
                    vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage,
                    vk::SharingMode sharingMode, AllocatedBuffer &toBuffer) {
  vk::BufferCreateInfo bufferInfo{{}, size, usage, sharingMode, {}, {}};
  VkBufferCreateInfo bi = static_cast<VkBufferCreateInfo>(bufferInfo);

  VmaAllocationCreateInfo vmaAllocInfo = {};
  vmaAllocInfo.usage = memoryUsage;

  VkBuffer tempBuffer;
  vmaCreateBuffer(allocator, &bi, &vmaAllocInfo, &tempBuffer,
                  &toBuffer._allocation, nullptr);
}

}  // namespace vkutils
