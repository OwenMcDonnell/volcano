/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 *
 * src/command is the 3rd-level bindings for the Vulkan graphics library.
 * src/command is part of github.com/ndsol/volcano.
 * This library is called "command" as a homage to Star Trek First Contact.
 * Like the Vulcan High Command, this library sends out the commands.
 *
 * This library has 3 sub-categories, all interdependent:
 *
 * 1. The RenderPass uses these classes:
 *    * RenderPass
 *    * Pipeline
 *    * PipelineCreateInfo
 *    * PipelineStage
 *    * Shader
 *
 * 2. The Semaphore (with PresentSemaphore), Fence, and Event classes.
 *
 * 3. The CommandPool and CommandBuilder classes.
 */

#include <src/language/VkInit.h>
#include <src/language/VkPtr.h>
#include <src/language/language.h>
#include <vulkan/vulkan.h>
// vk_enum_string_helper.h is not in the default vulkan installation, but is
// generated by the gn/vendor/vulkansamples/BUILD.gn file in this repo.
#include <vulkan/vk_enum_string_helper.h>
#include <memory>
#include <set>
#include <string>
// "command_builder.h" is #included at the end of the file (see below).

#pragma once

namespace command {

// Shader represents the compiled SPIR-V code generated by glslangVerifier.
// To add your Shader objects to a Pipeline:
// 1. Allocate the Shader:
//    auto shader = std::shared_ptr<command::Shader>(new command::Shader(dev));
// 2. Load the SPIR-V code from a file (or somewhere else):
//    if (shader->loadSPV("filename.spv")) { ... }
// 3. Call PipelineCreateInfo::addShader(shader, ...), which will also add the
//    shader to the RenderPass.
//
// Or a simpler approach is to use science::ShaderLibrary.
typedef struct Shader {
  Shader(language::Device& dev)
      : vkdev{dev.dev}, vk{dev.dev, vkDestroyShaderModule} {
    vk.allocator = dev.dev.allocator;
  }
  Shader(Shader&&) = default;
  Shader(const Shader& other) = delete;

  // loadSPV loads the SPIR-V bytecode in spvBegin into the Shader.
  WARN_UNUSED_RESULT int loadSPV(const uint32_t* spvBegin, size_t len);
  // loadSPV is a convenience method for a buffer of a different type.
  WARN_UNUSED_RESULT int loadSPV(const void* spvBegin, size_t len) {
    return loadSPV(reinterpret_cast<const uint32_t*>(spvBegin), len);
  }
  // loadSPV is a convenience method for a sub-buffer within a larger buffer.
  WARN_UNUSED_RESULT int loadSPV(const void* spvBegin, const void* spvEnd) {
    return loadSPV(spvBegin, reinterpret_cast<const char*>(spvEnd) -
                                 reinterpret_cast<const char*>(spvBegin));
  }
  // loadSPV is a convenience method for loading from a std::vector.
  WARN_UNUSED_RESULT int loadSPV(const std::vector<char>& spv) {
    return loadSPV(&*spv.begin(), &*spv.end());
  }
  // loadSPV is a convenience method for loading from a std::vector.
  WARN_UNUSED_RESULT int loadSPV(const std::vector<uint32_t>& spv) {
    return loadSPV(&*spv.begin(), &*spv.end());
  }
  // loadSPV is a convenience method for loading from a file.
  WARN_UNUSED_RESULT int loadSPV(const char* filename);
  // loadSPV is a convenience method for loading from a file.
  WARN_UNUSED_RESULT int loadSPV(std::string filename) {
    return loadSPV(filename.c_str());
  }

  VkDevice vkdev;
  VkPtr<VkShaderModule> vk;
} Shader;

// PipelineAttachment constructs a VkAttachmentDescription. When it is added to
// the VkRenderPassCreateInfo in RenderPass::ctorError(), it is given an index
// -- written to VkAttachmentReference refvk here. The refvk is then added to
// the PipelineCreateInfo::subpassDesc.
typedef struct PipelineAttachment {
  // Construct a PipelineAttachment which corresponds to a
  // Framebuffer attachment with the given VkFormat and VkImageLayout.
  PipelineAttachment(VkFormat format, VkImageLayout refLayout);

  VkAttachmentReference refvk;
  VkAttachmentDescription vk;
} PipelineAttachment;

// PipelineStage is the entrypoint to run a Shader as one of the programmable
// pipeline stages. (See the description of Pipeline, below.)
//
// PipelineStage::entryPoint sets what function is "main()" in the Shader.
// A library of useful code can be built as a single large shader with several
// entryPointNames -- glslangVerifier can build many source files as one unit.
//
// Or, keep it simple: set entryPointName = "main" on all your shaders to make
// them feel like "C".
//
// TODO: Find out if Vulkan errors out if two PipelineStages are added for the
// same stage. For example, two VK_SHADER_STAGE_VERTEX_BIT.
typedef struct PipelineStage {
  PipelineStage() { VkOverwrite(info); }
  PipelineStage(PipelineStage&&) = default;
  PipelineStage(const PipelineStage&) = default;

  std::shared_ptr<Shader> shader;
  std::string entryPointName;

  // You must initialize info.flag, but do not initialize
  // info.module and info.pName. They will be written by Pipeline::init().
  VkPipelineShaderStageCreateInfo info;
} PipelineStage;

// Forward declaration of RenderPass for Pipeline and PipelineCreateInfo.
struct RenderPass;

// Vulkan defines a render pipeline in terms of the following stages:
// 1. Input assembly: fixed function, reads input data.
// 2. Vertex shader: programmable, operates on input vertices, uniforms, etc.
// 3. Tesselation shader: programmable, reads the vertex shader's output and
//    produces a different number of vertices.
// 4. Geometry shader: programmable. Most GPUs cannot do geometry shading with
//    reasonable performance (the notable exception is Intel).
// 5. Rasterizer: fixed function, draws triangles / lines / points.
// 6. Fragment shader: programmable, operates on each "fragment" (each pixel).
// 7. Color blend: fixed function, writes fragments to the frame buffer.
//
// Use Pipeline in the following order:
// 1. Instantiate a Pipeline by calling RenderPass::addPipeline().
// 2. Customize the Pipeline::info, including calling addShader().
// 3. Call RenderPass:ctorError() to create the vulkan objects.
//
// PipelineCreateInfo is the Pipeline::info. A VkPipelineLayoutCreateInfo is
// built from this class in Pipeline::init, called from RenderPass::ctorError.
typedef struct PipelineCreateInfo {
  PipelineCreateInfo(language::Device& dev);
  PipelineCreateInfo(PipelineCreateInfo&&) = default;
  PipelineCreateInfo(const PipelineCreateInfo& other) = default;

  std::vector<PipelineStage> stages;

  // addShader adds a PipelineStage to stages, holding a reference to it
  // in renderPass (for de-duplication).
  //
  // stageBits define which stage(s) the shader is valid for (vertex shader,
  // fragment shader, etc.)
  //
  // entryPointName defines what "main" function to run.
  WARN_UNUSED_RESULT int addShader(std::shared_ptr<Shader> shader,
                                   RenderPass& renderPass,
                                   VkShaderStageFlagBits stageBits,
                                   std::string entryPointName = "main");

  // Helper function to create a blend state of "just write these pixels."
  static VkPipelineColorBlendAttachmentState withDisabledAlpha();

  // Helper function to create a blend state "do normal RGBA alpha blending."
  static VkPipelineColorBlendAttachmentState withEnabledAlpha();

  // Optionally modify these structures before calling RenderPass::ctorError().
  VkPipelineVertexInputStateCreateInfo vertsci;
  VkPipelineInputAssemblyStateCreateInfo asci;

  // Optionally modify these structures before calling RenderPass::ctorError().
  // viewports and scissors will be written to viewsci by Pipeline::init().
  // Optionally update the viewports and scissors in-place and use them in
  // CommandBuilder::setViewport() and CommandBuilder::setScissor()
  std::vector<VkViewport> viewports;
  std::vector<VkRect2D> scissors;
  VkPipelineViewportStateCreateInfo viewsci;

  // Optionally modify these structures before calling RenderPass::ctorError().
  VkPipelineRasterizationStateCreateInfo rastersci;
  VkPipelineMultisampleStateCreateInfo multisci;
  VkPipelineDepthStencilStateCreateInfo depthsci;

  // Use vkCreateDescriptorSetLayout to create layouts, which then
  // auto-generates VkPipelineLayoutCreateInfo.
  // TODO: Add pushConstants.
  std::vector<VkDescriptorSetLayout> setLayouts;

  std::vector<VkDynamicState> dynamicStates;

  // Optionally modify these structures before calling RenderPass::ctorError().
  // perFramebufColorBlend will be written to cbsci by Pipeline::init().
  std::vector<VkPipelineColorBlendAttachmentState> perFramebufColorBlend;
  VkPipelineColorBlendStateCreateInfo cbsci;

  // Optionally modify these structures before calling RenderPass::ctorError().
  // The PipelineAttachment::VkAttachmentReference::attachment index is set by
  // RenderPass::ctorError(), then written to PipelineCreateInfo::subpassDesc.
  std::vector<PipelineAttachment> attach;

  VkSubpassDescription subpassDesc;
} PipelineCreateInfo;

// Pipeline represents a VkPipeline and VkPipelineLayout pair.
typedef struct Pipeline {
  Pipeline(language::Device& dev);
  Pipeline(Pipeline&&) = default;
  Pipeline(const Pipeline& other) = delete;
  virtual ~Pipeline();

  PipelineCreateInfo info;

  VkPtr<VkPipelineLayout> pipelineLayout;
  VkPtr<VkPipeline> vk;

 protected:
  friend struct RenderPass;
  // Workaround bug in NVidia driver that driver does not keep a copy of the
  // VkPipelineShaderStageCreateInfo pName contents, just the pointer.
  std::vector<std::string> stageName;

  // init() sets up shaders (and references to them in info.stages), and
  // creates a VkPipeline. The parent renderPass and this Pipeline's
  // index in it are passed in as parameters. This method should be
  // called by RenderPass::ctorError().
  WARN_UNUSED_RESULT virtual int init(language::Device& dev,
                                      RenderPass& renderPass, size_t subpass_i);
} Pipeline;

// RenderPass is the main object to set up and control presenting pixels to the
// screen.
//
// To create a RenderPass:
// 1. Instantiate the RenderPass: RenderPass rp(dev);
// 2. Add each Pipeline:          Pipeline &p = rp.addPipeline(dev);
//    and customize the Pipeline to suit your application.
// 3. Instantiate Shader objects and load SPV binary code into them
// 4. Init the renderPass.
//        if (renderPass.init(dev) { ... handle errors ... }
//
// Resources for understanding RenderPasses:
// https://gpuopen.com/vulkan-renderpasses/
typedef struct RenderPass {
  RenderPass(language::Device& dev) : vk{dev.dev, vkDestroyRenderPass} {
    VkOverwrite(rpci);
    VkOverwrite(passBeginInfo);
    passBeginClearColors.emplace_back();
    setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  }
  RenderPass(RenderPass&&) = default;
  RenderPass(const RenderPass&) = delete;
  virtual ~RenderPass() = default;

  std::set<std::shared_ptr<Shader>> shaders;
  std::vector<Pipeline> pipelines;

  // addPipeline is a convenience method for adding a pipeline.
  Pipeline& addPipeline(language::Device& dev) {
    pipelines.emplace_back(dev);
    return pipelines.back();
  }

  VkRenderPassCreateInfo rpci;

  // Override this function to customize the subpass dependencies.
  // The default just executes subpasses serially (in order).
  WARN_UNUSED_RESULT virtual int getSubpassDeps(
      size_t subpass_i, std::vector<VkSubpassDependency>& subpassdeps);

  // ctorError() initializes each pipeline with their PipelineCreateInfo info.
  WARN_UNUSED_RESULT int ctorError(language::Device& dev);

  VkPtr<VkRenderPass> vk;

  // passBeginInfo is populated by ctorError(). Customize it as needed.
  // Note that YOU MUST update passBeginInfo.frameBuffer each frame -- it is not
  // something ctorError() can set for you. passBeginInfo.renderArea.extent
  // must also be updated any time the window is resized, but
  // CommandBuilder::beginRenderPass() just overwrites renderArea.extent for
  // you.
  VkRenderPassBeginInfo passBeginInfo;

  // passBeginClearColors is referenced in passBeginInfo.
  std::vector<VkClearValue> passBeginClearColors;

  // setClearColor is a helper method to quickly update passBeginClearColors.
  // passBeginClearColors is used by ctorError() to populate passBeginInfo.
  // Calling setClearColor() after ctorError() will have no effect.
  void setClearColor(float r, float g, float b, float a = 1.0f) {
    passBeginClearColors.at(0).color = {{r, g, b, a}};
  }
} RenderPass;

// Semaphore represents a GPU-only synchronization operation vs. Fence, below.
// Semaphores can be waited on in any queue vs. Events which must be waited on
// within a single queue.
typedef struct Semaphore {
  Semaphore(language::Device& dev) : vk{dev.dev, vkDestroySemaphore} {
    vk.allocator = dev.dev.allocator;
  }
  // Two-stage constructor: check the return code of ctorError().
  WARN_UNUSED_RESULT int ctorError(language::Device& dev);

  VkPtr<VkSemaphore> vk;
} Semaphore;

// PresentSemaphore is a special Semaphore that adds the present() method.
class PresentSemaphore : public Semaphore {
 public:
  language::Device& dev;
  VkQueue q;

