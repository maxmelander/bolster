#include "vk_engine.hpp"

#include <vulkan/vulkan_core.h>

#include <chrono>
#include <iostream>
#include <set>
#include <unordered_map>
#include <vulkan/vulkan.hpp>

#include "GLFW/glfw3.h"
#include "camera.hpp"
#include "glm/ext/matrix_transform.hpp"
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

  initDescriptorSetLayout();
  initMeshPipeline();

  initUniformBuffers();

  initTextureImage();
  initTextureImageSampler();
  initDescriptorPool();
  initDescriptorSets();
  initMesh();
  initScene();

  initDrawCommandBuffers();
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
  std::cout << _window << std::endl;
  std::cout << _instance.get() << std::endl;
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
}

void VulkanEngine::initDescriptorSetLayout() {
  // Uniform buffer
  vk::DescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;
  uboLayoutBinding.pImmutableSamplers = nullptr;

  // Texture sampler
  vk::DescriptorSetLayoutBinding samplerLayoutBinding{};
  samplerLayoutBinding.binding = 1;
  samplerLayoutBinding.descriptorType =
      vk::DescriptorType::eCombinedImageSampler;
  samplerLayoutBinding.descriptorCount = 1;
  samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
  samplerLayoutBinding.pImmutableSamplers = nullptr;

  std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {
      uboLayoutBinding, samplerLayoutBinding};
  vk::DescriptorSetLayoutCreateInfo createInfo{};
  createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  createInfo.pBindings = bindings.data();

  _descriptorSetLayout = _device->createDescriptorSetLayoutUnique(createInfo);
}

void VulkanEngine::initMeshPipeline() {
  // Pipeline Layout
  vk::PipelineLayoutCreateInfo createInfo{};
  createInfo.setLayoutCount = 1;
  createInfo.pSetLayouts = &_descriptorSetLayout.get();

  // Push constants
  vk::PushConstantRange pushConstant{vk::ShaderStageFlagBits::eVertex, 0,
                                     sizeof(MeshPushConstants)};

  createInfo.pPushConstantRanges = &pushConstant;
  createInfo.pushConstantRangeCount = 1;

  vk::UniquePipelineLayout pipelineLayout =
      _device->createPipelineLayoutUnique(createInfo);

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

  vk::UniquePipeline meshPipeline = pipelineBuilder.buildPipeline(
      _device.get(), _renderPass.get(), pipelineLayout.get());

  createMaterial(std::move(meshPipeline), std::move(pipelineLayout), "house");
}

void VulkanEngine::initUniformBuffers() {
  vk::DeviceSize bufferSize = sizeof(CameraBufferObject);

  for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    AllocatedBuffer buffer{};
    vkutils::allocateBuffer(
        _allocator, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
        VMA_MEMORY_USAGE_CPU_TO_GPU, vk::SharingMode::eExclusive, buffer);
    _frames[i]._cameraBuffer = std::move(buffer);
  }
}

void VulkanEngine::initTextureImage() {
  // Load texture from image file
  // TODO: Resource management

  int texWidth, texHeight, texChannels;
  stbi_uc *pixels = stbi_load("../textures/viking_room.png", &texWidth,
                              &texHeight, &texChannels, STBI_rgb_alpha);
  vk::DeviceSize imageSize = texWidth * texHeight * 4;

  _mipLevels = vkutils::getMipLevels(texWidth, texHeight);

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
      _mipLevels,
      1,
      vk::SampleCountFlagBits::e1,
      vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eTransferSrc |
          vk::ImageUsageFlagBits::eTransferDst |
          vk::ImageUsageFlagBits::eSampled,
      vk::SharingMode::eExclusive};

  vkutils::allocateImage(_allocator, imageCreateInfo, VMA_MEMORY_USAGE_GPU_ONLY,
                         _textureImage);

  transitionImageLayout(_textureImage._image, vk::Format::eB8G8R8A8Srgb,
                        vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eTransferDstOptimal, _mipLevels);

  copyBufferToImage(stagingBuffer._buffer, _textureImage._image,
                    static_cast<uint32_t>(texWidth),
                    static_cast<uint32_t>(texHeight));

  generateMipmaps(_textureImage._image, static_cast<uint32_t>(texWidth),
                  static_cast<uint32_t>(texHeight), _mipLevels);

  // Texture image view
  vk::ImageViewCreateInfo imageViewCi{
      vk::ImageViewCreateFlags{},
      _textureImage._image,
      vk::ImageViewType::e2D,
      vk::Format::eR8G8B8A8Srgb,
      vk::ComponentMapping{},
      vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, _mipLevels,
                                0, 1}};

  _textureImageView = _device->createImageViewUnique(imageViewCi);
}

