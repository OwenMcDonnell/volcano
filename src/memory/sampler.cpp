/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include <src/science/science.h>
#include "memory.h"

namespace memory {

int Sampler::ctorExisting() {
  auto& dev = image.mem.dev;
  vk.reset(dev.dev);
  VkResult v = vkCreateSampler(dev.dev, &info, dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateSampler", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

int Sampler::ctorError(command::CommandPool& cpool, Image& src) {
  science::SmartCommandBuffer setup{cpool, ASSUME_POOL_QINDEX};
  return setup.ctorError() || setup.autoSubmit() || ctorError(setup, src);
}

int Sampler::ctorError(command::CommandBuffer& buffer, Image& src) {
  if (&buffer.cpool.dev != &src.mem.dev || &image.mem.dev != &src.mem.dev) {
    logE("buffer.cpool.dev=%p src.mem.dev=%p image.mem.dev=%p: %s\n",
         &buffer.cpool.dev, &src.mem.dev, &image.mem.dev,
         "please use only one device.");
    return 1;
  }
  if (ctorExisting()) {
    return 1;
  }

  // Construct image as a USAGE_SAMPLED | TRANSFER_DST, then use
  // CommandBuffer::copyImage() to transfer its contents into it.
  image.info.extent = src.info.extent;
  image.info.format = src.info.format;
  image.info.mipLevels = src.info.mipLevels;
  image.info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image.info.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageView.info.subresourceRange = src.getSubresourceRange();

  return image.ctorDeviceLocal() || image.bindMemory() ||
         imageView.ctorError(image.mem.dev, image.vk, image.info.format) ||
         buffer.barrier(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) ||
         buffer.barrier(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) ||
         buffer.copyImage(src, image, science::ImageCopies(src, image)) ||
         buffer.barrier(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

int Sampler::ctorError(command::CommandPool& cpool, Buffer& src,
                       const std::vector<VkBufferImageCopy>& regions) {
  science::SmartCommandBuffer setup{cpool, ASSUME_POOL_QINDEX};
  return setup.ctorError() || setup.autoSubmit() ||
         ctorError(setup, src, regions);
}

int Sampler::ctorError(command::CommandBuffer& buffer, Buffer& src,
                       const std::vector<VkBufferImageCopy>& regions) {
  if (&buffer.cpool.dev != &src.mem.dev || &image.mem.dev != &src.mem.dev) {
    logE("buffer.cpool.dev=%p src.mem.dev=%p image.mem.dev=%p: %s\n",
         &buffer.cpool.dev, &src.mem.dev, &image.mem.dev,
         "please use only one device.");
    return 1;
  }
  if (!image.info.extent.width || !image.info.extent.height ||
      !image.info.extent.depth || !image.info.format || !image.info.mipLevels ||
      !image.info.arrayLayers) {
    logE("Sampler::ctorError found uninitialized fields in image.info\n");
    return 1;
  }
  if (imageView.info.subresourceRange.levelCount != image.info.mipLevels) {
    logE("Sampler::ctorError: image.info.mipLevels=%u\n", image.info.mipLevels);
    logE("but imageView.info.subresourceRange.levelCount=%u\n",
         imageView.info.subresourceRange.levelCount);
    return 1;
  }

  auto& dev = image.mem.dev;
  vk.reset(dev.dev);
  VkResult v = vkCreateSampler(dev.dev, &info, dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateSampler", v, string_VkResult(v));
    return 1;
  }

  // Construct image as a USAGE_SAMPLED | TRANSFER_DST, then use
  // CommandBuffer::copyBufferToImage() to copy the bytes into it.
  image.info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image.info.usage |=
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  return image.ctorDeviceLocal() || image.bindMemory() ||
         imageView.ctorError(image.mem.dev, image.vk, image.info.format) ||
         buffer.barrier(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) ||
         buffer.copyImage(src, image, regions) ||
         buffer.barrier(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

}  // namespace memory
