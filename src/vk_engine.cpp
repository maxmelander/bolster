#include "vk_engine.hpp"

#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <iterator>
#include <set>
#include <unordered_map>
#include <utility>
#include <vulkan/vulkan.hpp>

#include "GLFW/glfw3.h"
#include "camera.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/fwd.hpp"
#include "glm/matrix.hpp"
#include "vk_types.hpp"
#include "vk_utils.hpp"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

vk::UniquePipeline PipelineBuilder::buildPipeline(
    const vk::Device &device, const vk::RenderPass &renderPass,
    const vk::PipelineLayout &pipelineLayout) {
  _viewportStateInfo = vk::PipelineViewportStateCreateInfo{
      vk::PipelineViewportStateCreateFlags{}, 1, &_viewport, 1, &_scissor};

  // NOTE: Defaults to off. Requires enabling a GPU feature if used
  _multisampleInfo = vk::PipelineMultisampleStateCreateInfo{};

  _colorBlendAttachmentInfo = vk::PipelineColorBlendAttachmentState{false};
  _colorBlendAttachmentInfo.colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  _colorBlendingInfo = vk::PipelineColorBlendStateCreateInfo{
      vk::PipelineColorBlendStateCreateFlags{}, false, vk::LogicOp::eCopy, 1,
      &_colorBlendAttachmentInfo};

  _depthStencilInfo = vk::PipelineDepthStencilStateCreateInfo{
      {}, true, true, vk::CompareOp::eLess, false, false};

  vk::GraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = _shaderStages;
  pipelineInfo.pVertexInputState = &_vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &_inputAssemblyInfo;
  pipelineInfo.pViewportState = &_viewportStateInfo;
  pipelineInfo.pRasterizationState = &_rasterizationInfo;
  pipelineInfo.pMultisampleState = &_multisampleInfo;
  pipelineInfo.pDepthStencilState = &_depthStencilInfo;
  pipelineInfo.pColorBlendState = &_colorBlendingInfo;
  // pipelineInfo.pDynamicState = nullptr;
  pipelineInfo.layout = pipelineLayout;
  pipelineInfo.renderPass = renderPass;
  pipelineInfo.subpass =
      0;  // index of subpass where this pipeline will be used
  auto result = device.createGraphicsPipelineUnique(nullptr, pipelineInfo);
  assert(result.result == vk::Result::eSuccess);

  return std::move(result.value);
}

VulkanEngine::VulkanEngine() {}
VulkanEngine::~VulkanEngine() {}

void VulkanEngine::init(GLFWwindow *window) {
  _window = window;

  initInstance();
  initSurface();
  initPhysicalDevice();
  initLogicalDevice();
  initAllocator();
  initCommandPool();
  initQueues();
  initSwapchain();
  initSwapchainImages();
  initDepthImage();
  initRenderPass();
  initFramebuffers();

  initDescriptorPool();
  initDescriptorSetLayout();
  initPipelines();
  initComputePipelines();

  initTextures();
  initTextureImageSampler();
  initTextureDescriptorSet();

  initUniformBuffers();
  initMaterials();

  initMesh();
  // initScene();

  initDrawCommandBuffers();

  initDescriptorSets();

  initSyncObjects();
}

/******  INIT  ******/

void VulkanEngine::initInstance() {
  vk::ApplicationInfo appInfo{"Bolster", VK_MAKE_VERSION(1, 0, 0), "Vulkan.hpp",
                              VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_2};

  uint32_t glfwExtensionCount{};
  const char **glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  vk::InstanceCreateInfo instanceCreateInfo{
      {}, &appInfo, {}, {}, glfwExtensionCount, glfwExtensions};

  // in debug mode, enable validation layers if they are supported by the
  // vk::Instance
#if !defined(NDEBUG)
  instanceCreateInfo.enabledLayerCount = vkutils::validationLayers.size();
  instanceCreateInfo.ppEnabledLayerNames = vkutils::validationLayers.data();
#endif

  _instance = vk::createInstanceUnique(instanceCreateInfo);
}

void VulkanEngine::initSurface() {
  auto tempSurface = VkSurfaceKHR{};
  assert(glfwCreateWindowSurface(VkInstance{_instance.get()}, _window, nullptr,
                                 &tempSurface) == VK_SUCCESS);
  _surface = vk::UniqueSurfaceKHR{std::move(tempSurface)};
}

void VulkanEngine::initPhysicalDevice() {
  std::vector<vk::PhysicalDevice> physicalDevices =
      _instance->enumeratePhysicalDevices();

  vkutils::QueueFamilyIndices queueFamilyIndices{};

  auto device = std::find_if(
      physicalDevices.begin(), physicalDevices.end(),
      [this, &queueFamilyIndices](auto const &pd) {
        // NOTE: Move this into a named function like deviceSuitable
        // or something like that perhaps
        queueFamilyIndices = vkutils::findQueueFamilies(pd, _surface.get());

        auto swapChainSupport =
            vkutils::querySwapchainSupport(pd, _surface.get());

        auto supportedFeatures = pd.getFeatures();

        return queueFamilyIndices.isComplete() &&
               vkutils::checkDeviceExtensionSupport(pd) &&
               !swapChainSupport.formats.empty() &&
               !swapChainSupport.presentModes.empty() &&
               supportedFeatures.samplerAnisotropy;
      });

  assert(device != physicalDevices.end());
  assert(queueFamilyIndices.isComplete());

  _graphicsQueueFamily = queueFamilyIndices.graphicsFamily.value();
  _presentQueueFamily = queueFamilyIndices.presentFamily.value();
  _physicalDevice = *device;
  _deviceProperties = _physicalDevice.getProperties();
}

void VulkanEngine::initLogicalDevice() {
  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies{_graphicsQueueFamily,
                                         _presentQueueFamily};

  float queuePriority = 1.0f;
  for (auto queueFamilyIndex : uniqueQueueFamilies) {
    queueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags{},
                                  queueFamilyIndex, 1, &queuePriority);
  }

  vk::PhysicalDeviceFeatures deviceFeatures{};
  deviceFeatures.samplerAnisotropy = true;
  deviceFeatures.multiDrawIndirect = true;

  vk::DeviceCreateInfo createInfo(
      vk::DeviceCreateFlags{}, static_cast<uint32_t>(queueCreateInfos.size()),
      queueCreateInfos.data());

  createInfo.pEnabledFeatures = &deviceFeatures;

  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(vkutils::deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = vkutils::deviceExtensions.data();

#if !defined(NDEBUG)
  createInfo.enabledLayerCount =
      static_cast<uint32_t>(vkutils::validationLayers.size());
  createInfo.ppEnabledLayerNames = vkutils::validationLayers.data();
#else
  createInfo.enabledLayerCount = 0;
#endif

  _device = _physicalDevice.createDeviceUnique(createInfo);
}

void VulkanEngine::initAllocator() {
  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice = _physicalDevice;
  allocatorInfo.device = _device.get();
  allocatorInfo.instance = _instance.get();
  vmaCreateAllocator(&allocatorInfo, &_allocator);
}

void VulkanEngine::initQueues() {
  _graphicsQueue = _device->getQueue(_graphicsQueueFamily, 0);
  _presentQueue = _device->getQueue(_presentQueueFamily, 0);
}

