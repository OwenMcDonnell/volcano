/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "science.h"

namespace science {

void ImageCopies::add(memory::Image& src, memory::Image& dst) {
  if (dst.info.extent.width != src.info.extent.width ||
      dst.info.extent.height != src.info.extent.height ||
      dst.info.extent.depth != src.info.extent.depth) {
    logF("ImageCopies::add: src.extent != dst.extent\n");
  }
  uint32_t minLevels = src.info.mipLevels;
  if (minLevels > dst.info.mipLevels) {
    minLevels = dst.info.mipLevels;
  }
  for (uint32_t mipLevel = 0; mipLevel < minLevels; mipLevel++) {
    addSingleMipLevel(src, mipLevel, dst, mipLevel);
  }
}

void ImageCopies::addSingleMipLevel(memory::Image& src, uint32_t srcMipLevel,
                                    memory::Image& dst, uint32_t dstMipLevel) {
  emplace_back();
  VkImageCopy& region = back();
  region.srcSubresource = src.getSubresourceLayers(srcMipLevel);
  region.dstSubresource = dst.getSubresourceLayers(dstMipLevel);
  region.srcSubresource.aspectMask &= region.dstSubresource.aspectMask;
  region.dstSubresource.aspectMask &= region.srcSubresource.aspectMask;

  region.srcOffset = {0, 0, 0};
  region.dstOffset = {0, 0, 0};

  // Assume the dst size (dst.info.extent.{width,height} >> dstMipLevel)
  // matches the src size.
  region.extent = src.info.extent;
  region.extent.width >>= srcMipLevel;
  region.extent.height >>= srcMipLevel;
}

SmartCommandBuffer::~SmartCommandBuffer() {
  if (wantAutoSubmit) {
    if (end()) {
      logF("~SmartCommandBuffer: end failed\n");
    }
    if (submit(poolQindex)) {
      logF("~SmartCommandBuffer: submit(%zu) failed\n", poolQindex);
    }
  }
  if (ctorErrorSuccess) {
    if (cpool.unborrowOneTimeBuffer(vk)) {
      logF("~SmartCommandBuffer: unborrowOneTimeBuffer failed\n");
    }
  }
  vk = VK_NULL_HANDLE;
  if (wantAutoSubmit) {
    VkResult v = vkQueueWaitIdle(cpool.q(poolQindex));
    if (v != VK_SUCCESS) {
      logF("%s failed: %d (%s)\n", "vkQueueWaitIdle", v, string_VkResult(v));
    }
  }
}

int PipeBuilder::alphaBlendWithPreviousPass(
    const command::PipelineCreateInfo& prevPipeInfo) {
  auto& pipeInfo = info();
  if (pipeInfo.attach.size() > prevPipeInfo.attach.size()) {
    logE("alphaBlendWithPreviousPass: %zu attachments when prevPipe has %zu\n",
         pipeInfo.attach.size(), prevPipeInfo.attach.size());
    return 1;
  }

  // Tell pipeline to alpha blend with what is already in framebuffer.
  pipeInfo.perFramebufColorBlend.at(0) =
      command::PipelineCreateInfo::withEnabledAlpha();

  // Update the loadOp to load data from the framebuffer, instead of a CLEAR_OP.
  for (size_t i = 0; i < pipeInfo.attach.size(); i++) {
    auto& attach = pipeInfo.attach.at(i);
    attach.vk.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attach.vk.initialLayout = prevPipeInfo.attach.at(i).vk.finalLayout;
  }

  // Autodetect if the device is using a depth buffer.
  if (dev.GetDepthFormat() != VK_FORMAT_UNDEFINED) {
    // Match the format of the framebuf.
    if (addDepthImage({dev.GetDepthFormat()})) {
      logE("addDepthImage failed (trying to match framebuf format)\n");
      return 1;
    }
  }
  return 0;
}

}  // namespace science
