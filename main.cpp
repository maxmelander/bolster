#include "main.hpp"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "GLFW/glfw3.h"
#include "camera.hpp"
#include "dstack.hpp"
#include "tiny_obj_loader.h"
#include "utils.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static Camera camera{glm::vec3{2.0f, 2.0f, 2.0f}};

static float deltaTime = 0.0f;  // Time between current frame and last frame
static float lastFrame = 0.0f;  // Time of last frame

static float lastX = 400, lastY = 300;

Renderer::Renderer()
    : window{initWindow()},
      vkInstance{createInstance()},
      vkSurface{createSurface()},
      vkPhysicalDevice{pickPhysicalDevice()},
      vkDevice{createLogicalDevice()},

      vkGraphicsQueue{std::get<0>(getDeviceQueue())},
      vkPresentQueue{std::get<1>(getDeviceQueue())},

      vkSwapchain{createSwapchain()},
      vkSwapchainImages{getSwapchainImages()},
      vkSwapchainImageViews{createImageViews()},

      vkRenderPass{createRenderPass()},

      vkDescriptorSetLayout{createDescriptorSetLayout()},
      vkPipelineLayout{createPipelineLayout()},
      vkGraphicsPipeline{createGraphicsPipeline()},

      vkCommandPool{createCommandPool()},

      // Mesh stuff
      vertexPair{createVertexPair()},
      vertexBuffer{createBuffer(
          sizeof(std::get<0>(vertexPair)[0]) * std::get<0>(vertexPair).size(),
          vk::BufferUsageFlagBits::eTransferDst |
              vk::BufferUsageFlagBits::eVertexBuffer)},
      vertexBufferMemory{allocateBufferMemory(
          vertexBuffer.get(), vk::MemoryPropertyFlagBits::eHostVisible |
                                  vk::MemoryPropertyFlagBits::eHostCoherent)},
      indexBuffer{createBuffer(
          sizeof(std::get<1>(vertexPair)[0]) * std::get<1>(vertexPair).size(),
          vk::BufferUsageFlagBits::eTransferDst |
              vk::BufferUsageFlagBits::eIndexBuffer)},
      indexBufferMemory{allocateBufferMemory(
          indexBuffer.get(), vk::MemoryPropertyFlagBits::eHostVisible |
                                 vk::MemoryPropertyFlagBits::eHostCoherent)},
      uniformBuffers{createUniformBuffers()},
      uniformBuffersMemory{allocateUniformBuffersMemory()},
      textureImagePair{createTextureImage()},
      textureImageView{createTextureImageView()},
      textureSampler{createTextureSampler()},

      depthImagePair{createDepthImage()},
      depthImageView{createDepthImageView()},

      descriptorPool{createDescriptorPool()},
      descriptorSets{createDescriptorSets()},

      vkSwapchainFramebuffers{createFramebuffers()},

      vkCommandBuffers{createCommandBuffers()},
      vkImageAvailableSemaphores{createSemaphores()},
      vkRenderFinishedSemaphores{createSemaphores()},
      vkInFlightFences{createFences()},
      vkImagesInFlight{
          std::vector<vk::Fence>(vkSwapchainImages.size(), nullptr)} {
  fillVertexBuffer();
  fillIndexBuffer();

  DStack stack(4);
}

Renderer::~Renderer() {
  glfwDestroyWindow(window);
  glfwTerminate();
}

void Renderer::mouseCallback(GLFWwindow *window, double xpos, double ypos) {
  float xOffset = xpos - lastX;
  float yOffset = lastY - ypos;

  lastX = xpos;
  lastY = ypos;

  const float sensitivity = 0.1f;
  xOffset *= sensitivity;
  yOffset *= sensitivity;

  camera.yaw += xOffset;
  camera.pitch += yOffset;
}

void Renderer::processInput(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.setAcceleration(0.5f);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera.setAcceleration(-0.5f);
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    camera.setStrafeAcceleration(-0.5f);
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    camera.setStrafeAcceleration(0.5f);
}

void Renderer::run() {
  while (!glfwWindowShouldClose(window)) {
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    glfwPollEvents();
    processInput(window);
    camera.update(deltaTime);
    drawFrame();
  }

  vkDevice->waitIdle();
}

GLFWwindow *Renderer::initWindow() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  auto window = glfwCreateWindow(WIDTH, HEIGHT, WINDOW_TITLE, nullptr, nullptr);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(window, mouseCallback);
  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

  return window;
}

