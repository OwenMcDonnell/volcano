/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "VkEnum.h"
#include "VkInit.h"
#include "language.h"
// vk_enum_string_helper.h is not in the default vulkan installation, but is
// generated by the gn/vendor/vulkansamples/BUILD.gn file in this repo.
#include <vulkan/vk_enum_string_helper.h>

namespace language {
using namespace VkEnum;

namespace {  // an anonymous namespace hides its contents outside this file

int initSurfaceFormat(Device& dev) {
  if (dev.surfaceFormats.size() == 0) {
    fprintf(stderr, "BUG: should not init a device with 0 SurfaceFormats\n");
    return 1;
  }

  if (dev.surfaceFormats.size() == 1 &&
      dev.surfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
    // Vulkan specifies "you get to choose" by returning VK_FORMAT_UNDEFINED.
    // Default to 32-bit color and hardware SRGB color space. Your application
    // probably wants to test dev.surfaceFormats itself and choose its own
    // imageFormat.
    dev.swapChainInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    dev.swapChainInfo.imageColorSpace = dev.surfaceFormats[0].colorSpace;
    return 0;
  }

  // Default to the first surfaceFormat Vulkan indicates is acceptable.
  dev.swapChainInfo.imageFormat = dev.surfaceFormats.at(0).format;
  dev.swapChainInfo.imageColorSpace = dev.surfaceFormats.at(0).colorSpace;
  return 0;
}

int initPresentMode(Device& dev) {
  if (dev.presentModes.size() == 0) {
    fprintf(stderr, "BUG: should not init a device with 0 PresentModes\n");
    return 1;
  }

  bool haveMailbox = false;
  bool haveImmediate = false;
  bool haveFIFO = false;
  bool haveFIFOrelaxed = false;
  for (const auto& availableMode : dev.presentModes) {
    switch (availableMode) {
      case VK_PRESENT_MODE_MAILBOX_KHR:
        haveMailbox = true;
        break;
      case VK_PRESENT_MODE_IMMEDIATE_KHR:
        haveImmediate = true;
        break;
      case VK_PRESENT_MODE_FIFO_KHR:
        haveFIFO = true;
        break;
      case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
        haveFIFOrelaxed = true;
        break;
      case VK_PRESENT_MODE_RANGE_SIZE_KHR:
      case VK_PRESENT_MODE_MAX_ENUM_KHR:
        fprintf(stderr, "BUG: invalid presentMode 0x%x\n", availableMode);
        return 1;
    }
  }

  if (!haveFIFO) {
    // VK_PRESENT_MODE_FIFO_KHR is required to always be present by the spec.
    // TODO: Is this validated by Vulkan validation layers?
    fprintf(stderr,
            "Warn: initPresentMode() did not find "
            "VK_PRESENT_MODE_FIFO_KHR.\n"
            "      This is an unexpected surprise! Could you send us\n"
            "      what vendor/VulkamSamples/build/demo/vulkaninfo\n"
            "      outputs -- we would love a bug report at:\n"
            "      https://github.com/ndsol/volcano/issues/new\n");
  }

  if (haveMailbox) {
    dev.swapChainInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
  } else if (haveImmediate) {
    dev.swapChainInfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
  } else if (haveFIFOrelaxed) {
    dev.swapChainInfo.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
  } else if (haveFIFO) {
    dev.swapChainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  } else {
    fprintf(stderr, "initSurfaceFormatAndPresentMode: haveFIFO is false\n");
    return 1;
  }
  return 0;
}

}  // anonymous namespace

int Device::initSurfaceFormatAndPresentMode() {
  {
    auto* surfaceFormats = Vk::getSurfaceFormats(phys, swapChainInfo.surface);
    if (!surfaceFormats) {
      return 1;
    }
    this->surfaceFormats = *surfaceFormats;
    delete surfaceFormats;
    surfaceFormats = nullptr;

    auto* presentModes = Vk::getPresentModes(phys, swapChainInfo.surface);
    if (!presentModes) {
      return 1;
    }
    this->presentModes = *presentModes;
    delete presentModes;
    presentModes = nullptr;
  }

  if (surfaceFormats.size() == 0 || presentModes.size() == 0) {
    return 0;
  }

  int r = initSurfaceFormat(*this);
  if (r) {
    return r;
  }
  if ((r = initPresentMode(*this)) != 0) {
    return r;
  }
  return 0;
}

VkResult initSupportedQueues(Device& dev,
                             std::vector<VkQueueFamilyProperties>& vkQFams) {
  VkBool32 oneQueueWithPresentSupported = false;
  for (size_t q_i = 0; q_i < vkQFams.size(); q_i++) {
    VkBool32 isPresentSupported = false;
    if (dev.swapChainInfo.surface) {
      // Probe VkPhysicalDevice for surface support.
      VkResult v = vkGetPhysicalDeviceSurfaceSupportKHR(
          dev.phys, q_i, dev.swapChainInfo.surface, &isPresentSupported);
      if (v != VK_SUCCESS) {
        fprintf(stderr, "qfam %zu: %s failed: %d (%s)\n", q_i,
                "vkGetPhysicalDeviceSurfaceSupportKHR", v, string_VkResult(v));
        return VK_ERROR_INITIALIZATION_FAILED;
      }
      oneQueueWithPresentSupported |= isPresentSupported;
    }

    dev.qfams.emplace_back(vkQFams.at(q_i),
                           isPresentSupported ? PRESENT : NONE);
  }

  auto* devExtensions = Vk::getDeviceExtensions(dev.phys);
  if (!devExtensions) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  dev.availableExtensions = *devExtensions;
  delete devExtensions;
  devExtensions = nullptr;

  if (!oneQueueWithPresentSupported) {
    return VK_SUCCESS;
  }

  // A device with a queue with PRESENT support should have all of
  // deviceWithPresentRequiredExts.
  static const char* deviceWithPresentRequiredExts[] = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  size_t i = 0, j;
  for (; i < sizeof(deviceWithPresentRequiredExts) /
                 sizeof(deviceWithPresentRequiredExts[0]);
       i++) {
    for (j = 0; j < dev.availableExtensions.size(); j++) {
      if (!strcmp(dev.availableExtensions.at(j).extensionName,
                  deviceWithPresentRequiredExts[i])) {
        dev.extensionRequests.push_back(deviceWithPresentRequiredExts[i]);
        break;
      }
    }
    if (j >= dev.availableExtensions.size()) {
      // Do not add dev: it claims oneQueueWithPresentSupported but it lacks
      // required extensions. (If it does not do PRESENT at all, it is
      // assumed the device would not be used in the swap chain anyway, so it
      // is not removed.)
      return VK_ERROR_DEVICE_LOST;
    }
  }

  // Init dev.surfaceFormats and dev.presentModes early. Your app can inspect
  // and modify them and then call open().
  int r = dev.initSurfaceFormatAndPresentMode();
  if (r) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  if (dev.surfaceFormats.size() == 0 || dev.presentModes.size() == 0) {
    // Do not add dev: it claims oneQueueWithPresentSupported but it has no
    // surfaceFormats -- or no presentModes.
    return VK_ERROR_DEVICE_LOST;
  }
  return VK_SUCCESS;
}

Device::Device(VkSurfaceKHR surface) {
  VkOverwrite(swapChainInfo);
  swapChainInfo.surface = surface;
  swapChainInfo.imageArrayLayers = 1;  // e.g. 2 is for stereo displays.
  swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapChainInfo.clipped = VK_TRUE;
}

}  // namespace language