 public:
  PresentSemaphore(language::Device& dev) : Semaphore(dev), dev(dev) {}
  PresentSemaphore(PresentSemaphore&&) = default;
  PresentSemaphore(const PresentSemaphore&) = delete;

  // Two-stage constructor: check the return code of ctorError().
  WARN_UNUSED_RESULT int ctorError();

  // present() submits the given swapChain image_i to Device dev's screen
  // using the correct language::PRESENT queue and synchronization.
  WARN_UNUSED_RESULT int present(uint32_t image_i);
};

// Fence represents a GPU-to-CPU synchronization. Fences are the only sync
// primitive which the CPU can wait on.
typedef struct Fence {
  Fence(language::Device& dev) : vk{dev.dev, vkDestroyFence} {
    vk.allocator = dev.dev.allocator;
  }

  // Two-stage constructor: check the return code of ctorError().
  WARN_UNUSED_RESULT int ctorError(language::Device& dev);

  // reset resets the state of the fence to unsignaled.
  WARN_UNUSED_RESULT int reset(language::Device& dev);

  // wait waits for the state of the fence to become signaled by the Device.
  // The result MUST be checked for multiple possible success states.
  WARN_UNUSED_RESULT VkResult wait(language::Device& dev,
                                   uint64_t timeoutNanos);

  // getStatus returns the status of the fence using vkGetFenceStatus.
  // The result MUST be checked for multiple possible success states.
  WARN_UNUSED_RESULT VkResult getStatus(language::Device& dev);

  VkPtr<VkFence> vk;
} Fence;

// Event represents a GPU-only synchronization operation, and must be waited on
// and set (signalled) within a single queue. Events can also be set (signalled)
// from the CPU.
typedef struct Event {
  Event(language::Device& dev) : vk{dev.dev, vkDestroyEvent} {
    vk.allocator = dev.dev.allocator;
  }
  // Two-stage constructor: check the return code of ctorError().
  WARN_UNUSED_RESULT int ctorError(language::Device& dev);

  VkPtr<VkEvent> vk;
} Event;

// CommandPool holds a reference to the VkCommandPool from which commands are
// allocated. Create a CommandPool instance in each thread that submits
// commands to qfam_i.
class CommandPool {
 protected:
  language::QueueFamily* qf_ = nullptr;