vk::UniqueInstance Renderer::createInstance() {
  try {
    vk::ApplicationInfo appInfo{"Bolster", VK_MAKE_VERSION(1, 0, 0),
                                "Vulkan.hpp", VK_MAKE_VERSION(1, 0, 0),
                                VK_API_VERSION_1_2};

    uint32_t glfwExtensionCount{};
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    vk::InstanceCreateInfo instanceCreateInfo{
        {}, &appInfo, {}, {}, glfwExtensionCount, glfwExtensions};

    // in debug mode, enable validation layers if
    // they are supported by the vk::Instance

#if !defined(NDEBUG)
    if (!checkValidationLayerSupport()) {
      throw std::runtime_error(
          "validation layers requested, but not available!");
    }
    instanceCreateInfo.enabledLayerCount =
        static_cast<uint32_t>(vk::utils::validationLayers.size());
    instanceCreateInfo.ppEnabledLayerNames = vk::utils::validationLayers.data();
#endif

    return vk::createInstanceUnique(instanceCreateInfo);
  } catch (vk::SystemError &err) {
    std::exit(-1);
  } catch (std::exception &err) {
    std::exit(-1);
  } catch (...) {
    std::exit(-1);
  }
}

vk::UniqueSurfaceKHR Renderer::createSurface() {
  auto tempSurface = VkSurfaceKHR{};
  if (glfwCreateWindowSurface(VkInstance{vkInstance.get()}, window, nullptr,
                              &tempSurface) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create window surface.");
  }
  return vk::UniqueSurfaceKHR{std::move(tempSurface)};
}

vk::PhysicalDevice Renderer::pickPhysicalDevice() {
  std::vector<vk::PhysicalDevice> physicalDevices =
      vkInstance->enumeratePhysicalDevices();

  auto device = std::find_if(
      physicalDevices.begin(), physicalDevices.end(), [this](auto const &pd) {
        // NOTE: Move this into a named function like deviceSuitable
        // or something like that perhaps
        auto queueFamilyIndices =
            vk::utils::findQueueFamilies(pd, vkSurface.get());

        auto swapChainSupport =
            vk::utils::querySwapchainSupport(pd, vkSurface.get());

        auto supportedFeatures = pd.getFeatures();

        return queueFamilyIndices.isComplete() &&
               vk::utils::checkDeviceExtensionSupport(pd) &&
               !swapChainSupport.formats.empty() &&
               !swapChainSupport.presentModes.empty() &&
               supportedFeatures.samplerAnisotropy;
      });

  if (device == physicalDevices.end()) {
    throw std::runtime_error(
        "Could not find a physical device with Vulkan support!");
  }

  return *device;
}

vk::UniqueDevice Renderer::createLogicalDevice() {
  auto indices =
      vk::utils::findQueueFamilies(vkPhysicalDevice, vkSurface.get());

  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies{indices.graphicsFamily.value(),
                                         indices.presentFamily.value()};

  float queuePriority = 1.0f;
  for (auto queueFamilyIndex : uniqueQueueFamilies) {
    queueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags{},
                                  indices.graphicsFamily.value(), 1,
                                  &queuePriority);
  }

  vk::PhysicalDeviceFeatures deviceFeatures{};
  deviceFeatures.samplerAnisotropy = true;

  vk::DeviceCreateInfo createInfo(
      vk::DeviceCreateFlags{}, static_cast<uint32_t>(queueCreateInfos.size()),
      queueCreateInfos.data());

  createInfo.pEnabledFeatures = &deviceFeatures;

  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(vk::utils::deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = vk::utils::deviceExtensions.data();

#if !defined(NDEBUG)
  createInfo.enabledLayerCount =
      static_cast<uint32_t>(vk::utils::validationLayers.size());
  createInfo.ppEnabledLayerNames = vk::utils::validationLayers.data();
#else
  createInfo.enabledLayerCount = 0;
#endif

  return vkPhysicalDevice.createDeviceUnique(createInfo);
}

std::tuple<vk::Queue, vk::Queue> Renderer::getDeviceQueue() {
  auto indices =
      vk::utils::findQueueFamilies(vkPhysicalDevice, vkSurface.get());
  return std::make_tuple(vkDevice->getQueue(indices.graphicsFamily.value(), 0),
                         vkDevice->getQueue(indices.presentFamily.value(), 0));
}

vk::UniqueSwapchainKHR Renderer::createSwapchain() {
  auto swapchainSupport =
      vk::utils::querySwapchainSupport(vkPhysicalDevice, vkSurface.get());

  auto surfaceFormat =
      vk::utils::chooseSwapSurfaceFormat(swapchainSupport.formats);
  auto presentMode =
      vk::utils::chooseSwapPresentMode(swapchainSupport.presentModes);
  auto extent =
      vk::utils::chooseSwapExtent(swapchainSupport.capabilities, window);

  uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
  if (swapchainSupport.capabilities.maxImageCount > 0 &&
      imageCount > swapchainSupport.capabilities.maxImageCount) {
    imageCount = swapchainSupport.capabilities.maxImageCount;
  }

  vk::SwapchainCreateInfoKHR createInfo{};
  createInfo.surface = vkSurface.get();
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

  auto indices =
      vk::utils::findQueueFamilies(vkPhysicalDevice, vkSurface.get());
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                   indices.presentFamily.value()};

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = vk::SharingMode::eExclusive;
  }

  createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
  createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  // createInfo.oldSwapchain = nullptr;

  // TODO: Setting these member variables
  // as a side effect in here is not nice
  swapchainExtent = extent;
  swapchainImageFormat = surfaceFormat.format;

  return vkDevice->createSwapchainKHRUnique(createInfo);
}

