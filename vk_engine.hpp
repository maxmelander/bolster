#pragma once

#define NOMINMAX
#include <vulkan/vulkan.hpp>

#include "GLFW/glfw3.h"
#include "mesh.hpp"
#include "vk_mem_alloc.h"
#include "vk_types.hpp"
#include "vk_utils.hpp"

class VulkanEngine {
 public:
  VulkanEngine();
  ~VulkanEngine();

  void init();
  void run();
  void draw();

  void initWindow();
  void initInstance();
  void initPhysicalDevice();
  void initLogicalDevice();
  void initAllocator();
  void initQueues();
  void initSwapchain();
  void initSwapchainImages();
  void initDepthImage();
  void initRenderPass();
  void initFramebuffers();
  void initMesh();

  void uploadMesh();

 public:
  const char *_windowTitle = "Bolster";
  vk::Extent2D _windowExtent{800, 600};

  GLFWwindow *_window;
  vk::UniqueInstance _instance;
  vk::UniqueSurfaceKHR _surface;
  vk::PhysicalDevice _physicalDevice;
  vk::UniqueDevice _device;

  VmaAllocator _allocator;

  uint32_t _graphicsQueueFamily;
  uint32_t _presentQueueFamily;
  vk::Queue _graphicsQueue;
  vk::Queue _presentQueue;

  vk::UniqueSwapchainKHR _swapchain;
  vk::Extent2D _swapchainExtent;
  vk::Format _swapchainImageFormat;

  std::vector<vk::Image> _swapchainImages;
  std::vector<vk::UniqueImageView> _swapchainImageViews;

  AllocatedImage _depthImage;
  vk::UniqueImageView _depthImageView;

  vk::UniqueRenderPass _renderPass;
  std::vector<vk::UniqueFramebuffer> _framebuffers;

  Mesh _mesh;
};
