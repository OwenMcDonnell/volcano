/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 *
 * src/science is the 5th-level bindings for the Vulkan graphics library.
 * src/science is part of github.com/ndsol/volcano.
 * This library is called "science" as a homage to Star Trek First Contact.
 * Like the Vulcan Science Academy, this library is a repository of knowledge
 * as a series of builder classes.
 */

#include <src/command/command.h>
#include <src/language/language.h>
#include <src/memory/memory.h>
#include <string.h>
#include <limits>
#ifndef _WIN32
#include <unistd.h>
#endif

#pragma once

namespace science {

// ImageCopies is a vector of VkImageCopy with convenient methods for quickly
// creating each VkImageCopy.
//
// Example usage:
//  if (buffer.copyImage(src.vk, src.currentLayout, dst.vk, dst.currentLayout,
//                       science::ImageCopies(src))) { ... }
class ImageCopies : public std::vector<VkImageCopy> {
 public:
  // These are all the constructors that std::vector defines.
  typedef std::allocator<VkImageCopy> Allocator;
  explicit ImageCopies(const Allocator& alloc = Allocator())
      : std::vector<VkImageCopy>(alloc) {}
  ImageCopies(size_type count, const VkImageCopy& value = {},
              const Allocator& alloc = Allocator())
      : std::vector<VkImageCopy>(count, value, alloc) {}
#if !defined(__ANDROID__) && !defined(__clang__)
  explicit ImageCopies(size_type count, const Allocator& alloc = Allocator())
      : std::vector<VkImageCopy>(count, alloc) {}
#endif
  template <class InputIt>
  ImageCopies(InputIt first, InputIt last, const Allocator& alloc = Allocator())
      : std::vector<VkImageCopy>(first, last, alloc) {}
  ImageCopies(const ImageCopies& other) : std::vector<VkImageCopy>(other) {}
  ImageCopies(const ImageCopies& other, const Allocator& alloc)
      : std::vector<VkImageCopy>(other, alloc) {}
  ImageCopies(ImageCopies&& other) : std::vector<VkImageCopy>(other) {}
  ImageCopies(ImageCopies&& other, const Allocator& alloc)
      : std::vector<VkImageCopy>(other, alloc) {}
  ImageCopies(std::initializer_list<VkImageCopy> init,
              const Allocator& alloc = Allocator())
      : std::vector<VkImageCopy>(init, alloc) {}

  // Constructor for creating a straight 1:1 copy of src.
  ImageCopies(memory::Image& src, memory::Image& dst) { add(src, dst); }

  void add(memory::Image& src, memory::Image& dst);
  void addSingleMipLevel(memory::Image& src, uint32_t srcMipLevel,
                         memory::Image& dst, uint32_t dstMipLevel);
};

// CommandPoolContainer implements onResized() and automatically handles
// recreating the swapChain.
// NOTE: This assumes you use a single logical language::Device.
//       Even multiple GPUs likely only use a single language::Device
//       (see VkDeviceGroupSubmitInfoKHX).
// NOTE: Defaults the CommandPool::queueFamily to language::GRAPHICS. Your app
//       can change CommandPoolContainer::cpool.queueFamily before calling
//       CommandPoolContainer::cpool.ctorError().
struct CommandPoolContainer {
  CommandPoolContainer() = delete;  // You *must* call with a Device& argument.
  CommandPoolContainer(language::Device& dev) : cpool{dev}, pass{dev} {
    cpool.queueFamily = language::GRAPHICS;
  }
  virtual ~CommandPoolContainer();

  command::CommandPool cpool;
  command::RenderPass pass;
  // Your application can inspect prevSize in its resizeFramebufListeners.
  VkExtent2D prevSize;