void VulkanEngine::initSwapchain() {
  auto swapchainSupport =
      vkutils::querySwapchainSupport(_physicalDevice, _surface.get());

  auto surfaceFormat =
      vkutils::chooseSwapSurfaceFormat(swapchainSupport.formats);
  auto presentMode =
      vkutils::chooseSwapPresentMode(swapchainSupport.presentModes);
  auto extent =
      vkutils::chooseSwapExtent(swapchainSupport.capabilities, _window);

  uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
  if (swapchainSupport.capabilities.maxImageCount > 0 &&
      imageCount > swapchainSupport.capabilities.maxImageCount) {
    imageCount = swapchainSupport.capabilities.maxImageCount;
  }

  vk::SwapchainCreateInfoKHR createInfo{};
  createInfo.surface = _surface.get();
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

  uint32_t queueFamilyIndices[] = {_graphicsQueueFamily, _presentQueueFamily};

  if (_graphicsQueueFamily != _presentQueueFamily) {
    createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    // TODO: Is this correct?
    createInfo.queueFamilyIndexCount = 1;
    createInfo.pQueueFamilyIndices = &_graphicsQueueFamily;
  }

  createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
  createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;

  _swapchainExtent = extent;
  _swapchainImageFormat = surfaceFormat.format;
  _swapchain = _device->createSwapchainKHRUnique(createInfo);
}

void VulkanEngine::initSwapchainImages() {
  _swapchainImages = _device->getSwapchainImagesKHR(_swapchain.get());
  _swapchainImageViews.reserve(_swapchainImages.size());

  for (size_t i{}; i < _swapchainImages.size(); i++) {
    vk::ImageViewCreateInfo createInfo{
        vk::ImageViewCreateFlags{},
        _swapchainImages[i],
        vk::ImageViewType::e2D,
        _swapchainImageFormat,
        vk::ComponentMapping{},
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

    _swapchainImageViews.emplace_back(
        _device->createImageViewUnique(createInfo));
  }
}

void VulkanEngine::initDepthImage() {
  auto depthFormat = vkutils::findDepthFormat(_physicalDevice);

  vk::ImageCreateInfo imageCi(
      {}, vk::ImageType::e2D, depthFormat,
      vk::Extent3D{_swapchainExtent.width, _swapchainExtent.height, 1}, 1, 1,
      vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eDepthStencilAttachment,
      vk::SharingMode::eExclusive);

  vkutils::allocateImage(_allocator, imageCi, VMA_MEMORY_USAGE_GPU_ONLY,
                         _depthImage);

  vk::ImageViewCreateInfo imageViewCi{
      vk::ImageViewCreateFlags{},
      _depthImage._image,
      vk::ImageViewType::e2D,
      depthFormat,
      vk::ComponentMapping{},
      vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1}};

  _depthImageView = _device->createImageViewUnique(imageViewCi);
}

void VulkanEngine::initRenderPass() {
  vk::AttachmentDescription colorAttachment{};
  colorAttachment.format = _swapchainImageFormat;
  colorAttachment.samples = vk::SampleCountFlagBits::e1;
  colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
  colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
  colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
  colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

  // This is the reference to the color attachment above,
  // to be used by a subpass
  vk::AttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

  vk::AttachmentDescription depthAttachment{};
  depthAttachment.format = vkutils::findDepthFormat(_physicalDevice);
  depthAttachment.samples = vk::SampleCountFlagBits::e1;
  depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
  depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
  depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
  depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

  vk::AttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = 1;
  depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

  vk::SubpassDescription subpass{vk::SubpassDescriptionFlags{},
                                 vk::PipelineBindPoint::eGraphics};
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  vk::SubpassDependency dependency{
      VK_SUBPASS_EXTERNAL,
      0,
      vk::PipelineStageFlagBits::eColorAttachmentOutput |
          vk::PipelineStageFlagBits::eEarlyFragmentTests,
      vk::PipelineStageFlagBits::eColorAttachmentOutput |
          vk::PipelineStageFlagBits::eEarlyFragmentTests,
      {},
      vk::AccessFlagBits::eColorAttachmentWrite |
          vk::AccessFlagBits::eDepthStencilAttachmentWrite};

  std::array<vk::AttachmentDescription, 2> attachments = {colorAttachment,
                                                          depthAttachment};

  vk::RenderPassCreateInfo renderPassInfo{{},       2, attachments.data(), 1,
                                          &subpass, 1, &dependency};

  _renderPass = _device->createRenderPassUnique(renderPassInfo);
}

void VulkanEngine::initFramebuffers() {
  _framebuffers.reserve(_swapchainImageViews.size());

  for (size_t i{}; i < _swapchainImageViews.size(); i++) {
    std::array<vk::ImageView, 2> attachments = {_swapchainImageViews[i].get(),
                                                _depthImageView.get()};

    vk::FramebufferCreateInfo createInfo{};
    createInfo.renderPass = _renderPass.get();
    createInfo.attachmentCount = 2;
    createInfo.pAttachments = attachments.data();
    createInfo.width = _swapchainExtent.width;
    createInfo.height = _swapchainExtent.height;
    createInfo.layers = 1;

    _framebuffers.emplace_back(_device->createFramebufferUnique(createInfo));
  }
}

void VulkanEngine::initCommandPool() {
  vk::CommandPoolCreateInfo createInfo{
      vk::CommandPoolCreateFlagBits::eResetCommandBuffer};
  createInfo.queueFamilyIndex = _graphicsQueueFamily;
  _commandPool = _device->createCommandPoolUnique(createInfo);
  _immediateCommandPool = _device->createCommandPoolUnique(createInfo);
}

// TODO: Shader reflectance
// spirv_reflect ?
void VulkanEngine::initDescriptorSetLayout() {
  // COMPUTE SET
  //
  // Indirect draw command buffer
  vk::DescriptorSetLayoutBinding indirectDrawBufferBinding{};
  indirectDrawBufferBinding.binding = 0;
  indirectDrawBufferBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
  indirectDrawBufferBinding.descriptorCount = 1;
  indirectDrawBufferBinding.stageFlags = vk::ShaderStageFlagBits::eCompute;
  indirectDrawBufferBinding.pImmutableSamplers = nullptr;

  // Object storage buffer
  vk::DescriptorSetLayoutBinding objectBufferBinding{};
  objectBufferBinding.binding = 1;
  objectBufferBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
  objectBufferBinding.descriptorCount = 1;
  objectBufferBinding.stageFlags = vk::ShaderStageFlagBits::eCompute;
  objectBufferBinding.pImmutableSamplers = nullptr;

  // Camera buffer
  vk::DescriptorSetLayoutBinding cameraBufferBinding{};
  cameraBufferBinding.binding = 2;
  cameraBufferBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
  cameraBufferBinding.descriptorCount = 1;
  cameraBufferBinding.stageFlags = vk::ShaderStageFlagBits::eCompute;
  cameraBufferBinding.pImmutableSamplers = nullptr;

  std::array<vk::DescriptorSetLayoutBinding, 3> computeBindings = {
      indirectDrawBufferBinding, objectBufferBinding, cameraBufferBinding};
  vk::DescriptorSetLayoutCreateInfo computeCreateInfo{};
  computeCreateInfo.bindingCount =
      static_cast<uint32_t>(computeBindings.size());
  computeCreateInfo.pBindings = computeBindings.data();

  _computeDescriptorSetLayout =
      _device->createDescriptorSetLayoutUnique(computeCreateInfo);

  // SET 0
  //
  // Camera buffer
  cameraBufferBinding.binding = 0;
  cameraBufferBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

  // Scene buffer
  vk::DescriptorSetLayoutBinding sceneBufferBinding{};
  sceneBufferBinding.binding = 1;
  sceneBufferBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
  sceneBufferBinding.descriptorCount = 1;
  sceneBufferBinding.stageFlags =
      vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
  sceneBufferBinding.pImmutableSamplers = nullptr;

  std::array<vk::DescriptorSetLayoutBinding, 2> globalBindings = {
      cameraBufferBinding, sceneBufferBinding};
  vk::DescriptorSetLayoutCreateInfo globalCreateInfo{};
  globalCreateInfo.bindingCount = static_cast<uint32_t>(globalBindings.size());
  globalCreateInfo.pBindings = globalBindings.data();

  _globalDescriptorSetLayout =
      _device->createDescriptorSetLayoutUnique(globalCreateInfo);

  // SET 1
  //
  // Object storage buffer
  objectBufferBinding.binding = 0;
  objectBufferBinding.stageFlags =
      vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

  // Material storage buffer
  vk::DescriptorSetLayoutBinding materialBufferBinding{};
  materialBufferBinding.binding = 1;
  materialBufferBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
  materialBufferBinding.descriptorCount = 1;
  materialBufferBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
  materialBufferBinding.pImmutableSamplers = nullptr;

  std::array<vk::DescriptorSetLayoutBinding, 2> objectBindings = {
      objectBufferBinding, materialBufferBinding};
  vk::DescriptorSetLayoutCreateInfo objectCreateInfo{};
  objectCreateInfo.bindingCount = static_cast<uint32_t>(objectBindings.size());
  objectCreateInfo.pBindings = objectBindings.data();

  _objectDescriptorSetLayout =
      _device->createDescriptorSetLayoutUnique(objectCreateInfo);

  // SET 2
  //
  // Texture sampler
  vk::DescriptorSetLayoutBinding samplerLayoutBinding{};
  samplerLayoutBinding.binding = 0;
  samplerLayoutBinding.descriptorType =
      vk::DescriptorType::eCombinedImageSampler;
  samplerLayoutBinding.descriptorCount = 2;
  samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
  samplerLayoutBinding.pImmutableSamplers = nullptr;

  std::array<vk::DescriptorSetLayoutBinding, 1> textureBindings = {
      samplerLayoutBinding};
  vk::DescriptorSetLayoutCreateInfo textureCreateInfo{};
  textureCreateInfo.bindingCount =
      static_cast<uint32_t>(textureBindings.size());
  textureCreateInfo.pBindings = textureBindings.data();

  _singleTextureDescriptorSetLayout =
      _device->createDescriptorSetLayoutUnique(textureCreateInfo);
}

