/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 *
 * This header defines structures which gather related sub-structs as a single
 * struct which can add a VolcanoReflectionMap. All of this makes finding the
 * Vulkan field a little easier.
 *
 * E.g. VkPhysicalDeviceFeatures2 has a sub-struct VkPhysicalDeviceFeatures.
 * Accessing it explicitly looks like this:
 *   dev.enabledFeatures.features.samplerAnisotropy
 * It can be re-written as (using reflection to simplify the expression):
 *   dev.enabledFeatures.get("samplerAnisotropy", value)
 *
 * This file enumerates the sub-structures which are gathered into one.
 * "reflectionmap.h" is the C++ reflection implementation.
 *
 * "structs.h" #includes "reflectionmap.h" which #includes "VkPtr.h". Typically
 * your app only need #include <src/language/language.h> which will pull in the
 * correct headers.
 */

#include "reflectionmap.h"

#pragma once

namespace language {

#if defined(COMPILER_GCC) || defined(__clang__)
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#elif defined(COMPILER_MSVC)
#define WARN_UNUSED_RESULT _Check_return_
#else
#define WARN_UNUSED_RESULT
#endif

// Forward declaration of Device defined in language.h.
struct Device;

// DeviceFeatures gathers all the structures that are supported by Volcano
// for VkPhysicalDeviceFeatures2.
//
// See Device::availableFeatures and Device::enabledFeatures. availableFeatures
// is filled by Instance::ctorError(). VK_EXT_* structs are queried if the
// extension is available, but your app must add the extension name to
// Device::requiredExtensions before Instance::open for it to be enabled in
// Device::enabledFeatures.
struct DeviceFeatures : VkPhysicalDeviceFeatures2 {
  DeviceFeatures();

  // reset clears all structures to their default (zero) values.
  void reset();

  // getFeatures is a wrapper around vkGetPhysicalDeviceFeatures (and
  // vkGetPhysicalDeviceFeatures2).
  // Your app does not need to call get(): this is called from
  // Instance::ctorError while setting up the Device.
  WARN_UNUSED_RESULT int getFeatures(Device& dev);

  // get returns the named field regardless of which sub-structure it is in.
  // Reading a field from an extension not loaded gives meaningless data.
  int get(const char* fieldName, VkBool32& result) {
    return reflect.get(fieldName, result);
  }

  // set sets the named field regardless of which sub-structure it is in.
  // Writing a field from an extension not loaded is meaningless.
  WARN_UNUSED_RESULT int set(const char* fieldName, VkBool32 value) {
    return reflect.set(fieldName, value);
  }

  VkPhysicalDeviceVariablePointerFeatures variablePointer;
  VkPhysicalDeviceMultiviewFeatures multiview;
  VkPhysicalDeviceProtectedMemoryFeatures drm;
  VkPhysicalDeviceShaderDrawParameterFeatures shaderDraw;
  VkPhysicalDevice16BitStorageFeatures storage16Bit;

  // Used if VK_EXT_blend_operation_advanced:
  VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT blendOpAdvanced;

  // Used if VK_EXT_descriptor_indexing:
  VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexing;

  VolcanoReflectionMap reflect;
};

// PhysicalDeviceProperties gathers all the structures that are supported by
// Volcano for VkPhysicalDeviceProperties2.
struct PhysicalDeviceProperties : VkPhysicalDeviceProperties2 {
  PhysicalDeviceProperties();

  // reset clears all structures to their default (zero) values.
  void reset();

  // getProperties is a wrapper around vkGetPhysicalDeviceProperties (and
  // vkGetPhysicalDeviceProperties2).
  // Your app does not need to call get(): this is called from
  // Instance::ctorError while setting up the Device.
  WARN_UNUSED_RESULT int getProperties(Device& dev);

  VkPhysicalDeviceIDProperties id;
  VkPhysicalDeviceMaintenance3Properties maint3;
  VkPhysicalDeviceMultiviewProperties multiview;
  VkPhysicalDevicePointClippingProperties pointClipping;
  VkPhysicalDeviceProtectedMemoryProperties drm;
  VkPhysicalDeviceSubgroupProperties subgroup;

