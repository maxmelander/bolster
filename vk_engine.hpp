#pragma once

#include <array>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

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
  uint32_t materialIndex;
  glm::mat4 transformMatrix;
};

constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 2;
constexpr unsigned int MAX_DRAW_COMMANDS = 10000;
constexpr unsigned int MAX_OBJECTS = 10000;

class VulkanEngine {
 public:
  VulkanEngine();
  ~VulkanEngine();

  void init(GLFWwindow *);
  void run();
  void draw(Camera &, double, float);
  void drawObjects(vk::CommandBuffer, double);

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
  void initPipelines();
  void initMaterials();

  void initUniformBuffers();

  void initTextures();
  void initTextureImageSampler();
  void initTextureDescriptorSet();

  void initDescriptorPool();
  void initDescriptorSets();

  void initMesh();
  void initMeshBuffers();
  void uploadMeshes(const std::vector<Vertex> &, const std::vector<uint32_t> &);
  void initScene();

  void initDrawCommandBuffers();
  void initSyncObjects();

 private:
  /*  UTILS  */
  void immediateSubmit(std::function<void(vk::CommandBuffer cmd)> &&);

  void transitionImageLayout(const vk::Image &, vk::Format, vk::ImageLayout,
                             vk::ImageLayout, uint32_t);
  void copyBufferToImage(const vk::Buffer &, const vk::Image &, uint32_t,
                         uint32_t);
  void generateMipmaps(const vk::Image &, int32_t, int32_t, uint32_t);
  void recreateSwapchain();
  void updateCameraBuffer(Camera &, float);
  Material *createMaterial(uint32_t, const std::string &);
  Material *getMaterial(const std::string &);
  Mesh *getMesh(const std::string &);
  size_t padUniformBufferSize(size_t);
  void loadTextureFromFile(const std::string &, Texture &);
  MeshData loadMeshFromFile(const std::string &);

 public:
  GLFWwindow *_window;
  vk::UniqueInstance _instance;
  vk::UniqueSurfaceKHR _surface;
  vk::PhysicalDevice _physicalDevice;
  vk::PhysicalDeviceProperties _deviceProperties;
  vk::UniqueDevice _device;

  VmaAllocator _allocator;

  vk::UniqueCommandPool _commandPool;
  vk::UniqueCommandPool _immediateCommandPool;

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

  vk::UniqueDescriptorPool _descriptorPool;
  vk::UniqueDescriptorSetLayout _globalDescriptorSetLayout;
  vk::UniqueDescriptorSetLayout _objectDescriptorSetLayout;
  vk::UniqueDescriptorSetLayout _singleTextureDescriptorSetLayout;
  vk::UniqueDescriptorSetLayout _computeDescriptorSetLayout;

  vk::UniqueSampler _textureImageSampler;
  vk::UniqueDescriptorSet _textureDescriptorSet;

  std::array<vk::UniquePipelineLayout, 1> _computePipelineLayout;
  std::array<vk::UniquePipeline, 1> _computePipelines;
  std::array<vk::UniquePipelineLayout, 1> _pipelineLayouts;
  std::array<vk::UniquePipeline, 1> _pipelines;

  std::array<Mesh, 2> _meshes;
  vk::DeviceSize _vertexBufferSize;
  AllocatedBuffer _vertexBuffer;
  vk::DeviceSize _indexBufferSize;
  AllocatedBuffer _indexBuffer;
  // std::unordered_map<std::string, Mesh> _meshes;
  std::unordered_map<std::string, Texture> _textures;

  AllocatedBuffer _sceneUniformBuffer;

  std::array<FrameData, MAX_FRAMES_IN_FLIGHT> _frames;

  // TODO: Get rid of this?
  std::vector<vk::Fence> _imagesInFlight;
  size_t _currentFrame{};

  std::vector<RenderObject> _renderables;

  bool _framebufferResized = false;
};