void VulkanEngine::initPipelines() {
  // Pipeline Layout
  vk::PipelineLayoutCreateInfo createInfo{};
  vk::DescriptorSetLayout setLayouts[] = {
      _globalDescriptorSetLayout.get(), _objectDescriptorSetLayout.get(),
      _singleTextureDescriptorSetLayout.get()};

  createInfo.setLayoutCount = 3;
  createInfo.pSetLayouts = setLayouts;

  // Push constants
  // vk::PushConstantRange pushConstant{vk::ShaderStageFlagBits::eVertex, 0,
  // sizeof(MeshPushConstants)};

  // createInfo.pPushConstantRanges = &pushConstant;
  // createInfo.pushConstantRangeCount = 1;

  _pipelineLayouts[0] = _device->createPipelineLayoutUnique(createInfo);

  PipelineBuilder pipelineBuilder{};

  // NOTE: File paths are relative to the executable
  auto vertShaderCode = vkutils::readFile("../shaders/vert.spv");
  auto fragShaderCode = vkutils::readFile("../shaders/frag.spv");

  vk::UniqueShaderModule vertShaderModule =
      vkutils::createUniqueShaderModule(_device.get(), vertShaderCode);
  vk::UniqueShaderModule fragShaderModule =
      vkutils::createUniqueShaderModule(_device.get(), fragShaderCode);

  vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
      vk::PipelineShaderStageCreateFlags{}, vk::ShaderStageFlagBits::eVertex,
      vertShaderModule.get(), "main"};

  vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
      vk::PipelineShaderStageCreateFlags{}, vk::ShaderStageFlagBits::eFragment,
      fragShaderModule.get(), "main"};

  pipelineBuilder._shaderStages[0] = vertShaderStageInfo;
  pipelineBuilder._shaderStages[1] = fragShaderStageInfo;

  auto bindingDescription = Vertex::getBindingDescription();
  auto attributeDescriptions = Vertex::getAttributeDescriptions();
  pipelineBuilder._vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{
      {},
      1,
      &bindingDescription,
      static_cast<uint32_t>(attributeDescriptions.size()),
      attributeDescriptions.data()};

  pipelineBuilder._inputAssemblyInfo = vk::PipelineInputAssemblyStateCreateInfo{
      vk::PipelineInputAssemblyStateCreateFlags{},
      vk::PrimitiveTopology::eTriangleList, false};

  pipelineBuilder._viewport = vk::Viewport{
      0.0f, 0.0f, (float)_swapchainExtent.width, (float)_swapchainExtent.height,
      0.0f, 1.0f};

  pipelineBuilder._scissor = vk::Rect2D{vk::Offset2D{0, 0}, _swapchainExtent};

  pipelineBuilder._rasterizationInfo = vk::PipelineRasterizationStateCreateInfo{
      vk::PipelineRasterizationStateCreateFlags{},
      false,
      false,
      vk::PolygonMode::eFill,
      vk::CullModeFlagBits::eBack,
      vk::FrontFace::eCounterClockwise,
      false,
      0.0f,
      0.0f,
      0.0f,
      1.0f};

  _pipelines[0] = pipelineBuilder.buildPipeline(
      _device.get(), _renderPass.get(), _pipelineLayouts[0].get());
}

void VulkanEngine::initComputePipelines() {
  vk::PipelineLayoutCreateInfo layoutCreateInfo{};
  vk::DescriptorSetLayout setLayouts[] = {_computeDescriptorSetLayout.get()};
  layoutCreateInfo.setLayoutCount = 1;
  layoutCreateInfo.pSetLayouts = setLayouts;

  _computePipelineLayouts[0] =
      _device->createPipelineLayoutUnique(layoutCreateInfo);

  vk::ComputePipelineCreateInfo pipelineCreateInfo{};

  auto computeShaderCode = vkutils::readFile("../shaders/comp.spv");

  vk::UniqueShaderModule computeShaderModule =
      vkutils::createUniqueShaderModule(_device.get(), computeShaderCode);
  vk::PipelineShaderStageCreateInfo computeShaderStageInfo{
      {}, vk::ShaderStageFlagBits::eCompute, computeShaderModule.get(), "main"};
  pipelineCreateInfo.stage = computeShaderStageInfo;
  pipelineCreateInfo.layout = _computePipelineLayouts[0].get();

  auto result = _device->createComputePipelineUnique({}, pipelineCreateInfo);
  assert(result.result == vk::Result::eSuccess);
  _computePipelines[0] = std::move(result.value);
}

void VulkanEngine::initUniformBuffers() {
  // Allocate camera buffers
  vk::DeviceSize bufferSize = sizeof(CameraBufferObject);
  for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    AllocatedBuffer buffer{};
    vkutils::allocateBuffer(
        _allocator, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
        VMA_MEMORY_USAGE_CPU_TO_GPU, vk::SharingMode::eExclusive, buffer);
    _frames[i]._cameraBuffer = std::move(buffer);
  }

  // Allocate scene buffer
  bufferSize =
      MAX_FRAMES_IN_FLIGHT * padUniformBufferSize(sizeof(SceneBufferObject));
  AllocatedBuffer sb{};
  vkutils::allocateBuffer(
      _allocator, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
      VMA_MEMORY_USAGE_CPU_TO_GPU, vk::SharingMode::eExclusive, sb);
  _sceneUniformBuffer = std::move(sb);

  // Allocate object buffers
  const int MAX_OBJECTS = 10000;
  bufferSize = sizeof(ObjectBufferObject) * MAX_OBJECTS;
  for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    AllocatedBuffer buffer{};
    vkutils::allocateBuffer(
        _allocator, bufferSize, vk::BufferUsageFlagBits::eStorageBuffer,
        VMA_MEMORY_USAGE_CPU_TO_GPU, vk::SharingMode::eExclusive, buffer);
    _frames[i]._objectStorageBuffer = std::move(buffer);
  }

  // Allocate material buffers
  bufferSize = sizeof(MaterialBufferObject) * 2;
  for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    AllocatedBuffer buffer{};
    vkutils::allocateBuffer(
        _allocator, bufferSize, vk::BufferUsageFlagBits::eStorageBuffer,
        VMA_MEMORY_USAGE_CPU_TO_GPU, vk::SharingMode::eExclusive, buffer);
    _frames[i]._materialStorageBuffer = std::move(buffer);
  }
}

