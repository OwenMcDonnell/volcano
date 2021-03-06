/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "command.h"

namespace command {

int Semaphore::ctorError(language::Device& dev) {
  VkSemaphoreCreateInfo VkInit(sci);
  VkResult v = vkCreateSemaphore(dev.dev, &sci, nullptr, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateSemaphore", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

int Fence::ctorError(language::Device& dev) {
  VkFenceCreateInfo VkInit(fci);
  VkResult v = vkCreateFence(dev.dev, &fci, nullptr, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateFence", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

int Fence::reset(language::Device& dev) {
  VkFence fences[] = {vk};
  VkResult v =
      vkResetFences(dev.dev, sizeof(fences) / sizeof(fences[0]), fences);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkResetFences", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

VkResult Fence::wait(language::Device& dev, uint64_t timeoutNanos) {
  VkFence fences[] = {vk};
  return vkWaitForFences(dev.dev, sizeof(fences) / sizeof(fences[0]), fences,
                         VK_FALSE, timeoutNanos);
}

VkResult Fence::getStatus(language::Device& dev) {
  return vkGetFenceStatus(dev.dev, vk);
}

int Event::ctorError(language::Device& dev) {
  VkEventCreateInfo VkInit(eci);
  VkResult v = vkCreateEvent(dev.dev, &eci, nullptr, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateEvent", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

int CommandBuffer::validateLazyBarriers(CommandPool::lock_guard_t&) {
  auto& b = lazyBarriers;
  bool found = false;
  for (auto& mem : b.mem) {
    found = true;
    if (mem.sType != VK_STRUCTURE_TYPE_MEMORY_BARRIER) {
      logE("lazyBarriers::mem contains invalid VkMemoryBarrier\n");
      return 1;
    }
    trimSrcStage(mem.srcAccessMask, b.srcStageMask);
    trimDstStage(mem.dstAccessMask, b.dstStageMask);
  }
  for (auto& buf : b.buf) {
    found = true;
    if (buf.sType != VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER) {
      logE("lazyBarriers::buf contains invalid VkBufferMemoryBarrier\n");
      return 1;
    }
    if (!buf.buffer) {
      logE("lazyBarriers::buf contains invalid VkBuffer\n");
      return 1;
    }
    trimSrcStage(buf.srcAccessMask, b.srcStageMask);
    trimDstStage(buf.dstAccessMask, b.dstStageMask);
  }
  for (auto& img : b.img) {
    found = true;
    if (img.sType != VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER) {
      logE("lazyBarriers::img contains invalid VkImageMemoryBarrier\n");
      return 1;
    }
    if (!img.image) {
      logE("lazyBarriers::img contains invalid VkImage\n");
      return 1;
    }
    trimSrcStage(img.srcAccessMask, b.srcStageMask);
    trimDstStage(img.dstAccessMask, b.dstStageMask);
  }
  if (!vk) {
    logE("CommandBuffer::flushLazyBarriers: not allocated\n");
    return 1;
  }
  return found ? 0 : 2;
}

int CommandBuffer::flushLazyBarriers(CommandPool::lock_guard_t& lock) {
  switch (validateLazyBarriers(lock)) {
    case 1:
      return 1;
    case 2:
      return 0;  // Optimization: no need to call vkCmdPipelineBarrier.
    default:
      break;
  }
  auto& b = lazyBarriers;
  VkDependencyFlags dependencyFlags = 0;
  vkCmdPipelineBarrier(vk, b.srcStageMask, b.dstStageMask, dependencyFlags,
                       b.mem.size(), b.mem.data(), b.buf.size(), b.buf.data(),
                       b.img.size(), b.img.data());
  b.reset();
  return 0;
}

int CommandBuffer::waitBarrier(const BarrierSet& b,
                               VkDependencyFlags dependencyFlags /*= 0*/) {
  CommandPool::lock_guard_t lock(cpool.lockmutex);
  if (flushLazyBarriers(lock)) return 1;
  vkCmdPipelineBarrier(vk, b.srcStageMask, b.dstStageMask, dependencyFlags,
                       b.mem.size(), b.mem.data(), b.buf.size(), b.buf.data(),
                       b.img.size(), b.img.data());
  return 0;
}

int CommandBuffer::waitEvents(const std::vector<VkEvent>& events) {
  CommandPool::lock_guard_t lock(cpool.lockmutex);
  // use of b followed by b.reset() is the same as flushLazyBarriers().
  switch (validateLazyBarriers(lock)) {
    case 1:
      return 1;
    case 2:
      // FALLTHROUGH: Do not optimize because events is most likely not empty.
    default:
      break;
  }
  auto& b = lazyBarriers;
  vkCmdWaitEvents(vk, events.size(), events.data(), b.srcStageMask,
                  b.dstStageMask, b.mem.size(), b.mem.data(), b.buf.size(),
                  b.buf.data(), b.img.size(), b.img.data());
  b.reset();
  return 0;
}

}  // namespace command
