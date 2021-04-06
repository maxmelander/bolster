#include "vk_engine.hpp"

#include <vulkan/vulkan_core.h>

#include <chrono>
#include <set>
#include <unordered_map>
#include <vulkan/vulkan.hpp>

#include "GLFW/glfw3.h"
#include "camera.hpp"
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
  initPipelineLayout();
  initMeshPipeline();

  initUniformBuffers();

  initTextureImage();
  initTextureImageSampler();
  initDescriptorPool();
  initDescriptorSets();

  initDrawCommandBuffers();

  initMesh();
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
                                 &tempSurface) != VK_SUCCESS);
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
      _depthImage._image.get(),
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
  vk::CommandPoolCreateInfo createInfo{};
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

void VulkanEngine::initPipelineLayout() {
  vk::PipelineLayoutCreateInfo createInfo{};
  createInfo.setLayoutCount = 1;
  createInfo.pSetLayouts = &_descriptorSetLayout.get();
  _pipelineLayout = _device->createPipelineLayoutUnique(createInfo);
}

void VulkanEngine::initMeshPipeline() {
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

  _meshPipeline = pipelineBuilder.buildPipeline(
      _device.get(), _renderPass.get(), _pipelineLayout.get());
}

void VulkanEngine::initUniformBuffers() {
  _uniformBuffers.reserve(_swapchainImages.size());

  vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

  for (size_t i{}; i < _swapchainImages.size(); i++) {
    AllocatedBuffer buffer{};
    vkutils::allocateBuffer(
        _allocator, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
        VMA_MEMORY_USAGE_CPU_TO_GPU, vk::SharingMode::eExclusive, buffer);
    _uniformBuffers.emplace_back(std::move(buffer));
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

  vk::ImageCreateInfo imageCreateInfo(
      {}, vk::ImageType::e2D, vk::Format::eR8G8B8A8Srgb,
      vk::Extent3D{static_cast<uint32_t>(texWidth),
                   static_cast<uint32_t>(texHeight), 1},
      _mipLevels, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eTransferSrc |
          vk::ImageUsageFlagBits::eTransferDst |
          vk::ImageUsageFlagBits::eSampled,
      vk::SharingMode::eExclusive);

  vkutils::allocateImage(_allocator, imageCreateInfo, VMA_MEMORY_USAGE_GPU_ONLY,
                         _textureImage);

  transitionImageLayout(_textureImage._image.get(), vk::Format::eB8G8R8A8Srgb,
                        vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eTransferDstOptimal, _mipLevels);

  copyBufferToImage(stagingBuffer._buffer.get(), _textureImage._image.get(),
                    static_cast<uint32_t>(texWidth),
                    static_cast<uint32_t>(texHeight));

  generateMipmaps(_textureImage._image.get(), static_cast<uint32_t>(texWidth),
                  static_cast<uint32_t>(texHeight), _mipLevels);

  // Texture image view
  vk::ImageViewCreateInfo imageViewCi{
      vk::ImageViewCreateFlags{},
      _textureImage._image.get(),
      vk::ImageViewType::e2D,
      vk::Format::eB8G8R8A8Srgb,
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
  allocInfo.descriptorSetCount = static_cast<uint32_t>(_swapchainImages.size());
  allocInfo.pSetLayouts = layouts.data();

  _descriptorSets = _device->allocateDescriptorSets(allocInfo);

  // Populate with descriptors
  for (size_t i{}; i < _swapchainImages.size(); i++) {
    vk::DescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = _uniformBuffers[i]._buffer.get();
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    vk::DescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    imageInfo.imageView = _textureImageView.get();
    imageInfo.sampler = _textureImageSampler.get();

    std::array<vk::WriteDescriptorSet, 2> descriptorWrites{};

    descriptorWrites[0].dstSet = _descriptorSets[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    descriptorWrites[1].dstSet = _descriptorSets[i];
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
                                          (uint32_t)_framebuffers.size()};

  _drawCommandBuffers = _device->allocateCommandBuffersUnique(allocInfo);

  // Record the draw commands
  for (size_t i{}; i < _drawCommandBuffers.size(); i++) {
    vk::CommandBufferBeginInfo beginInfo{};
    vk::CommandBuffer commandBuffer = _drawCommandBuffers[i].get();
    commandBuffer.begin(beginInfo);

    vk::RenderPassBeginInfo renderPassInfo{
        _renderPass.get(),
        _framebuffers[i].get(),
        vk::Rect2D{vk::Offset2D{0, 0}, vk::Extent2D{_swapchainExtent}},
    };

    std::array<vk::ClearValue, 2> clearValues{
        vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}},
        vk::ClearDepthStencilValue{1.0f, 0}};

    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues.data();

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                               _meshPipeline.get());

    std::array<vk::Buffer, 1> vertexBuffers{_mesh._vertexBuffer._buffer.get()};
    std::array<vk::DeviceSize, 1> offsets{0};
    commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
    commandBuffer.bindIndexBuffer(_mesh._indexBuffer._buffer.get(), 0,
                                  vk::IndexType::eUint32);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                     _pipelineLayout.get(), 0,
                                     _descriptorSets[i], nullptr);

    commandBuffer.drawIndexed(static_cast<uint32_t>(_mesh._indices.size()), 1,
                              0, 0, 0);
    commandBuffer.endRenderPass();
    commandBuffer.end();
  }
}