void VulkanEngine::initMaterials() {
  // TODO: Holding on to these Material objects on the CPU
  // doesn't give us anything at this points, since both texture samplers
  // and pipelines are not a per material thing anymore

  // Update material storage buffer
  for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    void *materialData;
    vmaMapMemory(_allocator, _frames[i]._materialStorageBuffer._allocation,
                 &materialData);
    MaterialBufferObject *materialSSBO = (MaterialBufferObject *)materialData;

    // TODO: Some nice way of indexing textures and so on
    materialSSBO[0].albedoTexture = 0;
    materialSSBO[0].normalTexture = 0;
    materialSSBO[0].roughnessTexture = 0;

    materialSSBO[1].albedoTexture = 1;
    materialSSBO[1].normalTexture = 0;
    materialSSBO[1].roughnessTexture = 0;

    vmaUnmapMemory(_allocator, _frames[i]._materialStorageBuffer._allocation);
  }
}

void VulkanEngine::initTextures() {
  // Load texture from image file
  // TODO: Resource management
  Texture houseTexture;
  loadTextureFromFile("../textures/viking_room.png", houseTexture);
  _textures["house"] = std::move(houseTexture);

  Texture faceTexture;
  loadTextureFromFile("../textures/texture.jpg", faceTexture);
  _textures["face"] = std::move(faceTexture);
}

void VulkanEngine::initTextureImageSampler() {
  vk::SamplerCreateInfo createInfo{};
  createInfo.magFilter = vk::Filter::eLinear;
  createInfo.minFilter = vk::Filter::eLinear;
  createInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
  createInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
  createInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
  createInfo.anisotropyEnable = true;
  createInfo.maxAnisotropy = _deviceProperties.limits.maxSamplerAnisotropy;
  createInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
  createInfo.unnormalizedCoordinates = false;

  // NOTE: This is maily used for percentage-closer filtering on shadow maps
  createInfo.compareEnable = false;
  createInfo.compareOp = vk::CompareOp::eAlways;

  createInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
  createInfo.mipLodBias = 0.0f;
  createInfo.minLod = 0.0f;
  createInfo.maxLod = static_cast<float>(_textures["house"].mipLevels);

  _textureImageSampler = _device->createSamplerUnique(createInfo);
}

void VulkanEngine::initTextureDescriptorSet() {
  // Alloc and write texture descriptor sets
  vk::DescriptorSetAllocateInfo dInfo{};
  dInfo.descriptorPool = _descriptorPool.get();
  dInfo.descriptorSetCount = 1;
  dInfo.pSetLayouts = &_singleTextureDescriptorSetLayout.get();

  _textureDescriptorSet =
      std::move(_device->allocateDescriptorSetsUnique(dInfo)[0]);

  // Populate descriptor with the texture we want
  vk::DescriptorImageInfo defaultImageInfo{};
  defaultImageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  defaultImageInfo.imageView = _textures["house"].imageView.get();
  defaultImageInfo.sampler = _textureImageSampler.get();

  // Populate descriptor with the texture we want
  vk::DescriptorImageInfo faceImageInfo{};
  faceImageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  faceImageInfo.imageView = _textures["face"].imageView.get();
  faceImageInfo.sampler = _textureImageSampler.get();

  std::array<vk::WriteDescriptorSet, 2> descriptorWrites{};
  descriptorWrites[0].dstSet = _textureDescriptorSet.get();
  descriptorWrites[0].dstBinding = 0;
  descriptorWrites[0].dstArrayElement = 0;
  descriptorWrites[0].descriptorType =
      vk::DescriptorType::eCombinedImageSampler;
  descriptorWrites[0].descriptorCount = 1;
  descriptorWrites[0].pImageInfo = &defaultImageInfo;

  descriptorWrites[1].dstSet = _textureDescriptorSet.get();
  descriptorWrites[1].dstBinding = 0;
  descriptorWrites[1].dstArrayElement = 1;
  descriptorWrites[1].descriptorType =
      vk::DescriptorType::eCombinedImageSampler;
  descriptorWrites[1].descriptorCount = 1;
  descriptorWrites[1].pImageInfo = &faceImageInfo;

  _device->updateDescriptorSets(descriptorWrites, nullptr);
}

void VulkanEngine::initDescriptorPool() {
  std::array<vk::DescriptorPoolSize, 3> poolSizes{};
  poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
  poolSizes[0].descriptorCount = 10;

  poolSizes[1].type = vk::DescriptorType::eCombinedImageSampler;
  poolSizes[1].descriptorCount = 10;

  poolSizes[2].type = vk::DescriptorType::eStorageBuffer;
  poolSizes[2].descriptorCount = 10;

  vk::DescriptorPoolCreateInfo createInfo{};
  createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  createInfo.pPoolSizes = poolSizes.data();
  createInfo.maxSets = 10;

  _descriptorPool = _device->createDescriptorPoolUnique(createInfo);
}