void VulkanEngine::initTextureImageSampler() {
  auto deviceProperties = _physicalDevice.getProperties();

  vk::SamplerCreateInfo createInfo{};
  createInfo.magFilter = vk::Filter::eLinear;
  createInfo.minFilter = vk::Filter::eLinear;
  createInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
  createInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
  createInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
  createInfo.anisotropyEnable = true;
  createInfo.maxAnisotropy = deviceProperties.limits.maxSamplerAnisotropy;
  createInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
  createInfo.unnormalizedCoordinates = false;

  // NOTE: This is maily used for percentage-closer filtering on shadow maps
  createInfo.compareEnable = false;
  createInfo.compareOp = vk::CompareOp::eAlways;

  createInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
  createInfo.mipLodBias = 0.0f;
  createInfo.minLod = 0.0f;
  createInfo.maxLod = static_cast<float>(_mipLevels);

  _textureImageSampler = _device->createSamplerUnique(createInfo);
}

void VulkanEngine::initDescriptorPool() {
  std::array<vk::DescriptorPoolSize, 2> poolSizes{};
  poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
  poolSizes[0].descriptorCount = static_cast<uint32_t>(_swapchainImages.size());

  poolSizes[1].type = vk::DescriptorType::eCombinedImageSampler;
  poolSizes[1].descriptorCount = static_cast<uint32_t>(_swapchainImages.size());

  vk::DescriptorPoolCreateInfo createInfo{};
  createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  createInfo.pPoolSizes = poolSizes.data();
  createInfo.maxSets = static_cast<uint32_t>(_swapchainImages.size());

  _descriptorPool = _device->createDescriptorPoolUnique(createInfo);
}

void VulkanEngine::initDescriptorSets() {
  std::vector<vk::DescriptorSetLayout> layouts(_swapchainImages.size(),
                                               _descriptorSetLayout.get());

  vk::DescriptorSetAllocateInfo allocInfo{};
  allocInfo.descriptorPool = _descriptorPool.get();
  allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
  allocInfo.pSetLayouts = layouts.data();

  auto ds = _device->allocateDescriptorSetsUnique(allocInfo);
  for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    _frames[i]._globalDescriptorSet = std::move(ds[i]);
  }

  // Populate with descriptors
  for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = _frames[i]._cameraBuffer._buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(CameraBufferObject);

    vk::DescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    imageInfo.imageView = _textureImageView.get();
    imageInfo.sampler = _textureImageSampler.get();

    std::array<vk::WriteDescriptorSet, 2> descriptorWrites{};

    descriptorWrites[0].dstSet = _frames[i]._globalDescriptorSet.get();
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    descriptorWrites[1].dstSet = _frames[i]._globalDescriptorSet.get();
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType =
        vk::DescriptorType::eCombinedImageSampler;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;

    _device->updateDescriptorSets(descriptorWrites, nullptr);
  }
}

void VulkanEngine::initDrawCommandBuffers() {
  vk::CommandBufferAllocateInfo allocInfo{_commandPool.get(),
                                          vk::CommandBufferLevel::ePrimary,
                                          MAX_FRAMES_IN_FLIGHT};
  auto drawCommandBuffers = _device->allocateCommandBuffersUnique(allocInfo);

  for (uint32_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    _frames[i]._commandBuffer = std::move(drawCommandBuffers[i]);
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
  Mesh mesh;
  std::vector<Vertex> vertices{};
  std::vector<uint32_t> indices{};

  tinyobj::attrib_t attrib{};
  std::vector<tinyobj::shape_t> shapes{};
  std::vector<tinyobj::material_t> materials{};
  std::string warn, err;

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                        "../models/viking_room.obj")) {
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
      }

      indices.push_back(uniqueVertices[vertex]);
    }
  }

  mesh._indices = indices;
  mesh._vertices = vertices;

  // Allocate vertex and index buffers
  vkutils::allocateBuffer(_allocator, sizeof(Vertex) * vertices.size(),
                          vk::BufferUsageFlagBits::eTransferDst |
                              vk::BufferUsageFlagBits::eVertexBuffer,
                          VMA_MEMORY_USAGE_GPU_ONLY,
                          vk::SharingMode::eExclusive, mesh._vertexBuffer);

  vkutils::allocateBuffer(_allocator, sizeof(uint32_t) * indices.size(),
                          vk::BufferUsageFlagBits::eTransferDst |
                              vk::BufferUsageFlagBits::eIndexBuffer,
                          VMA_MEMORY_USAGE_GPU_ONLY,
                          vk::SharingMode::eExclusive, mesh._indexBuffer);

  uploadMesh(mesh);

  // Add to map
  _meshes["house"] = std::move(mesh);
}