  // onResized is called when cpool.dev.framebufs need to be resized.
  // * Register in resizeFramebufListeners to have CommandPoolContainer
  //   automatically handle per-framebuf initialization. (It is necessary to
  //   re-initialize each one any time there is a resize event.)
  //
  //   NOTE: You still must call CommandPoolContainer::onResized just before
  //   starting the main polling loop; this calls RenderPass::ctorError and
  //   builds the framebuffers and command buffers.
  //
  // * If CommandPoolContainer::onResized is useful but your app needs to
  //   customize the logic further, you probably want to override onResized,
  //   then call CommandPoolContainer::onResized first thing in your onResized.
  //
  //   NOTE: resetSwapChain *modifies* newSize. Your application *must not*
  //   assume newSize as passed in is the same after this onResized is done.
  //   Get the updated value like this:
  //     newSize = cpool.dev.swapChainInfo.imageExtent;
  WARN_UNUSED_RESULT virtual int onResized(VkExtent2D newSize,
                                           size_t poolQindex) {
    if (!pass.vk) {
      // Call RenderPass::ctorError the first time. It will set pass.vk.
      if (pass.ctorError()) {
        return 1;
      }
    }
    language::Device& dev = cpool.dev;
    prevSize = dev.swapChainInfo.imageExtent;
    dev.swapChainInfo.imageExtent = newSize;
    if (cpool.deviceWaitIdle() || dev.resetSwapChain(cpool, poolQindex)) {
      return 1;
    }

    for (size_t i = 0; i < dev.framebufs.size(); i++) {
      if (dev.framebufs.at(i).ctorError(dev, pass.vk)) {
        logE("CommandPoolContainer::onResized(): framebuf[%zu] failed\n", i);
        return 1;
      }
    }
    for (auto cb : resizeFramebufListeners) {
      for (size_t i = 0; i < dev.framebufs.size(); i++) {
        if (cb.first(cb.second, dev.framebufs.at(i), i, poolQindex)) {
          return 1;
        }
      }
    }
    return 0;
  }

  typedef int (*resizeFramebufCallback)(void* self,
                                        language::Framebuf& framebuf,
                                        size_t framebuf_i, size_t poolQindex);
  // resizeFramebufListeners get called for each framebuf that needs to be
  // rebuilt. Use this to rebuild the command buffers (since they are bound to
  // framebuf).
  std::vector<std::pair<resizeFramebufCallback, void*>> resizeFramebufListeners;

#if defined(_WIN32) && defined(max)
#undef max
#endif
  // acquireNextImage wraps vkAcquireNextImageKHR and handles the various cases
  // that can arise: Success returns 0 and sets next_image_i.
  //                 Failure returns 1 signalling to destroy the device.
  // Restartable errors like VK_ERROR_OUT_OF_DATE_KHR are also handled, but set
  // next_image_i = (uint32_t) -1, signalling that the app must immediately
  // jump to the top of its main loop.
  //
  // Pass in frameNumber for convenience; dev.setFrameNumber is called for you.
  //
  // Note: Use PresentSemaphore::present() to handle all the same edge cases as
  //       they can arise from the result of vkQueuePresentKHR.
  WARN_UNUSED_RESULT int acquireNextImage(
      uint32_t frameNumber, uint32_t* next_image_i,
      command::Semaphore& imageAvailableSemaphore,
      // A timeout of uint64_t::max means infinity.
      uint64_t timeout = std::numeric_limits<uint64_t>::max(),
      // An optional fence can signal the CPU when
      // imageAvailableSemaphore is signalled on the GPU.
      VkFence fence = VK_NULL_HANDLE,
      // poolQindex may be needed to call onResized.
      size_t poolQindex = memory::ASSUME_POOL_QINDEX);
};

// PresentSemaphore is a special Semaphore that adds the present() method.
class PresentSemaphore : public command::Semaphore {
 public:
  CommandPoolContainer& parent;
  VkQueue q;

 public:
  PresentSemaphore(CommandPoolContainer& parent)
      : Semaphore(parent.cpool.dev), parent(parent) {}
  PresentSemaphore(PresentSemaphore&&) = default;
  PresentSemaphore(const PresentSemaphore&) = delete;

  // Two-stage constructor: call ctorError() to build PresentSemaphore.
  WARN_UNUSED_RESULT int ctorError();

