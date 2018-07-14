/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "memory.h"

#include <vulkan/vk_format_utils.h>

namespace memory {

#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
DeviceMemory::~DeviceMemory() {
  DeviceMemory::lock_guard_t lock(lockmutex);
  if (vmaAlloc) {
    if (!dev.vmaAllocator) {
      logF("~DeviceMemory: Device destroyed already or not created yet.\n");
      return;
    }
    VmaAllocationInfo info;
    if (getAllocInfo(info)) {
      logF("~DeviceMemory: BUG: getAllocInfo failed\n");
    }
    if (info.pMappedData) {
      vmaUnmapMemory(dev.vmaAllocator, vmaAlloc);
    }
    vmaFreeMemory(dev.vmaAllocator, vmaAlloc);
  }
}

int DeviceMemory::alloc(MemoryRequirements req) {
  if (!dev.vmaAllocator) {
    DeviceMemory::lock_guard_t lock(dev.lockmutex);
    if (!dev.phys || !dev.dev) {
      logE("alloc: device not created yet\n");
      return 1;
    }
    VmaAllocatorCreateInfo allocatorInfo;
    memset(&allocatorInfo, 0, sizeof(allocatorInfo));
    allocatorInfo.physicalDevice = dev.phys;
    allocatorInfo.device = dev.dev;
#ifdef __ANDROID__
    if (!vkGetPhysicalDeviceProperties) {
      logF("InitVulkan in glfwglue.cpp was not called yet.\n");
    }

    VmaVulkanFunctions vulkanFns;
    memset(&vulkanFns, 0, sizeof(vulkanFns));

    vulkanFns.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vulkanFns.vkGetPhysicalDeviceMemoryProperties =
        vkGetPhysicalDeviceMemoryProperties;
    vulkanFns.vkAllocateMemory = vkAllocateMemory;
    vulkanFns.vkFreeMemory = vkFreeMemory;
    vulkanFns.vkMapMemory = vkMapMemory;
    vulkanFns.vkUnmapMemory = vkUnmapMemory;
    vulkanFns.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vulkanFns.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vulkanFns.vkBindBufferMemory = vkBindBufferMemory;
    vulkanFns.vkBindImageMemory = vkBindImageMemory;
    vulkanFns.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vulkanFns.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vulkanFns.vkCreateBuffer = vkCreateBuffer;
    vulkanFns.vkDestroyBuffer = vkDestroyBuffer;
    vulkanFns.vkCreateImage = vkCreateImage;
    vulkanFns.vkDestroyImage = vkDestroyImage;
#if VMA_DEDICATED_ALLOCATION
    vulkanFns.vkGetBufferMemoryRequirements2KHR =
        vkGetBufferMemoryRequirements2KHR;
    vulkanFns.vkGetImageMemoryRequirements2KHR =
        vkGetImageMemoryRequirements2KHR;
#endif

    allocatorInfo.pVulkanFunctions = &vulkanFns;
#endif /*__ANDROID__*/

    VkResult r = vmaCreateAllocator(&allocatorInfo, &dev.vmaAllocator);
    if (r != VK_SUCCESS) {
      logE("%s failed: %d (%s)\n", "vmaCreateAllocator", r, string_VkResult(r));
      return 1;
    }
  }

  VmaAllocationCreateInfo* pInfo = &req.info;
  if (pInfo->usage == VMA_MEMORY_USAGE_UNKNOWN && !pInfo->requiredFlags) {
    logE("Please set MemoryRequirements::info.usage before calling alloc.\n");
    return 1;
  }
  DeviceMemory::lock_guard_t lock(lockmutex);
  VkResult r;
  if (req.vkbuf) {
    if (req.vkimg) {
      logE("MemoryRequirements with both vkbuf and vkimg is invalid.\n");
      return 1;
    }
    r = vmaAllocateMemoryForBuffer(dev.vmaAllocator, req.vkbuf, pInfo,
                                   &vmaAlloc, NULL);
  } else if (req.vkimg) {
    r = vmaAllocateMemoryForImage(dev.vmaAllocator, req.vkimg, pInfo, &vmaAlloc,
                                  NULL);
  } else {
    logE("MemoryRequirements::get not called yet.\n");
    return 1;
  }
  if (r != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vmaAllocateMemoryFor(Buffer or Image)", r,
         string_VkResult(r));
    return 1;
  }
  return 0;
}