void VulkanEngine::initDescriptorSets() {
  std::vector<vk::DescriptorSetLayout> computeLayouts(
      MAX_FRAMES_IN_FLIGHT, _computeDescriptorSetLayout.get());

  // Allocate compute descriptor sets
  vk::DescriptorSetAllocateInfo computeAllocInfo{};
  computeAllocInfo.descriptorPool = _descriptorPool.get();
  computeAllocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
  computeAllocInfo.pSetLayouts = computeLayouts.data();

  auto cds = _device->allocateDescriptorSetsUnique(computeAllocInfo);
  for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    _frames[i]._computeDescriptorSet = std::move(cds[i]);
  }

  std::vector<vk::DescriptorSetLayout> globalLayouts(
      MAX_FRAMES_IN_FLIGHT, _globalDescriptorSetLayout.get());

  // Allocate global descriptor sets
  vk::DescriptorSetAllocateInfo globalAllocInfo{};
  globalAllocInfo.descriptorPool = _descriptorPool.get();
  globalAllocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
  globalAllocInfo.pSetLayouts = globalLayouts.data();

  auto gds = _device->allocateDescriptorSetsUnique(globalAllocInfo);
  for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    _frames[i]._globalDescriptorSet = std::move(gds[i]);
  }

  // Allocate object descriptor sets
  std::vector<vk::DescriptorSetLayout> objectLayouts(
      MAX_FRAMES_IN_FLIGHT, _objectDescriptorSetLayout.get());

  vk::DescriptorSetAllocateInfo objectAllocInfo{};
  objectAllocInfo.descriptorPool = _descriptorPool.get();
  objectAllocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
  objectAllocInfo.pSetLayouts = objectLayouts.data();

  auto ods = _device->allocateDescriptorSetsUnique(objectAllocInfo);
  for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    _frames[i]._objectDescriptorSet = std::move(ods[i]);
  }

  // Populate with descriptors
  for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    // Buffer Info
    vk::DescriptorBufferInfo indirectCommandBufferInfo{};
    indirectCommandBufferInfo.buffer =
        _frames[i]._indirectCommandBuffer._buffer;
    indirectCommandBufferInfo.offset = 0;
    indirectCommandBufferInfo.range =
        sizeof(DrawIndexedIndirectCommandBufferObject) * MAX_DRAW_COMMANDS;

    vk::DescriptorBufferInfo objectBufferInfo{};
    objectBufferInfo.buffer = _frames[i]._objectStorageBuffer._buffer;
    objectBufferInfo.offset = 0;
    objectBufferInfo.range = sizeof(ObjectBufferObject) * MAX_OBJECTS;

    vk::DescriptorBufferInfo cameraBufferInfo{};
    cameraBufferInfo.buffer = _frames[i]._cameraBuffer._buffer;
    cameraBufferInfo.offset = 0;
    cameraBufferInfo.range = sizeof(CameraBufferObject);

    vk::DescriptorBufferInfo materialBufferInfo{};
    materialBufferInfo.buffer = _frames[i]._materialStorageBuffer._buffer;
    materialBufferInfo.offset = 0;
    materialBufferInfo.range = sizeof(MaterialBufferObject) * 2;

    vk::DescriptorBufferInfo sceneBufferInfo{};
    sceneBufferInfo.buffer = _sceneUniformBuffer._buffer;
    sceneBufferInfo.offset =
        padUniformBufferSize(sizeof(SceneBufferObject) * i);
    sceneBufferInfo.range = sizeof(SceneBufferObject);

    // Compute descriptors
    std::array<vk::WriteDescriptorSet, 3> computeDescriptorWrites{};

    computeDescriptorWrites[0].dstSet = _frames[i]._computeDescriptorSet.get();
    computeDescriptorWrites[0].dstBinding = 0;
    computeDescriptorWrites[0].dstArrayElement = 0;
    computeDescriptorWrites[0].descriptorType =
        vk::DescriptorType::eStorageBuffer;
    computeDescriptorWrites[0].descriptorCount = 1;
    computeDescriptorWrites[0].pBufferInfo = &indirectCommandBufferInfo;

    computeDescriptorWrites[1].dstSet = _frames[i]._computeDescriptorSet.get();
    computeDescriptorWrites[1].dstBinding = 1;
    computeDescriptorWrites[1].dstArrayElement = 0;
    computeDescriptorWrites[1].descriptorType =
        vk::DescriptorType::eStorageBuffer;
    computeDescriptorWrites[1].descriptorCount = 1;
    computeDescriptorWrites[1].pBufferInfo = &objectBufferInfo;

    computeDescriptorWrites[2].dstSet = _frames[i]._computeDescriptorSet.get();
    computeDescriptorWrites[2].dstBinding = 2;
    computeDescriptorWrites[2].dstArrayElement = 0;
    computeDescriptorWrites[2].descriptorType =
        vk::DescriptorType::eUniformBuffer;
    computeDescriptorWrites[2].descriptorCount = 1;
    computeDescriptorWrites[2].pBufferInfo = &cameraBufferInfo;

    _device->updateDescriptorSets(computeDescriptorWrites, nullptr);

    // Global descriptors
    std::array<vk::WriteDescriptorSet, 2> globalDescriptorWrites{};

    globalDescriptorWrites[0].dstSet = _frames[i]._globalDescriptorSet.get();
    globalDescriptorWrites[0].dstBinding = 0;
    globalDescriptorWrites[0].dstArrayElement = 0;
    globalDescriptorWrites[0].descriptorType =
        vk::DescriptorType::eUniformBuffer;
    globalDescriptorWrites[0].descriptorCount = 1;
    globalDescriptorWrites[0].pBufferInfo = &cameraBufferInfo;

    globalDescriptorWrites[1].dstSet = _frames[i]._globalDescriptorSet.get();
    globalDescriptorWrites[1].dstBinding = 1;
    globalDescriptorWrites[1].dstArrayElement = 0;
    globalDescriptorWrites[1].descriptorType =
        vk::DescriptorType::eUniformBuffer;
    globalDescriptorWrites[1].descriptorCount = 1;
    globalDescriptorWrites[1].pBufferInfo = &sceneBufferInfo;

    _device->updateDescriptorSets(globalDescriptorWrites, nullptr);

    // Object descriptors
    std::array<vk::WriteDescriptorSet, 2> objectDescriptorWrites{};

    objectDescriptorWrites[0].dstSet = _frames[i]._objectDescriptorSet.get();
    objectDescriptorWrites[0].dstBinding = 0;
    objectDescriptorWrites[0].dstArrayElement = 0;
    objectDescriptorWrites[0].descriptorType =
        vk::DescriptorType::eStorageBuffer;
    objectDescriptorWrites[0].descriptorCount = 1;
    objectDescriptorWrites[0].pBufferInfo = &objectBufferInfo;

    objectDescriptorWrites[1].dstSet = _frames[i]._objectDescriptorSet.get();
    objectDescriptorWrites[1].dstBinding = 1;
    objectDescriptorWrites[1].dstArrayElement = 0;
    objectDescriptorWrites[1].descriptorType =
        vk::DescriptorType::eStorageBuffer;
    objectDescriptorWrites[1].descriptorCount = 1;
    objectDescriptorWrites[1].pBufferInfo = &materialBufferInfo;
    // TODO: We can do all descriptor writes with one call, no need to split it
    // up
    _device->updateDescriptorSets(objectDescriptorWrites, nullptr);
  }
}

void VulkanEngine::initDrawCommandBuffers() {
  vk::CommandBufferAllocateInfo allocInfo{_commandPool.get(),
                                          vk::CommandBufferLevel::ePrimary,
                                          MAX_FRAMES_IN_FLIGHT};
  auto drawCommandBuffers = _device->allocateCommandBuffersUnique(allocInfo);

  vk::DeviceSize indirectBufferSize =
      sizeof(DrawIndexedIndirectCommandBufferObject) * MAX_DRAW_COMMANDS;

  for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    _frames[i]._commandBuffer = std::move(drawCommandBuffers[i]);

    // Allocate indirect draw command buffer
    vkutils::allocateBuffer(_allocator, indirectBufferSize,
                            vk::BufferUsageFlagBits::eIndirectBuffer |
                                vk::BufferUsageFlagBits::eStorageBuffer |
                                vk::BufferUsageFlagBits::eTransferDst,
                            VMA_MEMORY_USAGE_CPU_TO_GPU,
                            vk::SharingMode::eExclusive,
                            _frames[i]._indirectCommandBuffer);
  }
}

void VulkanEngine::setupDrawables(
    const bs::GraphicsComponent entities[bs::MAX_ENTITIES],
    size_t numEntities) {
  for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    // Encode the draw data of each object into the indirect draw buffer
    void *indirectData;
    vmaMapMemory(_allocator, _frames[i]._indirectCommandBuffer._allocation,
                 &indirectData);
    DrawIndexedIndirectCommandBufferObject *indirectCommand =
        (DrawIndexedIndirectCommandBufferObject *)indirectData;
    for (size_t e{}; e < numEntities; e++) {
      const bs::GraphicsComponent &entity = entities[e];

      indirectCommand[e].indexCount = entity._mesh->indexSize;
      indirectCommand[e].instanceCount = 1;
      indirectCommand[e].firstIndex = entity._mesh->indexOffset;
      indirectCommand[e].vertexOffset = entity._mesh->vertexOffset;
      indirectCommand[e].firstInstance = e;
    }
    vmaUnmapMemory(_allocator, _frames[i]._indirectCommandBuffer._allocation);
  }
}

void VulkanEngine::initSyncObjects() {
  for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    _frames[i]._imageAvailableSemaphore =
        _device->createSemaphoreUnique(vk::SemaphoreCreateInfo{});
    _frames[i]._renderFinishedSemaphore =
        _device->createSemaphoreUnique(vk::SemaphoreCreateInfo{});
    _frames[i]._inFlightFence = _device->createFenceUnique(
        vk::FenceCreateInfo{vk::FenceCreateFlagBits::eSignaled});
  }

  _imagesInFlight = std::vector<vk::Fence>(_swapchainImages.size(), nullptr);
}