  // present submits the given swapChain image_i to Device dev's screen using
  // the correct language::PRESENT queue and synchronization.
  // * Success returns 0.
  // * Failure returns 1 signalling to destroy the device.
  // * Restartable errors like VK_ERROR_OUT_OF_DATE_KHR are also handled, but
  //   set image_i = (uint32_t) -1, signalling that the app must immediately
  //   jump to the top of its main loop.
  //
  // Note: CommandPoolContainer::acquireNextImage() handles all the same edge
  //       cases as they can arise from the result of vkAcquireNextImageKHR.
  WARN_UNUSED_RESULT int present(
      uint32_t* image_i,
      // poolQindex may be needed to call onResized.
      size_t poolQindex = memory::ASSUME_POOL_QINDEX);

  // waitIdle must be used to prevent validation layers from leaking memory,
  // and will slightly reduce the overall application's performance. Removing
  // waitIdle may or may not be worth the trouble.
  WARN_UNUSED_RESULT int waitIdle();

  language::SurfaceSupport queueFamily{language::PRESENT};
};

// SmartCommandBuffer builds on top of CommandBuffer with convenience method
// AutoSubmit()
typedef struct SmartCommandBuffer : public command::CommandBuffer {
  SmartCommandBuffer(command::CommandPool& cpool_, size_t poolQindex_)
      : CommandBuffer{cpool_}, poolQindex{poolQindex_} {}
  // Move constructor.
  SmartCommandBuffer(SmartCommandBuffer&& other)
      : CommandBuffer(std::move(other)),
        poolQindex(other.poolQindex),
        ctorErrorSuccess(other.ctorErrorSuccess),
        wantAutoSubmit(other.wantAutoSubmit) {}
  // The copy constructor is not allowed. The VkCommandBuffer cannot be copied.
  SmartCommandBuffer(const CommandBuffer& other) = delete;

  virtual ~SmartCommandBuffer();
  SmartCommandBuffer& operator=(SmartCommandBuffer&&) = delete;

  // ctorError sets up SmartCommandBuffer for one time use by borowing
  // a pre-allocated command buffer from cpool. This is useful for init
  // commands because the command buffer is managed by the cpool.
  //
  // CommandPool::updateBuffersAndPass can automatically set up a vector of
  // SmartCommandBuffer -- in that case do not call ctorError().
  WARN_UNUSED_RESULT int ctorError() {
    vk = cpool.borrowOneTimeBuffer();
    if (!vk) {
      logE("SmartCommandBuffer:%s failed\n", " borrowOneTimeBuffer");
      return 1;
    }
    if (beginOneTimeUse()) {
      logE("SmartCommandBuffer:%s failed\n", " beginOneTimeUse");
      return 1;
    }
    ctorErrorSuccess = true;
    return 0;
  }

  // autoSubmit() will set a flag so that ~SmartCommandBuffer() will
  // "auto-submit" the buffer by calling end(), submit(), and vkQueueWaitIdle.
  // Automatically submitting a buffer when it goes out of scope is useful for
  // init commands.
  WARN_UNUSED_RESULT int autoSubmit() {
    if (!ctorErrorSuccess) {
      logE("SmartCommandBuffer:%s failed\n",
           " ctorError not called, autoSubmit");
      return 1;
    }
    wantAutoSubmit = true;
    return 0;
  }

  const size_t poolQindex{0};

 protected:
  bool ctorErrorSuccess{false};
  bool wantAutoSubmit{false};
} SmartCommandBuffer;

// PipeBuilder is a builder for command::Pipeline.
// PipeBuilder immediately installs a new command::Pipeline in the
// command::RenderPass it gets in its constructor, so instantiating a
// PipeBuilder is an immediate commitment to completing the Pipeline before
// calling RenderPass:ctorError().
typedef struct PipeBuilder {
  PipeBuilder(language::Device& dev, command::RenderPass& pass)
      : dev(dev), pass(pass) {}
  PipeBuilder(PipeBuilder&&) = default;
  PipeBuilder(const PipeBuilder& other) = delete;

  language::Device& dev;
  command::RenderPass& pass;
  std::shared_ptr<command::Pipeline> pipe;

  // addPipelineOnce is automatically called by the other methods in PipeBuilder
  // to initialize PipeBuilder::pipe from PipeBuilder::pass.addPipeline().
  //
  // Though it would not hurt if you call it directly, it is not necessary.
  void addPipelineOnce() {
    if (!pipe) {
      pipe = pass.addPipeline(dev);
    }
  }

  // info() returns the PipelineCreateInfo as if this were a command::Pipeline.
  command::PipelineCreateInfo& info() {
    addPipelineOnce();
    return pipe->info;
  }

  // addDepthImage calls the same method in memory::Pipeline.
  int addDepthImage(const std::vector<VkFormat>& formatChoices) {
    addPipelineOnce();
    return pipe->addDepthImage(formatChoices, pass);
  }

  // alphaBlendWithPreviousPass() modifies this PipeBuilder to make it
  // automatically compatible with the dev.framebufs as configured by a
  // previous pipeline (possibly a distict RenderPass - it does not matter).
  //
  // prevPipeInfo.finalLayout is checked for compatibility.
  int alphaBlendWithPreviousPass(
      const command::PipelineCreateInfo& prevPipeInfo);

  // addVertexInput initializes a vertex *type* as an input to shaders. The
  // type variable is passed to addVertexInput at compile time to define the
  // vertex structure.
  //
  // You must define the structure of your vertex shader inputs,
  // and also define a static method getAttributes() returning
  // std::vector<VkVertexInputAttributeDescription> to tell pipeBuilder the
  // structure layout:
  //   // In your vertex shader
  //   layout(location = 0) in vec3 inPos;
  //
  //   // In your C++ code
  //   struct MyVertex {
  //     glm::vec3 pos;
  //     std::vector<VkVertexInputAttributeDescription> getAttributes() {
  //       return std::vector<VkVertexInputAttributeDescription>{ ... };
  //     }
  //   };
  //   ...
  //   // addVertexInput calls getAttributes.
  //   if (pipeBuilder.addVertexInput<MyVertex>()) { ... }
  template <typename T>
  WARN_UNUSED_RESULT int addVertexInput(uint32_t binding = 0) {
    return addVertexInputBySize(binding, sizeof(T), T::getAttributes());
  }

  // addVertexInputBySize is the non-template version of addVertexInput().
  WARN_UNUSED_RESULT int addVertexInputBySize(
      uint32_t binding, size_t nBytes,
      const std::vector<VkVertexInputAttributeDescription> typeAttributes) {
    vertexInputs.emplace_back();
    VkVertexInputBindingDescription& bindingDescription = vertexInputs.back();
    bindingDescription.binding = binding;
    bindingDescription.stride = nBytes;
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    command::PipelineCreateInfo& pinfo(info());
    pinfo.vertsci.vertexBindingDescriptionCount = vertexInputs.size();
    pinfo.vertsci.pVertexBindingDescriptions = vertexInputs.data();

    attributeInputs.insert(attributeInputs.end(), typeAttributes.begin(),
                           typeAttributes.end());
    pinfo.vertsci.vertexAttributeDescriptionCount = attributeInputs.size();
    pinfo.vertsci.pVertexAttributeDescriptions = attributeInputs.data();
    return 0;
  }

  std::vector<VkVertexInputBindingDescription> vertexInputs;
  std::vector<VkVertexInputAttributeDescription> attributeInputs;
} PipeBuilder;

#ifdef USE_SPIRV_CROSS_REFLECTION

// DescriptorLibrary is the DescriptorSet objects and DescriptorPool they are
// allocated from.
class DescriptorLibrary {
 public:
  DescriptorLibrary(language::Device& dev) : pool{dev} {}

