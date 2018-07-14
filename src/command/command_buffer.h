/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 *
 * src/command is the 3rd-level bindings for the Vulkan graphics library.
 *
 * Because class CommandBuffer is such a large class, it is defined in its
 * own header file. DO NOT INCLUDE THIS FILE DIRECTLY.
 *
 * You should #include "src/command/command.h" which will automatically include
 * this file.
 */

#pragma once

namespace memory {
// Forward declaration of Buffer for CommandBuffer.
typedef struct Buffer Buffer;
}  // namespace memory

namespace command {

// CommandBuffer holds a VkCommandBuffer, and provides helpful utility methods
// to create commands in the buffer.
//
// NOTE: CommandBuffer does not have a ctorError() method. The member
// CommandBuffer::vk is public and something outside this class should manage
// its lifecycle. See CommandPool::updateBuffersAndPass() for one potential way
// to do that.
class CommandBuffer {
 protected:
  // trimSrcStage modifies access bits that are not supported by stage. It also
  // simplifies stage selection by tailoring the stage to the access bits'
  // implied operation.
  void trimSrcStage(VkAccessFlags& access, VkPipelineStageFlags& stage);

  // trimDstStage modifies access bits that are not supported by stage.
  void trimDstStage(VkAccessFlags& access, VkPipelineStageFlags& stage);

  // validateLazyBarriers is called by flushLazyBarriers.
  WARN_UNUSED_RESULT int validateLazyBarriers(CommandPool::lock_guard_t& lock);

  // flushLazyBarriers needs to be called with the lock held, hence it is passed
  // in (but never used).
  WARN_UNUSED_RESULT int flushLazyBarriers(CommandPool::lock_guard_t& lock);

 public:
  // This form of constructor creates an empty CommandBuffer.
  CommandBuffer(CommandPool& cpool_) : cpool(cpool_) {}
  // Move constructor.
  CommandBuffer(CommandBuffer&& other) : cpool(other.cpool) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    vk = other.vk;
    other.vk = VK_NULL_HANDLE;
  }
  // The copy constructor is not allowed. The VkCommandBuffer cannot be copied.
  CommandBuffer(const CommandBuffer& other) = delete;

  virtual ~CommandBuffer();
  CommandBuffer& operator=(CommandBuffer&&) = delete;

  CommandPool& cpool;
  VkCommandBuffer vk{VK_NULL_HANDLE};

  // submit calls vkQueueSubmit on poolQindex.
  // Note vkQueueSubmit is a high overhead operation; submitting multiple
  // command buffers and even multiple VkSubmitInfo batches is recommended --
  // see submitMany.
  //
  // An optional VkFence parameter can be specified to signal the VkFence when
  // the operation is complete.
  WARN_UNUSED_RESULT int submit(
      size_t poolQindex,
      const std::vector<VkSemaphore>& waitSemaphores =
          std::vector<VkSemaphore>(),
      const std::vector<VkPipelineStageFlags>& waitStages =
          std::vector<VkPipelineStageFlags>(),
      const std::vector<VkSemaphore>& signalSemaphores =
          std::vector<VkSemaphore>(),
      VkFence fence = VK_NULL_HANDLE) {
    if (waitSemaphores.size() != waitStages.size()) {
      logE("submit: waitSemaphores len=%zu but waitStages len=%zu\n",
           waitSemaphores.size(), waitStages.size());
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    VkSubmitInfo VkInit(submitInfo);
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vk;
    submitInfo.waitSemaphoreCount = waitSemaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.signalSemaphoreCount = signalSemaphores.size();
    submitInfo.pSignalSemaphores = signalSemaphores.data();
    return cpool.submitMany(poolQindex, std::vector<VkSubmitInfo>{submitInfo},
                            fence);
  }

  // reset deallocates and clears the current VkCommandBuffer. Note that in
  // most cases, begin() calls vkBeginCommandBuffer() which implicitly resets
  // the buffer and clears any old data it may have had.
  WARN_UNUSED_RESULT int reset(
      VkCommandBufferResetFlagBits flags =
          VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    VkResult v;
    if ((v = vkResetCommandBuffer(vk, flags)) != VK_SUCCESS) {
      logE("%s failed: %d (%s)\n", "vkResetCommandBuffer", v,
           string_VkResult(v));
      return 1;
    }
    return 0;
  }

  WARN_UNUSED_RESULT int begin(VkCommandBufferUsageFlagBits usageFlags) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    VkCommandBufferBeginInfo VkInit(cbbi);
    cbbi.flags = usageFlags;
    VkResult v = vkBeginCommandBuffer(vk, &cbbi);
    if (v != VK_SUCCESS) {
      logE("%s failed: %d (%s)\n", "vkBeginCommandBuffer", v,
           string_VkResult(v));
      return 1;
    }
    return 0;
  }

