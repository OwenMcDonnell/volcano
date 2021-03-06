/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#ifdef _WIN32
#define NOMINMAX
#endif
#include "VkEnum.h"
#include "VkInit.h"
#include "language.h"
// vk_enum_string_helper.h is not in the default vulkan installation, but is
// generated by the gn/vendor/vulkansamples/BUILD.gn file in this repo.
#include <vulkan/vk_enum_string_helper.h>

#include <algorithm>

namespace language {
using namespace VkEnum;

namespace {  // an anonymous namespace hides its contents outside this file

uint32_t calculateMinRequestedImages(const VkSurfaceCapabilitiesKHR& scap) {
  // An optimal number of images is one more than the minimum. For example:
  // double buffering minImageCount = 1. imageCount = 2.
  // triple buffering minImageCount = 2. imageCount = 3.
  uint32_t imageCount = scap.minImageCount + 1;

  // maxImageCount = 0 means "there is no maximum except device memory limits".
  if (scap.maxImageCount > 0 && imageCount > scap.maxImageCount) {
    imageCount = scap.maxImageCount;
  }

  // Note: The GPU driver can create more than the number returned here.
  // Device::images.size() gives the actual number created by the GPU driver.
  //
  // https://forums.khronos.org/showthread.php/13489-Number-of-images-created-in-a-swapchain
  return imageCount;
}

VkExtent2D calculateSurfaceExtent2D(const VkSurfaceCapabilitiesKHR& scap,
                                    VkExtent2D sizeRequest) {
  // If currentExtent != { UINT32_MAX, UINT32_MAX } then Vulkan is telling us:
  // "this is the right extent: you already created a surface and Vulkan
  // computed the right size to match it."
  if (scap.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return scap.currentExtent;
  }

  // Vulkan is telling us "choose width, height from scap.minImageExtent
  // to scap.maxImageExtent." Attempt to satisfy sizeRequest.
  const VkExtent2D &lo = scap.minImageExtent, hi = scap.maxImageExtent;
  if (hi.width == 0 || hi.height == 0 || hi.width < lo.width ||
      hi.height < lo.height) {
    logF("calculateSurfaceExtent2D: window is minimized, will fail.\n");
  }
  return {
      /*width:*/ std::max(lo.width, std::min(hi.width, sizeRequest.width)),
      /*height:*/
      std::max(lo.height, std::min(hi.height, sizeRequest.height)),
  };
}

VkSurfaceTransformFlagBitsKHR calculateSurfaceTransform(
    const VkSurfaceCapabilitiesKHR& scap) {
  // Most platforms can just use the currentTransform value for preTransform.
  return scap.currentTransform;
}

}  // anonymous namespace

int Device::resetSwapChain(command::CommandPool& cpool, size_t poolQindex) {
  VkSurfaceCapabilitiesKHR scap;
  VkResult v = getSurfaceCapabilities(scap);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkGetPhysicalDeviceSurfaceCapabilitiesKHR", v,
         string_VkResult(v));
    return 1;
  }

  swapChainInfo.imageExtent =
      calculateSurfaceExtent2D(scap, swapChainInfo.imageExtent);
  swapChainInfo.preTransform = calculateSurfaceTransform(scap);
  swapChainInfo.minImageCount = calculateMinRequestedImages(scap);

  VkSwapchainCreateInfoKHR scci = swapChainInfo;
  scci.oldSwapchain = VK_NULL_HANDLE;
  if (swapChain) {
    scci.oldSwapchain = swapChain;
  }
  uint32_t qfamIndices[] = {
      (uint32_t)getQfamI(PRESENT),
      (uint32_t)getQfamI(GRAPHICS),
  };
  if (qfamIndices[0] == qfamIndices[1]) {
    // Device queues were set up such that one QueueFamily does both
    // PRESENT and GRAPHICS.
    scci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    scci.queueFamilyIndexCount = 0;
    scci.pQueueFamilyIndices = nullptr;
  } else {
    // Device queues were set up such that a different QueueFamily does PRESENT
    // and a different QueueFamily does GRAPHICS.
    logW(
        "SHARING_MODE_CONCURRENT: what GPU is this? It has never been seen.\n");
    logW("TODO: Test a per-resource barrier (queue ownership transfer).\n");
    // Is a queue ownership transfer faster than SHARING_MODE_CONCURRENT?
    // Measure, measure, measure!
    //
    // Note also that a CONCURRENT swapchain, if moved to a different queue in
    // the same QueueFamily, must be done by an ownership barrier.
    scci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    scci.queueFamilyIndexCount = 2;
    scci.pQueueFamilyIndices = qfamIndices;
  }
  VkSwapchainKHR newSwapChain;
  v = vkCreateSwapchainKHR(dev, &scci, dev.allocator, &newSwapChain);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateSwapchainKHR", v, string_VkResult(v));
    return 1;
  }
  // swapChain.inst == VK_NULL_HANDLE the first time through,
  // swapChain needs to be reset to use dev.
  //
  // Also, calling reset here avoids deleting dev.swapChain until after
  // vkCreateSwapchainKHR().
  swapChain.reset(dev);          // Delete the old dev.swapChain.
  *(&swapChain) = newSwapChain;  // Install the new dev.swapChain.
  swapChain.allocator = dev.allocator;

  auto* vkImages = Vk::getSwapchainImages(dev, swapChain);
  if (!vkImages) {
    return 1;
  }

  // Update framebufs preserving existing FrameBuf elements and adding new ones.
  if (addOrUpdateFramebufs(*vkImages, cpool, poolQindex)) {
    delete vkImages;
    return 1;
  }
  delete vkImages;
  return 0;
}

}  // namespace language