void VulkanEngine::uploadMesh(const Mesh &mesh) {
  // Fill vertex buffer
  vk::DeviceSize bufferSize = sizeof(Vertex) * mesh._vertices.size();
  AllocatedBuffer vertexStagingBuffer;
  vkutils::allocateBuffer(_allocator, bufferSize,
                          vk::BufferUsageFlagBits::eTransferSrc,
                          VMA_MEMORY_USAGE_CPU_TO_GPU,
                          vk::SharingMode::eExclusive, vertexStagingBuffer);

  // Copy vertex data to staging buffer
  void *data;
  vmaMapMemory(_allocator, vertexStagingBuffer._allocation, &data);
  memcpy(data, mesh._vertices.data(), bufferSize);
  vmaUnmapMemory(_allocator, vertexStagingBuffer._allocation);

  // Copy staging buffer to vertex buffer
  auto commandBuffer = beginSingleTimeCommands();
  vk::BufferCopy copyRegion{0, 0, bufferSize};
  commandBuffer->copyBuffer(vertexStagingBuffer._buffer,
                            mesh._vertexBuffer._buffer, copyRegion);
  endSingleTimeCommands(std::move(commandBuffer), _graphicsQueue);

  // Fill infex buffer
  bufferSize = sizeof(uint32_t) * mesh._indices.size();
  AllocatedBuffer indexStagingBuffer;
  vkutils::allocateBuffer(_allocator, bufferSize,
                          vk::BufferUsageFlagBits::eTransferSrc,
                          VMA_MEMORY_USAGE_CPU_TO_GPU,
                          vk::SharingMode::eExclusive, indexStagingBuffer);

  vmaMapMemory(_allocator, indexStagingBuffer._allocation, &data);
  memcpy(data, mesh._indices.data(), bufferSize);
  vmaUnmapMemory(_allocator, indexStagingBuffer._allocation);

  // Copy staging buffer to index buffer
  commandBuffer = beginSingleTimeCommands();
  copyRegion = vk::BufferCopy{0, 0, bufferSize};
  commandBuffer->copyBuffer(indexStagingBuffer._buffer,
                            mesh._indexBuffer._buffer, copyRegion);
  endSingleTimeCommands(std::move(commandBuffer), _graphicsQueue);
}

void VulkanEngine::initScene() {
  for (int x{}; x < 10; x++) {
    for (int y{}; y < 10; y++) {
      RenderObject house;
      house.mesh = getMesh("house");
      house.material = getMaterial("house");
      house.transformMatrix =
          glm::rotate(glm::mat4(1.0f), -1.5708f, glm::vec3(1.0f, 0.0f, 0.0f)) *
          glm::translate(glm::mat4{1.0}, glm::vec3(x * 2.2, y * 2.2, 0.0f));

      _renderables.push_back(house);
    }
  }
}

/******  UTILS  ******/
vk::UniqueCommandBuffer VulkanEngine::beginSingleTimeCommands() {
  vk::CommandBufferAllocateInfo allocInfo{_commandPool.get(),
                                          vk::CommandBufferLevel::ePrimary, 1};

  auto commandBuffers = _device->allocateCommandBuffersUnique(allocInfo);

  vk::CommandBufferBeginInfo beginInfo{
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  commandBuffers[0]->begin(beginInfo);

  return std::move(commandBuffers[0]);
}

void VulkanEngine::endSingleTimeCommands(vk::UniqueCommandBuffer commandBuffer,
                                         const vk::Queue &queue) {
  commandBuffer->end();
  vk::SubmitInfo submitInfo{{}, {}, {}, 1, &commandBuffer.get()};
  queue.submit(std::array{submitInfo});
  queue.waitIdle();  // TODO: Is this something we actually want to do?
}

void VulkanEngine::transitionImageLayout(const vk::Image &image,
                                         vk::Format format,
                                         vk::ImageLayout oldLayout,
                                         vk::ImageLayout newLayout,
                                         uint32_t mipLevels) {
  auto commandBuffer = beginSingleTimeCommands();

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

  commandBuffer->pipelineBarrier(srcStage, dstStage, {}, {}, {}, barrier);

  endSingleTimeCommands(std::move(commandBuffer), _graphicsQueue);
}

void VulkanEngine::copyBufferToImage(const vk::Buffer &buffer,
                                     const vk::Image &image, uint32_t width,
                                     uint32_t height) {
  auto commandBuffer = beginSingleTimeCommands();

  vk::BufferImageCopy copyRegion{
      0,
      0,
      0,
      vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
      vk::Offset3D{0, 0, 0},
      vk::Extent3D{width, height, 1}};

  commandBuffer->copyBufferToImage(
      buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &copyRegion);

  endSingleTimeCommands(std::move(commandBuffer), _graphicsQueue);
}

void VulkanEngine::generateMipmaps(const vk::Image &image, int32_t texWidth,
                                   int32_t texHeight, uint32_t mipLevels) {
  auto commandBuffer = beginSingleTimeCommands();

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

    commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                   vk::PipelineStageFlagBits::eTransfer, {}, {},
                                   {}, barrier);

    vk::ImageBlit blit{
        vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, i - 1, 0,
                                   1},
        {vk::Offset3D{0, 0, 0}, vk::Offset3D{mipWidth, mipHeight, 1}},
        vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, i, 0, 1},
        {vk::Offset3D{0, 0, 0},
         vk::Offset3D{mipWidth > 1 ? mipWidth / 2 : 1,
                      mipHeight > 1 ? mipHeight / 2 : 1, 1}}};

    commandBuffer->blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image,
                             vk::ImageLayout::eTransferDstOptimal, 1, &blit,
                             vk::Filter::eLinear);

    barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                   vk::PipelineStageFlagBits::eFragmentShader,
                                   {}, {}, {}, barrier);

    if (mipWidth > 1) mipWidth /= 2;
    if (mipHeight > 1) mipHeight /= 2;
  }

  barrier.subresourceRange.baseMipLevel = mipLevels - 1;
  barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
  barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
  barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

  commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                 vk::PipelineStageFlagBits::eFragmentShader, {},
                                 {}, {}, barrier);

  endSingleTimeCommands(std::move(commandBuffer), _graphicsQueue);
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
  // _meshPipeline = {};
  initMeshPipeline();

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

