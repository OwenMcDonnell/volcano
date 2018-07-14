/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "memory.h"

#include <vulkan/vk_format_utils.h>

namespace memory {

int Image::validateImageCreateInfo() {
  if (!info.extent.width || !info.extent.height || !info.extent.depth ||
      !info.format || !info.usage || !info.mipLevels || !info.arrayLayers) {
    logE("Image::ctorError found uninitialized fields\n");
    return 1;
  }
  mem.dev.apiUsage(
      1, 1, 0,
      info.flags & (VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT |
                    VK_IMAGE_CREATE_EXTENDED_USAGE_BIT),
      "Image::info flags=%x", info.flags);
  return 0;
}

int Image::getSubresourceLayouts() {
  // NOTE: vkGetImageSubresourceLayout is only valid for an image with
  // LINEAR tiling.
  //
  // Vulkan Spec: "The layout of a subresource (mipLevel/arrayLayer) of an
  // image created with linear tiling is queried by calling
  // vkGetImageSubresourceLayout".
  if (info.tiling != VK_IMAGE_TILING_LINEAR) {
    return 0;
  }
  VkImageSubresource s = {};
  if (FormatIsColor(info.format)) {
    s.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    for (s.arrayLayer = 0; s.arrayLayer < info.arrayLayers; s.arrayLayer++) {
      for (s.mipLevel = 0; s.mipLevel < info.mipLevels; s.mipLevel++) {
        colorMem.emplace_back();
        vkGetImageSubresourceLayout(mem.dev.dev, vk, &s, &colorMem.back());
      }
    }
  }
  if (FormatHasDepth(info.format)) {
    s.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    for (s.arrayLayer = 0; s.arrayLayer < info.arrayLayers; s.arrayLayer++) {
      for (s.mipLevel = 0; s.mipLevel < info.mipLevels; s.mipLevel++) {
        depthMem.emplace_back();
        vkGetImageSubresourceLayout(mem.dev.dev, vk, &s, &depthMem.back());
      }
    }
  }
  if (FormatHasStencil(info.format)) {
    s.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    for (s.arrayLayer = 0; s.arrayLayer < info.arrayLayers; s.arrayLayer++) {
      for (s.mipLevel = 0; s.mipLevel < info.mipLevels; s.mipLevel++) {
        stencilMem.emplace_back();
        vkGetImageSubresourceLayout(mem.dev.dev, vk, &s, &stencilMem.back());
      }
    }
  }
  return 0;
}

int Image::ctorError(VkMemoryPropertyFlags props) {
  if (validateImageCreateInfo()) {
    return 1;
  }

  vk.reset(mem.dev.dev);
  VkResult v = vkCreateImage(mem.dev.dev, &info, mem.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateImage", v, string_VkResult(v));
    return 1;
  }
  currentLayout = info.initialLayout;

#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  MemoryRequirements req(mem.dev, *this);
  mem.vmaAlloc.requiredProps = props;
#else
  MemoryRequirements req(mem.dev, *this, VMA_MEMORY_USAGE_UNKNOWN);
  req.info.requiredFlags = props;
#endif
  if (mem.alloc(req)) {
    return 1;
  }

  return getSubresourceLayouts();
}

#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
int Image::ctorError(VmaMemoryUsage usage) {
  if (validateImageCreateInfo()) {
    return 1;
  }

  vk.reset(mem.dev.dev);
  VkResult v = vkCreateImage(mem.dev.dev, &info, mem.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateImage", v, string_VkResult(v));
    return 1;
  }
  currentLayout = info.initialLayout;
  return mem.alloc({mem.dev, *this, usage}) || getSubresourceLayouts();
}
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

int Image::bindMemory(VkDeviceSize offset /*= 0*/) {
  VkResult v;
  const char* functionName;
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  if (offset) {
    logE("VulkanMemoryAllocator sets offset automatically.\n");
    logE("Image::bindMemory(offset=%llu) is invalid.\n",
         (unsigned long long)offset);
    return 1;
  }
  functionName = "vmaBindImageMemory";
  v = vmaBindImageMemory(mem.dev.vmaAllocator, mem.vmaAlloc, vk);
#else /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
#if VK_HEADER_VERSION != 74
/* Fix the excessive #ifndef __ANDROID__ below to just use the Android Loader
 * once KhronosGroup lands support. */
#error KhronosGroup update detected, splits Vulkan-LoaderAndValidationLayers
#endif
#ifndef __ANDROID__
  if (mem.dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
#endif
    functionName = "vkBindImageMemory";
    v = vkBindImageMemory(mem.dev.dev, vk, mem.vmaAlloc.vk, offset);
#ifndef __ANDROID__
  } else {
    // Use Vulkan 1.1 features if supported.
    functionName = "vkBindImageMemory2";
    VkBindImageMemoryInfo infos[1];
    VkOverwrite(infos[0]);
    infos[0].image = vk;
    infos[0].memory = mem.vmaAlloc.vk;
    infos[0].memoryOffset = offset;
    v = vkBindImageMemory2(mem.dev.dev, sizeof(infos) / sizeof(infos[0]),
                           infos);
  }
#endif /* __ANDROID__ */
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", functionName, v, string_VkResult(v));
    return 1;
  }
  return 0;
}

int Image::reset() {
#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  mem.vmaAlloc.allocSize = 0;
  mem.vmaAlloc.vk.reset(mem.dev.dev);
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  vk.reset(mem.dev.dev);
  return 0;
}

VkImageAspectFlags Image::getAllAspects() const {
  VkImageAspectFlags aspects = 0;
  if (FormatIsColor(info.format)) {
    aspects |= VK_IMAGE_ASPECT_COLOR_BIT;
  }
  if (FormatHasDepth(info.format)) {
    aspects |= VK_IMAGE_ASPECT_DEPTH_BIT;
  }
  if (FormatHasStencil(info.format)) {
    aspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  if (info.flags & (VK_IMAGE_CREATE_SPARSE_BINDING_BIT |
                    VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT |
                    VK_IMAGE_CREATE_SPARSE_ALIASED_BIT)) {
    aspects |= VK_IMAGE_ASPECT_METADATA_BIT;
  }
  if (FormatPlaneCount(info.format) > 1u) {
    aspects |= VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT;
  }
  if (FormatPlaneCount(info.format) > 2u) {
    aspects |= VK_IMAGE_ASPECT_PLANE_2_BIT;
  }
  return aspects;
}

}  // namespace memory