int DeviceMemory::getAllocInfo(VmaAllocationInfo& info) {
  if (!dev.vmaAllocator) {
    logE("getAllocInfo: alloc not called yet.\n");
    return 1;
  }
  memset(&info, 0, sizeof(info));
  lock_guard_t lock(lockmutex);
  vmaGetAllocationInfo(dev.vmaAllocator, vmaAlloc, &info);
  return 0;
}

int DeviceMemory::mmap(void** pData, VkDeviceSize offset /*= 0*/,
                       VkDeviceSize size /*= VK_WHOLE_SIZE*/,
                       VkMemoryMapFlags flags /*= 0*/) {
  lock_guard_t lock(lockmutex);
  (void)size;
  (void)flags;
  VkResult v = vmaMapMemory(dev.vmaAllocator, vmaAlloc, pData);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vmaMapMemory", v, string_VkResult(v));
    return 1;
  }
  if (offset) {
    logW("mmap: offset != 0 when using VulkanMemoryAllocator - SLOW!\n");
    *pData = reinterpret_cast<void*>(reinterpret_cast<char*>(*pData) + offset);
  }
  return 0;
}

void DeviceMemory::makeRange(VkMappedMemoryRange& range,
                             const VmaAllocationInfo& info) {
  VkOverwrite(range);
  range.memory = info.deviceMemory;
  range.offset = info.offset;
  range.size = info.size;
}

int DeviceMemory::flush() {
  VmaAllocationInfo info;
  if (getAllocInfo(info)) {
    logE("flush: getAllocInfo failed\n");
    return 1;
  }
  VkResult v =
      vmaFlushAllocation(dev.vmaAllocator, vmaAlloc, info.offset, info.size);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vmaFlushAllocation", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

int DeviceMemory::invalidate() {
  VmaAllocationInfo info;
  if (getAllocInfo(info)) {
    logE("flush: getAllocInfo failed\n");
    return 1;
  }
  VkResult v = vmaInvalidateAllocation(dev.vmaAllocator, vmaAlloc, info.offset,
                                       info.size);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vmaInvalidateAllocation", v,
         string_VkResult(v));
    return 1;
  }
  return 0;
}

void DeviceMemory::munmap() {
  lock_guard_t lock(lockmutex);
  vmaUnmapMemory(dev.vmaAllocator, vmaAlloc);
}

#else  /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

DeviceMemory::~DeviceMemory() {
  if (vmaAlloc.mapped) {
    vmaAlloc.mapped = 0;
    vkUnmapMemory(dev.dev, vmaAlloc.vk);
  }
}

int DeviceMemory::alloc(MemoryRequirements req) {
  if (req.findVkalloc(vmaAlloc.requiredProps)) {
    return 1;
  }
  vmaAlloc.allocSize = req.vkalloc.allocationSize;
  vmaAlloc.vk.reset(req.dev.dev);
  VkResult v = vkAllocateMemory(req.dev.dev, &req.vkalloc,
                                req.dev.dev.allocator, &vmaAlloc.vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkAllocateMemory", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

int DeviceMemory::mmap(void** pData, VkDeviceSize offset /*= 0*/,
                       VkDeviceSize size /*= VK_WHOLE_SIZE*/,
                       VkMemoryMapFlags flags /*= 0*/) {
  VkResult v = vkMapMemory(dev.dev, vmaAlloc.vk, offset, size, flags, pData);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkMapMemory", v, string_VkResult(v));
    return 1;
  }
  vmaAlloc.mapped = *pData;
  return 0;
}

void DeviceMemory::makeRange(VkMappedMemoryRange& range, VkDeviceSize offset,
                             VkDeviceSize size) {
  VkOverwrite(range);
  range.memory = vmaAlloc.vk;
  range.offset = offset;
  range.size = size;
}

int DeviceMemory::flush(std::vector<VkMappedMemoryRange> mem) {
  if (!mem.size()) {
    logE("DeviceMemory::flush: vector<VkMappedMemoryRange>.size=0\n");
    return 1;
  }
  for (auto i = mem.begin(); i != mem.end(); i++) {  // Force .memory to be vk.
    i->memory = vmaAlloc.vk;
  }
  VkResult v = vkFlushMappedMemoryRanges(dev.dev, mem.size(), mem.data());
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkFlushMappedMemoryRanges", v,
         string_VkResult(v));
    return 1;
  }
  return 0;
}