  // Used if VK_EXT_blend_operation_advanced:
  VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT blendOpAdvanced;

  // Used if VK_EXT_conservative_rasterization:
  VkPhysicalDeviceConservativeRasterizationPropertiesEXT conservativeRasterize;

  // Used if VK_EXT_descriptor_indexing:
  VkPhysicalDeviceDescriptorIndexingPropertiesEXT descriptorIndexing;

  // Used if VK_EXT_discard_rectangles:
  VkPhysicalDeviceDiscardRectanglePropertiesEXT discardRectangle;

  // Used if VK_EXT_external_memory_host:
  VkPhysicalDeviceExternalMemoryHostPropertiesEXT externalMemoryHost;

  // Used if VK_EXT_sample_locations:
  VkPhysicalDeviceSampleLocationsPropertiesEXT sampleLocations;

  // Used if VK_EXT_sampler_filter_minmax:
  VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT samplerFilterMinmax;

  // Used if VK_EXT_vertex_attribute_divisor:
  VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT vertexAttributeDivisor;

  // Used if VK_KHR_push_descriptor:
  VkPhysicalDevicePushDescriptorPropertiesKHR pushDescriptor;

  // Used if VK_NVX_multiview_per_view_attributes:
  VkPhysicalDeviceMultiviewPerViewAttributesPropertiesNVX
      nvMultiviewPerViewAttr;

  // Used if VK_AMD_shader_core_properties:
  VkPhysicalDeviceShaderCorePropertiesAMD amdShaderCore;

  VolcanoReflectionMap reflect;
};

// FormatProperties gathers all the structures that are supported by Volcano
// for VkFormatProperties2.
//
// Note: VkFormatProperties2 is for the VkFormat format member in this struct,
//       and any other VkFormat will have a different VkFormatProperties2.
struct FormatProperties : VkFormatProperties2 {
  FormatProperties(VkFormat format_) : format(format_) { reset(); }
  const VkFormat format;

  // reset clears all structures to their default (zero) values.
  void reset();

  // getProperties is a wrapper around vkGetFormatProperties (and
  // vkGetFormatProperties2).
  // Your app does not need to call get(): this is called from
  // Instance::ctorError while setting up the Device.
  WARN_UNUSED_RESULT int getProperties(Device& dev);

  // There are no sub-structures defined for VkFormatProperties2 at this time.
};

// DeviceMemoryProperties gathers all the structures that are supported by
// Volcano for VkPhysicalDeviceMemoryProperties2.
struct DeviceMemoryProperties : VkPhysicalDeviceMemoryProperties2 {
  DeviceMemoryProperties() { reset(); }

  // reset clears all structures to their default (zero) values.
  void reset();

  // getProperties is a wrapper around vkGetPhysicalDeviceMemoryProperties (and
  // vkGetPhysicalDeviceMemoryProperties2).
  // Your app does not need to call get(): this is called from
  // Instance::ctorError while setting up the Device.
  WARN_UNUSED_RESULT int getProperties(Device& dev);

  // There are no sub-structures defined for VkPhysicalDeviceMemoryProperties2
  // at this time.
};

// TODO: SparseImageFormatProperties for VkSparseImageFormatProperties2.
// retrieved via vkGetPhysicalDeviceSparseImageFormatProperties2
// which uses VkPhysicalDeviceSparseImageFormatInfo2
// (vkGetPhysicalDeviceSparseImageFormatProperties passes all as args)

// ImageFormatProperties gathers all the structures that are supported by
// Volcano for VkImageFormatProperties2.
struct ImageFormatProperties : VkImageFormatProperties2 {
  ImageFormatProperties();

  // reset clears all structures to their default (zero) values.
  void reset();

  // getProperties is a wrapper around vkGetPhysicalDeviceImageFormatProperties
  // (and vkGetPhysicalDeviceImageFormatProperties2).
  //
  // This form takes as input the fields for VkPhysicalDeviceImageFormatInfo2.
  // optionalExternalMemoryFlags should only be nonzero if requesting support
  // for external memory.
  WARN_UNUSED_RESULT VkResult getProperties(
      Device& dev, VkFormat format, VkImageType type, VkImageTiling tiling,
      VkImageUsageFlags usage, VkImageCreateFlags flags,
      VkExternalMemoryHandleTypeFlagBits optionalExternalMemoryFlags =
          (VkExternalMemoryHandleTypeFlagBits)0);