 public:
  CommandPool(language::Device& dev, language::SurfaceSupport queueFamily)
      : dev(dev), queueFamily(queueFamily), vk{dev.dev, vkDestroyCommandPool} {
    vk.allocator = dev.dev.allocator;
  }
  CommandPool(CommandPool&&) = default;
  CommandPool(const CommandPool&) = delete;

  // Two-stage constructor: check the return code of ctorError().
  WARN_UNUSED_RESULT int ctorError(
      VkCommandPoolCreateFlags flags =
          VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  VkQueue q(size_t i) { return qf_->queues.at(i); }

  // free releases any VkCommandBuffer in buf. Command Buffers are automatically
  // freed when the CommandPool is destroyed, so free() is really only needed
  // when dynamically replacing an existing set of CommandBuffers.
  void free(std::vector<VkCommandBuffer>& buf) {
    if (!buf.size()) return;
    vkFreeCommandBuffers(dev.dev, vk, buf.size(), buf.data());
  }

  // alloc calls vkAllocateCommandBuffers to populate buf with empty buffers.
  // Specify the VkCommandBufferLevel for a secondary command buffer.
  WARN_UNUSED_RESULT int alloc(
      std::vector<VkCommandBuffer>& buf,
      VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  // reset deallocates all command buffers in the pool (very quickly).
  WARN_UNUSED_RESULT int reset(
      VkCommandPoolResetFlagBits flags =
          VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT) {
    VkResult v;
    if ((v = vkResetCommandPool(dev.dev, vk, flags)) != VK_SUCCESS) {
      fprintf(stderr, "%s failed: %d (%s)\n", "vkResetCommandPool", v,
              string_VkResult(v));
      return 1;
    }
    return 0;
  }

  // deviceWaitIdle is a helper method to wait for the device to complete all
  // outstanding commands and return to the idle state. Use of this function is
  // suboptimal, since it bypasses all of the regular Vulkan synchronization
  // primitives.
  //
  // WARNING: A known bug may lead to an out-of-memory crash when using this
  // function and the standard validation layers.
  // https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/issues/1628
  WARN_UNUSED_RESULT int deviceWaitIdle() {
    VkResult v = vkDeviceWaitIdle(dev.dev);
    if (v != VK_SUCCESS) {
      fprintf(stderr, "%s failed: %d (%s)\n", "vkDeviceWaitIdle", v,
              string_VkResult(v));
      return 1;
    }
    return 0;
  }

  language::Device& dev;
  const language::SurfaceSupport queueFamily;
  VkPtr<VkCommandPool> vk;
};

}  // namespace command

#include "command_builder.h"