std::vector<vk::Image> Renderer::getSwapchainImages() {
  return vkDevice->getSwapchainImagesKHR(vkSwapchain.get());
}

// NOTE: Stack overflow says that this should be fine and not
// create any copies or moves. The return value here is a
// temporary, and the move constructor of vkSwapchainImageViews
// will automatically be called. But can spend some time to
// double check this at some point!
std::vector<vk::UniqueImageView> Renderer::createImageViews() {
  std::vector<vk::UniqueImageView> imageViews;
  imageViews.reserve(vkSwapchainImages.size());

  for (size_t i{}; i < vkSwapchainImages.size(); i++) {
    imageViews.emplace_back(vk::utils::createImageView(
        vkDevice.get(), vkSwapchainImages[i], swapchainImageFormat,
        vk::ImageAspectFlagBits::eColor, 1));
  }

  return imageViews;
}

// NOTE: There looks to be a lot of helper functions in vulkan.hpp
// to set things that might be easier than what I've done here.
vk::UniqueRenderPass Renderer::createRenderPass() {
  vk::AttachmentDescription colorAttachment{};
  colorAttachment.format = swapchainImageFormat;
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
  depthAttachment.format = vk::utils::findDepthFormat(vkPhysicalDevice);
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

  return vkDevice->createRenderPassUnique(renderPassInfo);
}

vk::UniqueDescriptorSetLayout Renderer::createDescriptorSetLayout() {
  vk::DescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;
  uboLayoutBinding.pImmutableSamplers = nullptr;

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
  return vkDevice->createDescriptorSetLayoutUnique(createInfo);
}

vk::UniquePipelineLayout Renderer::createPipelineLayout() {
  vk::PipelineLayoutCreateInfo createInfo{};
  createInfo.setLayoutCount = 1;
  createInfo.pSetLayouts = &vkDescriptorSetLayout.get();
  return vkDevice->createPipelineLayoutUnique(createInfo);
}

vk::UniquePipeline Renderer::createGraphicsPipeline() {
  // Programmable stages
  // NOTE: File paths are relative to the executable
  auto vertShaderCode = vk::utils::readFile("../shaders/vert.spv");
  auto fragShaderCode = vk::utils::readFile("../shaders/frag.spv");

  vk::UniqueShaderModule vertShaderModule =
      vk::utils::createUniqueShaderModule(vkDevice, vertShaderCode);
  vk::UniqueShaderModule fragShaderModule =
      vk::utils::createUniqueShaderModule(vkDevice, fragShaderCode);

  vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
      vk::PipelineShaderStageCreateFlags{}, vk::ShaderStageFlagBits::eVertex,
      vertShaderModule.get(), "main"};

  vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
      vk::PipelineShaderStageCreateFlags{}, vk::ShaderStageFlagBits::eFragment,
      fragShaderModule.get(), "main"};

  vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                      fragShaderStageInfo};

  // Fixed function stages

  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
  auto bindingDescription = Vertex::getBindingDescription();
  auto attributeDescriptions = Vertex::getAttributeDescriptions();
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
      vk::PipelineInputAssemblyStateCreateFlags{},
      vk::PrimitiveTopology::eTriangleList, false};

  vk::Viewport viewport{
      0.0f, 0.0f, (float)swapchainExtent.width, (float)swapchainExtent.height,
      0.0f, 1.0f};

  vk::Rect2D scissor{vk::Offset2D{0, 0}, swapchainExtent};

  vk::PipelineViewportStateCreateInfo viewportStateInfo{
      vk::PipelineViewportStateCreateFlags{}, 1, &viewport, 1, &scissor};

  vk::PipelineRasterizationStateCreateInfo rasterizationInfo{
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

  // NOTE: Defaults to off. Requires enabling a GPU feature if used
  vk::PipelineMultisampleStateCreateInfo multisampleInfo{};

  // NOTE: This is used for things like transparency
  vk::PipelineColorBlendAttachmentState colorBlendAttachmentInfo{false};
  colorBlendAttachmentInfo.colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  vk::PipelineColorBlendStateCreateInfo colorBlendingInfo{
      vk::PipelineColorBlendStateCreateFlags{}, false, vk::LogicOp::eCopy, 1,
      &colorBlendAttachmentInfo};

  vk::PipelineDepthStencilStateCreateInfo depthStencilInfo{};
  depthStencilInfo.depthTestEnable = true;
  depthStencilInfo.depthWriteEnable = true;
  depthStencilInfo.depthCompareOp = vk::CompareOp::eLess;
  depthStencilInfo.depthBoundsTestEnable = false;
  depthStencilInfo.stencilTestEnable = false;

  // Stuff that can be changed during runtime, without recreating the pipeline
  // vk::DynamicState dynamicStates[] = {vk::DynamicState::eViewport,
  // vk::DynamicState::eLineWidth};

  // vk::PipelineDynamicStateCreateInfo dynamicStateInfo{
  // vk::PipelineDynamicStateCreateFlags{}, 2, dynamicStatesInfo};

  vk::GraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
  pipelineInfo.pViewportState = &viewportStateInfo;
  pipelineInfo.pRasterizationState = &rasterizationInfo;
  pipelineInfo.pMultisampleState = &multisampleInfo;
  pipelineInfo.pDepthStencilState = &depthStencilInfo;
  pipelineInfo.pColorBlendState = &colorBlendingInfo;
  // pipelineInfo.pDynamicState = nullptr;
  pipelineInfo.layout = vkPipelineLayout.get();
  pipelineInfo.renderPass = vkRenderPass.get();
  pipelineInfo.subpass =
      0;  // index of subpass where this pipeline will be used

  // vkDevice->createGraphicsPipelineUnique(vk::PipelineCache{});
  auto result = vkDevice->createGraphicsPipelineUnique(nullptr, pipelineInfo);
  assert(result.result == vk::Result::eSuccess);
  return std::move(result.value);
}

