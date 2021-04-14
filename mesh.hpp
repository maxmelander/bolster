#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "vk_types.hpp"

struct Vertex {
  glm::vec3 _position;
  glm::vec3 _normal;
  glm::vec3 _color;
  glm::vec2 _texCoord;

  static vk::VertexInputBindingDescription getBindingDescription() {
    vk::VertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = vk::VertexInputRate::eVertex;

    return bindingDescription;
  }

  static std::array<vk::VertexInputAttributeDescription, 4>
  getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 4> attributeDescriptions{};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[0].offset = offsetof(Vertex, _position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[1].offset = offsetof(Vertex, _normal);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[2].offset = offsetof(Vertex, _color);

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = vk::Format::eR32G32Sfloat;
    attributeDescriptions[3].offset = offsetof(Vertex, _texCoord);

    return attributeDescriptions;
  }

  bool operator==(const Vertex &other) const {
    return _position == other._position && _color == other._color &&
           _texCoord == other._texCoord;
  }
};

namespace std {
template <>
struct hash<Vertex> {
  size_t operator()(Vertex const &vertex) const {
    return ((hash<glm::vec3>()(vertex._position) ^
             (hash<glm::vec3>()(vertex._color) << 1)) >>
            1) ^
           (hash<glm::vec2>()(vertex._texCoord) << 1);
  }
};
}  // namespace std

struct MeshData {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  glm::vec4 boundingSphere;
};

struct Mesh {
  uint32_t vertexOffset;
  uint32_t indexOffset;
  uint32_t vertexSize;
  uint32_t indexSize;
  glm::vec4 boundingSphere;
};
