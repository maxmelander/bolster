#pragma once

#include <array>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

#include "camera.hpp"
#include "dstack.hpp"
#include "glm/mat4x4.hpp"
#define NOMINMAX
#include <vulkan/vulkan.hpp>

#include "GLFW/glfw3.h"
#include "bs_graphics_component.hpp"
#include "bs_types.hpp"
#include "mesh.hpp"
#include "tiny_gltf.h"
#include "vk_mem_alloc.h"
#include "vk_types.hpp"
#include "vk_utils.hpp"

class PipelineBuilder {
 public:
  uint32_t _stageCount;
  vk::PipelineShaderStageCreateInfo _shaderStages[2]{};
  vk::PipelineVertexInputStateCreateInfo _vertexInputInfo;
  vk::PipelineInputAssemblyStateCreateInfo _inputAssemblyInfo;
  vk::Viewport _viewport;
  vk::Rect2D _scissor;
  vk::PipelineViewportStateCreateInfo _viewportStateInfo;
  vk::PipelineRasterizationStateCreateInfo _rasterizationInfo;
  // NOTE: Defaults to off. Requires enabling a GPU feature if used
  vk::PipelineMultisampleStateCreateInfo _multisampleInfo;
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

constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 2;
constexpr unsigned int MAX_DRAW_COMMANDS = 10000;
constexpr unsigned int MAX_OBJECTS = 10000;

class VulkanEngine {
 public:
  VulkanEngine();
  ~VulkanEngine();

  void init(GLFWwindow *, DStack &);
  void setupDrawables(const bs::GraphicsComponent[bs::MAX_ENTITIES],
                      size_t numEntities);
  void run();
  void draw(const bs::GraphicsComponent[bs::MAX_ENTITIES], size_t numEntities,
            Camera &, double, float);
  void drawObjects(const bs::GraphicsComponent[bs::MAX_ENTITIES],
                   size_t numEntities, vk::CommandBuffer, double);

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
  void initSwapchainImages(DStack &);
  void initDepthImage();
  void initRenderPass();
  void initFramebuffers(DStack &);

  void initDescriptorSetLayout();
  void initPipelines();
  void initComputePipelines();
  void initMaterials(const tinygltf::Model &model);

  void initUniformBuffers();

  void initTextures();
  void initTextureImageSampler();
  void initTextureDescriptorSet();

  void initDescriptorPool();
  void initDescriptorSets();

  // TODO: Expose all of these as a single
  // loadMeshes type function, that can be used
  // by a resource manager to load different things
  // at different times
  void initMesh();
  void initMeshBuffers();
  void uploadMeshes(const std::vector<Vertex> &, const std::vector<uint32_t> &);

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
  void updateSceneBuffer(float, float);
  void updateObjectBuffer(const bs::GraphicsComponent *, size_t);

  size_t padUniformBufferSize(size_t);

  void loadGltfTextures(const tinygltf::Model &model);
  void loadGltfNode(const tinygltf::Model &model, const tinygltf::Node &node,
                    std::vector<Vertex> &vertexBuffer,
                    std::vector<uint32_t> &indexBuffer,
                    std::vector<glm::vec3> &vertexPositions);

  void loadTextureFromFile(const std::string &, Texture &);
  void loadTexture(const tinygltf::Image &image, Texture &outTexture);
  MeshData loadMeshFromFile(const std::string &);

 public:
  DStack *_dstack;
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

  size_t _nSwapchainImages;
  vk::Image *_swapchainImages;

  size_t _nSwapchainImageViews;
  vk::ImageView *_swapchainImageViews;  // TODO: Unique

  AllocatedImage _depthImage;
  vk::UniqueImageView _depthImageView;

  AllocatedImage _shadowDepthImage;
  vk::UniqueImageView _shadowDepthImageView;
  vk::UniqueSampler _shadowDepthImageSampler;

  vk::UniqueRenderPass _shadowPass;
  vk::UniqueRenderPass _forwardPass;

  size_t _nFramebuffers;
  vk::Framebuffer *_framebuffers;  // TODO: Unique

  vk::UniqueFramebuffer _depthFramebuffer;

  vk::UniqueDescriptorPool _descriptorPool;
  vk::UniqueDescriptorSetLayout _globalDescriptorSetLayout;
  vk::UniqueDescriptorSetLayout _objectDescriptorSetLayout;
  vk::UniqueDescriptorSetLayout _singleTextureDescriptorSetLayout;
  vk::UniqueDescriptorSetLayout _computeDescriptorSetLayout;

  vk::UniqueSampler _textureImageSampler;
  vk::UniqueDescriptorSet _textureDescriptorSet;

  std::array<vk::UniquePipelineLayout, 1> _computePipelineLayouts;
  std::array<vk::UniquePipeline, 1> _computePipelines;

  std::array<vk::UniquePipelineLayout, 2> _pipelineLayouts;
  std::array<vk::UniquePipeline, 2> _pipelines;

  std::array<Mesh, 3> _meshes;
  vk::DeviceSize _vertexBufferSize;
  AllocatedBuffer _vertexBuffer;
  vk::DeviceSize _indexBufferSize;
  AllocatedBuffer _indexBuffer;
  // std::unordered_map<std::string, Mesh> _meshes;
  //

  size_t _nTextures;
  Texture *_textures;
  // std::unordered_map<std::string, Texture> _textures;

  AllocatedBuffer _sceneUniformBuffer;
  SceneBufferObject _sceneUbo;

  std::array<FrameData, MAX_FRAMES_IN_FLIGHT> _frames;

  // TODO: Get rid of this?
  // std::vector<vk::Fence> _imagesInFlight;
  size_t _currentFrame{};

  bool _framebufferResized = false;
};