// TODO: This should live in some kind of resource handler
void VulkanEngine::initMesh() {
  std::array<MeshData, 2> meshDatas;

  Mesh vikingMesh;
  meshDatas[0] = loadMeshFromFile("../models/viking_room.obj");
  Mesh spaceshipMesh;
  meshDatas[1] = loadMeshFromFile("../models/cube.obj");

  // TODO: Figure out a good way of setting up and keeping track of offsets
  vikingMesh.vertexSize = meshDatas[0].vertices.size();
  vikingMesh.indexSize = meshDatas[0].indices.size();
  vikingMesh.vertexOffset = 0;
  vikingMesh.indexOffset = 0;
  vikingMesh.boundingSphere = std::move(
      meshDatas[0].boundingSphere);  // TODO: Does move do anything here?

  spaceshipMesh.vertexSize = meshDatas[1].vertices.size();
  spaceshipMesh.indexSize = meshDatas[1].indices.size();
  spaceshipMesh.vertexOffset = vikingMesh.vertexSize;
  spaceshipMesh.indexOffset = vikingMesh.indexSize;
  spaceshipMesh.boundingSphere = std::move(
      meshDatas[1].boundingSphere);  // TODO: Does move do anything here?

  _meshes[0] = vikingMesh;
  _meshes[1] = spaceshipMesh;

  initMeshBuffers();

  // Combine vertices and indices
  // into single vectors to be uploaded to the buffer
  std::vector<Vertex> allVertices;
  std::vector<uint32_t> allIndices;
  for (const MeshData &md : meshDatas) {
    allVertices.insert(allVertices.end(), md.vertices.begin(),
                       md.vertices.end());
    allIndices.insert(allIndices.end(), md.indices.begin(), md.indices.end());
  }

  uploadMeshes(allVertices, allIndices);
}

// Allocate buffers the size of all loaded meshes
void VulkanEngine::initMeshBuffers() {
  size_t vertexBufferSize = 0;
  size_t indexBufferSize = 0;
  for (const Mesh &mesh : _meshes) {
    vertexBufferSize += sizeof(Vertex) * mesh.vertexSize;
    indexBufferSize += sizeof(uint32_t) * mesh.indexSize;
  }

  _vertexBufferSize = vertexBufferSize;
  _indexBufferSize = indexBufferSize;

  vkutils::allocateBuffer(_allocator, vertexBufferSize,
                          vk::BufferUsageFlagBits::eTransferDst |
                              vk::BufferUsageFlagBits::eVertexBuffer,
                          VMA_MEMORY_USAGE_GPU_ONLY,
                          vk::SharingMode::eExclusive, _vertexBuffer);

  vkutils::allocateBuffer(_allocator, indexBufferSize,
                          vk::BufferUsageFlagBits::eTransferDst |
                              vk::BufferUsageFlagBits::eIndexBuffer,
                          VMA_MEMORY_USAGE_GPU_ONLY,
                          vk::SharingMode::eExclusive, _indexBuffer);
}

// Fills the vertex and index buffers
// with all uploaded meshes
void VulkanEngine::uploadMeshes(const std::vector<Vertex> &vertices,
                                const std::vector<uint32_t> &indices) {
  // Fill vertex buffer
  AllocatedBuffer vertexStagingBuffer;
  vkutils::allocateBuffer(_allocator, _vertexBufferSize,
                          vk::BufferUsageFlagBits::eTransferSrc,
                          VMA_MEMORY_USAGE_CPU_TO_GPU,
                          vk::SharingMode::eExclusive, vertexStagingBuffer);

  // Copy vertex data to staging buffer
  void *data;
  vmaMapMemory(_allocator, vertexStagingBuffer._allocation, &data);
  memcpy(data, vertices.data(), _vertexBufferSize);
  vmaUnmapMemory(_allocator, vertexStagingBuffer._allocation);

  // Copy staging buffer to vertex buffer
  vk::BufferCopy copyRegion{0, 0, _vertexBufferSize};
  immediateSubmit([&](vk::CommandBuffer cmd) {
    cmd.copyBuffer(vertexStagingBuffer._buffer, _vertexBuffer._buffer,
                   copyRegion);
  });

  // Fill indfex buffer
  AllocatedBuffer indexStagingBuffer;
  vkutils::allocateBuffer(_allocator, _indexBufferSize,
                          vk::BufferUsageFlagBits::eTransferSrc,
                          VMA_MEMORY_USAGE_CPU_TO_GPU,
                          vk::SharingMode::eExclusive, indexStagingBuffer);

  vmaMapMemory(_allocator, indexStagingBuffer._allocation, &data);
  memcpy(data, indices.data(), _indexBufferSize);
  vmaUnmapMemory(_allocator, indexStagingBuffer._allocation);

  // Copy staging buffer to index buffer
  immediateSubmit([&](vk::CommandBuffer cmd) {
    copyRegion = vk::BufferCopy{0, 0, _indexBufferSize};
    cmd.copyBuffer(indexStagingBuffer._buffer, _indexBuffer._buffer,
                   copyRegion);
  });
}

/******  UTILS  ******/

// TODO: Submit these things on its own queue in a separate thread
// Then need to figure out how to transfer ownership and stuff?
void VulkanEngine::immediateSubmit(
    std::function<void(vk::CommandBuffer cmd)> &&function) {
  vk::CommandBufferAllocateInfo allocInfo{_immediateCommandPool.get(),
                                          vk::CommandBufferLevel::ePrimary, 1};

  auto commandBuffers = _device->allocateCommandBuffersUnique(allocInfo);

  vk::CommandBufferBeginInfo beginInfo{
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  commandBuffers[0]->begin(beginInfo);

  function(commandBuffers[0].get());

  commandBuffers[0]->end();
  vk::SubmitInfo submitInfo{{}, {}, {}, 1, &commandBuffers[0].get()};

  _graphicsQueue.submit({submitInfo});
  _graphicsQueue.waitIdle();

  _device->resetCommandPool(_immediateCommandPool.get());
}

void VulkanEngine::transitionImageLayout(const vk::Image &image,
                                         vk::Format format,
                                         vk::ImageLayout oldLayout,
                                         vk::ImageLayout newLayout,
                                         uint32_t mipLevels) {
  immediateSubmit([&](vk::CommandBuffer cmd) {
    vk::ImageMemoryBarrier barrier{
        {},
        {},
        oldLayout,
        newLayout,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        image,
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, mipLevels,
                                  0, 1}};

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
      abort();
    }

    cmd.pipelineBarrier(srcStage, dstStage, {}, {}, {}, barrier);
  });
}

void VulkanEngine::copyBufferToImage(const vk::Buffer &buffer,
                                     const vk::Image &image, uint32_t width,
                                     uint32_t height) {
  immediateSubmit([&](vk::CommandBuffer cmd) {
    vk::BufferImageCopy copyRegion{
        0,
        0,
        0,
        vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        vk::Offset3D{0, 0, 0},
        vk::Extent3D{width, height, 1}};

    cmd.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal,
                          1, &copyRegion);
  });
}

void VulkanEngine::generateMipmaps(const vk::Image &image, int32_t texWidth,
                                   int32_t texHeight, uint32_t mipLevels) {
  immediateSubmit([&](vk::CommandBuffer cmd) {
    vk::ImageMemoryBarrier barrier{};
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange =
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
      barrier.subresourceRange.baseMipLevel = i - 1;
      barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
      barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
      barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
      barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

      cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                          vk::PipelineStageFlagBits::eTransfer, {}, {}, {},
                          barrier);

      vk::ImageBlit blit{
          vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, i - 1, 0,
                                     1},
          {vk::Offset3D{0, 0, 0}, vk::Offset3D{mipWidth, mipHeight, 1}},
          vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, i, 0, 1},
          {vk::Offset3D{0, 0, 0},
           vk::Offset3D{mipWidth > 1 ? mipWidth / 2 : 1,
                        mipHeight > 1 ? mipHeight / 2 : 1, 1}}};

      cmd.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image,
                    vk::ImageLayout::eTransferDstOptimal, 1, &blit,
                    vk::Filter::eLinear);

      barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
      barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
      barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
      barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

      cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                          vk::PipelineStageFlagBits::eFragmentShader, {}, {},
                          {}, barrier);

      if (mipWidth > 1) mipWidth /= 2;
      if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {},
                        barrier);
  });
}