// TODO: Remove this whole thing
void VulkanEngine::updateUniformBuffer(Camera &camera, float deltaTime) {
  CameraBufferObject ubo{};

  ubo.view = camera.getView();

  if (_swapchainExtent.width > 0) {
    ubo.proj = glm::perspective(
        glm::radians(45.0f),
        (float)_swapchainExtent.width / _swapchainExtent.height, 0.1f, 100.0f);
  }

  ubo.proj[1][1] *= -1;

  void *data;
  vmaMapMemory(_allocator, _frames[_currentFrame]._cameraBuffer._allocation,
               &data);
  memcpy(data, &ubo, sizeof(ubo));
  vmaUnmapMemory(_allocator, _frames[_currentFrame]._cameraBuffer._allocation);
}

void VulkanEngine::draw(Camera &camera, float deltaTime) {
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

  updateUniformBuffer(camera, deltaTime);

  // Record command buffer
  vk::CommandBufferBeginInfo beginInfo{
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  vk::CommandBuffer commandBuffer = _frames[_currentFrame]._commandBuffer.get();
  commandBuffer.begin(beginInfo);

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

  drawObjects(commandBuffer);

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

void VulkanEngine::drawObjects(vk::CommandBuffer commandBuffer) {
  Material *lastMaterial = nullptr;
  Mesh *lastMesh = nullptr;
  for (RenderObject renderObject : _renderables) {
    if (renderObject.mesh != lastMesh) {
      commandBuffer.bindVertexBuffers(
          0, {renderObject.mesh->_vertexBuffer._buffer}, {0});
      commandBuffer.bindIndexBuffer(renderObject.mesh->_indexBuffer._buffer, 0,
                                    vk::IndexType::eUint32);
    }

    if (renderObject.material != lastMaterial) {
      commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 renderObject.material->pipeline.get());
      commandBuffer.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics,
          renderObject.material->pipelineLayout.get(), 0,
          _frames[_currentFrame]._globalDescriptorSet.get(), nullptr);
    }

    // Push constant update
    MeshPushConstants constant{renderObject.transformMatrix};
    commandBuffer.pushConstants(renderObject.material->pipelineLayout.get(),
                                vk::ShaderStageFlagBits::eVertex, 0,
                                sizeof(MeshPushConstants), &constant);

    commandBuffer.drawIndexed(
        static_cast<uint32_t>(renderObject.mesh->_indices.size()), 1, 0, 0, 0);

    lastMaterial = renderObject.material;
    lastMesh = renderObject.mesh;
  }
}

Material *VulkanEngine::createMaterial(vk::UniquePipeline pipeline,
                                       vk::UniquePipelineLayout pipelineLayout,
                                       const std::string &name) {
  Material mat{std::move(pipeline), std::move(pipelineLayout)};
  _materials[name] = std::move(mat);
  return &_materials[name];
}

Material *VulkanEngine::getMaterial(const std::string &name) {
  auto it = _materials.find(name);
  if (it == _materials.end()) {
    return nullptr;
  }
  return &(*it).second;
}

Mesh *VulkanEngine::getMesh(const std::string &name) {
  auto it = _meshes.find(name);
  if (it == _meshes.end()) {
    return nullptr;
  }
  return &(*it).second;
}