// NOTE: I created an error where I initialized a vector with a size,
// then used emplace_back. Meaning I had a bunch of empty shit in the front,
// then the actual content.
std::vector<vk::UniqueFramebuffer> Renderer::createFramebuffers() {
  std::vector<vk::UniqueFramebuffer> framebuffers;
  framebuffers.reserve(vkSwapchainImageViews.size());

  for (size_t i{}; i < vkSwapchainImageViews.size(); i++) {
    std::array<vk::ImageView, 2> attachments = {vkSwapchainImageViews[i].get(),
                                                depthImageView.get()};

    vk::FramebufferCreateInfo createInfo{};
    createInfo.renderPass = vkRenderPass.get();
    createInfo.attachmentCount = 2;
    createInfo.pAttachments = attachments.data();
    createInfo.width = swapchainExtent.width;
    createInfo.height = swapchainExtent.height;
    createInfo.layers = 1;

    framebuffers.emplace_back(vkDevice->createFramebufferUnique(createInfo));
  }

  return framebuffers;
}

vk::UniqueCommandPool Renderer::createCommandPool() {
  vk::utils::QueueFamilyIndices queueFamilyIndices =
      vk::utils::findQueueFamilies(vkPhysicalDevice, vkSurface.get());

  vk::CommandPoolCreateInfo createInfo{};
  createInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

  return vkDevice->createCommandPoolUnique(createInfo);
}

std::vector<vk::UniqueCommandBuffer> Renderer::createCommandBuffers() {
  vk::CommandBufferAllocateInfo allocInfo{
      vkCommandPool.get(), vk::CommandBufferLevel::ePrimary,
      (uint32_t)vkSwapchainFramebuffers.size()};
  auto commandBuffers = vkDevice->allocateCommandBuffersUnique(allocInfo);

  // Record the command buffers
  for (size_t i{}; i < commandBuffers.size(); i++) {
    vk::CommandBufferBeginInfo beginInfo{};
    vk::CommandBuffer commandBuffer = commandBuffers[i].get();
    commandBuffer.begin(beginInfo);

    vk::RenderPassBeginInfo renderPassInfo{
        vkRenderPass.get(),
        vkSwapchainFramebuffers[i].get(),
        vk::Rect2D{vk::Offset2D{0, 0}, vk::Extent2D{swapchainExtent}},
    };

    std::array<vk::ClearValue, 2> clearValues{
        vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}},
        vk::ClearDepthStencilValue{1.0f, 0}};

    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues.data();

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                               vkGraphicsPipeline.get());

    std::array<vk::Buffer, 1> vertexBuffers{vertexBuffer.get()};
    std::array<vk::DeviceSize, 1> offsets{0};
    commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
    commandBuffer.bindIndexBuffer(indexBuffer.get(), 0, vk::IndexType::eUint32);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                     vkPipelineLayout.get(), 0,
                                     descriptorSets[i], nullptr);

    commandBuffer.drawIndexed(
        static_cast<uint32_t>(std::get<1>(vertexPair).size()), 1, 0, 0, 0);
    commandBuffer.endRenderPass();
    commandBuffer.end();
  }

  return commandBuffers;
}

// TODO: These two sync object creation functions
// could easily be parameterized and moved into utils
std::vector<vk::UniqueSemaphore> Renderer::createSemaphores() {
  std::vector<vk::UniqueSemaphore> semaphores{};
  semaphores.reserve(MAX_FRAMES_IN_FLIGHT);

  for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    semaphores.emplace_back(
        vkDevice->createSemaphoreUnique(vk::SemaphoreCreateInfo{}));
  }

  return semaphores;
}

