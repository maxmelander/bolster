#include <vulkan/vulkan_core.h>

#include <vulkan/vulkan.hpp>
#define NOMINMAX

#include <algorithm>
#include <fstream>
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

  toImage._image = vk::Image{tempImage};
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

  toBuffer._buffer = vk::Buffer{tempBuffer};
}

std::vector<char> readFile(const std::string &filename) {
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

vk::UniqueShaderModule createUniqueShaderModule(const vk::Device &device,
                                                const std::vector<char> &code) {
  vk::ShaderModuleCreateInfo createInfo{
      vk::ShaderModuleCreateFlags{}, code.size(),
      reinterpret_cast<const uint32_t *>(code.data())};

  return device.createShaderModuleUnique(createInfo);
}

uint32_t getMipLevels(int texWidth, int texHeight) {
  return static_cast<uint32_t>(
             std::floor(std::log2(std::max(texWidth, texHeight)))) +
         1;
}

// Jack Ritter. An Efficient Bounding Sphere. 1990
void computeBoundingSphere(glm::vec4 &result, const glm::vec3 points[],
                           size_t count) {
  assert(count > 0);

  // find extremum points along all 3 axes; for each axis we get a pair of
  // points with min/max coordinates
  size_t pmin[3] = {0, 0, 0};
  size_t pmax[3] = {0, 0, 0};

  for (size_t i = 0; i < count; ++i) {
    glm::vec3 p = points[i];

    for (int axis = 0; axis < 3; ++axis) {
      pmin[axis] = (p[axis] < points[pmin[axis]][axis]) ? i : pmin[axis];
      pmax[axis] = (p[axis] > points[pmax[axis]][axis]) ? i : pmax[axis];
    }
  }

  // find the pair of points with largest distance
  float paxisd2 = 0;
  int paxis = 0;

  for (int axis = 0; axis < 3; ++axis) {
    glm::vec3 p1 = points[pmin[axis]];
    glm::vec3 p2 = points[pmax[axis]];

    float d2 = (p2[0] - p1[0]) * (p2[0] - p1[0]) +
               (p2[1] - p1[1]) * (p2[1] - p1[1]) +
               (p2[2] - p1[2]) * (p2[2] - p1[2]);

    if (d2 > paxisd2) {
      paxisd2 = d2;
      paxis = axis;
    }
  }

  // use the longest segment as the initial sphere diameter
  glm::vec3 p1 = points[pmin[paxis]];
  glm::vec3 p2 = points[pmax[paxis]];

  float center[3] = {(p1[0] + p2[0]) / 2, (p1[1] + p2[1]) / 2,
                     (p1[2] + p2[2]) / 2};
  float radius = sqrtf(paxisd2) / 2;

  // iteratively adjust the sphere up until all points fit
  for (size_t i = 0; i < count; ++i) {
    glm::vec3 p = points[i];
    float d2 = (p[0] - center[0]) * (p[0] - center[0]) +
               (p[1] - center[1]) * (p[1] - center[1]) +
               (p[2] - center[2]) * (p[2] - center[2]);

    if (d2 > radius * radius) {
      float d = sqrtf(d2);
      assert(d > 0);

      float k = 0.5f + (radius / d) / 2;

      center[0] = center[0] * k + p[0] * (1 - k);
      center[1] = center[1] * k + p[1] * (1 - k);
      center[2] = center[2] * k + p[2] * (1 - k);
      radius = (radius + d) / 2;
    }
  }

  result[0] = center[0];
  result[1] = center[1];
  result[2] = center[2];
  result[3] = radius;
}

}  // namespace vkutils
