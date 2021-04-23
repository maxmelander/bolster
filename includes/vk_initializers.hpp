#pragma once

#include <vulkan/vulkan.hpp>

#include "vk_types.hpp"

namespace vkinit {
inline vk::UniqueRenderPass buildRenderPass(
    const vk::Device &device, bool hasColorAttachment, vk::Format colorFormat,
    vk::Format depthFormat, vk::AttachmentStoreOp depthStoreOp,
    vk::ImageLayout depthFinalLayout,
    vk::SubpassDependency *subpassDependencies, uint32_t nSubpassDependencies) {
  uint32_t attachmentNumber = 0;

  vk::AttachmentReference colorAttachmentRef{};
  vk::AttachmentDescription colorAttachment{};
  if (hasColorAttachment) {
    colorAttachment.format = colorFormat;
    colorAttachment.samples = vk::SampleCountFlagBits::e1;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    // This is the reference to the color attachment above,
    // to be used by a subpass
    colorAttachmentRef.attachment = attachmentNumber;
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    attachmentNumber++;
  }

  vk::AttachmentDescription depthAttachment{};
  depthAttachment.format = depthFormat;
  depthAttachment.samples = vk::SampleCountFlagBits::e1;
  depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
  depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
  depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  depthAttachment.stencilStoreOp = depthStoreOp;
  depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
  depthAttachment.finalLayout = depthFinalLayout;

  vk::AttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = attachmentNumber;
  depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

  attachmentNumber++;

  vk::SubpassDescription subpass{vk::SubpassDescriptionFlags{},
                                 vk::PipelineBindPoint::eGraphics};
  subpass.colorAttachmentCount = hasColorAttachment ? 1 : 0;
  subpass.pColorAttachments =
      hasColorAttachment ? &colorAttachmentRef : nullptr;
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  if (hasColorAttachment) {
    std::array<vk::AttachmentDescription, 2> attachments = {colorAttachment,
                                                            depthAttachment};

    vk::RenderPassCreateInfo createInfo{{},
                                        2,
                                        attachments.data(),
                                        1,
                                        &subpass,
                                        nSubpassDependencies,
                                        subpassDependencies};

    return device.createRenderPassUnique(createInfo);
  } else {
    std::array<vk::AttachmentDescription, 1> attachments = {depthAttachment};

    vk::RenderPassCreateInfo createInfo{{},
                                        1,
                                        attachments.data(),
                                        1,
                                        &subpass,
                                        nSubpassDependencies,
                                        subpassDependencies};

    return device.createRenderPassUnique(createInfo);
  }
}
}  // namespace vkinit