std::vector<vk::UniqueFence> Renderer::createFences() {
  std::vector<vk::UniqueFence> fences{};
  fences.reserve(MAX_FRAMES_IN_FLIGHT);

  for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
    fences.emplace_back(vkDevice->createFenceUnique(
        vk::FenceCreateInfo{vk::FenceCreateFlagBits::eSignaled}));
  }

  return fences;
}

std::pair<std::vector<Vertex>, std::vector<uint32_t>>
Renderer::createVertexPair() {
  std::vector<Vertex> vertices{};
  std::vector<uint32_t> indices{};

  tinyobj::attrib_t attrib{};
  std::vector<tinyobj::shape_t> shapes{};
  std::vector<tinyobj::material_t> materials{};
  std::string warn, err;

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                        MODEL_PATH.c_str())) {
    throw std::runtime_error(warn + err);
  }

  std::unordered_map<Vertex, uint32_t> uniqueVertices{};

  for (const auto &shape : shapes) {
    for (const auto &index : shape.mesh.indices) {
      Vertex vertex{};
      vertex.pos = {
          attrib.vertices[3 * index.vertex_index + 0],
          attrib.vertices[3 * index.vertex_index + 1],
          attrib.vertices[3 * index.vertex_index + 2],
      };

      vertex.texCoord = {
          attrib.texcoords[2 * index.texcoord_index + 0],
          1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
      };

      vertex.color = {1.0f, 1.0f, 1.0f};

      if (uniqueVertices.count(vertex) == 0) {
        uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
        vertices.push_back(vertex);
      }

      indices.push_back(uniqueVertices[vertex]);
    }
  }

  return std::make_pair(std::move(vertices), std::move(indices));
}

vk::UniqueBuffer Renderer::createBuffer(vk::DeviceSize size,
                                        vk::BufferUsageFlags usage) {
  vk::BufferCreateInfo bufferInfo{};
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = vk::SharingMode::eExclusive;
  return vkDevice->createBufferUnique(bufferInfo);
}

vk::UniqueDeviceMemory Renderer::allocateBufferMemory(
    const vk::Buffer &buffer, vk::MemoryPropertyFlags properties) {
  auto memoryRequirements = vkDevice->getBufferMemoryRequirements(buffer);

  vk::MemoryAllocateInfo allocInfo{};
  allocInfo.allocationSize = memoryRequirements.size;
  allocInfo.memoryTypeIndex = vk::utils::findMemoryType(
      vkPhysicalDevice, memoryRequirements.memoryTypeBits, properties);

  auto bufferMemory = vkDevice->allocateMemoryUnique(allocInfo);

  vkDevice->bindBufferMemory(buffer, bufferMemory.get(), 0);

  return bufferMemory;
}

// TODO: Should probably wrap all of this stuff into smaller classes
// to hide away most of the shit that is the same every time.
vk::UniqueImage Renderer::createImage(vk::Extent3D extent, uint32_t mipLevels,
                                      vk::Format format, vk::ImageTiling tiling,
                                      vk::ImageUsageFlags usage) {
  vk::ImageCreateInfo createInfo({}, vk::ImageType::e2D, format, extent,
                                 mipLevels, 1, vk::SampleCountFlagBits::e1,
                                 tiling, usage, vk::SharingMode::eExclusive);

  return vkDevice->createImageUnique(createInfo);
}

// TODO: Use Vulkan Memory Allocator for allocating memory
// for both buffers and images. This would return images and buffers
// with allocated memory already bound to them. Which would be a lot nicer than
// doing all of this stuff all the time. And also this way of doing it
// is not optimal.
vk::UniqueDeviceMemory Renderer::allocateImageMemory(
    const vk::Image &image, vk::MemoryPropertyFlags properties) {
  auto memRequirements = vkDevice->getImageMemoryRequirements(image);

  vk::MemoryAllocateInfo allocInfo(
      memRequirements.size,
      vk::utils::findMemoryType(vkPhysicalDevice,
                                memRequirements.memoryTypeBits, properties));

  auto imageMemory = vkDevice->allocateMemoryUnique(allocInfo);
  vkDevice->bindImageMemory(image, imageMemory.get(), 0);
  return imageMemory;
}

