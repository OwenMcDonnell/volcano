/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 *
 * This is Instance::ctorError(), though it is broken into a few different
 * methods.
 */
#include "language.h"
#include "VkEnum.h"
#include "VkInit.h"
// vk_enum_string_helper.h is not in the default vulkan installation, but is
// generated by the gn/vendor/vulkansamples/BUILD.gn file in this repo.
#include <vulkan/vk_enum_string_helper.h>

namespace language {
using namespace VkEnum;

const char VK_LAYER_LUNARG_standard_validation[] =
    "VK_LAYER_LUNARG_standard_validation";

VkResult initSupportedQueues(Device& dev,
                             std::vector<VkQueueFamilyProperties>& vkQFams);

namespace {  // an anonymous namespace hides its contents outside this file

int initInstance(Instance& inst,
                 const std::vector<std::string>& enabledExtensions,
                 std::vector<VkLayerProperties>& layers) {
  std::vector<const char*> enabledLayers;
  for (const auto& layerprop : layers) {
    // Enable instance layer "VK_LAYER_LUNARG_standard_validation"
    // TODO: permit customization of the enabled instance layers.
    //
    // Getting validation working involves more than just enabling the layer!
    // https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/tree/master/layers
    // 1. Copy libVkLayer_<name>.so to the same dir as your binary, or
    //    set VK_LAYER_PATH to point to where the libVkLayer_<name>.so is.
    // 2. Create a vk_layer_settings.txt file next to libVkLayer_<name>.so.
    // 3. Set the environment variable VK_INSTANCE_LAYERS to activate layers:
    //    export VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_standard_validation
    if (!strcmp(VK_LAYER_LUNARG_standard_validation, layerprop.layerName)) {
      enabledLayers.push_back(layerprop.layerName);
    }
  }

  VkInstanceCreateInfo VkInit(iinfo);
  iinfo.pApplicationInfo = &inst.applicationInfo;
  std::vector<const char*> extPointers;
  if (enabledExtensions.size()) {
    iinfo.enabledExtensionCount = enabledExtensions.size();
    for (size_t i = 0; i < enabledExtensions.size(); i++) {
      extPointers.emplace_back(enabledExtensions.at(i).c_str());
    }
    iinfo.ppEnabledExtensionNames = extPointers.data();
  }
  iinfo.enabledLayerCount = enabledLayers.size();
  iinfo.ppEnabledLayerNames = enabledLayers.data();
  iinfo.enabledLayerCount = 0;

  VkResult v = vkCreateInstance(&iinfo, inst.pAllocator, &inst.vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkCreateInstance", v,
            string_VkResult(v));
    if (v == VK_ERROR_INCOMPATIBLE_DRIVER) {
      fprintf(stderr,
              "Most likely cause: your GPU does not support Vulkan yet.\n"
              "You may try updating your graphics driver.\n");
    } else if (v == VK_ERROR_OUT_OF_HOST_MEMORY) {
      fprintf(stderr,
              "Primary cause: you *might* be out of memory (unlikely).\n"
              "Secondary causes: conflicting vulkan drivers installed.\n"
              "Secondary causes: broken driver installation.\n"
              "You may want to search the web for more information.\n");
    }
    return 1;
  }
  return 0;
}

}  // anonymous namespace

Instance::Instance() {
  applicationName = "TODO: " __FILE__ ": customize applicationName";
  engineName = "github.com/ndsol/volcano";

  VkOverwrite(applicationInfo);
  applicationInfo.apiVersion = VK_API_VERSION_1_0;
  applicationInfo.pApplicationName = applicationName.c_str();
  applicationInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  applicationInfo.pEngineName = engineName.c_str();
}

int Instance::ctorError(const char** requiredExtensions,
                        size_t requiredExtensionCount,
                        CreateWindowSurfaceFn createWindowSurface,
                        void* window) {
  InstanceExtensionChooser instanceExtensions;
  for (size_t i = 0; i < requiredExtensionCount; i++) {
    if (requiredExtensions[i] == nullptr) {
      fprintf(stderr, "invalid requiredExtensions[%zu]\n", i);
      return 1;
    }
    instanceExtensions.required.emplace_back(requiredExtensions[i]);
  }
  if (instanceExtensions.choose()) return 1;

  int r;
  {
    auto* layers = Vk::getLayers();
    if (layers == nullptr) return 1;

    r = initInstance(*this, instanceExtensions.chosen, *layers);
    delete layers;
    if (r) return r;
  }

  if ((r = initDebug()) != 0) return r;

  VkResult v = createWindowSurface(*this, window);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)",
            "createWindowSurface (the user-provided fn)", v,
            string_VkResult(v));
    return 1;
  }
  surface.allocator = pAllocator;

  std::vector<VkPhysicalDevice>* physDevs = Vk::getDevices(vk);
  if (physDevs == nullptr) return 1;

  for (const auto& phys : *physDevs) {
    auto* vkQFams = Vk::getQueueFamilies(phys);
    if (vkQFams == nullptr) {
      delete physDevs;
      return 1;
    }

    // Construct a new dev.
    //
    // Be careful to also call pop_back() unless initSupportQueues()
    // succeeded.
    devs.emplace_back(surface);
    Device& dev = devs.back();
    dev.phys = phys;
    vkGetPhysicalDeviceProperties(phys, &dev.physProp);
    vkGetPhysicalDeviceMemoryProperties(dev.phys, &dev.memProps);

    VkResult r = initSupportedQueues(dev, *vkQFams);
    delete vkQFams;
    if (r != VK_SUCCESS) {
      devs.pop_back();
      if (r != VK_ERROR_DEVICE_LOST) {
        delete physDevs;
        return 1;
      }
    }
  }
  delete physDevs;

  if (devs.size() == 0) {
    fprintf(stderr,
            "No Vulkan-capable devices found on your system.\n"
            "Try running vulkaninfo to troubleshoot.\n");
    return 1;
  }

  if (dbg_lvl > 0) {
    fprintf(stderr, "%zu physical device%s:\n", devs.size(),
            devs.size() != 1 ? "s" : "");
    for (size_t n = 0; n < devs.size(); n++) {
      fprintf(stderr, "  [%zu] \"%s\"\n", n, devs.at(n).physProp.deviceName);
    }
  }
  return r;
}

}  // namespace language