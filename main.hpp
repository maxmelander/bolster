#ifndef __MAIN_H_
#define __MAIN_H_

#include <chrono>
#include <tuple>
#include <utility>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <optional>
#include <vulkan/vulkan.hpp>

#include "GLFW/glfw3.h"
#include "glm/ext/matrix_transform.hpp"
#include "utils.hpp"

constexpr const uint32_t WIDTH = 800;
constexpr const uint32_t HEIGHT = 600;
constexpr const char *WINDOW_TITLE = "Bolster";

struct UniformBufferObject {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
};

struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;

  static vk::VertexInputBindingDescription getBindingDescription() {
    vk::VertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = vk::VertexInputRate::eVertex;

    return bindingDescription;
  }

  static std::array<vk::VertexInputAttributeDescription, 2>
  getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = vk::Format::eR32G32Sfloat;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[1].offset = offsetof(Vertex, color);
    return attributeDescriptions;
  }
};

const std::array<Vertex, 4> vertices{Vertex{{-0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
                                     Vertex{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                     Vertex{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                     Vertex{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};

const std::array<uint16_t, 6> indices{0, 1, 2, 2, 3, 0};

// NOTE: Some of these util type functions
// should live under a util namespace and not
// be part of the class definition
class Renderer {
  // NOTE: Rule of 8 or whatever?
  // What is the correct way of handling that when we know
  // we only ever want one renderer?
 public:
  Renderer();
  ~Renderer();
  void run();

 private:
  GLFWwindow *initWindow();
  vk::UniqueInstance createInstance();
  vk::UniqueSurfaceKHR createSurface();
  vk::PhysicalDevice pickPhysicalDevice();
  vk::UniqueDevice createLogicalDevice();
  std::tuple<vk::Queue, vk::Queue> getDeviceQueue();
  vk::UniqueSwapchainKHR createSwapchain();
  std::vector<vk::Image> getSwapchainImages();
  std::vector<vk::UniqueImageView> createImageViews();
  vk::UniqueRenderPass createRenderPass();
  vk::UniqueDescriptorSetLayout createDescriptorSetLayout();
  vk::UniquePipelineLayout createPipelineLayout();
  vk::UniquePipeline createGraphicsPipeline();
  std::vector<vk::UniqueFramebuffer> createFramebuffers();
  vk::UniqueCommandPool createCommandPool();
  vk::UniqueBuffer createBuffer(vk::DeviceSize, vk::BufferUsageFlags);
  vk::UniqueDeviceMemory allocateBufferMemory(const vk::Buffer &,
                                              vk::MemoryPropertyFlags);
  vk::UniqueImage createImage(vk::Extent3D, vk::Format, vk::ImageTiling,
                              vk::ImageUsageFlags);
  vk::UniqueDeviceMemory allocateImageMemory(const vk::Image &,
                                             vk::MemoryPropertyFlags);
  std::pair<vk::UniqueImage, vk::UniqueDeviceMemory> createTextureImage();
  std::vector<vk::UniqueBuffer> createUniformBuffers();
  std::vector<vk::UniqueDeviceMemory> allocateUniformBuffersMemory();
  vk::UniqueDescriptorPool createDescriptorPool();
  std::vector<vk::DescriptorSet> createDescriptorSets();
  std::vector<vk::UniqueCommandBuffer> createCommandBuffers();
  std::vector<vk::UniqueSemaphore> createSemaphores();
  std::vector<vk::UniqueFence> createFences();

  void fillVertexBuffer();
  void fillIndexBuffer();
  void fillTextureImage();
  void recreateSwapchain();
  void updateUniformBuffer(uint32_t);

  void drawFrame();

#if !defined(NDEBUG)
  bool checkValidationLayerSupport();
#endif

 private:
  // NOTE: These are initialized in side-effects for now
  // This means they need to go first, otherwise they would
  // be initialied back to zero by the initializer list
  vk::Format swapchainImageFormat;
  vk::Extent2D swapchainExtent;

  GLFWwindow *window;
  vk::UniqueInstance vkInstance;
  vk::UniqueSurfaceKHR vkSurface;
  vk::PhysicalDevice vkPhysicalDevice;
  vk::UniqueDevice vkDevice;
  vk::Queue vkGraphicsQueue;
  vk::Queue vkPresentQueue;
  vk::UniqueSwapchainKHR vkSwapchain;
  std::vector<vk::Image> vkSwapchainImages;
  std::vector<vk::UniqueImageView> vkSwapchainImageViews;
  vk::UniqueRenderPass vkRenderPass;
  vk::UniqueDescriptorSetLayout vkDescriptorSetLayout;
  vk::UniquePipelineLayout vkPipelineLayout;
  vk::UniquePipeline vkGraphicsPipeline;
  std::vector<vk::UniqueFramebuffer> vkSwapchainFramebuffers;
  vk::UniqueCommandPool vkCommandPool;
  vk::UniqueBuffer vertexBuffer;
  vk::UniqueDeviceMemory vertexBufferMemory;
  vk::UniqueBuffer indexBuffer;
  vk::UniqueDeviceMemory indexBufferMemory;
  std::vector<vk::UniqueBuffer> uniformBuffers;
  std::vector<vk::UniqueDeviceMemory> uniformBuffersMemory;
  std::pair<vk::UniqueImage, vk::UniqueDeviceMemory> textureImagePair;
  vk::UniqueDescriptorPool descriptorPool;
  std::vector<vk::DescriptorSet> descriptorSets;
  std::vector<vk::UniqueCommandBuffer> vkCommandBuffers;
  std::vector<vk::UniqueSemaphore> vkImageAvailableSemaphores;
  std::vector<vk::UniqueSemaphore> vkRenderFinishedSemaphores;
  std::vector<vk::UniqueFence> vkInFlightFences;
  std::vector<vk::Fence> vkImagesInFlight;

  size_t currentFrame{};

  static const int MAX_FRAMES_IN_FLIGHT = 2;

 public:
  bool framebufferResized = false;
};

static void framebufferResizeCallback(GLFWwindow *window, int width,
                                      int height) {
  auto app = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));
  app->framebufferResized = true;
}
#endif  // __MAIN_H_
