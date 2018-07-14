/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 *
 * src/memory is the 4th-level bindings for the Vulkan graphics library.
 * src/memory is part of github.com/ndsol/volcano.
 * This library is called "memory" as a homage to Star Trek First Contact.
 * Like a Vulcan's Memory, this library remembers everything.
 */

#include <src/command/command.h>
#include <src/language/VkInit.h>
#include <src/language/language.h>
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
#ifdef __ANDROID__
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DEDICATED_ALLOCATION 0
#endif /*__ANDROID__*/
#include <vendor/vulkanmemoryallocator/vk_mem_alloc.h>
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

#pragma once

namespace memory {

// ASSUME_POOL_QINDEX is used when the CommandPool queue must be assumed.
// (There are many use cases where a non-zero queue index is so uncommon,
// it is not supported. The use of this constant documents the assumption.)
constexpr size_t ASSUME_POOL_QINDEX = 0;
// ASSUME_PRESENT_QINDEX is used in science::PresentSemaphore to assume the
// queue index is zero. The use of this constant documents the assumption.
constexpr size_t ASSUME_PRESENT_QINDEX = 0;

#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
// Define a simple substitute for struct VmaAllocation.
// This is only used if your app defines VOLCANO_DISABLE_VULKANMEMORYALLOCATOR.
typedef struct VmaAllocation {
  VmaAllocation(language::Device& dev) : vk{dev.dev, vkFreeMemory} {}

  VkMemoryPropertyFlags requiredProps;
  VkDeviceSize allocSize{0};
  void* mapped{0};
  VkPtr<VkDeviceMemory> vk;
} VmaAllocation;
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

struct MemoryRequirements;

// DeviceMemory represents a raw chunk of bytes that can be accessed by the
// device. Because GPUs are in everything now, the memory may not be physically
// "on the device," but all that is hidden by the device driver to make it
// seem like it is.
//
// DeviceMemory is not very useful on its own. But alloc() can be fed a
// MemoryRequirements object with an in-place constructor, like this:
//   DeviceMemory mem(dev);
//   mem.alloc({dev, img});  // MemoryRequirements constructed from an Image.
//
//   DeviceMemory mem(dev);
//   mem.alloc({dev, buf});  // MemoryRequirements constructed from a Buffer.
//
// By using the overloaded constructors in MemoryRequirements,
// DeviceMemory::alloc() is kept simple.
typedef struct DeviceMemory {
  DeviceMemory(language::Device& dev)
      : dev(dev),
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
        vmaAlloc(0)
#else  /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
        vmaAlloc(dev)  // The simple substitute struct uses Device directly.
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  {
  }

  // Explicit move constructor because of lockmutex:
  DeviceMemory(DeviceMemory&& other)
      : dev(other.dev), vmaAlloc(std::move(other.vmaAlloc)) {
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
    if (other.lockmutex.try_lock()) {
      other.vmaAlloc = 0;
      other.lockmutex.unlock();
    } else {
      logE("DeviceMemory(DeviceMemory&&): other.lockmutex is locked!");
      logE("DeviceMemory(DeviceMemory&&): thread collision!\n");
    }
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  }

  virtual ~DeviceMemory();

  // alloc() calls vmaAllocateMemoryFor{Buffer,Image}()
  // (or vkAllocateMemory() if not using vulkanmemoryallocator).
  // returns non-zero on error.
  // Note: if you use Image, Buffer, etc. below, alloc() is automatically called
  // for you by Image::ctorError(), Buffer::ctorError(), etc.
  //
  // Note: if not using vulkanmemoryallocator, vmaAlloc.requiredProps *must* be
  // set before alloc() is called. This is automatically done for you by
  // Image::ctorError(), Buffer::ctorError(), etc.
  WARN_UNUSED_RESULT int alloc(MemoryRequirements req);

  // mmap() calls vkMapMemory() and returns non-zero on error.
  // NOTE: The vkMapMemory spec currently says "flags is reserved for future
  // use." You probably can ignore the flags parameter.
  WARN_UNUSED_RESULT int mmap(void** pData, VkDeviceSize offset = 0,
                              VkDeviceSize size = VK_WHOLE_SIZE,
                              VkMemoryMapFlags flags = 0);

  // makeRange overwrites range and constructs it to point to this DeviceMemory
  // block, with the given offset and size.
  void makeRange(VkMappedMemoryRange& range,
#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
                 VkDeviceSize offset, VkDeviceSize size
#else  /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
                 const VmaAllocationInfo& info
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  );

  // flush tells the Vulkan driver to flush any CPU writes that may still be
  // pending in CPU caches; flush makes all CPU writes visible to the device.
  // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT generally means that calling flush
  // is not neeed.
  // Note: all VkMappedMemoryRange provided will have .memory set this this->vk.
  WARN_UNUSED_RESULT int flush(
#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
      std::vector<VkMappedMemoryRange> mem
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  );

  // invalidate tells the Vulkan driver to flush any device writes so that they
  // are visible to the CPU.
  // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT generally means that calling
  // invalidate is not neeed.
  WARN_UNUSED_RESULT int invalidate(
#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
      const std::vector<VkMappedMemoryRange>& mem
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  );

  // munmap() calls vkUnmapMemory().
  void munmap();

  language::Device& dev;
  // vmaAlloc is an internal struct if VOLCANO_DISABLE_VULKANMEMORYALLOCATOR:
  VmaAllocation vmaAlloc;

#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  // getAllocInfo is a convenient wrapper around vmaGetAllocationInfo
  // WARNING: calling vmaGetAllocationInfo directly must be synchronized
  // (consider synchronizing on lockmutex below like getAllocInfo() does).
  WARN_UNUSED_RESULT int getAllocInfo(VmaAllocationInfo& info);

  // lock_guard_t: like c++17's constructor type inference, but we are in c++11
  // Instead of: std::lock_guard<std::recursive_mutex> lock(mem.lockmutex);
  // Do this:    DeviceMemory::lock_guard_t            lock(mem.lockmutex);
  // (Then uses of lock_guard_t do not assume lockmutex is a recursive_mutex.)
  typedef std::lock_guard<std::recursive_mutex> lock_guard_t;
  // unique_lock_t: like c++17's constructor type inference, but in c++11
  // Instead of: std::unique_lock<std::recursive_mutex> lock(mem.lockmutex);
  // Do this:    DeviceMemory::unique_lock_t            lock(mem.lockmutex);
  // (Then uses of unique_lock_t do not assume lockmutex is a recursive_mutex.)
  typedef std::unique_lock<std::recursive_mutex> unique_lock_t;

  // lockmutex is used internally when vulkanmemoryallocator requires a mutex.
  std::recursive_mutex lockmutex;
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
} DeviceMemory;

// Image represents a VkImage.
typedef struct Image {
  Image(language::Device& dev) : vk{dev.dev, vkDestroyImage}, mem(dev) {
    vk.allocator = dev.dev.allocator;
    VkOverwrite(info);
    info.imageType = VK_IMAGE_TYPE_2D;
    // You must set info.extent.width, info.extent.height, and
    // info.extent.depth. For a 2D image, set depth = 1. For a 1D image,
    // set height = 1 and depth = 1.
    info.mipLevels = 1;
    info.arrayLayers = 1;
    // You must set info.format
    // You probably want tiling = VK_IMAGE_TILING_OPTIMAL most of the time:
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    // You must set info.usage
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // ctorError() sets currentLayout = info.initialLayout.
    currentLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
  }
  Image(Image&&) = default;
  Image(const Image&) = delete;

  // ctorError must be called after filling in this->info to construct the
  // Image. Note that bindMemory() should be called after ctorError().
  //
  // The Vulkan spec says vkGetImageSubresourceLayout is only valid for
  // info.tiling == VK_IMAGE_TILING_LINEAR, and is invariant after ctorError().
  // It is automatically queried in the `colorMem` vector and associates.
  //
  // Your application may not need to call ctorError directly.
  // Sampler::ctorError(), below, automatically sets up an image for a shader
  // sampler, and science::Pipeline() automatically sets up an image for a depth
  // buffer.
  WARN_UNUSED_RESULT int ctorError(VkMemoryPropertyFlags props);

#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  // ctorError must be called after filling in this->info to construct the
  // Image. Note that bindMemory() should be alled after ctorError(). This is
  // a more convenient form of ctorError that uses VmaMemoryUsage.
  WARN_UNUSED_RESULT int ctorError(VmaMemoryUsage usage);
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

  WARN_UNUSED_RESULT int ctorDeviceLocal() {
    return ctorError(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }

  // ctorHostVisible is for linear, host visible images. NOTE: You probably
  // should look at using buffers instead, which support loading compressed,
  // tiled image formats directly.
  WARN_UNUSED_RESULT int ctorHostVisible() {
    info.tiling = VK_IMAGE_TILING_LINEAR;
    info.usage |=
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return ctorError(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  }

  // ctorHostCoherent is for linear, host coherent images. NOTE: You probably
  // should look at using buffers instead, which support loading compressed,
  // tiled image formats directly.
  WARN_UNUSED_RESULT int ctorHostCoherent() {
    info.tiling = VK_IMAGE_TILING_LINEAR;
    info.usage |=
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return ctorError(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  }

  // bindMemory() calls vkBindImageMemory which binds this->mem
  // or automatically upgrades to vkBindImageMemory2 if supported.
  // Note: do not call bindMemory() until a point after ctorError().
  WARN_UNUSED_RESULT int bindMemory(VkDeviceSize offset = 0);

  // reset() releases this and this->mem.
  WARN_UNUSED_RESULT int reset();

  // makeTransition() makes a VkImageMemoryBarrier for commandBuffer::barrier()
  // Your app can use commandBuffer::barrier(), which will call this for you.
  VkImageMemoryBarrier makeTransition(VkImageLayout newLayout);

  // getAllAspects computes VkImageAspectFlags purely as a function of
  // info.format.
  VkImageAspectFlags getAllAspects() const;

  // getSubresource is a convenience to get a VkImageSubresource.
  // Note: getSubresource is purely a function of info.format, so set it first.
  VkImageSubresource getSubresource(uint32_t mipLevel,
                                    uint32_t arrayLayer = 0) const {
    VkImageSubresource r;
    r.aspectMask = getAllAspects();
    r.mipLevel = mipLevel;
    r.arrayLayer = arrayLayer;
    return r;
  }

  // getSubresourceRange is a convenience to get a VkImageSubresourceRange.
  // Note: getSubresourceRange is purely a function of info.format,
  // info.mipLevels, and info.arrayLayers. They must have valid values first.
  VkImageSubresourceRange getSubresourceRange() const {
    VkImageSubresourceRange r;
    r.aspectMask = getAllAspects();
    r.baseMipLevel = 0;
    r.levelCount = info.mipLevels;
    r.baseArrayLayer = 0;
    r.layerCount = info.arrayLayers;
    return r;
  }

  // getSubresourceLayers is a convenience to get a VkImageSubresourceLayers.
  // Note: getSubresourceLayers is purely a function of info.format and
  // info.arrayLayers. They must have valid values first.
  VkImageSubresourceLayers getSubresourceLayers(uint32_t mipLevel) const {
    VkImageSubresourceLayers r;
    r.aspectMask = getAllAspects();
    r.mipLevel = mipLevel;
    r.baseArrayLayer = 0;
    r.layerCount = info.arrayLayers;
    return r;
  }

  VkImageCreateInfo info;
  VkImageLayout currentLayout;
  VkPtr<VkImage> vk;  // populated after ctorError().
  DeviceMemory mem;   // ctorError() calls mem.alloc() for you.

  // colorMem is populated by ctorError() if layout is LINEAR and the format
  // includes color channels.
  std::vector<VkSubresourceLayout> colorMem;
  // depthMem is populated by ctorError() if layout is LINEAR and the format
  // includes a depth channel.
  std::vector<VkSubresourceLayout> depthMem;
  // stencilMem is populated by ctorError() if layout is LINEAR and the format
  // includes a stencil channel.
  std::vector<VkSubresourceLayout> stencilMem;

 protected:
  int makeTransitionAccessMasks(VkImageMemoryBarrier& imageB);
  int validateImageCreateInfo();
  int getSubresourceLayouts();
} Image;

// Buffer represents a VkBuffer.
typedef struct Buffer {
  Buffer(language::Device& dev) : vk{dev.dev, vkDestroyBuffer}, mem(dev) {
    vk.allocator = dev.dev.allocator;
    VkOverwrite(info);
    // You must set info.size.
    // You must set info.usage.
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }
  Buffer(Buffer&&) = default;
  Buffer(const Buffer&) = delete;

  // ctorError() must be called after filling in this->info to construct the
  // Buffer. Note that bindMemory() should be called after ctorError().
  //
  // Some aliases of ctorError() are defined below, which may make your
  // application less verbose. These are not all the possible combinations.
  //
  // If queueFams is empty, info.sharingMode can be anything. But if queueFams
  // is set, it holds the queue families that share this buffer and
  // info.sharingMode is overwritten with VK_SHARING_MODE_CONCURRENT.
  WARN_UNUSED_RESULT int ctorError(
      VkMemoryPropertyFlags props,
      const std::vector<uint32_t>& queueFams = std::vector<uint32_t>());

#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  // ctorError must be called after filling in this->info to construct the
  // Buffer. Note that bindMemory() should be alled after ctorError(). This is
  // a more convenient form of ctorError that uses VmaMemoryUsage.
  WARN_UNUSED_RESULT int ctorError(
      VmaMemoryUsage usage,
      const std::vector<uint32_t>& queueFams = std::vector<uint32_t>());
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

  // ctorDeviceLocal() adds TRANSFER_DST to usage, but you should set
  // its primary uses (for example, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
  // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
  // or all three).
  WARN_UNUSED_RESULT int ctorDeviceLocal(
      const std::vector<uint32_t>& queueFams = std::vector<uint32_t>()) {
    info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return ctorError(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, queueFams);
  }

  WARN_UNUSED_RESULT int ctorHostVisible(
      const std::vector<uint32_t>& queueFams = std::vector<uint32_t>()) {
    info.usage |=
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return ctorError(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, queueFams);
  }

  WARN_UNUSED_RESULT int ctorHostCoherent(
      const std::vector<uint32_t>& queueFams = std::vector<uint32_t>()) {
    info.usage |=
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return ctorError(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     queueFams);
  }

  // bindMemory() calls vkBindBufferMemory which binds this->mem.
  // Note: do not call bindMemory() until a point after ctorError().
  WARN_UNUSED_RESULT int bindMemory(VkDeviceSize offset = 0);

  // reset() releases this and this->mem.
  WARN_UNUSED_RESULT int reset();

  // copyFromHost copies bytes from the host at 'src' into this buffer.
  // Note that copyFromHost only makes sense if the buffer has been constructed
  // with ctorHostVisible or ctorHostCoherent.
  WARN_UNUSED_RESULT int copyFromHost(const void* src, size_t len,
                                      VkDeviceSize dstOffset = 0);

  // copyFromHost specialization for a std::vector<T>.
  template <typename T>
  WARN_UNUSED_RESULT int copyFromHost(const std::vector<T>& vec,
                                      VkDeviceSize dstOffset = 0) {
    return copyFromHost(vec.data(), sizeof(vec[0]) * vec.size(), dstOffset);
  }

  // copyFrom copies all the contents of Buffer src immediately and waits
  // until the copy is complete (synchronizing host and device).
  // This is the simplest form of copy.
  WARN_UNUSED_RESULT int copy(command::CommandPool& pool, Buffer& src);

  // copy copies all the contents of Buffer src using cmdBuffer, and does
  // not wait for the copy to complete.
  // Note that more finely controlled copies can be done with
  // command::CommandBuffer::copyBuffer().
  WARN_UNUSED_RESULT int copy(command::CommandBuffer& cmdBuffer, Buffer& src,
                              VkDeviceSize dstOffset = 0) {
    if (dstOffset + src.info.size > info.size) {
      logE("Buffer::copy(dstOffset=0x%llx,src.info.size=0x%llx): size=0x%llx\n",
           (unsigned long long)dstOffset, (unsigned long long)src.info.size,
           (unsigned long long)info.size);
      return 1;
    }

    VkBufferCopy region = {};
    region.dstOffset = dstOffset;
    region.size = src.info.size;
    return cmdBuffer.copyBuffer(src.vk, vk, std::vector<VkBufferCopy>{region});
  }
  VkBufferCreateInfo info;
  VkPtr<VkBuffer> vk;  // populated after ctorError().
  DeviceMemory mem;    // ctorError() calls mem.alloc() for you.

 protected:
  int validateBufferCreateInfo(const std::vector<uint32_t>& queueFams);
} Buffer;

// MemoryRequirements automatically gets the VkMemoryRequirements from
// the Device, and has helper methods for finding the VkMemoryAllocateInfo.
typedef struct MemoryRequirements {
#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  // Automatically get MemoryRequirements of a VkImage.
  MemoryRequirements(language::Device& dev, VkImage img) : dev(dev) {
    if (get(img)) {
      logF("MemoryRequirements ctor: get(VkImage) failed\n");
    }
  }
  // Automatically get MemoryRequirements of an Image.
  MemoryRequirements(language::Device& dev, Image& img) : dev(dev) {
    if (get(img)) {
      logF("MemoryRequirements ctor: get(Image) failed\n");
    }
  }
  // Automatically get MemoryRequirements of a VkBuffer.
  MemoryRequirements(language::Device& dev, VkBuffer buf) : dev(dev) {
    if (get(buf)) {
      logF("MemoryRequirements ctor: get(VkBuffer) failed\n");
    }
  }
  // Automatically get MemoryRequirements of a Buffer.
  MemoryRequirements(language::Device& dev, Buffer& buf) : dev(dev) {
    if (get(buf)) {
      logF("MemoryRequirements ctor: get(Buffer) failed\n");
    }
  }
#else  /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  // Automatically get MemoryRequirements of a VkImage.
  MemoryRequirements(language::Device& dev, VkImage img, VmaMemoryUsage usage)
      : dev(dev) {
    if (get(img)) {
      logF("MemoryRequirements ctor: get(VkImage) failed\n");
    }
    info.usage = usage;
  }
  // Automatically get MemoryRequirements of an Image.
  MemoryRequirements(language::Device& dev, Image& img, VmaMemoryUsage usage)
      : dev(dev) {
    if (get(img)) {
      logF("MemoryRequirements ctor: get(Image) failed\n");
    }
    info.usage = usage;
  }
  // Automatically get MemoryRequirements of a VkBuffer.
  MemoryRequirements(language::Device& dev, VkBuffer buf, VmaMemoryUsage usage)
      : dev(dev) {
    if (get(buf)) {
      logF("MemoryRequirements ctor: get(VkBuffer) failed\n");
    }
    info.usage = usage;
  }
  // Automatically get MemoryRequirements of a Buffer.
  MemoryRequirements(language::Device& dev, Buffer& buf, VmaMemoryUsage usage)
      : dev(dev) {
    if (get(buf)) {
      logF("MemoryRequirements ctor: get(Buffer) failed\n");
    }
    info.usage = usage;
  }
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

  // reset clears any previous requirements.
  void reset() {
#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
    VkOverwrite(vk);
    VkOverwrite(dedicated);
    VkOverwrite(vkalloc);
    vk.pNext = &dedicated;
#else  /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
    vkbuf = VK_NULL_HANDLE;
    vkimg = VK_NULL_HANDLE;
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  };

  // get populates VkMemoryRequirements2 vk from VkImage img.
  // If aspect is not 0 (0 is an invalid aspect), then img *must* be a planar
  // format and *must* have been created with VK_IMAGE_CREATE_DISJOINT_BIT.
  WARN_UNUSED_RESULT int get(VkImage img, VkImageAspectFlagBits optionalAspect =
                                              (VkImageAspectFlagBits)0);

  // get populates VkMemoryRequirements2 vk from memory::Image img.
  WARN_UNUSED_RESULT int get(Image& img, VkImageAspectFlagBits optionalAspect =
                                             (VkImageAspectFlagBits)0);

  // get populates VkMemoryRequirements2 vk from VkBuffer buf.
  WARN_UNUSED_RESULT int get(VkBuffer buf);

  // get populates VkMemoryRequirements2 vk from memory::Buffer img.
  WARN_UNUSED_RESULT int get(Buffer& buf) { return get(buf.vk); }

#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  // vkbuf and vkimg cannot both be non-NULL.
  VkBuffer vkbuf;
  // vkimg and vkbuf cnanot both be non-NULL.
  VkImage vkimg;
  // info is initialized after get, and your app should then fill in
  // info.usage and optionally info.flags. Or, leave info.usage unset and
  // set info.requiredFlags.
  VmaAllocationCreateInfo info;

#else  /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
 protected:
  friend struct DeviceMemory;

  // findVkalloc() populates vkalloc using the requirements in vk and the flags
  // specified in props. If an error occurs, findVkalloc returns 1.
  int findVkalloc(VkMemoryPropertyFlags props);

 private:
  // indexOf() returns -1 if the props cannot be found.
  int indexOf(VkMemoryPropertyFlags props) const;

 public:
  VkMemoryRequirements2 vk;
  VkMemoryDedicatedRequirements dedicated;
  VkMemoryAllocateInfo vkalloc;
  // TODO: VkMemoryDedicatedAllocateInfo in vkalloc.pNext.
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  language::Device& dev;
} MemoryRequirements;

// Sampler contains an Image, the ImageView, and the VkSampler, and has
// convenience methods for passing the VkSampler to descriptor sets and shaders.
typedef struct Sampler {
  // Construct a Sampler with info set to defaults (set to NEAREST mode,
  // which looks very blocky / pixellated).
  Sampler(language::Device& dev)
      : image{dev}, imageView{dev}, vk{dev.dev, vkDestroySampler} {
    vk.allocator = dev.dev.allocator;
    VkOverwrite(info);
    // info.magFilter = VK_FILTER_NEAREST;
    // info.minFilter = VK_FILTER_NEAREST;
    // info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.minLod = 0.0f;
    info.maxLod = 0.25f;  // 0.25 suggested in VkSamplerCreateInfo doc.
    if (dev.enabledFeatures.features.samplerAnisotropy == VK_TRUE) {
      info.anisotropyEnable = VK_TRUE;
      info.maxAnisotropy = dev.physProp.properties.limits.maxSamplerAnisotropy;
    } else {
      info.anisotropyEnable = VK_FALSE;
      info.maxAnisotropy = 1.0f;
    }
    info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;
    info.compareEnable = VK_FALSE;
    info.compareOp = VK_COMPARE_OP_ALWAYS;
  }

  // ctorError() constructs vk, the Image, and ImageView. It enqueues calls on
  // command::CommandBuffer 'buffer' to do layout transitions and copyImage().
  // The Image.info.{extent,format} are set to src.info.{extent,format}.
  // Also src.currentLayout is modified, which does not happen to the actual
  // image until the command buffer is submitted.
  // NOTE: Prefer using ctorError below which accepts a Buffer, not an Image.
  WARN_UNUSED_RESULT int ctorError(command::CommandBuffer& buffer, Image& src);

  // ctorError() constructs vk, the Image, and ImageView by calling the form of
  // ctorError above that accepts a CommandBuffer, then flushing the temporary
  // CommandBuffer before returning as a convenience.
  // NOTE: Prefer using ctorError below which accepts a Buffer, not an Image.
  WARN_UNUSED_RESULT int ctorError(command::CommandPool& cpool, Image& src);

  // ctorError() constructs vk, the Image, and ImageView. It enqueues calls on
  // command::CommandBuffer 'buffer' to do layout transitions and
  // copyBufferToImage().
  //
  // You must set image.info.{extent,format} before this call.
  // NOTE: This ctorError is preferred over a ctorError for Image& src, even
  // though you must set image.info here. A Buffer can hold a compressed, tiled
  // image format, multiple mip levels and array layers.
  WARN_UNUSED_RESULT int ctorError(
      command::CommandBuffer& buffer, Buffer& src,
      const std::vector<VkBufferImageCopy>& regions);

  // ctorError() is a convenience method that uses a temporary CommandBuffer
  // and flushes the CommandBuffer before returning. You must set image.info.
  WARN_UNUSED_RESULT int ctorError(
      command::CommandPool& cpool, Buffer& src,
      const std::vector<VkBufferImageCopy>& regions);

  // ctorExisting destroys and recreates the VkSampler, and is useful if your
  // app changes any members of VkSamplerCreateInfo info.
  WARN_UNUSED_RESULT int ctorExisting();

  // toDescriptor is a convenience method to add this Sampler to a descriptor
  // set.
  void toDescriptor(VkDescriptorImageInfo* imageInfo) {
    imageInfo->imageLayout = image.currentLayout;
    imageInfo->imageView = imageView.vk;
    imageInfo->sampler = vk;
  }

  Image image;
  language::ImageView imageView;
  VkSamplerCreateInfo info;
  VkPtr<VkSampler> vk;
} Sampler;

// UniformBuffer contains a buffer (just plain ordinary bytes) and adds a
// helper method for updating it before starting a RenderPass.
typedef struct UniformBuffer : public Buffer {
  UniformBuffer(language::Device& dev) : Buffer{dev}, stage{dev} {}
  UniformBuffer(UniformBuffer&&) = default;
  UniformBuffer(const UniformBuffer&) = delete;

  WARN_UNUSED_RESULT int ctorError(size_t nBytes) {
    info.size = stage.info.size = nBytes;
    info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    return stage.ctorHostCoherent() || stage.bindMemory() ||
           ctorDeviceLocal() || bindMemory();
  }

  // copy automatically handles staging the host data in a host-visible
  // Buffer 'stage', then copying it to the device-optimal Buffer 'this'.
  WARN_UNUSED_RESULT int copy(command::CommandPool& pool, const void* src,
                              size_t len, VkDeviceSize dstOffset = 0) {
    if (stage.copyFromHost(src, len, dstOffset)) {
      logE("stage.copyFromHost failed\n");
      return 1;
    }
    return Buffer::copy(pool, stage);
  }

  // copyAndKeepMmap automatically handles staging the host data in a Buffer
  // 'stage', then copying to 'this'. The mmap() pointer is cached and reused
  // when copyAndKeepMmap is called repeatedly.
  // NOTE: If pool.dev is destroyed, the UniformBuffer will not know that the
  // mmap is invalid!
  WARN_UNUSED_RESULT int copyAndKeepMmap(command::CommandPool& pool,
                                         const void* src, size_t len,
                                         VkDeviceSize dstOffset = 0);

  Buffer stage;

 protected:
  void* stageMmap{nullptr};
} UniformBuffer;

// DescriptorPool represents memory reserved for a DescriptorSet (or many).
// The assumption is that your application knows in advance the max number of
// DescriptorSet instances that will exist.
//
// It is also assumed your application knows the max number of each descriptor
// (VkDescriptorType) that will make up the DescriptorSet or sets.
typedef struct DescriptorPool {
  DescriptorPool(language::Device& dev)
      : dev(dev), vk{dev.dev, vkDestroyDescriptorPool} {
    vk.allocator = dev.dev.allocator;
  }
  DescriptorPool(DescriptorPool&&) = default;
  DescriptorPool(const DescriptorPool&) = delete;

  // ctorError calls vkCreateDescriptorPool to enable creating
  // a DescriptorSet from this pool.
  //
  // maxSets is how many DescriptorSet instances can be created.
  //
  // descriptors is how many of each VkDescriptorType to create:
  // add 1 VkDescriptorType (in any order) for every descriptor of
  // that type.
  WARN_UNUSED_RESULT int ctorError(
      uint32_t maxSets, const std::multiset<VkDescriptorType>& descriptors);

  WARN_UNUSED_RESULT int reset() {
    VkResult v = vkResetDescriptorPool(dev.dev, vk, 0 /*flags is reserved*/);
    if (v != VK_SUCCESS) {
      logE("%s failed: %d (%s)\n", "vkResetDescriptorPool", v,
           string_VkResult(v));
      return 1;
    }
    return 0;
  }

  language::Device& dev;
  VkPtr<VkDescriptorPool> vk;
} DescriptorPool;

// DescriptorSetLayout represents a group of VkDescriptorSetLayoutBinding
// objects. This is useful when several groups are being assembled into a
// DescriptorSet.
//
// It may be simpler to use science::ShaderLibrary.
typedef struct DescriptorSetLayout {
  DescriptorSetLayout(language::Device& dev)
      : vk{dev.dev, vkDestroyDescriptorSetLayout} {
    vk.allocator = dev.dev.allocator;
  }
  DescriptorSetLayout(DescriptorSetLayout&&) = default;
  DescriptorSetLayout(const DescriptorSetLayout&) = delete;

  // ctorError calls vkCreateDescriptorSetLayout.
  WARN_UNUSED_RESULT int ctorError(
      language::Device& dev,
      const std::vector<VkDescriptorSetLayoutBinding>& bindings);

  std::vector<VkDescriptorType> types;
  VkPtr<VkDescriptorSetLayout> vk;
} DescriptorSetLayout;

// DescriptorSet represents a set of bindings (which represent blocks of memory,
// containing an image, buffer, etc.).
//
// The host application must bind (provide) all the inputs the shader expects.
// If the DescriptorSet does not match the layout defined in the shader, Vulkan
// will report an error (and/or crash) - the VkDescriptorSetLayout is sent by
// your app to specify what it is providing. (See VkDescriptorType for the
// enumeration of all types that can be specified in a VkDescriptorSetLayout.)
//
// Notes:
// 1. When a DescriptorSet is allocated, it does not contain a valid type or
//    buffer! Use DescriptorSet::write to populate the DescriptorSet with the
//    type and buffer.
// 2. Create DescriptorSetLayout objects during Pipeline initialization to
//    assemble a valid pipeline (see PipelineCreateInfo).
// 3. Bind a DescriptorSet to the pipeline (specifically to the shader) during
//    a RenderPass to pass in inputs and receive outputs of the shader.
//    Note: the 'uint32_t binding' is confusingly named: it is just an index
//    for referring to similarly-typed descriptors (e.g. the uniform buffer
//    for the vertex shader may be at binding=0 while the uniform buffer for
//    the fragment shader may be at binding=1). Your app would still have to
//    bind buffers to both the binding=0 input and the binding=1 input.
typedef struct DescriptorSet {
  DescriptorSet(DescriptorPool& pool) : pool(pool) {}
  virtual ~DescriptorSet();

  // ctorError calls vkAllocateDescriptorSets.
  WARN_UNUSED_RESULT int ctorError(const DescriptorSetLayout& layout);

  // write populates the DescriptorSet with type and buffer.
  WARN_UNUSED_RESULT int write(
      uint32_t binding, const std::vector<VkDescriptorImageInfo> imageInfo,
      uint32_t arrayI = 0);
  // write populates the DescriptorSet with type and buffer.
  WARN_UNUSED_RESULT int write(
      uint32_t binding, const std::vector<VkDescriptorBufferInfo> bufferInfo,
      uint32_t arrayI = 0);
  // write populates the DescriptorSet with type and buffer.
  WARN_UNUSED_RESULT int write(
      uint32_t binding, const std::vector<VkBufferView> texelBufferViewInfo,
      uint32_t arrayI = 0);

  // write populates the DescriptorSet with type and buffer.
  WARN_UNUSED_RESULT int write(uint32_t binding,
                               const std::vector<Sampler*> samplers,
                               uint32_t arrayI = 0) {
    std::vector<VkDescriptorImageInfo> imageInfo;
    imageInfo.resize(samplers.size());
    for (size_t i = 0; i < samplers.size(); i++) {
      samplers.at(i)->toDescriptor(&imageInfo.at(i));
    }
    return write(binding, imageInfo, arrayI);
  }

  // write populates the DescriptorSet with type and buffer.
  WARN_UNUSED_RESULT int write(uint32_t binding,
                               const std::vector<Buffer*> buffers,
                               uint32_t arrayI = 0) {
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    bufferInfos.resize(buffers.size());
    for (size_t i = 0; i < buffers.size(); i++) {
      auto& bufferInfo = bufferInfos.at(i);
      auto& buffer = *buffers.at(i);
      bufferInfo.buffer = buffer.vk;
      bufferInfo.offset = 0;
      bufferInfo.range = buffer.info.size;
    }
    return write(binding, bufferInfos, arrayI);
  }

  DescriptorPool& pool;
  std::vector<VkDescriptorType> types;
  VkDescriptorSet vk;
} DescriptorSet;

// TODO: VkDescriptorUpdateTemplate

}  // namespace memory