  // getProperties is a wrapper around vkGetPhysicalDeviceImageFormatProperties
  // (and vkGetPhysicalDeviceImageFormatProperties2).
  //
  // This is a convenience method that accepts VkImageCreateInfo.
  // Example:
  //   memory::Image img;
  //   // Call getProperties before img.ctorError to check for any issues.
  //   ImageFormatProperties props;
  //   if (props.getProperties(dev, img.info)) { ... }
  WARN_UNUSED_RESULT VkResult
  getProperties(Device& dev, const VkImageCreateInfo& ici,
                VkExternalMemoryHandleTypeFlagBits optionalExternalMemoryFlags =
                    (VkExternalMemoryHandleTypeFlagBits)0) {
    return getProperties(dev, ici.format, ici.imageType, ici.tiling, ici.usage,
                         ici.flags, optionalExternalMemoryFlags);
  }

  VkSamplerYcbcrConversionImageFormatProperties ycbcrConversion;

  // Used if getProperties is called with optionalExternalMemoryFlags nonzero.
  // i.e. nonzero requests support for external memory.
  VkExternalImageFormatProperties externalImage;

#if defined(VK_HEADER_VERSION) && VK_HEADER_VERSION > 75
  // Used if VK_ANDROID_external_memory_android_hardware_buffer:
  VkAndroidHardwareBufferUsageANDROID androidHardware;
#endif

  // Used if VK_AMD_texture_gather_bias_lod:
  VkTextureLODGatherFormatPropertiesAMD amdLODGather;
};

// SurfaceSupport encodes the result of vkGetPhysicalDeviceSurfaceSupportKHR().
// As an exception, the GRAPHICS value in QueueFamilyProperties requests a
// VkQueue with queueFlags & VK_QUEUE_GRAPHICS_BIT in Instance::requestQfams()
// and Device::getQfamI().
//
// TODO: Add COMPUTE.
// GRAPHICS and COMPUTE support are not tied to a surface, but volcano makes the
// simplifying assumption that all these bits can be lumped together here.
enum SurfaceSupport {
  UNDEFINED = 0,
  NONE = 1,
  PRESENT = 2,

  GRAPHICS = 0x1000,  // Special case. Not used in QueueFamilyProperties.
};

// QueueFamilyProperties gathers all the structures that are supported by
// Volcano for VkQueueFamilyProperties2. SurfaceSupport is also per-QueueFamily
// as a simplifying assumption.
//
// There is no getProperties for this class because the Vulkan API does not
// support querying a single VkQueueFamilyProperties2, only an array. See
// Instance::initSupportedQueues() which calls getQueueFamilies()
struct QueueFamilyProperties : VkQueueFamilyProperties2 {
  QueueFamilyProperties() { reset(); }
  QueueFamilyProperties(QueueFamilyProperties&&) = default;
  QueueFamilyProperties(const QueueFamilyProperties&) = delete;

  // surfaceSupport is what vkGetPhysicalDeviceSurfaceSupportKHR reported for
  // this QueueFamily.
  SurfaceSupport surfaceSupport() const { return surfaceSupport_; }

  // setSurfaceSupport sets surfaceSupport_.
  void setSurfaceSupport(SurfaceSupport s) { surfaceSupport_ = s; }

  inline bool isGraphics() const {
    return queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
  }

  // prios and queues store what VkQueues were actually created.
  // Populated only after open().
  std::vector<float> prios;

  // queues and prios store what VkQueues were actually created.
  // Populated only after open().
  std::vector<VkQueue> queues;

  // There are no sub-structures defined for VkQueueFamilyProperties2
  // at this time.

 protected:
  // reset clears VkQueueFamilyProperties2 and any sub-structures. This is
  // protected and hidden because there is no getProperties and thus no way
  // for your app to use reset().
  void reset();

  SurfaceSupport surfaceSupport_;
};

}  // namespace language