void VulkanEngine::recreateSwapchain() {
  // If minimized or size == 0 for some other reason,
  // wait until the size is > 0 again
  int width{}, height{};
  glfwGetFramebufferSize(_window, &width, &height);

  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(_window, &width, &height);
    glfwWaitEvents();
  }

  _device->waitIdle();

  _swapchain = {};
  initSwapchain();

  _swapchainImages.clear();
  _swapchainImageViews.clear();
  initSwapchainImages();

  _renderPass = {};
  initRenderPass();

  //_pipelineLayout = {};
  // initPipelineLayout();

  // TODO: recreate the materials and stuff?
  _pipelines = {};
  _pipelineLayouts = {};
  initPipelines();
  initMaterials();

  _depthImage = {};
  initDepthImage();

  _framebuffers.clear();
  initFramebuffers();

  initUniformBuffers();

  _descriptorPool = {};
  initDescriptorPool();

  initDescriptorSets();

  initDrawCommandBuffers();
}

void VulkanEngine::updateCameraBuffer(Camera &camera, float deltaTime) {
  CameraBufferObject ubo{};

  ubo.view = camera.getView();

  const float zNear = 0.1f;
  const float zFar = 100.0f;

  glm::mat4 projection = glm::perspective(
      glm::radians(45.0f),
      (float)_swapchainExtent.width / _swapchainExtent.height, zNear, zFar);

  ubo.proj = projection;
  ubo.proj[1][1] *= -1;

  glm::mat4 projectionT = glm::transpose(projection);

  glm::vec4 frustumX =
      glm::normalize(projectionT[3] + projectionT[0]);  // x + w < 0
  glm::vec4 frustumY =
      glm::normalize(projectionT[3] + projectionT[1]);  // y + w < 0

  // TODO: Global constants
  ubo.zNear = 0.1f;
  ubo.zFar = 100.0f;
  ubo.frustum[0] = frustumX.x;
  ubo.frustum[1] = frustumX.z;
  ubo.frustum[2] = frustumY.y;
  ubo.frustum[3] = frustumY.z;

  void *data;
  vmaMapMemory(_allocator, _frames[_currentFrame]._cameraBuffer._allocation,
               &data);
  memcpy(data, &ubo, sizeof(ubo));
  vmaUnmapMemory(_allocator, _frames[_currentFrame]._cameraBuffer._allocation);
}

void VulkanEngine::draw(const bs::GraphicsComponent entities[bs::MAX_ENTITIES],
                        size_t numEntities, Camera &camera, double currentTime,
                        float deltaTime) {
  // Fence wait timeout 1s
  auto waitResult = _device->waitForFences(
      1, &_frames[_currentFrame]._inFlightFence.get(), true, 1000000000);

  assert(waitResult == vk::Result::eSuccess);

  // Aquire next swapchain image
  auto imageIndex = _device->acquireNextImageKHR(
      _swapchain.get(), 1000000000,
      _frames[_currentFrame]._imageAvailableSemaphore.get(), nullptr);

  if (imageIndex.result == vk::Result::eErrorOutOfDateKHR) {
    recreateSwapchain();
    return;
  }
  assert(imageIndex.result == vk::Result::eSuccess);

  // Check if a previous frame is using this image (i.e. there is a fence to
  // wait on)
  if (_imagesInFlight[imageIndex.value]) {
    auto waitResult = _device->waitForFences(
        1, &_imagesInFlight[imageIndex.value], true, 1000000000);
    assert(waitResult == vk::Result::eSuccess);
  }

  // Mark the image as now being used by this frame
  _imagesInFlight[imageIndex.value] =
      _frames[_currentFrame]._inFlightFence.get();

  updateCameraBuffer(camera, deltaTime);

  // Record command buffer
  vk::CommandBufferBeginInfo beginInfo{
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  vk::CommandBuffer commandBuffer = _frames[_currentFrame]._commandBuffer.get();
  commandBuffer.begin(beginInfo);

  // Compute culling
  // NOTE: I don't think we need a memory barrier before the compute
  // because the buffer we're writing into is a per frame thing,
  // and we would never reach this point if the frame was still
  // in flight.
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute,
                             _computePipelines[0].get());

  commandBuffer.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, _computePipelineLayouts[0].get(), 0,
      _frames[_currentFrame]._computeDescriptorSet.get(), nullptr);

  uint32_t groupCount = (static_cast<uint32_t>(numEntities) / 256) + 1;
  commandBuffer.dispatch(groupCount, 1, 1);

  vk::BufferMemoryBarrier barrier{
      vk::AccessFlagBits::eShaderWrite,
      vk::AccessFlagBits::eShaderRead,
      _graphicsQueueFamily,
      _graphicsQueueFamily,
      _frames[_currentFrame]._indirectCommandBuffer._buffer,
      {},
      sizeof(DrawIndexedIndirectCommandBufferObject) * MAX_DRAW_COMMANDS};

  commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                vk::PipelineStageFlagBits::eVertexShader, {},
                                {}, {barrier}, {});

  // Render pass
  vk::RenderPassBeginInfo renderPassInfo{
      _renderPass.get(), _framebuffers[imageIndex.value].get(),
      vk::Rect2D{vk::Offset2D{0, 0}, vk::Extent2D{_swapchainExtent}}};

  // TODO: Maybe these already have sensible default?
  std::array<vk::ClearValue, 2> clearValues{
      vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}},
      vk::ClearDepthStencilValue{1.0f, 0}};

  renderPassInfo.clearValueCount = 2;
  renderPassInfo.pClearValues = clearValues.data();

  commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

  drawObjects(entities, numEntities, commandBuffer, currentTime);

  commandBuffer.endRenderPass();
  commandBuffer.end();

  // Submit draw
  vk::SubmitInfo submitInfo{};
  vk::Semaphore waitSemaphores[] = {
      _frames[_currentFrame]._imageAvailableSemaphore.get()};
  vk::PipelineStageFlags waitStages[] = {
      vk::PipelineStageFlagBits::eColorAttachmentOutput};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vk::Semaphore signalSemaphore[] = {
      _frames[_currentFrame]._renderFinishedSemaphore.get()};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphore;

  auto resetResult =
      _device->resetFences(1, &_frames[_currentFrame]._inFlightFence.get());
  assert(resetResult == vk::Result::eSuccess);

  _graphicsQueue.submit(std::array<vk::SubmitInfo, 1>{submitInfo},
                        _frames[_currentFrame]._inFlightFence.get());

  // Present
  vk::SwapchainKHR swapchains[] = {_swapchain.get()};
  vk::PresentInfoKHR presentInfo{1, signalSemaphore, 1, swapchains,
                                 &imageIndex.value};

  auto presentResult = _presentQueue.presentKHR(&presentInfo);

  if (presentResult == vk::Result::eErrorOutOfDateKHR ||
      presentResult == vk::Result::eSuboptimalKHR || _framebufferResized) {
    _framebufferResized = false;
    recreateSwapchain();
  } else {
    assert(presentResult == vk::Result::eSuccess);
  }

  _currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanEngine::drawObjects(
    const bs::GraphicsComponent entities[bs::MAX_ENTITIES], size_t numEntities,
    vk::CommandBuffer commandBuffer, double currentTime) {
  // Update object storage buffer
  void *objectData;
  vmaMapMemory(_allocator,
               _frames[_currentFrame]._objectStorageBuffer._allocation,
               &objectData);
  ObjectBufferObject *objectSSBO = (ObjectBufferObject *)objectData;
  for (size_t i{}; i < numEntities; i++) {
    const bs::GraphicsComponent &object = entities[i];
    objectSSBO[i].transform = object._transform;

    // NOTE: These would pretty much never change?
    objectSSBO[i].vertexOffset = object._mesh->vertexOffset;
    objectSSBO[i].indexOffset = object._mesh->indexOffset;
    objectSSBO[i].materialIndex = object._materialIndex;

    // NOTE: Should we do the transform multiplication on the CPU or GPU?
    objectSSBO[i].boundingSphere = object._mesh->boundingSphere;
  }

  vmaUnmapMemory(_allocator,
                 _frames[_currentFrame]._objectStorageBuffer._allocation);

  // Scene uniform update
  // NOTE: The scene buffer stores the scene data for both frames
  // in one buffer, and uses offsets to write into the correct buffer
  // and likewise offsets in the descriptor for the shader to access the
  // correct buffer data
  SceneBufferObject sceneUbo;
  sceneUbo.ambientColor = glm::vec4{std::sin(currentTime) * 0.1f,
                                    std::cos(currentTime) * 0.1f, 0.0f, 1.0f};
  char *sceneData;
  vmaMapMemory(_allocator, _sceneUniformBuffer._allocation,
               (void **)&sceneData);
  sceneData += padUniformBufferSize(sizeof(SceneBufferObject)) * _currentFrame;
  memcpy(sceneData, &sceneUbo, sizeof(SceneBufferObject));
  vmaUnmapMemory(_allocator, _sceneUniformBuffer._allocation);

  // Bind the uber pipeline
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                             _pipelines[0].get());

  // Bind the global descriptor set
  commandBuffer.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, _pipelineLayouts[0].get(), 0,
      _frames[_currentFrame]._globalDescriptorSet.get(), nullptr);

  // Bind the object descriptor set
  commandBuffer.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, _pipelineLayouts[0].get(), 1,
      _frames[_currentFrame]._objectDescriptorSet.get(), nullptr);

  // Bind the texture descriptor array
  commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                   _pipelineLayouts[0].get(), 2,
                                   _textureDescriptorSet.get(), nullptr);

  // Bind vertex and index buffers
  commandBuffer.bindVertexBuffers(0, {_vertexBuffer._buffer}, {0});
  commandBuffer.bindIndexBuffer(_indexBuffer._buffer, 0,
                                vk::IndexType::eUint32);

  // TODO: Multiple binds for multiple pipelines and whatnot
  uint32_t drawStride = sizeof(DrawIndexedIndirectCommandBufferObject);
  commandBuffer.drawIndexedIndirect(
      _frames[_currentFrame]._indirectCommandBuffer._buffer, 0, numEntities,
      drawStride);
}