// TODO: Create a command buffer that we record all
// setup commands into, then at the end of the init
// we flush/end it. That way, all setup stuff can happen
// async on the graphcis card
std::pair<vk::UniqueImage, vk::UniqueDeviceMemory>
Renderer::createTextureImage() {
  // Load texture from image file
  //
  int texWidth, texHeight, texChannels;
  stbi_uc *pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight,
                              &texChannels, STBI_rgb_alpha);
  vk::DeviceSize imageSize = texWidth * texHeight * 4;

  // TODO: Setting a member variable as a side-effect here
  mipLevels = vk::utils::getMipLevels(texWidth, texHeight);

  if (!pixels) {
    throw std::runtime_error("Failed to load texture image");
  }

  // Create staging buffer
  auto stagingBuffer =
      createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc);
  auto stagingMemory = allocateBufferMemory(
      stagingBuffer.get(), vk::MemoryPropertyFlagBits::eHostVisible |
                               vk::MemoryPropertyFlagBits::eHostCoherent);

  // Copy data from cpu to gpu
  auto data = vkDevice->mapMemory(stagingMemory.get(), 0, imageSize);
  memcpy(data, pixels, static_cast<size_t>(imageSize));
  vkDevice->unmapMemory(stagingMemory.get());
  stbi_image_free(pixels);

  auto textureImage = createImage(
      vk::Extent3D{static_cast<uint32_t>(texWidth),
                   static_cast<uint32_t>(texHeight), 1},
      mipLevels, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eTransferSrc |
          vk::ImageUsageFlagBits::eTransferDst |
          vk::ImageUsageFlagBits::eSampled);

  auto textureMemory = allocateImageMemory(
      textureImage.get(), vk::MemoryPropertyFlagBits::eDeviceLocal);

  vk::utils::transitionImageLayout(
      vkDevice.get(), vkCommandPool.get(), vkGraphicsQueue, textureImage.get(),
      vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined,
      vk::ImageLayout::eTransferDstOptimal, mipLevels);

  vk::utils::copyBufferToImage(
      vkDevice.get(), vkCommandPool.get(), vkGraphicsQueue, stagingBuffer.get(),
      textureImage.get(), static_cast<uint32_t>(texWidth),
      static_cast<uint32_t>(texHeight));

  vk::utils::generateMipmaps(vkDevice.get(), vkCommandPool.get(),
                             vkGraphicsQueue, textureImage.get(),
                             static_cast<uint32_t>(texWidth),
                             static_cast<uint32_t>(texHeight), mipLevels);

  return std::make_pair(std::move(textureImage), std::move(textureMemory));
}

std::vector<vk::UniqueBuffer> Renderer::createUniformBuffers() {
  std::vector<vk::UniqueBuffer> uniformBuffers{};
  vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
  uniformBuffers.reserve(vkSwapchainImages.size());

  for (size_t i{}; i < vkSwapchainImages.size(); i++) {
    uniformBuffers.emplace_back(
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer));
  }

  return uniformBuffers;
}

vk::UniqueImageView Renderer::createTextureImageView() {
  return vk::utils::createImageView(
      vkDevice.get(), std::get<0>(textureImagePair).get(),
      vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, mipLevels);
}

vk::UniqueSampler Renderer::createTextureSampler() {
  vk::SamplerCreateInfo createInfo{};
  createInfo.magFilter = vk::Filter::eLinear;
  createInfo.minFilter = vk::Filter::eLinear;
  createInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
  createInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
  createInfo.addressModeW = vk::SamplerAddressMode::eRepeat;

  // TODO: Device properties should probably only be queried
  // once, then be a global thingy
  auto deviceProperties = vkPhysicalDevice.getProperties();
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
  createInfo.maxLod = static_cast<float>(mipLevels);

  return vkDevice->createSamplerUnique(createInfo);
}

std::pair<vk::UniqueImage, vk::UniqueDeviceMemory>
Renderer::createDepthImage() {
  auto depthFormat = vk::utils::findDepthFormat(vkPhysicalDevice);

  auto depthImage = createImage(
      vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1}, 1,
      depthFormat, vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eDepthStencilAttachment);

  auto depthMemory = allocateImageMemory(
      depthImage.get(), vk::MemoryPropertyFlagBits::eDeviceLocal);

  return std::make_pair(std::move(depthImage), std::move(depthMemory));
}

vk::UniqueImageView Renderer::createDepthImageView() {
  auto depthFormat = vk::utils::findDepthFormat(vkPhysicalDevice);
  return vk::utils::createImageView(
      vkDevice.get(), std::get<0>(depthImagePair).get(), depthFormat,
      vk::ImageAspectFlagBits::eDepth, 1);
}

std::vector<vk::UniqueDeviceMemory> Renderer::allocateUniformBuffersMemory() {
  std::vector<vk::UniqueDeviceMemory> buffersMemory{};

  for (const auto &buffer : uniformBuffers) {
    buffersMemory.emplace_back(allocateBufferMemory(
        buffer.get(), vk::MemoryPropertyFlagBits::eHostVisible |
                          vk::MemoryPropertyFlagBits::eHostCoherent));
  }

  return buffersMemory;
}

// TODO: Use the vulkan memory library for allocation
// with pools and stuff like that
vk::UniqueDescriptorPool Renderer::createDescriptorPool() {
  std::array<vk::DescriptorPoolSize, 2> poolSizes{};
  poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
  poolSizes[0].descriptorCount =
      static_cast<uint32_t>(vkSwapchainImages.size());
  poolSizes[1].type = vk::DescriptorType::eCombinedImageSampler;
  poolSizes[1].descriptorCount =
      static_cast<uint32_t>(vkSwapchainImages.size());

  vk::DescriptorPoolCreateInfo createInfo{};
  createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  createInfo.pPoolSizes = poolSizes.data();
  createInfo.maxSets = static_cast<uint32_t>(vkSwapchainImages.size());

  return vkDevice->createDescriptorPoolUnique(createInfo);
}

