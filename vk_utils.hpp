#pragma once

#include <stdint.h>

#include <optional>
#include <vulkan/vulkan.hpp>

#include "GLFW/glfw3.h"
#include "vk_mem_alloc.h"
#include "vk_types.hpp"

namespace vkutils {

constexpr std::array<const char *, 1> deviceExtensions{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#if !defined(NDEBUG)
constexpr std::array<const char *, 1> validationLayers{
    "VK_LAYER_KHRONOS_validation"};
#endif

struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  inline bool isComplete() {
    return graphicsFamily.has_value() && presentFamily.has_value();
  };
};

QueueFamilyIndices findQueueFamilies(const vk::PhysicalDevice &,
                                     const vk::SurfaceKHR &);

struct SwapchainSupportDetails {
  vk::SurfaceCapabilitiesKHR capabilities;
  std::vector<vk::SurfaceFormatKHR> formats;
  std::vector<vk::PresentModeKHR> presentModes;
};

SwapchainSupportDetails querySwapchainSupport(const vk::PhysicalDevice &,
                                              const vk::SurfaceKHR &);

bool checkDeviceExtensionSupport(const vk::PhysicalDevice &);

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR> &);

vk::PresentModeKHR chooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR> &);

vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &, GLFWwindow *);

vk::Format findDepthFormat(const vk::PhysicalDevice &);

void allocateImage(const VmaAllocator &, const vk::ImageCreateInfo &,
                   VmaMemoryUsage, AllocatedImage &);

void allocateBuffer(const VmaAllocator &, size_t, vk::BufferUsageFlags,
                    VmaMemoryUsage, vk::SharingMode, AllocatedBuffer &);

// TODO: Write this nicer without std::string and stuff?
std::vector<char> readFile(const std::string &);

vk::UniqueShaderModule createUniqueShaderModule(const vk::Device &,
                                                const std::vector<char> &code);

uint32_t getMipLevels(int, int);

}  // namespace vkutils
