#pragma once

#include <array>
#include <string>
#include <unordered_map>

#include "camera.hpp"
#include "glm/mat4x4.hpp"
#define NOMINMAX
#include <vulkan/vulkan.hpp>

#include "GLFW/glfw3.h"
#include "mesh.hpp"
#include "vk_mem_alloc.h"
#include "vk_types.hpp"
#include "vk_utils.hpp"

class PipelineBuilder {
 public:
  vk::PipelineShaderStageCreateInfo _shaderStages[2]{};
  vk::PipelineVertexInputStateCreateInfo _vertexInputInfo;
  vk::PipelineInputAssemblyStateCreateInfo _inputAssemblyInfo;
  vk::Viewport _viewport;
  vk::Rect2D _scissor;
  vk::PipelineViewportStateCreateInfo _viewportStateInfo;
  vk::PipelineRasterizationStateCreateInfo _rasterizationInfo;
  // NOTE: Defaults to off. Requires enabling a GPU feature if used
  vk::PipelineMultisampleStateCreateInfo _multisampleInfo;
  vk::PipelineColorBlendAttachmentState _colorBlendAttachmentInfo;
  vk::PipelineColorBlendStateCreateInfo _colorBlendingInfo;
  vk::PipelineDepthStencilStateCreateInfo _depthStencilInfo;
  // Stuff that can be changed during runtime, without recreating the pipeline
  // vk::DynamicState dynamicStates[] = {vk::DynamicState::eViewport,
  // vk::DynamicState::eLineWidth};

  // vk::PipelineDynamicStateCreateInfo dynamicStateInfo{
  // vk::PipelineDynamicStateCreateFlags{}, 2, dynamicStatesInfo};
  //
  vk::UniquePipeline buildPipeline(const vk::Device &, const vk::RenderPass &,
                                   const vk::PipelineLayout &);
};

struct RenderObject {
  Mesh *mesh;
  Material *material;
  glm::mat4 transformMatrix;
};

constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 2;

class VulkanEngine {
 public:
  VulkanEngine();
  ~VulkanEngine();

  void init(GLFWwindow *);
  void run();
  void draw(Camera &, float);
  void drawObjects(vk::CommandBuffer);

 private:
  /*  INIT  */
  void initInstance();
  void initSurface();
  void initPhysicalDevice();
  void initLogicalDevice();
  void initAllocator();
  void initCommandPool();
  void initQueues();
  void initSwapchain();
  void initSwapchainImages();
  void initDepthImage();
  void initRenderPass();
  void initFramebuffers();

  void initDescriptorSetLayout();
  void initMeshPipeline();

  void initUniformBuffers();

  void initTextureImage();
  void initTextureImageSampler();
  void initDescriptorPool();
  void initDescriptorSets();

  void initDrawCommandBuffers();
  void initSyncObjects();

  void initMesh();
  void uploadMesh(const Mesh &);
  void initScene();

 private:
  /*  UTILS  */
  vk::UniqueCommandBuffer beginSingleTimeCommands();
  void endSingleTimeCommands(vk::UniqueCommandBuffer, const vk::Queue &);
  void transitionImageLayout(const vk::Image &, vk::Format, vk::ImageLayout,
                             vk::ImageLayout, uint32_t);
  void copyBufferToImage(const vk::Buffer &, const vk::Image &, uint32_t,
                         uint32_t);
  void generateMipmaps(const vk::Image &, int32_t, int32_t, uint32_t);
  void recreateSwapchain();
  void updateUniformBuffer(Camera &, float);
  Material *createMaterial(vk::UniquePipeline, vk::UniquePipelineLayout,
                           const std::string &);
  Material *getMaterial(const std::string &);
  Mesh *getMesh(const std::string &);

 public:
  GLFWwindow *_window;
  vk::UniqueInstance _instance;
  vk::UniqueSurfaceKHR _surface;
  vk::PhysicalDevice _physicalDevice;
  vk::UniqueDevice _device;

  VmaAllocator _allocator;

  vk::UniqueCommandPool _commandPool;

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

  // TODO: Per object
  vk::UniqueDescriptorSetLayout _descriptorSetLayout;

  std::unordered_map<std::string, Material> _materials;
  std::unordered_map<std::string, Mesh> _meshes;

  // TODO: Material
  AllocatedImage _textureImage;
  vk::UniqueImageView _textureImageView;
  vk::UniqueSampler _textureImageSampler;
  uint32_t _mipLevels;
  vk::UniqueDescriptorPool _descriptorPool;

  // TODO: Differ between global descriptor sets and per object
  // to minimize the amount of binding we need to do

  std::array<FrameData, MAX_FRAMES_IN_FLIGHT> _frames;

  // TODO: Get rid of this?
  std::vector<vk::Fence> _imagesInFlight;
  size_t _currentFrame{};

  std::vector<RenderObject> _renderables;

  bool _framebufferResized = false;
};