std::vector<vk::DescriptorSet> Renderer::createDescriptorSets() {
  std::vector<vk::DescriptorSetLayout> layouts(vkSwapchainImages.size(),
                                               vkDescriptorSetLayout.get());

  vk::DescriptorSetAllocateInfo allocInfo{};
  allocInfo.descriptorPool = descriptorPool.get();
  allocInfo.descriptorSetCount =
      static_cast<uint32_t>(vkSwapchainImages.size());
  allocInfo.pSetLayouts = layouts.data();

  auto descriptorSets = vkDevice->allocateDescriptorSets(allocInfo);

  // Populate with descriptors
  for (size_t i{}; i < vkSwapchainImages.size(); i++) {
    vk::DescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffers[i].get();
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    vk::DescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    imageInfo.imageView = textureImageView.get();
    imageInfo.sampler = textureSampler.get();

    std::array<vk::WriteDescriptorSet, 2> descriptorWrites{};

    descriptorWrites[0].dstSet = descriptorSets[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    descriptorWrites[1].dstSet = descriptorSets[i];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType =
        vk::DescriptorType::eCombinedImageSampler;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;

    vkDevice->updateDescriptorSets(descriptorWrites, nullptr);
  }

  return descriptorSets;
}

void Renderer::fillVertexBuffer() {
  // Create a temporary staging buffer
  vk::DeviceSize bufferSize =
      sizeof(std::get<0>(vertexPair)[0]) * std::get<0>(vertexPair).size();

  vk::UniqueBuffer stagingBuffer =
      createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc);
  vk::UniqueDeviceMemory stagingBufferMemory = allocateBufferMemory(
      stagingBuffer.get(), vk::MemoryPropertyFlagBits::eHostVisible |
                               vk::MemoryPropertyFlagBits::eHostCoherent);

  // Copy vertex data into staging buffer
  auto data = vkDevice->mapMemory(stagingBufferMemory.get(), 0, bufferSize);
  memcpy(data, std::get<0>(vertexPair).data(), (size_t)bufferSize);

  vk::utils::copyBuffer(vkDevice.get(), vkCommandPool.get(), vkGraphicsQueue,
                        stagingBuffer.get(), vertexBuffer.get(), bufferSize);
}

void Renderer::fillIndexBuffer() {
  // Create a temporary staging buffer
  vk::DeviceSize bufferSize =
      sizeof(std::get<1>(vertexPair)[0]) * std::get<1>(vertexPair).size();

  vk::UniqueBuffer stagingBuffer =
      createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc);
  vk::UniqueDeviceMemory stagingBufferMemory = allocateBufferMemory(
      stagingBuffer.get(), vk::MemoryPropertyFlagBits::eHostVisible |
                               vk::MemoryPropertyFlagBits::eHostCoherent);

  // Copy vertex data into staging buffer
  auto data = vkDevice->mapMemory(stagingBufferMemory.get(), 0, bufferSize);
  memcpy(data, std::get<1>(vertexPair).data(), (size_t)bufferSize);

  vk::utils::copyBuffer(vkDevice.get(), vkCommandPool.get(), vkGraphicsQueue,
                        stagingBuffer.get(), indexBuffer.get(), bufferSize);
}

#if !defined(NDEBUG)
bool Renderer::checkValidationLayerSupport() {
  uint32_t layerCount;
  if (vk::enumerateInstanceLayerProperties(&layerCount, nullptr) !=
      vk::Result::eSuccess) {
    throw std::runtime_error("Could not get instance layer properties count.");
  };

  std::vector<vk::LayerProperties> availableLayers(layerCount);
  if (vk::enumerateInstanceLayerProperties(
          &layerCount, availableLayers.data()) != vk::Result::eSuccess) {
    throw std::runtime_error("Could not get instance layer properties.");
  };

  for (const char *layerName : vk::utils::validationLayers) {
    bool layerFound = false;

    for (const auto &layerProperty : availableLayers) {
      if (std::strcmp(layerName, layerProperty.layerName) == 0) {
        layerFound = true;
        break;
      }
    }

    if (!layerFound) {
      return false;
    }
  }

  return true;
}
#endif

