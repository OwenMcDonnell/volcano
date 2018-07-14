/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "memory.h"

#include <src/science/science.h>
#include <sstream>

namespace memory {

int Buffer::copy(command::CommandPool& pool, Buffer& src) {
  if (src.info.size > info.size) {
    logE("Buffer::copy: src.info.size=0x%llx is larger than my size 0x%llx\n",
         (unsigned long long)src.info.size, (unsigned long long)info.size);
    return 1;
  }

  science::SmartCommandBuffer cmdBuffer{pool, ASSUME_POOL_QINDEX};
  return cmdBuffer.ctorError() || cmdBuffer.autoSubmit() ||
         copy(cmdBuffer, src);
}

int Buffer::validateBufferCreateInfo(const std::vector<uint32_t>& queueFams) {
  if (!info.size || !info.usage) {
    logE("Buffer::ctorError found uninitialized fields\n");
    return 1;
  }

  if (queueFams.size()) {
    info.sharingMode = VK_SHARING_MODE_CONCURRENT;
  }
  info.queueFamilyIndexCount = queueFams.size();
  info.pQueueFamilyIndices = queueFams.data();
  return 0;
}

int Buffer::ctorError(VkMemoryPropertyFlags props,
                      const std::vector<uint32_t>& queueFams) {
  if (validateBufferCreateInfo(queueFams)) {
    return 1;
  }
  vk.reset(mem.dev.dev);
  VkResult v = vkCreateBuffer(mem.dev.dev, &info, mem.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateBuffer", v, string_VkResult(v));
    return 1;
  }

#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  MemoryRequirements req(mem.dev, *this);
  mem.vmaAlloc.requiredProps = props;
#else
  MemoryRequirements req(mem.dev, *this, VMA_MEMORY_USAGE_UNKNOWN);
  req.info.requiredFlags = props;
#endif
  return mem.alloc(req);
}

#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
int Buffer::ctorError(VmaMemoryUsage usage,
                      const std::vector<uint32_t>& queueFams) {
  if (validateBufferCreateInfo(queueFams)) {
    return 1;
  }
  vk.reset(mem.dev.dev);
  VkResult v = vkCreateBuffer(mem.dev.dev, &info, mem.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateBuffer", v, string_VkResult(v));
    return 1;
  }
  return mem.alloc({mem.dev, *this, usage});
}
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

int Buffer::bindMemory(VkDeviceSize offset /*= 0*/) {
  auto& dev = mem.dev;
  VkResult v;
  const char* functionName;
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  if (offset) {
    logE("VulkanMemoryAllocator sets offset automatically.\n");
    logE("Buffer::bindMemory(offset=%llu) is invalid.\n",
         (unsigned long long)offset);
    return 1;
  }
  functionName = "vmaBindBufferMemory";
  v = vmaBindBufferMemory(dev.vmaAllocator, mem.vmaAlloc, vk);
#else /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
#if VK_HEADER_VERSION != 74
/* Fix the excessive #ifndef __ANDROID__ below to just use the Android Loader
 * once KhronosGroup lands support. */
#error KhronosGroup update detected, splits Vulkan-LoaderAndValidationLayers
#endif
#ifndef __ANDROID__
  if (dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
#endif
    functionName = "vkBindBufferMemory";
    v = vkBindBufferMemory(dev.dev, vk, mem.vmaAlloc.vk, offset);
#ifndef __ANDROID__
  } else {
    // Use Vulkan 1.1 features if supported.
    functionName = "vkBindBufferMemory2";
    VkBindBufferMemoryInfo infos[1];
    VkOverwrite(infos[0]);
    infos[0].buffer = vk;
    infos[0].memory = mem.vmaAlloc.vk;
    infos[0].memoryOffset = offset;
    v = vkBindBufferMemory2(dev.dev, sizeof(infos) / sizeof(infos[0]), infos);
  }
#endif /* __ANDROID__ */
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", functionName, v, string_VkResult(v));
    return 1;
  }
  return 0;
}

int Buffer::reset() {
#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  mem.vmaAlloc.allocSize = 0;
  mem.vmaAlloc.vk.reset(mem.dev.dev);
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  vk.reset(mem.dev.dev);
  return 0;
}

int Buffer::copyFromHost(const void* src, size_t len,
                         VkDeviceSize dstOffset /*= 0*/) {
  if (!(info.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) {
    logW("WARNING: Buffer::copyFromHost on a Buffer where neither\n");
    logW("ctorHostVisible nor ctorHostCoherent was used.\n");
    std::ostringstream msg;
    msg << "usage = 0x" << std::hex << info.usage;

    // Dump info.usage bits.
    const char* firstPrefix = " (";
    const char* prefix = firstPrefix;
    for (uint64_t bit = 1; bit < VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
         bit <<= 1) {
      if (((uint64_t)info.usage) & bit) {
        msg << prefix
            << string_VkBufferUsageFlagBits((VkBufferUsageFlagBits)bit);
        prefix = " | ";
      }
    }
    if (prefix != firstPrefix) {
      msg << ")";
    }
    auto s = msg.str();
    logW("%s\n", s.c_str());
    return 1;
  }

  if (dstOffset + len > info.size) {
    logE("BUG: Buffer::copyFromHost(len=0x%zx, dstOffset=0x%llx).\n", len,
         (unsigned long long)dstOffset);
    logE("BUG: when Buffer.info.size=0x%llx\n", (unsigned long long)info.size);
    return 1;
  }

  void* mapped;
  if (mem.mmap(&mapped)) {
    return 1;
  }
  memcpy(reinterpret_cast<char*>(mapped) + dstOffset, src, len);
  mem.munmap();
  return 0;
}

int UniformBuffer::copyAndKeepMmap(command::CommandPool& pool, const void* src,
                                   size_t len, VkDeviceSize dstOffset /*= 0*/) {
  if (!(stage.info.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) {
    logW("WARNING: UniformBuffer::copyAndKeepMmap on a Buffer where\n");
    logW("neither ctorHostVisible nor ctorHostCoherent was used.\n");
    std::ostringstream msg;
    msg << std::hex << stage.info.usage;
    auto s = msg.str();
    logW("usage = 0x%s\n", s.c_str());
    return 1;
  }

  if (dstOffset + len > stage.info.size) {
    logE("BUG: UniformBuffer::copyAndKeepMmap(len=0x%zx, dstOffset=0x%llx).\n",
         len, (unsigned long long)dstOffset);
    logE("BUG: when Buffer.info.size=0x%llx\n",
         (unsigned long long)stage.info.size);
    return 1;
  }

  if (!stageMmap) {
    if (stage.mem.mmap(&stageMmap)) {
      logE("UniformBuffer::copyAndKeepMmap: stage.mem.mmap failed\n");
      return 1;
    }
  }

  memcpy(reinterpret_cast<char*>(stageMmap) + dstOffset, src, len);
  return Buffer::copy(pool, stage);
}

}  // namespace memory
