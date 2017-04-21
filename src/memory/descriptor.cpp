/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include <src/science/science.h>
#include "memory.h"

namespace memory {

int DescriptorPool::ctorError(uint32_t maxSets,
                              std::vector<VkDescriptorType> maxDescriptors) {
  VkDescriptorPoolCreateInfo VkInit(info);
  info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

  // Vulkan Spec says: "If multiple VkDescriptorPoolSize structures appear in
  // the pPoolSizes array then the pool will be created with enough storage
  // for the total number of descriptors of each type."
  //
  // The Vulkan driver will count the number of times each VkDescriptorType
  // occurs. This just translates the VkDescriptorType into a request for
  // 1 descriptor of that type.
  std::vector<VkDescriptorPoolSize> poolSizes;
  for (auto& dType : maxDescriptors) {
    poolSizes.emplace_back();
    auto& poolSize = poolSizes.back();
    VkOverwrite(poolSize);
    poolSize.type = dType;
    poolSize.descriptorCount = 1;
  }
  info.poolSizeCount = poolSizes.size();
  info.pPoolSizes = poolSizes.data();
  info.maxSets = maxSets;

  vk.reset();
  VkResult v = vkCreateDescriptorPool(dev.dev, &info, dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkCreateDescriptorPool", v,
            string_VkResult(v));
    return 1;
  }
  vk.allocator = dev.dev.allocator;
  return 0;
}

int DescriptorSetLayout::ctorError(
    language::Device& dev,
    const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
  // Save the descriptor types that make up this layout.
  types.clear();
  types.reserve(bindings.size());
  for (auto& binding : bindings) {
    types.emplace_back(binding.descriptorType);
  }

  VkDescriptorSetLayoutCreateInfo VkInit(info);
  info.bindingCount = bindings.size();
  info.pBindings = bindings.data();

  vk.reset();
  VkResult v =
      vkCreateDescriptorSetLayout(dev.dev, &info, dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkCreateDescriptorSetLayout", v,
            string_VkResult(v));
    return 1;
  }
  return 0;
}

DescriptorSet::~DescriptorSet() {
  VkResult v = vkFreeDescriptorSets(pool.dev.dev, pool.vk, 1, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkFreeDescriptorSets", v,
            string_VkResult(v));
    exit(1);
  }
}

int DescriptorSet::ctorError(const DescriptorSetLayout& layout) {
  types = layout.types;
  const VkDescriptorSetLayout& setLayout = layout.vk;

  VkDescriptorSetAllocateInfo VkInit(info);
  info.descriptorPool = pool.vk;
  info.descriptorSetCount = 1;
  info.pSetLayouts = &setLayout;

  VkResult v = vkAllocateDescriptorSets(pool.dev.dev, &info, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr,
            "vkAllocateDescriptorSets failed: %d (%s)\n"
            "The Vulkan spec suggests:\n"
            "1. Ignore the exact error code returned.\n"
            "2. Try creating a new DescriptorPool.\n"
            "3. Retry DescriptorSet::ctorError().\n"
            "4. If that fails, abort.\n",
            v, string_VkResult(v));
    return 1;
  }
  return 0;
}

int DescriptorSet::write(uint32_t binding,
                         const std::vector<VkDescriptorImageInfo> imageInfo,
                         uint32_t arrayI /*= 0*/) {
  if (binding > types.size()) {
    fprintf(stderr,
            "DescriptorSet::write(%u, imageInfo): binding=%u with only %zu "
            "bindings\n",
            binding, binding, types.size());
    return 1;
  }
  switch (types.at(binding)) {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      break;
    default:
      fprintf(stderr,
              "DescriptorSet::write(%u, imageInfo): binding=%u has type %s\n",
              binding, binding, string_VkDescriptorType(types.at(binding)));
      return 1;
  }
  VkWriteDescriptorSet VkInit(w);
  w.dstSet = vk;
  w.dstBinding = binding;
  w.dstArrayElement = arrayI;
  w.descriptorType = types.at(binding);
  w.descriptorCount = imageInfo.size();
  w.pImageInfo = imageInfo.data();
  vkUpdateDescriptorSets(pool.dev.dev, 1, &w, 0, nullptr);
  return 0;
}

int DescriptorSet::write(uint32_t binding,
                         const std::vector<VkDescriptorBufferInfo> bufferInfo,
                         uint32_t arrayI /*= 0*/) {
  if (binding > types.size()) {
    fprintf(stderr,
            "DescriptorSet::write(%u, bufferInfo): binding=%u with only %zu "
            "bindings\n",
            binding, binding, types.size());
    return 1;
  }
  switch (types.at(binding)) {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      break;
    default:
      fprintf(stderr,
              "DescriptorSet::write(%u, bufferInfo): binding=%u has type %s\n",
              binding, binding, string_VkDescriptorType(types.at(binding)));
      return 1;
  }
  VkWriteDescriptorSet VkInit(w);
  w.dstSet = vk;
  w.dstBinding = binding;
  w.dstArrayElement = arrayI;
  w.descriptorType = types.at(binding);
  w.descriptorCount = bufferInfo.size();
  w.pBufferInfo = bufferInfo.data();
  vkUpdateDescriptorSets(pool.dev.dev, 1, &w, 0, nullptr);
  return 0;
}

int DescriptorSet::write(uint32_t binding,
                         const std::vector<VkBufferView> texelBufferViewInfo,
                         uint32_t arrayI /*= 0*/) {
  if (binding > types.size()) {
    fprintf(stderr,
            "DescriptorSet::write(%u, VkBufferView): binding=%u with only %zu "
            "bindings\n",
            binding, binding, types.size());
    return 1;
  }
  switch (types.at(binding)) {
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      break;
    default:
      fprintf(
          stderr,
          "DescriptorSet::write(%u, VkBufferView): binding=%u has type %s\n",
          binding, binding, string_VkDescriptorType(types.at(binding)));
      return 1;
  }
  VkWriteDescriptorSet VkInit(w);
  w.dstSet = vk;
  w.dstBinding = binding;
  w.dstArrayElement = arrayI;
  w.descriptorType = types.at(binding);
  w.descriptorCount = texelBufferViewInfo.size();
  w.pTexelBufferView = texelBufferViewInfo.data();
  vkUpdateDescriptorSets(pool.dev.dev, 1, &w, 0, nullptr);
  return 0;
}

}  // namespace memory