// NOTE: Destroy and recreate anything that is dependent
// on the swapchain. Mostly swapchainextent aka screen size
void Renderer::recreateSwapchain() {
  // If minimized or size == 0 for some other reason,
  // wait until the size is > 0 again
  int width{}, height{};
  glfwGetFramebufferSize(window, &width, &height);

  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(window, &width, &height);
    glfwWaitEvents();
  }

  vkDevice->waitIdle();
  // NOTE: Needs investigation
  // No explicit cleanup needed since the vulkan.hpp
  // unique wrappers will handle this for us when they
  // are dropped.
  vkSwapchain = {};
  vkSwapchain = createSwapchain();

  vkSwapchainImages.clear();
  vkSwapchainImages = getSwapchainImages();

  vkSwapchainImageViews.clear();
  vkSwapchainImageViews = createImageViews();

  vkRenderPass = {};
  vkRenderPass = createRenderPass();

  vkPipelineLayout = {};
  vkPipelineLayout = createPipelineLayout();

  vkGraphicsPipeline = {};
  vkGraphicsPipeline = createGraphicsPipeline();

  depthImagePair = {};
  depthImagePair = createDepthImage();

  depthImageView = {};
  depthImageView = createDepthImageView();

  vkSwapchainFramebuffers.clear();
  vkSwapchainFramebuffers = createFramebuffers();

  uniformBuffers.clear();
  uniformBuffersMemory.clear();
  uniformBuffers = createUniformBuffers();
  uniformBuffersMemory = allocateUniformBuffersMemory();

  descriptorPool = {};
  descriptorPool = createDescriptorPool();

  descriptorSets.clear();
  descriptorSets = createDescriptorSets();

  vkCommandBuffers.clear();
  vkCommandBuffers = createCommandBuffers();
}

void Renderer::updateUniformBuffer(uint32_t imageIndex) {
  static auto startTime = std::chrono::high_resolution_clock::now();

  auto currentTime = std::chrono::high_resolution_clock::now();

  float time = std::chrono::duration<float, std::chrono::seconds::period>(
                   currentTime - startTime)
                   .count();

  UniformBufferObject ubo{};
  ubo.model =
      glm::rotate(glm::mat4(1.0f), -1.5708f, glm::vec3(1.0f, 0.0f, 0.0f));

  ubo.view = camera.getView();

  // glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
  // glm::vec3(0.0f, 0.0f, 1.0f));

  // TODO: Why is swapchainExtent 0 the first time around?
  if (swapchainExtent.width > 0) {
    ubo.proj = glm::perspective(
        glm::radians(45.0f),
        (float)swapchainExtent.width / swapchainExtent.height, 0.1f, 100.0f);
  }

  ubo.proj[1][1] *= -1;

  auto data = vkDevice->mapMemory(uniformBuffersMemory[imageIndex].get(), 0,
                                  sizeof(ubo));
  memcpy(data, &ubo, sizeof(ubo));
  vkDevice->unmapMemory(uniformBuffersMemory[imageIndex].get());
}

void Renderer::drawFrame() {
  auto waitResult = vkDevice->waitForFences(
      1, &vkInFlightFences[currentFrame].get(), true, UINT64_MAX);
  assert(waitResult == vk::Result::eSuccess);

  auto imageIndex = vkDevice->acquireNextImageKHR(
      vkSwapchain.get(), UINT64_MAX,
      vkImageAvailableSemaphores[currentFrame].get(), nullptr);

  if (imageIndex.result == vk::Result::eErrorOutOfDateKHR) {
    recreateSwapchain();
    return;
  }
  assert(imageIndex.result == vk::Result::eSuccess);

  // Check if a previous frame is using this image (i.e. there is a fence to
  // wait on)
  if (vkImagesInFlight[imageIndex.value]) {
    auto waitResult = vkDevice->waitForFences(
        1, &vkImagesInFlight[imageIndex.value], true, UINT64_MAX);
    assert(waitResult == vk::Result::eSuccess);
  }
  // Mark the image as now being used by this frame
  vkImagesInFlight[imageIndex.value] = vkInFlightFences[currentFrame].get();

  updateUniformBuffer(imageIndex.value);

  vk::SubmitInfo submitInfo{};
  vk::Semaphore waitSemaphores[] = {
      vkImageAvailableSemaphores[currentFrame].get()};
  vk::PipelineStageFlags waitStages[] = {
      vk::PipelineStageFlagBits::eColorAttachmentOutput};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &vkCommandBuffers[imageIndex.value].get();

  vk::Semaphore signalSemaphore[] = {
      vkRenderFinishedSemaphores[currentFrame].get()};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphore;

  auto resetResult =
      vkDevice->resetFences(1, &vkInFlightFences[currentFrame].get());
  assert(resetResult == vk::Result::eSuccess);

  vkGraphicsQueue.submit(std::array<vk::SubmitInfo, 1>{submitInfo},
                         vkInFlightFences[currentFrame].get());

  vk::SwapchainKHR swapchains[] = {vkSwapchain.get()};
  vk::PresentInfoKHR presentInfo{1, signalSemaphore, 1, swapchains,
                                 &imageIndex.value};

  auto presentResult = vkPresentQueue.presentKHR(&presentInfo);

  if (presentResult == vk::Result::eErrorOutOfDateKHR ||
      presentResult == vk::Result::eSuboptimalKHR || framebufferResized) {
    framebufferResized = false;
    recreateSwapchain();
  } else {
    assert(presentResult == vk::Result::eSuccess);
  }

  currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

int main() {
  Renderer renderer{};
  renderer.run();

  return 0;
}