Material *VulkanEngine::createMaterial(uint32_t albedoTexture,
                                       const std::string &name) {
  Material mat{albedoTexture, 0, 0};
  // _materials[name] = std::move(mat);
  //  return &_materials[name];
  return nullptr;
}

Material *VulkanEngine::getMaterial(const std::string &name) {
  // auto it = _materials.find(name);
  // if (it == _materials.end()) {
  return nullptr;
  //}
  // return &(*it).second;
}

Mesh *VulkanEngine::getMesh(const std::string &name) {
  // auto it = _meshes.find(name);
  // if (it == _meshes.end()) {
  return nullptr;
  //}
  // return &(*it).second;
}

size_t VulkanEngine::padUniformBufferSize(size_t originalSize) {
  // Calculate required alignment based on minimum device offset alignment
  size_t minUboAlignment =
      _deviceProperties.limits.minUniformBufferOffsetAlignment;
  size_t alignedSize = originalSize;
  if (minUboAlignment > 0) {
    alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
  }
  return alignedSize;
}

void VulkanEngine::loadTextureFromFile(const std::string &filename,
                                       Texture &texture) {
  int texWidth, texHeight, texChannels;
  stbi_uc *pixels = stbi_load(filename.c_str(), &texWidth, &texHeight,
                              &texChannels, STBI_rgb_alpha);
  vk::DeviceSize imageSize = texWidth * texHeight * 4;

  texture.mipLevels = vkutils::getMipLevels(texWidth, texHeight);

  assert(pixels);

  AllocatedBuffer stagingBuffer{};
  vkutils::allocateBuffer(
      _allocator, imageSize, vk::BufferUsageFlagBits::eTransferSrc,
      VMA_MEMORY_USAGE_CPU_TO_GPU, vk::SharingMode::eExclusive, stagingBuffer);

  void *data;
  vmaMapMemory(_allocator, stagingBuffer._allocation, &data);
  memcpy(data, pixels, static_cast<size_t>(imageSize));
  vmaUnmapMemory(_allocator, stagingBuffer._allocation);

  vk::ImageCreateInfo imageCreateInfo{
      {},
      vk::ImageType::e2D,
      vk::Format::eR8G8B8A8Srgb,
      vk::Extent3D{static_cast<uint32_t>(texWidth),
                   static_cast<uint32_t>(texHeight), 1},
      texture.mipLevels,
      1,
      vk::SampleCountFlagBits::e1,
      vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eTransferSrc |
          vk::ImageUsageFlagBits::eTransferDst |
          vk::ImageUsageFlagBits::eSampled,
      vk::SharingMode::eExclusive};

  vkutils::allocateImage(_allocator, imageCreateInfo, VMA_MEMORY_USAGE_GPU_ONLY,
                         texture.image);

  transitionImageLayout(texture.image._image, vk::Format::eB8G8R8A8Srgb,
                        vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eTransferDstOptimal,
                        texture.mipLevels);

  copyBufferToImage(stagingBuffer._buffer, texture.image._image,
                    static_cast<uint32_t>(texWidth),
                    static_cast<uint32_t>(texHeight));

  generateMipmaps(texture.image._image, static_cast<uint32_t>(texWidth),
                  static_cast<uint32_t>(texHeight), texture.mipLevels);

  // Texture image view
  vk::ImageViewCreateInfo imageViewCi{
      vk::ImageViewCreateFlags{},
      texture.image._image,
      vk::ImageViewType::e2D,
      vk::Format::eR8G8B8A8Srgb,
      vk::ComponentMapping{},
      vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0,
                                texture.mipLevels, 0, 1}};

  texture.imageView = _device->createImageViewUnique(imageViewCi);
}

MeshData VulkanEngine::loadMeshFromFile(const std::string &filename) {
  std::vector<Vertex> vertices{};
  std::vector<uint32_t> indices{};

  std::vector<glm::vec3>
      vertexPositions{};  // Used for calculation bounding sphere

  tinyobj::attrib_t attrib{};
  std::vector<tinyobj::shape_t> shapes{};
  std::vector<tinyobj::material_t> materials{};
  std::string warn, err;

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                        filename.c_str())) {
    abort();
  }

  std::unordered_map<Vertex, uint32_t> uniqueVertices{};

  for (const auto &shape : shapes) {
    for (const auto &index : shape.mesh.indices) {
      Vertex vertex{};
      vertex._position = {
          attrib.vertices[3 * index.vertex_index + 0],
          attrib.vertices[3 * index.vertex_index + 1],
          attrib.vertices[3 * index.vertex_index + 2],
      };

      vertex._normal = {
          attrib.normals[3 * index.normal_index + 0],
          attrib.normals[3 * index.normal_index + 1],
          attrib.normals[3 * index.normal_index + 2],
      };

      vertex._texCoord = {
          attrib.texcoords[2 * index.texcoord_index + 0],
          1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
      };

      vertex._color = {1.0f, 1.0f, 1.0f};

      if (uniqueVertices.count(vertex) == 0) {
        uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
        vertices.push_back(vertex);
        vertexPositions.push_back(vertex._position);
      }

      indices.push_back(uniqueVertices[vertex]);
    }
  }

  glm::vec4 boundingSphere{};
  vkutils::computeBoundingSphere(boundingSphere, vertexPositions.data(),
                                 vertexPositions.size());

  return MeshData{.vertices = std::move(vertices),
                  .indices = std::move(indices),
                  .boundingSphere = boundingSphere};
}