  std::vector<memory::DescriptorSetLayout> layouts;

  // makeSet creates a new DescriptorSet from layouts[layoutI].
  // Use the layoutI the shader declared with "layout(set = layoutI)".
  //
  // Example usage:
  //   DescriptorLibrary library;
  //   // You MUST call makeDescriptorLibrary to init library.
  //   shaderLibrary.makeDescriptorLibrary(library);
  //   std::unique_ptr<memory::DescriptorSet> d = library.makeSet(0);
  //   if (!d) {
  //     handleErrors;
  //   }
  //   pipe.info.setLayouts.emplace_back(library.layouts.at(0).vk);
  std::unique_ptr<memory::DescriptorSet> makeSet(size_t layoutI);

 protected:
  friend class ShaderLibrary;
  memory::DescriptorPool pool;
};

struct ShaderLibraryInternal;

// ShaderLibrary uses //vendor/spirv_cross to determine the number of
// descriptors in each shader's descriptor set.
//
// Best practice with Vulkan is to have a single DescriptorSet which is used by
// all active shaders. Since not all shaders need all descriptors, it is
// expected there will be "unused variables" in some or all shaders if the
// shaders share the DescriptorSet in this manner.
//
// ShaderLibrary will print a warning if a Shader uses a different layout
// (requiring a different DescriptorSet) but will still work:
// "Shader does not match other Shader's layouts. Performance penalty."
class ShaderLibrary {
 public:
  ShaderLibrary(language::Device& dev) : dev(dev), _i(nullptr) {}
  virtual ~ShaderLibrary();