int DeviceMemory::invalidate(const std::vector<VkMappedMemoryRange>& mem) {
  VkResult v = vkInvalidateMappedMemoryRanges(dev.dev, mem.size(), mem.data());
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkInvalidateMappedMemoryRanges", v,
         string_VkResult(v));
    return 1;
  }
  return 0;
}

void DeviceMemory::munmap() {
  vkUnmapMemory(dev.dev, vmaAlloc.vk);
  vmaAlloc.mapped = 0;
}
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

int MemoryRequirements::get(VkImage img,
                            VkImageAspectFlagBits optionalAspect /*= 0*/) {
  reset();
#if VK_HEADER_VERSION != 74
/* Fix the excessive #ifndef __ANDROID__ below to just use the Android Loader
 * once KhronosGroup lands support. */
#error KhronosGroup update detected, splits Vulkan-LoaderAndValidationLayers
#endif
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  vkbuf = VK_NULL_HANDLE;
  vkimg = img;
  memset(&info, 0, sizeof(info));
  (void)dev;
  if (optionalAspect != (VkImageAspectFlagBits)0) {
    logE("VkImageAspectFlagBits is not supported by VulkanMemoryAllocator!\n");
    return 1;
  }
#else /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
#ifndef __ANDROID__
  if (dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
#else
  (void)optionalAspect;
#endif
    vkGetImageMemoryRequirements(dev.dev, img, &vk.memoryRequirements);
    return 0;
#ifndef __ANDROID__
  }

  // Use Vulkan 1.1 features if supported.
  VkImageMemoryRequirementsInfo2 VkInit(info);
  VkImagePlaneMemoryRequirementsInfo VkInit(planeInfo);
  if (optionalAspect != (VkImageAspectFlagBits)0) {
    info.pNext = &planeInfo;
    planeInfo.planeAspect = optionalAspect;
  }
  info.image = img;
  vkGetImageMemoryRequirements2(dev.dev, &info, &vk);
#endif /* __ANDROID__ */
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  return 0;
}

int MemoryRequirements::get(Image& img,
                            VkImageAspectFlagBits optionalAspect /*= 0*/) {
  if (optionalAspect != (VkImageAspectFlagBits)0) {
    if (!FormatIsMultiplane(img.info.format) ||
        !(img.info.flags & VK_IMAGE_CREATE_DISJOINT_BIT)) {
      logW("MemoryRequirements::get(Image): optionalAspect ignored, need\n");
      logW("multiplane format and VK_IMAGE_CREATE_DISJOINT_BIT set\n");
      optionalAspect = (VkImageAspectFlagBits)0;
    }
  }
  return get(img.vk, optionalAspect);
}

int MemoryRequirements::get(VkBuffer buf) {
  reset();
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  vkbuf = buf;
  vkimg = VK_NULL_HANDLE;
  memset(&info, 0, sizeof(info));
  (void)dev;
#else /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
#ifndef __ANDROID__
  if (dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
#endif
    vkGetBufferMemoryRequirements(dev.dev, buf, &vk.memoryRequirements);
    return 0;
#ifndef __ANDROID__
  }

  // Use Vulkan 1.1 features if supported.
  VkBufferMemoryRequirementsInfo2 VkInit(info);
  // No pNext structures defined at this time.
  info.buffer = buf;
  vkGetBufferMemoryRequirements2(dev.dev, &info, &vk);
#endif /* __ANDROID__ */
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  return 0;
}

#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
int MemoryRequirements::indexOf(VkMemoryPropertyFlags props) const {
  auto& memProps = dev.memProps.memoryProperties;
  for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
    if ((vk.memoryRequirements.memoryTypeBits & (1 << i)) &&
        (memProps.memoryTypes[i].propertyFlags & props) == props) {
      return i;
    }
  }
  return -1;
}

int MemoryRequirements::findVkalloc(VkMemoryPropertyFlags props) {
  // TODO: optionally accept another props with fewer bits (i.e. first props are
  // the most optimal, second props are the bare minimum).
  int i = indexOf(props);
  if (i == -1) {
    logE("MemoryRequirements::indexOf(%x): not found in %x\n", props,
         vk.memoryRequirements.memoryTypeBits);
    return 1;
  }

  vkalloc.memoryTypeIndex = i;
  vkalloc.allocationSize = vk.memoryRequirements.size;

  return 0;
}
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

}  // namespace memory
