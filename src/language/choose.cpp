/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "VkEnum.h"
#include "language.h"
#include "vk_enum_string_helper.h"

namespace language {
using namespace VkEnum;

namespace {  // use an anonymous namespace to hide all its contents (only
             // reachable from this file)

int chooseExtensions(const std::vector<std::string>& required,
                     const std::vector<VkExtensionProperties>& found,
                     std::vector<std::string>& chosen) {
  // Enable extension "VK_EXT_debug_report".
  chosen.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

  int r = 0;
  for (const auto& req : required) {
    if (req == VK_EXT_DEBUG_REPORT_EXTENSION_NAME) {
      continue;
    }

    bool ok = false;
    for (const auto& ext : found) {
      if (req == ext.extensionName) {
        chosen.emplace_back(req);
        ok = true;
        break;
      }
    }

    if (!ok) {
      logE("requiredExtension \"%s\": no devices with this extension found.\n",
           req.c_str());
      r = 1;
    }
  }

  return r;
}

}  // anonymous namespace

int InstanceExtensionChooser::choose() {
  auto* found = Vk::getExtensions();
  if (found == nullptr) {
    return 1;
  }

  int r = chooseExtensions(required, *found, chosen);
  delete found;
  return r;
}

InstanceExtensionChooser::InstanceExtensionChooser(Instance& inst) {
  if (inst.requiredExtensions.size() == 0 &&
      inst.minSurfaceSupport.find(language::PRESENT) !=
          inst.minSurfaceSupport.end()) {
    // Do not immediately abort. Maybe you know what you're doing and
    // this error message is out of date? If so, please submit a bug.
    logW("Instance::ctorError: did you forget to set requiredExtensions?\n");
    logW("creating a surface will fail due to missing extension.\n");
  }

  required = inst.requiredExtensions;
}

InstanceExtensionChooser::~InstanceExtensionChooser(){};

VkFormat Device::chooseFormat(VkImageTiling tiling, VkFormatFeatureFlags flags,
                              const std::vector<VkFormat>& fmts) {
  apiUsage(1, 1, 0,
           flags & (VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                    VK_FORMAT_FEATURE_TRANSFER_DST_BIT),
           "chooseFormat(flags=%x) %s", flags,
           "uses VK_FORMAT_FEATURE_TRANSFER_{SRC or DST}_BIT");
  switch (tiling) {
    case VK_IMAGE_TILING_LINEAR:
      for (auto format : fmts) {
        FormatProperties props(format);
        if (props.getProperties(*this)) {
          logE("Device::chooseFormat(%s, %x):%s", string_VkImageTiling(tiling),
               flags, "FormatProperties.getProperties failed\n");
          return VK_FORMAT_UNDEFINED;
        }
        if ((props.formatProperties.linearTilingFeatures & flags) == flags) {
          return format;
        }
      }
      break;

    case VK_IMAGE_TILING_OPTIMAL:
      for (auto format : fmts) {
        FormatProperties props(format);
        if (props.getProperties(*this)) {
          logE("Device::chooseFormat(%s, %x):%s", string_VkImageTiling(tiling),
               flags, "FormatProperties.getProperties failed\n");
          return VK_FORMAT_UNDEFINED;
        }
        if ((props.formatProperties.optimalTilingFeatures & flags) == flags) {
          return format;
        }
      }
      break;

    case VK_IMAGE_TILING_RANGE_SIZE:
      logE("_RANGE_SIZE are only placeholders. This should never happen.");
      break;
    case VK_IMAGE_TILING_MAX_ENUM:
      logE("_MAX_ENUM are only placeholders. This should never happen.");
      break;
  }

  return VK_FORMAT_UNDEFINED;
}

int Device::isExtensionAvailable(const char* name) {
  for (auto& extProps : availableExtensions) {
    if (!strcmp(extProps.extensionName, name)) {
      return 1;
    }
  }
  return 0;
}

}  // namespace language