  // descriptorSetMaxCopies is a hint to ShaderLibrary to allocate room in the
  // descriptor pool for multiple copies of the types found by reflection.
  size_t descriptorSetMaxCopies{1};

  // load creates and calls loadSPV() on a Shader. If loadSPV() fails, it
  // returns an empty shared_ptr.
  std::shared_ptr<command::Shader> load(const uint32_t* spvBegin, size_t len);
  // load creates and calls loadSPV() on a Shader. If loadSPV() fails, it
  // returns an empty shared_ptr.
  std::shared_ptr<command::Shader> load(const void* spvBegin, size_t len) {
    return load(reinterpret_cast<const uint32_t*>(spvBegin), len);
  }
  // load creates and calls loadSPV() on a Shader. If loadSPV() fails, it
  // returns an empty shared_ptr.
  std::shared_ptr<command::Shader> load(const void* spvBegin,
                                        const void* spvEnd) {
    return load(spvBegin, reinterpret_cast<const char*>(spvEnd) -
                              reinterpret_cast<const char*>(spvBegin));
  }
  // load creates and calls loadSPV() on a Shader. If loadSPV() fails, it
  // returns an empty shared_ptr.
  std::shared_ptr<command::Shader> load(const std::vector<char>& spv) {
    return load(&*spv.begin(), &*spv.end());
  }
  // load creates and calls loadSPV() on a Shader. If loadSPV() fails, it
  // returns an empty shared_ptr.
  std::shared_ptr<command::Shader> load(const std::vector<uint32_t>& spv) {
    return load(&*spv.begin(), &*spv.end());
  }
  // load creates and calls loadSPV() on a Shader. If loadSPV() fails, it
  // returns an empty shared_ptr.
  std::shared_ptr<command::Shader> load(const char* filename);
  // load creates and calls loadSPV() on a Shader. If loadSPV() fails, it
  // returns an empty shared_ptr.
  std::shared_ptr<command::Shader> load(std::string filename) {
    return load(filename.c_str());
  }

  // stage puts a shader into a pipeline at the specified stageBits.
  WARN_UNUSED_RESULT int stage(command::RenderPass& renderPass,
                               PipeBuilder& pipe,
                               VkShaderStageFlagBits stageBits,
                               std::shared_ptr<command::Shader> shader,
                               std::string entryPointName = "main");

  // makeDescriptorLibrary inits a DescriptorLibrary to the ShaderLibrary and
  // inits its layouts from the layouts in the shaders in ShaderLibrary.
  //
  // Note: you MUST call load() at least once before calling
  // makeDescriptorLibrary.
  int makeDescriptorLibrary(DescriptorLibrary& descriptorLibrary,
                            const std::multiset<VkDescriptorType>& wantTypes =
                                std::multiset<VkDescriptorType>());

 protected:
  language::Device& dev;
  ShaderLibraryInternal* _i;
};

#endif /*USE_SPIRV_CROSS_REFLECTION*/

}  // namespace science
