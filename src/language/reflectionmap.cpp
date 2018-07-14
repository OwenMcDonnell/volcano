/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */

#include "VkInit.h"
#include "language.h"

namespace language {

reflect::Pointer* VolcanoReflectionMap::getField(const char* fieldName) {
  auto i = find(fieldName);
  return (i == end()) ? nullptr : &i->second;
}

// get specialized because VkBool32 is a typedef of uint32_t
template <>
int VolcanoReflectionMap::get(const char* fieldName, VkBool32& value) {
  auto pointer = getField(fieldName);
  if (pointer) {
    return pointer->getVkBool32(fieldName, value);
  }
  logE("%s(%s): field not found\n", "get", fieldName);
  return 1;
}

// set specialized because VkBool32 is a typedef of uint32_t
template <>
int VolcanoReflectionMap::set(const char* fieldName, VkBool32 value) {
  auto pointer = getField(fieldName);
  if (pointer) {
    return pointer->setVkBool32(fieldName, value);
  }
  logE("%s(%s): field not found\n", "set", fieldName);
  return 1;
}

// addField specialized because VkBool32 is a typedef of uint32_t
template <>
int VolcanoReflectionMap::addField(const char* fieldName, VkBool32* field) {
  auto pointer = getField(fieldName);
  if (pointer) {
    logW("addField(%s): already exists, type %s\n", fieldName,
         pointer->desc.toString().c_str());
    return 1;
  }
  pointer = &operator[](fieldName);
  pointer->addFieldVkBool32(field);
  return 0;
}

// addArrayField specialized because VkBool32 is a typedef of uint32_t
template <>
int VolcanoReflectionMap::addArrayField(const char* arrayName, VkBool32* field,
                                        size_t len) {
  auto pointer = getField(arrayName);
  if (pointer) {
    logW("addArrayField(%s): already exists, type %s\n", arrayName,
         pointer->desc.toString().c_str());
    return 1;
  }
  pointer = &operator[](arrayName);
  pointer->addArrayFieldVkBool32(field, len);
  return 0;
}

#if SIZE_MAX != UINT32_MAX
// get specialized because size_t is a typedef of unsigned long
// (skip this specialization if size_t is the same size as uint32_t)
template <>
int VolcanoReflectionMap::get(const char* fieldName, size_t& value) {
  auto pointer = getField(fieldName);
  if (pointer) {
    return pointer->getsize_t(fieldName, value);
  }
  logE("%s(%s): field not found\n", "get", fieldName);
  return 1;
}

// set specialized because size_t is a typedef of unsigned long
template <>
int VolcanoReflectionMap::set(const char* fieldName, size_t value) {
  auto pointer = getField(fieldName);
  if (pointer) {
    return pointer->setsize_t(fieldName, value);
  }
  logE("%s(%s): field not found\n", "set", fieldName);
  return 1;
}

// addField specialized because size_t is a typedef of unsigned long
template <>
int VolcanoReflectionMap::addField(const char* fieldName, size_t* field) {
  auto pointer = getField(fieldName);
  if (pointer) {
    logW("addField(%s): already exists, type %s\n", fieldName,
         pointer->desc.toString().c_str());
    return 1;
  }
  pointer = &operator[](fieldName);
  pointer->addFieldsize_t(field);
  return 0;
}

// addArrayField specialized because size_t is a typedef of unsigned long
template <>
int VolcanoReflectionMap::addArrayField(const char* arrayName, size_t* field,
                                        size_t len) {
  auto pointer = getField(arrayName);
  if (pointer) {
    logW("addArrayField(%s): already exists, type %s\n", arrayName,
         pointer->desc.toString().c_str());
    return 1;
  }
  pointer = &operator[](arrayName);
  pointer->addArrayFieldsize_t(field, len);
  return 0;
}
#endif /*SIZE_MAX != UINT32_MAX*/

DeviceFeatures::DeviceFeatures() {
#define STRINGIFY2(n) #n
#define STRINGIFY(n) STRINGIFY2(n)
#define ADD_FIELD(substruct, field) \
  if (reflect.addField(STRINGIFY(field), &(substruct.field))) return
  ADD_FIELD(features, robustBufferAccess);
  ADD_FIELD(features, fullDrawIndexUint32);
  ADD_FIELD(features, imageCubeArray);
  ADD_FIELD(features, independentBlend);
  ADD_FIELD(features, geometryShader);
  ADD_FIELD(features, tessellationShader);
  ADD_FIELD(features, sampleRateShading);
  ADD_FIELD(features, dualSrcBlend);
  ADD_FIELD(features, logicOp);
  ADD_FIELD(features, multiDrawIndirect);
  ADD_FIELD(features, drawIndirectFirstInstance);
  ADD_FIELD(features, depthClamp);
  ADD_FIELD(features, depthBiasClamp);
  ADD_FIELD(features, fillModeNonSolid);
  ADD_FIELD(features, depthBounds);
  ADD_FIELD(features, wideLines);
  ADD_FIELD(features, largePoints);
  ADD_FIELD(features, alphaToOne);
  ADD_FIELD(features, multiViewport);
  ADD_FIELD(features, samplerAnisotropy);
  ADD_FIELD(features, textureCompressionETC2);
  ADD_FIELD(features, textureCompressionASTC_LDR);
  ADD_FIELD(features, textureCompressionBC);
  ADD_FIELD(features, occlusionQueryPrecise);
  ADD_FIELD(features, pipelineStatisticsQuery);
  ADD_FIELD(features, vertexPipelineStoresAndAtomics);
  ADD_FIELD(features, fragmentStoresAndAtomics);
  ADD_FIELD(features, shaderTessellationAndGeometryPointSize);
  ADD_FIELD(features, shaderImageGatherExtended);
  ADD_FIELD(features, shaderStorageImageExtendedFormats);
  ADD_FIELD(features, shaderStorageImageMultisample);
  ADD_FIELD(features, shaderStorageImageReadWithoutFormat);
  ADD_FIELD(features, shaderStorageImageWriteWithoutFormat);
  ADD_FIELD(features, shaderUniformBufferArrayDynamicIndexing);
  ADD_FIELD(features, shaderSampledImageArrayDynamicIndexing);
  ADD_FIELD(features, shaderStorageBufferArrayDynamicIndexing);
  ADD_FIELD(features, shaderStorageImageArrayDynamicIndexing);
  ADD_FIELD(features, shaderClipDistance);
  ADD_FIELD(features, shaderCullDistance);
  ADD_FIELD(features, shaderFloat64);
  ADD_FIELD(features, shaderInt64);
  ADD_FIELD(features, shaderInt16);
  ADD_FIELD(features, shaderResourceResidency);
  ADD_FIELD(features, shaderResourceMinLod);
  ADD_FIELD(features, sparseBinding);
  ADD_FIELD(features, sparseResidencyBuffer);
  ADD_FIELD(features, sparseResidencyImage2D);
  ADD_FIELD(features, sparseResidencyImage3D);
  ADD_FIELD(features, sparseResidency2Samples);
  ADD_FIELD(features, sparseResidency4Samples);
  ADD_FIELD(features, sparseResidency8Samples);
  ADD_FIELD(features, sparseResidency16Samples);
  ADD_FIELD(features, sparseResidencyAliased);
  ADD_FIELD(features, variableMultisampleRate);
  ADD_FIELD(features, inheritedQueries);
  ADD_FIELD(variablePointer, variablePointersStorageBuffer);
  ADD_FIELD(variablePointer, variablePointers);
  ADD_FIELD(multiview, multiview);
  ADD_FIELD(multiview, multiviewGeometryShader);
  ADD_FIELD(multiview, multiviewTessellationShader);
  ADD_FIELD(drm, protectedMemory);
  ADD_FIELD(shaderDraw, shaderDrawParameters);
  ADD_FIELD(storage16Bit, storageBuffer16BitAccess);
  ADD_FIELD(storage16Bit, uniformAndStorageBuffer16BitAccess);
  ADD_FIELD(storage16Bit, storagePushConstant16);
  ADD_FIELD(storage16Bit, storageInputOutput16);
  ADD_FIELD(blendOpAdvanced, advancedBlendCoherentOperations);
  ADD_FIELD(descriptorIndexing, shaderInputAttachmentArrayDynamicIndexing);
  ADD_FIELD(descriptorIndexing, shaderUniformTexelBufferArrayDynamicIndexing);
  ADD_FIELD(descriptorIndexing, shaderStorageTexelBufferArrayDynamicIndexing);
  ADD_FIELD(descriptorIndexing, shaderUniformBufferArrayNonUniformIndexing);
  ADD_FIELD(descriptorIndexing, shaderSampledImageArrayNonUniformIndexing);
  ADD_FIELD(descriptorIndexing, shaderStorageBufferArrayNonUniformIndexing);
  ADD_FIELD(descriptorIndexing, shaderStorageImageArrayNonUniformIndexing);
  ADD_FIELD(descriptorIndexing, shaderInputAttachmentArrayNonUniformIndexing);
  ADD_FIELD(descriptorIndexing,
            shaderUniformTexelBufferArrayNonUniformIndexing);
  ADD_FIELD(descriptorIndexing,
            shaderStorageTexelBufferArrayNonUniformIndexing);
  ADD_FIELD(descriptorIndexing, descriptorBindingUniformBufferUpdateAfterBind);
  ADD_FIELD(descriptorIndexing, descriptorBindingSampledImageUpdateAfterBind);
  ADD_FIELD(descriptorIndexing, descriptorBindingStorageImageUpdateAfterBind);
  ADD_FIELD(descriptorIndexing, descriptorBindingStorageBufferUpdateAfterBind);
  ADD_FIELD(descriptorIndexing,
            descriptorBindingUniformTexelBufferUpdateAfterBind);
  ADD_FIELD(descriptorIndexing,
            descriptorBindingStorageTexelBufferUpdateAfterBind);
  ADD_FIELD(descriptorIndexing, descriptorBindingUpdateUnusedWhilePending);
  ADD_FIELD(descriptorIndexing, descriptorBindingPartiallyBound);
  ADD_FIELD(descriptorIndexing, descriptorBindingVariableDescriptorCount);
  ADD_FIELD(descriptorIndexing, runtimeDescriptorArray);
#undef ADD_FIELD

  reset();
}

void DeviceFeatures::reset() {
  VkOverwrite(*this);
  VkOverwrite(variablePointer);
  VkOverwrite(multiview);
  VkOverwrite(drm);
  VkOverwrite(shaderDraw);
  VkOverwrite(storage16Bit);
  VkOverwrite(blendOpAdvanced);
  VkOverwrite(descriptorIndexing);
}

int DeviceFeatures::getFeatures(Device& dev) {
  reset();
  if (dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
    vkGetPhysicalDeviceFeatures(dev.phys, &features);
    return 0;
  }

  // Create pNext chain for Vulkan 1.1 features.
  pNext = &variablePointer;
  variablePointer.pNext = &multiview;
  multiview.pNext = &drm;
  drm.pNext = &shaderDraw;
  shaderDraw.pNext = &storage16Bit;
  auto ppNext = &storage16Bit.pNext;

#define ifExtension(extname, membername)   \
  if (dev.isExtensionAvailable(extname)) { \
    *ppNext = &membername;                 \
    ppNext = &membername.pNext;            \
  }
  ifExtension("VK_EXT_blend_operation_advanced", blendOpAdvanced);
  ifExtension("VK_EXT_descriptor_indexing", descriptorIndexing);

  vkGetPhysicalDeviceFeatures2(dev.phys, this);
  return 0;
}

PhysicalDeviceProperties::PhysicalDeviceProperties() {
#define ADD_FIELD(substruct, field) \
  if (reflect.addField(STRINGIFY(field), &(substruct.field))) return
#define ADD_ARRAY_FIELD(substruct, field)                          \
  if (reflect.addArrayField(                                       \
          STRINGIFY(field), (substruct.field),                     \
          sizeof(substruct.field) / sizeof(substruct.field[0]))) { \
    return;                                                        \
  }
  ADD_FIELD(properties, apiVersion);
  ADD_FIELD(properties, driverVersion);
  ADD_FIELD(properties, vendorID);
  ADD_FIELD(properties, deviceID);
  ADD_FIELD(properties, deviceType);
  ADD_ARRAY_FIELD(properties, deviceName);
  ADD_FIELD(properties.limits, maxImageDimension1D);
  ADD_FIELD(properties.limits, maxImageDimension2D);
  ADD_FIELD(properties.limits, maxImageDimension3D);
  ADD_FIELD(properties.limits, maxImageDimensionCube);
  ADD_FIELD(properties.limits, maxImageArrayLayers);
  ADD_FIELD(properties.limits, maxTexelBufferElements);
  ADD_FIELD(properties.limits, maxUniformBufferRange);
  ADD_FIELD(properties.limits, maxStorageBufferRange);
  ADD_FIELD(properties.limits, maxPushConstantsSize);
  ADD_FIELD(properties.limits, maxMemoryAllocationCount);
  ADD_FIELD(properties.limits, maxSamplerAllocationCount);
  ADD_FIELD(properties.limits, bufferImageGranularity);
  ADD_FIELD(properties.limits, sparseAddressSpaceSize);
  ADD_FIELD(properties.limits, maxBoundDescriptorSets);
  ADD_FIELD(properties.limits, maxPerStageDescriptorSamplers);
  ADD_FIELD(properties.limits, maxPerStageDescriptorUniformBuffers);
  ADD_FIELD(properties.limits, maxPerStageDescriptorStorageBuffers);
  ADD_FIELD(properties.limits, maxPerStageDescriptorSampledImages);
  ADD_FIELD(properties.limits, maxPerStageDescriptorStorageImages);
  ADD_FIELD(properties.limits, maxPerStageDescriptorInputAttachments);
  ADD_FIELD(properties.limits, maxPerStageResources);
  ADD_FIELD(properties.limits, maxDescriptorSetSamplers);
  ADD_FIELD(properties.limits, maxDescriptorSetUniformBuffers);
  ADD_FIELD(properties.limits, maxDescriptorSetUniformBuffersDynamic);
  ADD_FIELD(properties.limits, maxDescriptorSetStorageBuffers);
  ADD_FIELD(properties.limits, maxDescriptorSetStorageBuffersDynamic);
  ADD_FIELD(properties.limits, maxDescriptorSetSampledImages);
  ADD_FIELD(properties.limits, maxDescriptorSetStorageImages);
  ADD_FIELD(properties.limits, maxDescriptorSetInputAttachments);
  ADD_FIELD(properties.limits, maxVertexInputAttributes);
  ADD_FIELD(properties.limits, maxVertexInputBindings);
  ADD_FIELD(properties.limits, maxVertexInputAttributeOffset);
  ADD_FIELD(properties.limits, maxVertexInputBindingStride);
  ADD_FIELD(properties.limits, maxVertexOutputComponents);
  ADD_FIELD(properties.limits, maxTessellationGenerationLevel);
  ADD_FIELD(properties.limits, maxTessellationPatchSize);
  ADD_FIELD(properties.limits, maxTessellationControlPerVertexInputComponents);
  ADD_FIELD(properties.limits, maxTessellationControlPerVertexOutputComponents);
  ADD_FIELD(properties.limits, maxTessellationControlPerPatchOutputComponents);
  ADD_FIELD(properties.limits, maxTessellationControlTotalOutputComponents);
  ADD_FIELD(properties.limits, maxTessellationEvaluationInputComponents);
  ADD_FIELD(properties.limits, maxTessellationEvaluationOutputComponents);
  ADD_FIELD(properties.limits, maxGeometryShaderInvocations);
  ADD_FIELD(properties.limits, maxGeometryInputComponents);
  ADD_FIELD(properties.limits, maxGeometryOutputComponents);
  ADD_FIELD(properties.limits, maxGeometryOutputVertices);
  ADD_FIELD(properties.limits, maxGeometryTotalOutputComponents);
  ADD_FIELD(properties.limits, maxFragmentInputComponents);
  ADD_FIELD(properties.limits, maxFragmentOutputAttachments);
  ADD_FIELD(properties.limits, maxFragmentDualSrcAttachments);
  ADD_FIELD(properties.limits, maxFragmentCombinedOutputResources);
  ADD_FIELD(properties.limits, maxComputeSharedMemorySize);
  ADD_ARRAY_FIELD(properties.limits, maxComputeWorkGroupCount);
  ADD_FIELD(properties.limits, maxComputeWorkGroupInvocations);
  ADD_ARRAY_FIELD(properties.limits, maxComputeWorkGroupSize);
  ADD_FIELD(properties.limits, subPixelPrecisionBits);
  ADD_FIELD(properties.limits, subTexelPrecisionBits);
  ADD_FIELD(properties.limits, mipmapPrecisionBits);
  ADD_FIELD(properties.limits, maxDrawIndexedIndexValue);
  ADD_FIELD(properties.limits, maxDrawIndirectCount);
  ADD_FIELD(properties.limits, maxSamplerLodBias);
  ADD_FIELD(properties.limits, maxSamplerAnisotropy);
  ADD_FIELD(properties.limits, maxViewports);
  ADD_ARRAY_FIELD(properties.limits, maxViewportDimensions);
  ADD_ARRAY_FIELD(properties.limits, viewportBoundsRange);
  ADD_FIELD(properties.limits, viewportSubPixelBits);
  ADD_FIELD(properties.limits, minMemoryMapAlignment);
  ADD_FIELD(properties.limits, minTexelBufferOffsetAlignment);
  ADD_FIELD(properties.limits, minUniformBufferOffsetAlignment);
  ADD_FIELD(properties.limits, minStorageBufferOffsetAlignment);
  ADD_FIELD(properties.limits, minTexelOffset);
  ADD_FIELD(properties.limits, maxTexelOffset);
  ADD_FIELD(properties.limits, minTexelGatherOffset);
  ADD_FIELD(properties.limits, maxTexelGatherOffset);
  ADD_FIELD(properties.limits, minInterpolationOffset);
  ADD_FIELD(properties.limits, maxInterpolationOffset);
  ADD_FIELD(properties.limits, subPixelInterpolationOffsetBits);
  ADD_FIELD(properties.limits, maxFramebufferWidth);
  ADD_FIELD(properties.limits, maxFramebufferHeight);
  ADD_FIELD(properties.limits, maxFramebufferLayers);
  ADD_FIELD(properties.limits, framebufferColorSampleCounts);
  ADD_FIELD(properties.limits, framebufferDepthSampleCounts);
  ADD_FIELD(properties.limits, framebufferStencilSampleCounts);
  ADD_FIELD(properties.limits, framebufferNoAttachmentsSampleCounts);
  ADD_FIELD(properties.limits, maxColorAttachments);
  ADD_FIELD(properties.limits, sampledImageColorSampleCounts);
  ADD_FIELD(properties.limits, sampledImageIntegerSampleCounts);
  ADD_FIELD(properties.limits, sampledImageDepthSampleCounts);
  ADD_FIELD(properties.limits, sampledImageStencilSampleCounts);
  ADD_FIELD(properties.limits, storageImageSampleCounts);
  ADD_FIELD(properties.limits, maxSampleMaskWords);
  ADD_FIELD(properties.limits, timestampComputeAndGraphics);
  ADD_FIELD(properties.limits, timestampPeriod);
  ADD_FIELD(properties.limits, maxClipDistances);
  ADD_FIELD(properties.limits, maxCullDistances);
  ADD_FIELD(properties.limits, maxCombinedClipAndCullDistances);
  ADD_FIELD(properties.limits, discreteQueuePriorities);
  ADD_ARRAY_FIELD(properties.limits, pointSizeRange);
  ADD_ARRAY_FIELD(properties.limits, lineWidthRange);
  ADD_FIELD(properties.limits, pointSizeGranularity);
  ADD_FIELD(properties.limits, lineWidthGranularity);
  ADD_FIELD(properties.limits, strictLines);
  ADD_FIELD(properties.limits, standardSampleLocations);
  ADD_FIELD(properties.limits, optimalBufferCopyOffsetAlignment);
  ADD_FIELD(properties.limits, optimalBufferCopyRowPitchAlignment);
  ADD_FIELD(properties.limits, nonCoherentAtomSize);
  ADD_FIELD(properties.sparseProperties, residencyStandard2DBlockShape);
  ADD_FIELD(properties.sparseProperties,
            residencyStandard2DMultisampleBlockShape);
  ADD_FIELD(properties.sparseProperties, residencyStandard3DBlockShape);
  ADD_FIELD(properties.sparseProperties, residencyAlignedMipSize);
  ADD_FIELD(properties.sparseProperties, residencyNonResidentStrict);
  ADD_ARRAY_FIELD(id, deviceUUID);
  ADD_ARRAY_FIELD(id, driverUUID);
  ADD_ARRAY_FIELD(id, deviceLUID);
  ADD_FIELD(id, deviceNodeMask);
  ADD_FIELD(id, deviceLUIDValid);
  ADD_FIELD(maint3, maxPerSetDescriptors);
  ADD_FIELD(maint3, maxMemoryAllocationSize);
  ADD_FIELD(multiview, maxMultiviewViewCount);
  ADD_FIELD(multiview, maxMultiviewInstanceIndex);
  ADD_FIELD(pointClipping, pointClippingBehavior);
  ADD_FIELD(drm, protectedNoFault);
  ADD_FIELD(subgroup, subgroupSize);
  ADD_FIELD(subgroup, supportedStages);
  ADD_FIELD(subgroup, supportedOperations);
  ADD_FIELD(subgroup, quadOperationsInAllStages);
  ADD_FIELD(blendOpAdvanced, advancedBlendMaxColorAttachments);
  ADD_FIELD(blendOpAdvanced, advancedBlendIndependentBlend);
  ADD_FIELD(blendOpAdvanced, advancedBlendNonPremultipliedSrcColor);
  ADD_FIELD(blendOpAdvanced, advancedBlendNonPremultipliedDstColor);
  ADD_FIELD(blendOpAdvanced, advancedBlendCorrelatedOverlap);
  ADD_FIELD(blendOpAdvanced, advancedBlendAllOperations);
  ADD_FIELD(conservativeRasterize, primitiveOverestimationSize);
  ADD_FIELD(conservativeRasterize, maxExtraPrimitiveOverestimationSize);
  ADD_FIELD(conservativeRasterize, extraPrimitiveOverestimationSizeGranularity);
  ADD_FIELD(conservativeRasterize, primitiveUnderestimation);
  ADD_FIELD(conservativeRasterize, conservativePointAndLineRasterization);
  ADD_FIELD(conservativeRasterize, degenerateTrianglesRasterized);
  ADD_FIELD(conservativeRasterize, degenerateLinesRasterized);
  ADD_FIELD(conservativeRasterize, fullyCoveredFragmentShaderInputVariable);
  ADD_FIELD(conservativeRasterize, conservativeRasterizationPostDepthCoverage);
  ADD_FIELD(descriptorIndexing, maxUpdateAfterBindDescriptorsInAllPools);
  ADD_FIELD(descriptorIndexing,
            shaderUniformBufferArrayNonUniformIndexingNative);
  ADD_FIELD(descriptorIndexing,
            shaderSampledImageArrayNonUniformIndexingNative);
  ADD_FIELD(descriptorIndexing,
            shaderStorageBufferArrayNonUniformIndexingNative);
  ADD_FIELD(descriptorIndexing,
            shaderStorageImageArrayNonUniformIndexingNative);
  ADD_FIELD(descriptorIndexing,
            shaderInputAttachmentArrayNonUniformIndexingNative);
  ADD_FIELD(descriptorIndexing, robustBufferAccessUpdateAfterBind);
  ADD_FIELD(descriptorIndexing, quadDivergentImplicitLod);
  ADD_FIELD(descriptorIndexing, maxPerStageDescriptorUpdateAfterBindSamplers);
  ADD_FIELD(descriptorIndexing,
            maxPerStageDescriptorUpdateAfterBindUniformBuffers);
  ADD_FIELD(descriptorIndexing,
            maxPerStageDescriptorUpdateAfterBindStorageBuffers);
  ADD_FIELD(descriptorIndexing,
            maxPerStageDescriptorUpdateAfterBindSampledImages);
  ADD_FIELD(descriptorIndexing,
            maxPerStageDescriptorUpdateAfterBindStorageImages);
  ADD_FIELD(descriptorIndexing,
            maxPerStageDescriptorUpdateAfterBindInputAttachments);
  ADD_FIELD(descriptorIndexing, maxPerStageUpdateAfterBindResources);
  ADD_FIELD(descriptorIndexing, maxDescriptorSetUpdateAfterBindSamplers);
  ADD_FIELD(descriptorIndexing, maxDescriptorSetUpdateAfterBindUniformBuffers);
  ADD_FIELD(descriptorIndexing,
            maxDescriptorSetUpdateAfterBindUniformBuffersDynamic);
  ADD_FIELD(descriptorIndexing, maxDescriptorSetUpdateAfterBindStorageBuffers);
  ADD_FIELD(descriptorIndexing,
            maxDescriptorSetUpdateAfterBindStorageBuffersDynamic);
  ADD_FIELD(descriptorIndexing, maxDescriptorSetUpdateAfterBindSampledImages);
  ADD_FIELD(descriptorIndexing, maxDescriptorSetUpdateAfterBindStorageImages);
  ADD_FIELD(descriptorIndexing,
            maxDescriptorSetUpdateAfterBindInputAttachments);
  ADD_FIELD(discardRectangle, maxDiscardRectangles);
  ADD_FIELD(externalMemoryHost, minImportedHostPointerAlignment);
  ADD_FIELD(sampleLocations, sampleLocationSampleCounts);
  ADD_FIELD(sampleLocations, maxSampleLocationGridSize);
  ADD_ARRAY_FIELD(sampleLocations, sampleLocationCoordinateRange);
  ADD_FIELD(sampleLocations, sampleLocationSubPixelBits);
  ADD_FIELD(sampleLocations, variableSampleLocations);
  ADD_FIELD(samplerFilterMinmax, filterMinmaxSingleComponentFormats);
  ADD_FIELD(samplerFilterMinmax, filterMinmaxImageComponentMapping);
  ADD_FIELD(vertexAttributeDivisor, maxVertexAttribDivisor);
  ADD_FIELD(pushDescriptor, maxPushDescriptors);
  ADD_FIELD(nvMultiviewPerViewAttr, perViewPositionAllComponents);
  ADD_FIELD(amdShaderCore, shaderEngineCount);
  ADD_FIELD(amdShaderCore, shaderArraysPerEngineCount);
  ADD_FIELD(amdShaderCore, computeUnitsPerShaderArray);
  ADD_FIELD(amdShaderCore, simdPerComputeUnit);
  ADD_FIELD(amdShaderCore, wavefrontsPerSimd);
  ADD_FIELD(amdShaderCore, wavefrontSize);
  ADD_FIELD(amdShaderCore, sgprsPerSimd);
  ADD_FIELD(amdShaderCore, minSgprAllocation);
  ADD_FIELD(amdShaderCore, maxSgprAllocation);
  ADD_FIELD(amdShaderCore, sgprAllocationGranularity);
  ADD_FIELD(amdShaderCore, vgprsPerSimd);
  ADD_FIELD(amdShaderCore, minVgprAllocation);
  ADD_FIELD(amdShaderCore, maxVgprAllocation);
  ADD_FIELD(amdShaderCore, vgprAllocationGranularity);
#undef ADD_FIELD
#undef ADD_ARRAY_FIELD

  reset();
}

void PhysicalDeviceProperties::reset() {
  VkOverwrite(*this);
  VkOverwrite(id);
  VkOverwrite(maint3);
  VkOverwrite(multiview);
  VkOverwrite(pointClipping);
  VkOverwrite(drm);
  VkOverwrite(subgroup);
  VkOverwrite(blendOpAdvanced);
  VkOverwrite(conservativeRasterize);
  VkOverwrite(descriptorIndexing);
  VkOverwrite(discardRectangle);
  VkOverwrite(externalMemoryHost);
  VkOverwrite(sampleLocations);
  VkOverwrite(samplerFilterMinmax);
  VkOverwrite(vertexAttributeDivisor);
  VkOverwrite(pushDescriptor);
  VkOverwrite(nvMultiviewPerViewAttr);
  VkOverwrite(amdShaderCore);
}

int PhysicalDeviceProperties::getProperties(Device& dev) {
  reset();
#if VK_HEADER_VERSION != 74
/* Fix the excessive #ifndef __ANDROID__ below to just use the Android Loader
 * once KhronosGroup lands support. */
#error KhronosGroup update detected, splits Vulkan-LoaderAndValidationLayers
#endif
#ifndef __ANDROID__
  if (dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
#endif
    vkGetPhysicalDeviceProperties(dev.phys, &properties);
    return 0;
#ifndef __ANDROID__
  }

  // Create pNext chain for Vulkan 1.1 features.
  pNext = &id;
  id.pNext = &maint3;
  maint3.pNext = &multiview;
  multiview.pNext = &pointClipping;
  pointClipping.pNext = &drm;
  drm.pNext = &subgroup;
  auto ppNext = &subgroup.pNext;

  ifExtension("VK_EXT_blend_operation_advanced", blendOpAdvanced);
  ifExtension("VK_EXT_conservative_rasterization", conservativeRasterize);
  ifExtension("VK_EXT_descriptor_indexing", descriptorIndexing);
  ifExtension("VK_EXT_discard_rectangles", discardRectangle);
  ifExtension("VK_EXT_external_memory_host", externalMemoryHost);
  ifExtension("VK_EXT_sample_locations", sampleLocations);
  ifExtension("VK_EXT_sampler_filter_minmax", samplerFilterMinmax);
  ifExtension("VK_EXT_vertex_attribute_divisor", vertexAttributeDivisor);
  ifExtension("VK_KHR_push_descriptor", pushDescriptor);
  ifExtension("VK_NVX_multiview_per_view_attributes", nvMultiviewPerViewAttr);
  ifExtension("VK_AMD_shader_core_properties", amdShaderCore);

  vkGetPhysicalDeviceProperties2(dev.phys, this);
  return 0;
#endif /* __ANDROID__ */
}

void FormatProperties::reset() { VkOverwrite(*this); }

int FormatProperties::getProperties(Device& dev) {
  reset();
#ifndef __ANDROID__
  if (dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
#endif
    vkGetPhysicalDeviceFormatProperties(dev.phys, format, &formatProperties);
    return 0;
#ifndef __ANDROID__
  }

  vkGetPhysicalDeviceFormatProperties2(dev.phys, format, this);
  return 0;
#endif /* __ANDROID__ */
}

void DeviceMemoryProperties::reset() { VkOverwrite(*this); }

int DeviceMemoryProperties::getProperties(Device& dev) {
  reset();
#ifndef __ANDROID__
  if (dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
#endif
    vkGetPhysicalDeviceMemoryProperties(dev.phys, &memoryProperties);
    return 0;
#ifndef __ANDROID__
  }

  vkGetPhysicalDeviceMemoryProperties2(dev.phys, this);
  return 0;
#endif /* __ANDROID__ */
}

ImageFormatProperties::ImageFormatProperties() { reset(); }

void ImageFormatProperties::reset() {
  VkOverwrite(*this);
  VkOverwrite(externalImage);
  VkOverwrite(ycbcrConversion);
#if defined(VK_HEADER_VERSION) && VK_HEADER_VERSION > 75
  VkOverwrite(androidHardware);
#endif
  VkOverwrite(amdLODGather);
}

VkResult ImageFormatProperties::getProperties(
    Device& dev, VkFormat format, VkImageType type, VkImageTiling tiling,
    VkImageUsageFlags usage, VkImageCreateFlags flags,
    VkExternalMemoryHandleTypeFlagBits optionalExternalMemoryFlags /*= 0*/) {
  reset();
#ifndef __ANDROID__
  if (dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
#else  /*__ANDROID__*/
  (void)optionalExternalMemoryFlags;
#endif /*__ANDROID__*/
    return vkGetPhysicalDeviceImageFormatProperties(
        dev.phys, format, type, tiling, usage, flags, &imageFormatProperties);
#ifndef __ANDROID__
  }

  // Create pNext chain for Vulkan 1.1 features.
  pNext = &ycbcrConversion;
  auto ppNext = &ycbcrConversion.pNext;
  if (optionalExternalMemoryFlags) {
    *ppNext = &externalImage;
    ppNext = &externalImage.pNext;
  }

#if defined(VK_HEADER_VERSION) && VK_HEADER_VERSION > 75
  ifExtension("VK_ANDROID_external_memory_android_hardware_buffer",
              androidHardware);
#endif
  ifExtension("VK_AMD_texture_gather_bias_lod", amdLODGather);

  VkPhysicalDeviceImageFormatInfo2 VkInit(ifi);
  VkPhysicalDeviceExternalImageFormatInfo VkInit(externalIfi);
  ifi.format = format;
  ifi.type = type;
  ifi.tiling = tiling;
  ifi.usage = usage;
  ifi.flags = flags;
  if (optionalExternalMemoryFlags) {
    ifi.pNext = &externalIfi;
    externalIfi.handleType = optionalExternalMemoryFlags;
  }

  auto r = vkGetPhysicalDeviceImageFormatProperties2(dev.phys, &ifi, this);

  // if r is an error, return it.
  // if VkImageFormatProperties2::imageFormatProperties is *not* filled with
  // zeros, it indicates success.
  if (r != VK_SUCCESS || imageFormatProperties.maxExtent.width ||
      imageFormatProperties.maxExtent.height ||
      imageFormatProperties.maxExtent.depth ||
      imageFormatProperties.maxMipLevels ||
      imageFormatProperties.maxArrayLayers ||
      imageFormatProperties.sampleCounts ||
      imageFormatProperties.maxResourceSize) {
    return r;
  }

  // if VkImageFormatProperties2::imageFormatProperties is filled with zeros
  logE("VkImageFormatProperties filled with zeros, but VK_SUCCESS returned\n");
  return VK_ERROR_VALIDATION_FAILED_EXT;
#endif /*__ANDROID__*/
}

}  // namespace language