  WARN_UNUSED_RESULT int beginOneTimeUse() {
    return begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  }
  WARN_UNUSED_RESULT int beginSimultaneousUse() {
    return begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
  }

  WARN_UNUSED_RESULT int end() {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    VkResult v = vkEndCommandBuffer(vk);
    if (v != VK_SUCCESS) {
      logE("%s failed: %d (%s)\n", "vkEndCommandBuffer", v, string_VkResult(v));
      return 1;
    }
    return 0;
  }

  WARN_UNUSED_RESULT int executeCommands(uint32_t secondaryCmdsCount,
                                         VkCommandBuffer* pSecondaryCmds) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdExecuteCommands(vk, secondaryCmdsCount, pSecondaryCmds);
    return 0;
  }

  WARN_UNUSED_RESULT int pushConstants(Pipeline& pipe,
                                       VkShaderStageFlags stageFlags,
                                       uint32_t offset, uint32_t size,
                                       const void* pValues) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdPushConstants(vk, pipe.pipelineLayout, stageFlags, offset, size,
                       pValues);
    return 0;
  }

  template <typename T>
  WARN_UNUSED_RESULT int pushConstants(Pipeline& pipe,
                                       VkShaderStageFlags stageFlags,
                                       const T& value, uint32_t offset = 0) {
    return pushConstants(pipe, stageFlags, offset, sizeof(T), &value);
  }

  WARN_UNUSED_RESULT int fillBuffer(VkBuffer dst, VkDeviceSize dstOffset,
                                    VkDeviceSize size, uint32_t data) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdFillBuffer(vk, dst, dstOffset, size, data);
    return 0;
  }

  WARN_UNUSED_RESULT int updateBuffer(VkBuffer dst, VkDeviceSize dstOffset,
                                      VkDeviceSize dataSize,
                                      const void* pData) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdUpdateBuffer(vk, dst, dstOffset, dataSize, pData);
    return 0;
  }

  WARN_UNUSED_RESULT int copyBuffer(VkBuffer src, VkBuffer dst,
                                    const std::vector<VkBufferCopy>& regions) {
    if (regions.size() == 0) {
      logE("copyBuffer with empty regions\n");
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdCopyBuffer(vk, src, dst, regions.size(), regions.data());
    return 0;
  }
  WARN_UNUSED_RESULT int copyBuffer(VkBuffer src, VkBuffer dst, size_t size) {
    VkBufferCopy region = {};
    region.size = size;
    return copyBuffer(src, dst, std::vector<VkBufferCopy>{region});
  }

  WARN_UNUSED_RESULT int copyBufferToImage(
      VkBuffer src, VkImage dst, VkImageLayout dstLayout,
      const std::vector<VkBufferImageCopy>& regions) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdCopyBufferToImage(vk, src, dst, dstLayout, regions.size(),
                           regions.data());
    return 0;
  }

  WARN_UNUSED_RESULT int copyImageToBuffer(
      VkImage src, VkImageLayout srcLayout, VkBuffer dst,
      const std::vector<VkBufferImageCopy>& regions) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdCopyImageToBuffer(vk, src, srcLayout, dst, regions.size(),
                           regions.data());
    return 0;
  }

  WARN_UNUSED_RESULT int copyImage(VkImage src, VkImageLayout srcLayout,
                                   VkImage dst, VkImageLayout dstLayout,
                                   const std::vector<VkImageCopy>& regions) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdCopyImage(vk, src, srcLayout, dst, dstLayout, regions.size(),
                   regions.data());
    return 0;
  }

  // copyImage is a convenience method to get the layout from memory::Image.
  WARN_UNUSED_RESULT int copyImage(memory::Image& src, memory::Image& dst,
                                   const std::vector<VkImageCopy>& regions);

  // copyImage is a convenience method to get the layout from memory::Image.
  WARN_UNUSED_RESULT int copyImage(
      memory::Buffer& src, memory::Image& dst,
      const std::vector<VkBufferImageCopy>& regions);

  // copyImage is a convenience method to get the layout from memory::Image.
  WARN_UNUSED_RESULT int copyImage(
      memory::Image& src, memory::Buffer& dst,
      const std::vector<VkBufferImageCopy>& regions);

  WARN_UNUSED_RESULT int blitImage(VkImage src, VkImageLayout srcLayout,
                                   VkImage dst, VkImageLayout dstLayout,
                                   const std::vector<VkImageBlit>& regions,
                                   VkFilter filter = VK_FILTER_LINEAR) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdBlitImage(vk, src, srcLayout, dst, dstLayout, regions.size(),
                   regions.data(), filter);
    return 0;
  }

  // blitImage is a convenience method to get the layout from memory::Image.
  WARN_UNUSED_RESULT int blitImage(memory::Image& src, memory::Image& dst,
                                   const std::vector<VkImageBlit>& regions,
                                   VkFilter filter = VK_FILTER_LINEAR);

  WARN_UNUSED_RESULT int resolveImage(
      VkImage src, VkImageLayout srcLayout, VkImage dst,
      VkImageLayout dstLayout, const std::vector<VkImageResolve>& regions) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdResolveImage(vk, src, srcLayout, dst, dstLayout, regions.size(),
                      regions.data());
    return 0;
  }

  // resolveImage is a convenience method to get the layout from memory::Image.
  WARN_UNUSED_RESULT int resolveImage(
      memory::Image& src, memory::Image& dst,
      const std::vector<VkImageResolve>& regions);

  WARN_UNUSED_RESULT int copyQueryPoolResults(
      VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount,
      VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride,
      VkQueryResultFlags flags) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdCopyQueryPoolResults(vk, queryPool, firstQuery, queryCount, dstBuffer,
                              dstOffset, stride, flags);
    return 0;
  }

  WARN_UNUSED_RESULT int resetQueryPool(VkQueryPool queryPool,
                                        uint32_t firstQuery,
                                        uint32_t queryCount) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdResetQueryPool(vk, queryPool, firstQuery, queryCount);
    return 0;
  }

  WARN_UNUSED_RESULT int beginQuery(VkQueryPool queryPool, uint32_t query,
                                    VkQueryControlFlags flags) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdBeginQuery(vk, queryPool, query, flags);
    return 0;
  }

  WARN_UNUSED_RESULT int endQuery(VkQueryPool queryPool, uint32_t query) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdEndQuery(vk, queryPool, query);
    return 0;
  }

  WARN_UNUSED_RESULT int writeTimestamp(VkPipelineStageFlagBits stage,
                                        VkQueryPool queryPool, uint32_t query) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdWriteTimestamp(vk, stage, queryPool, query);
    return 0;
  }

  WARN_UNUSED_RESULT int beginRenderPass(VkRenderPassBeginInfo& passBeginInfo,
                                         VkSubpassContents contents) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    if (!passBeginInfo.framebuffer) {
      logE("CommandBuffer::beginRenderPass: framebuffer was not set\n");
      return 1;
    }
    vkCmdBeginRenderPass(vk, &passBeginInfo, contents);
    return 0;
  }
  WARN_UNUSED_RESULT int beginRenderPass(RenderPass& pass,
                                         language::Framebuf& framebuf,
                                         VkSubpassContents contents) {
    VkRenderPassBeginInfo VkInit(passBeginInfo);
    passBeginInfo.renderPass = pass.vk;
    passBeginInfo.framebuffer = framebuf.vk;
    passBeginInfo.renderArea.offset = {0, 0};
    passBeginInfo.renderArea.extent = cpool.dev.swapChainInfo.imageExtent;
    passBeginInfo.clearValueCount = pass.clearColors.size();
    passBeginInfo.pClearValues = pass.clearColors.data();
    return beginRenderPass(passBeginInfo, contents);
  }

  // beginPrimaryPass starts a RenderPass using a primary command buffer.
  WARN_UNUSED_RESULT int beginPrimaryPass(RenderPass& pass,
                                          language::Framebuf& framebuf) {
    return beginRenderPass(pass, framebuf, VK_SUBPASS_CONTENTS_INLINE);
  }
  // beginSecondaryPass is necessary to bind a secondary command buffer to
  // its associated RenderPass even though it will be called from the correct
  // primary command buffer.
  WARN_UNUSED_RESULT int beginSecondaryPass(RenderPass& pass,
                                            language::Framebuf& framebuf) {
    return beginRenderPass(pass, framebuf,
                           VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
  }

  WARN_UNUSED_RESULT int nextSubpass(VkSubpassContents contents) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdNextSubpass(vk, contents);
    return 0;
  }
  // nextPrimarySubpass starts the next subpass, but must be in a primary
  // command buffer.
  WARN_UNUSED_RESULT int nextPrimarySubpass() {
    return nextSubpass(VK_SUBPASS_CONTENTS_INLINE);
  }
  // nextSecondarySubpass allows a secondary command buffer to have subpasses.
  WARN_UNUSED_RESULT int nextSecondarySubpass() {
    return nextSubpass(VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
  }

  WARN_UNUSED_RESULT int endRenderPass() {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdEndRenderPass(vk);
    return 0;
  }

  WARN_UNUSED_RESULT int bindPipeline(VkPipelineBindPoint bindPoint,
                                      Pipeline& pipe) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdBindPipeline(vk, bindPoint, pipe.vk);
    return 0;
  }

  WARN_UNUSED_RESULT int bindDescriptorSets(
      VkPipelineBindPoint bindPoint, VkPipelineLayout layout, uint32_t firstSet,
      uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets,
      uint32_t dynamicOffsetCount = 0,
      const uint32_t* pDynamicOffsets = nullptr) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdBindDescriptorSets(vk, bindPoint, layout, firstSet, descriptorSetCount,
                            pDescriptorSets, dynamicOffsetCount,
                            pDynamicOffsets);
    return 0;
  }
  WARN_UNUSED_RESULT int bindGraphicsPipelineAndDescriptors(
      Pipeline& pipe, uint32_t firstSet, uint32_t descriptorSetCount,
      const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount = 0,
      const uint32_t* pDynamicOffsets = nullptr) {
    return bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipe) ||
           bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipe.pipelineLayout, firstSet, descriptorSetCount,
                              pDescriptorSets, dynamicOffsetCount,
                              pDynamicOffsets);
  }
  WARN_UNUSED_RESULT int bindComputePipelineAndDescriptors(
      Pipeline& pipe, uint32_t firstSet, uint32_t descriptorSetCount,
      const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount = 0,
      const uint32_t* pDynamicOffsets = nullptr) {
    return bindPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, pipe) ||
           bindDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE,
                              pipe.pipelineLayout, firstSet, descriptorSetCount,
                              pDescriptorSets, dynamicOffsetCount,
                              pDynamicOffsets);
  }

  WARN_UNUSED_RESULT int bindVertexBuffers(uint32_t firstBinding,
                                           uint32_t bindingCount,
                                           const VkBuffer* pBuffers,
                                           const VkDeviceSize* pOffsets) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdBindVertexBuffers(vk, firstBinding, bindingCount, pBuffers, pOffsets);
    return 0;
  }

  WARN_UNUSED_RESULT int bindIndexBuffer(VkBuffer indexBuf, VkDeviceSize offset,
                                         VkIndexType indexType) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdBindIndexBuffer(vk, indexBuf, offset, indexType);
    return 0;
  }

  WARN_UNUSED_RESULT int drawIndexed(uint32_t indexCount,
                                     uint32_t instanceCount,
                                     uint32_t firstIndex, int32_t vertexOffset,
                                     uint32_t firstInstance) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdDrawIndexed(vk, indexCount, instanceCount, firstIndex, vertexOffset,
                     firstInstance);
    return 0;
  }
  WARN_UNUSED_RESULT int bindAndDraw(const std::vector<uint16_t>& indices,
                                     VkBuffer indexBuf, VkDeviceSize offset,
                                     uint32_t instanceCount = 1,
                                     uint32_t firstIndex = 0,
                                     int32_t vertexOffset = 0,
                                     uint32_t firstInstance = 0) {
    return bindIndexBuffer(indexBuf, offset, VK_INDEX_TYPE_UINT16) ||
           drawIndexed(indices.size(), instanceCount, firstIndex, vertexOffset,
                       firstInstance);
  }
  WARN_UNUSED_RESULT int bindAndDraw(const std::vector<uint32_t>& indices,
                                     VkBuffer indexBuf,
                                     VkDeviceSize indexBufOffset,
                                     uint32_t instanceCount = 1,
                                     uint32_t firstIndex = 0,
                                     int32_t vertexOffset = 0,
                                     uint32_t firstInstance = 0) {
    return bindIndexBuffer(indexBuf, indexBufOffset, VK_INDEX_TYPE_UINT32) ||
           drawIndexed(indices.size(), instanceCount, firstIndex, vertexOffset,
                       firstInstance);
  }

  WARN_UNUSED_RESULT int drawIndexedIndirect(VkBuffer buffer,
                                             VkDeviceSize offset,
                                             uint32_t drawCount,
                                             uint32_t stride) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdDrawIndexedIndirect(vk, buffer, offset, drawCount, stride);
    return 0;
  }

  WARN_UNUSED_RESULT int draw(uint32_t vertexCount, uint32_t instanceCount,
                              uint32_t firstVertex, uint32_t firstInstance) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdDraw(vk, vertexCount, instanceCount, firstVertex, firstInstance);
    return 0;
  }

  WARN_UNUSED_RESULT int drawIndirect(VkBuffer buffer, VkDeviceSize offset,
                                      uint32_t drawCount, uint32_t stride) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdDrawIndirect(vk, buffer, offset, drawCount, stride);
    return 0;
  }

  WARN_UNUSED_RESULT int clearAttachments(uint32_t attachmentCount,
                                          const VkClearAttachment* pAttachments,
                                          uint32_t rectCount,
                                          const VkClearRect* pRects) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdClearAttachments(vk, attachmentCount, pAttachments, rectCount, pRects);
    return 0;
  }

  WARN_UNUSED_RESULT int clearColorImage(
      VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor,
      uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdClearColorImage(vk, image, imageLayout, pColor, rangeCount, pRanges);
    return 0;
  }

  WARN_UNUSED_RESULT int clearDepthStencilImage(
      VkImage image, VkImageLayout imageLayout,
      const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount,
      const VkImageSubresourceRange* pRanges) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdClearDepthStencilImage(vk, image, imageLayout, pDepthStencil,
                                rangeCount, pRanges);
    return 0;
  }

  WARN_UNUSED_RESULT int dispatch(uint32_t groupCountX, uint32_t groupCountY,
                                  uint32_t groupCountZ) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdDispatch(vk, groupCountX, groupCountY, groupCountZ);
    return 0;
  }

  WARN_UNUSED_RESULT int dispatchIndirect(VkBuffer buffer,
                                          VkDeviceSize offset) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdDispatchIndirect(vk, buffer, offset);
    return 0;
  }

  // References for understanding memory synchronization primitives:
  // https://www.khronos.org/assets/uploads/developers/library/2017-khronos-uk-vulkanised/004-Synchronization-Keeping%20Your%20Device%20Fed_May17.pdf
  //
  // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
  struct BarrierSet {
    BarrierSet() { reset(); }

    void reset() {
      mem.clear();
      buf.clear();
      img.clear();
      srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    std::vector<VkMemoryBarrier> mem;
    std::vector<VkBufferMemoryBarrier> buf;
    std::vector<VkImageMemoryBarrier> img;

    VkPipelineStageFlags srcStageMask;
    VkPipelineStageFlags dstStageMask;
  };
  BarrierSet lazyBarriers;

  // waitBarrier calls vkCmdPipelineBarrier. This will flush previous barrier()
  // calls if they were used, but gives direct access to vkCmdPipelineBarrier.
  // The other forms of barrier() below lazily construct a BarrierSet which is
  // automatically flushed before the command buffer is used again.
  WARN_UNUSED_RESULT int waitBarrier(const BarrierSet& b,
                                     VkDependencyFlags dependencyFlags = 0);

  // waitEvents calls vkCmdWaitEvents. Since vkCmdWaitEvents also accepts
  // all the barrier structs, this flushes all lazy barriers.
  // Please see setEvent() and resetEvent() for using VkEvent.
  WARN_UNUSED_RESULT int waitEvents(const std::vector<VkEvent>& events);

  // waitEvents calls vkCmdWaitEvents. But it accepts all the barrier structs
  // as well, except not VkDependencyFlags. In addition, any VkEvents are
  // waited on. Please see setEvent() and resetEvent() for using VkEvent.
  WARN_UNUSED_RESULT int waitEvents(const std::vector<VkEvent>& events,
                                    BarrierSet& b) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdWaitEvents(vk, events.size(), events.data(), b.srcStageMask,
                    b.dstStageMask, b.mem.size(), b.mem.data(), b.buf.size(),
                    b.buf.data(), b.img.size(), b.img.data());
    return 0;
  }

  // barrier(Image, VkImageLayout) adds a barrier that transitions Image to
  // a new layout.
  WARN_UNUSED_RESULT int barrier(memory::Image& img, VkImageLayout newLayout);

  // barrier(Image, VkImageLayout, VkImageSubresourceRange) adds a barrier that
  // transitions the part of Image given by 'range' to a new layout.
  WARN_UNUSED_RESULT int barrier(memory::Image& img, VkImageLayout newLayout,
                                 const VkImageSubresourceRange& range);

  // barrier(VkBufferMemoryBarrier) can be used to transition a buffer to a new
  // queue.
  WARN_UNUSED_RESULT int barrier(const VkBufferMemoryBarrier& b) {
    if (b.sType != VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER) {
      logE("barrier(%s): invalid %s.sType\n", "VkBufferMemoryBarrier",
           "VkBufferMemoryBarrier");
      return 1;
    }
    if (!b.buffer) {
      logE("CommandBuffer::barrier(VkBufferMemoryBarrier): invalid VkBuffer\n");
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    lazyBarriers.buf.emplace_back(b);
    return 0;
  }

  // barrier(VkMemoryBarrier) can be used to enforce a device-wide barrier.
  WARN_UNUSED_RESULT int barrier(const VkMemoryBarrier& b) {
    if (b.sType != VK_STRUCTURE_TYPE_MEMORY_BARRIER) {
      logE("barrier(%s): invalid %s.sType\n", "VkMemoryBarrier",
           "VkMemoryBarrier");
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    lazyBarriers.mem.emplace_back(b);
    return 0;
  }

  WARN_UNUSED_RESULT int setEvent(VkEvent event,
                                  VkPipelineStageFlags stageMask) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetEvent(vk, event, stageMask);
    return 0;
  }
  WARN_UNUSED_RESULT int setEvent(Event& event,
                                  VkPipelineStageFlags stageMask) {
    return setEvent(event.vk, stageMask);
  }

  WARN_UNUSED_RESULT int resetEvent(VkEvent event,
                                    VkPipelineStageFlags stageMask) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdResetEvent(vk, event, stageMask);
    return 0;
  }
  WARN_UNUSED_RESULT int resetEvent(Event& event,
                                    VkPipelineStageFlags stageMask) {
    return resetEvent(event.vk, stageMask);
  }

  //
  // The following commands require the currently bound pipeline had
  // VK_DYNAMIC_STATE_* flags enabled first.
  //

  WARN_UNUSED_RESULT int setBlendConstants(const float blendConstants[4]) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetBlendConstants(vk, blendConstants);
    return 0;
  }
  WARN_UNUSED_RESULT int setDepthBias(float constantFactor, float clamp,
                                      float slopeFactor) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetDepthBias(vk, constantFactor, clamp, slopeFactor);
    return 0;
  }
  WARN_UNUSED_RESULT int setDepthBounds(float minBound, float maxBound) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetDepthBounds(vk, minBound, maxBound);
    return 0;
  }
  WARN_UNUSED_RESULT int setLineWidth(float lineWidth) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetLineWidth(vk, lineWidth);
    return 0;
  }
  WARN_UNUSED_RESULT int setScissor(uint32_t firstScissor,
                                    uint32_t scissorCount,
                                    const VkRect2D* pScissors) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetScissor(vk, firstScissor, scissorCount, pScissors);
    return 0;
  }
  WARN_UNUSED_RESULT int setStencilCompareMask(VkStencilFaceFlags faceMask,
                                               uint32_t compareMask) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetStencilCompareMask(vk, faceMask, compareMask);
    return 0;
  }
  WARN_UNUSED_RESULT int setStencilReference(VkStencilFaceFlags faceMask,
                                             uint32_t reference) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetStencilReference(vk, faceMask, reference);
    return 0;
  }
  WARN_UNUSED_RESULT int setStencilWriteMask(VkStencilFaceFlags faceMask,
                                             uint32_t writeMask) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetStencilWriteMask(vk, faceMask, writeMask);
    return 0;
  }
  WARN_UNUSED_RESULT int setViewport(uint32_t firstViewport,
                                     uint32_t viewportCount,
                                     const VkViewport* pViewports) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetViewport(vk, firstViewport, viewportCount, pViewports);
    return 0;
  }
  // setViewport is a convenience method to update all viewports in a
  // VkRenderPass from the viewports in pass.pipelines[]->info.
  WARN_UNUSED_RESULT int setViewport(RenderPass& pass) {
    std::vector<VkViewport> viewports;
    for (auto& pipe : pass.pipelines) {
      viewports.insert(viewports.end(), pipe->info.viewports.begin(),
                       pipe->info.viewports.end());
    }
    return setViewport(0, viewports.size(), viewports.data());
  }
  // setScissor is a convenience method to update all scissors in a
  // VkRenderPass from the scissors in pass.pipelines[]->info.
  WARN_UNUSED_RESULT int setScissor(RenderPass& pass) {
    std::vector<VkRect2D> scissors;
    for (auto& pipe : pass.pipelines) {
      scissors.insert(scissors.end(), pipe->info.scissors.begin(),
                      pipe->info.scissors.end());
    }
    return setScissor(0, scissors.size(), scissors.data());
  }
};

}  // namespace command