void VulkanEngine::initSyncObjects() {
  _imageAvailableSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
  _renderFinishedSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
  _inFlightFences.reserve(MAX_FRAMES_IN_FLIGHT);

  for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    _imageAvailableSemaphores.emplace_back(
        _device->createSemaphoreUnique(vk::SemaphoreCreateInfo{}));
    _renderFinishedSemaphores.emplace_back(
        _device->createSemaphoreUnique(vk::SemaphoreCreateInfo{}));

    _inFlightFences.emplace_back(_device->createFenceUnique(
        vk::FenceCreateInfo{vk::FenceCreateFlagBits::eSignaled}));
  }

  _imagesInFlight = std::vector<vk::Fence>(_swapchainImages.size(), nullptr);
}

// TODO: This should live in some kind of resource handler
void VulkanEngine::initMesh() {
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

  // Allocate vertex and index buffers
  vkutils::allocateBuffer(_allocator, sizeof(Vertex) * vertices.size(),
                          vk::BufferUsageFlagBits::eTransferDst |
                              vk::BufferUsageFlagBits::eVertexBuffer,
                          VMA_MEMORY_USAGE_GPU_ONLY,
                          vk::SharingMode::eExclusive, _mesh._vertexBuffer);

  vkutils::allocateBuffer(_allocator, sizeof(uint32_t) * indices.size(),
                          vk::BufferUsageFlagBits::eTransferDst |
                              vk::BufferUsageFlagBits::eIndexBuffer,
                          VMA_MEMORY_USAGE_GPU_ONLY,
                          vk::SharingMode::eExclusive, _mesh._indexBuffer);

  uploadMesh();
}

void VulkanEngine::uploadMesh() {
  // Allocate staging buffer
  vk::DeviceSize bufferSize = sizeof(Vertex) * _mesh._vertices.size();
  AllocatedBuffer stagingBuffer;
  vkutils::allocateBuffer(
      _allocator, bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
      VMA_MEMORY_USAGE_CPU_TO_GPU, vk::SharingMode::eExclusive, stagingBuffer);

  // Copy vertex data to staging buffer
  void *data;
  vmaMapMemory(_allocator, stagingBuffer._allocation, &data);
  memcpy(data, _mesh._vertices.data(), bufferSize);
  vmaUnmapMemory(_allocator, stagingBuffer._allocation);

  // Copy staging buffer to vertex buffer
  auto commandBuffer = beginSingleTimeCommands();
  vk::BufferCopy copyRegion{0, 0, bufferSize};
  commandBuffer->copyBuffer(stagingBuffer._buffer.get(),
                            _mesh._vertexBuffer._buffer.get(), copyRegion);
  endSingleTimeCommands(std::move(commandBuffer), _graphicsQueue);
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

  _pipelineLayout = {};
  initPipelineLayout();

  _meshPipeline = {};
  initMeshPipeline();

  _depthImage = {};
  initDepthImage();

  _framebuffers.clear();
  initFramebuffers();

  _uniformBuffers.clear();
  initUniformBuffers();

  _descriptorPool = {};
  initDescriptorPool();

  _descriptorSets.clear();
  initDescriptorSets();

  _drawCommandBuffers.clear();
  initDrawCommandBuffers();
}

// TODO: Remove this whole thing
void VulkanEngine::updateUniformBuffer(uint32_t imageIndex, Camera &camera,
                                       float deltaTime) {
  UniformBufferObject ubo{};
  ubo.model =
      glm::rotate(glm::mat4(1.0f), -1.5708f, glm::vec3(1.0f, 0.0f, 0.0f));

  ubo.view = camera.getView();

  if (_swapchainExtent.width > 0) {
    ubo.proj = glm::perspective(
        glm::radians(45.0f),
        (float)_swapchainExtent.width / _swapchainExtent.height, 0.1f, 100.0f);
  }

  ubo.proj[1][1] *= -1;

  void *data;
  vmaMapMemory(_allocator, _uniformBuffers[imageIndex]._allocation, &data);
  memcpy(data, &ubo, sizeof(ubo));
  vmaUnmapMemory(_allocator, _uniformBuffers[imageIndex]._allocation);
}

void VulkanEngine::draw(Camera &camera, float deltaTime) {
  auto waitResult = _device->waitForFences(
      1, &_inFlightFences[_currentFrame].get(), true, UINT64_MAX);
  assert(waitResult == vk::Result::eSuccess);

  auto imageIndex = _device->acquireNextImageKHR(
      _swapchain.get(), UINT64_MAX,
      _imageAvailableSemaphores[_currentFrame].get(), nullptr);

  if (imageIndex.result == vk::Result::eErrorOutOfDateKHR) {
    recreateSwapchain();
    return;
  }
  assert(imageIndex.result == vk::Result::eSuccess);

  // Check if a previous frame is using this image (i.e. there is a fence to
  // wait on)
  if (_imagesInFlight[imageIndex.value]) {
    auto waitResult = _device->waitForFences(
        1, &_imagesInFlight[imageIndex.value], true, UINT64_MAX);
    assert(waitResult == vk::Result::eSuccess);
  }
  // Mark the image as now being used by this frame
  _imagesInFlight[imageIndex.value] = _inFlightFences[_currentFrame].get();

  updateUniformBuffer(imageIndex.value, camera, deltaTime);

  vk::SubmitInfo submitInfo{};
  vk::Semaphore waitSemaphores[] = {
      _imageAvailableSemaphores[_currentFrame].get()};
  vk::PipelineStageFlags waitStages[] = {
      vk::PipelineStageFlagBits::eColorAttachmentOutput};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &_drawCommandBuffers[imageIndex.value].get();

  vk::Semaphore signalSemaphore[] = {
      _renderFinishedSemaphores[_currentFrame].get()};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphore;

  auto resetResult =
      _device->resetFences(1, &_inFlightFences[_currentFrame].get());
  assert(resetResult == vk::Result::eSuccess);

  _graphicsQueue.submit(std::array<vk::SubmitInfo, 1>{submitInfo},
                        _inFlightFences[_currentFrame].get());

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
