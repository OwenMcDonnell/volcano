#!/usr/bin/env python
# Copyright (c) 2017 the Volcano Authors. Licensed under GPLv3.
# The patches of build-posix.sh written in python for win32 platforms.

from fileinput import FileInput
import re
import sys

class FileEditor:
  def __init__(self, filename):
    self.contents = []
    self.filename = filename

  def readFile(self):
    for line in open(self.filename, "r"):
      line = line.rstrip("\n\r")
      self.contents.append(line)
      yield line

  def run(self, customGenerator):
    changed = False
    result = []
    for line in customGenerator(self.readFile()):
      result.append(line)
      if len(self.contents) != len(result):
        changed = True
      else:
        n = len(self.contents)
        if self.contents[n - 1] != result[n - 1]:
          changed = True
    if changed:
      print("patching %s" % self.filename)
      outf = open(self.filename, "w")
      for line in result:
        outf.write(line + "\n")

def gliFilter(srcFile):
  p1 = re.compile("(HeaderDesc\\.format\\.flags = Storage\\.layers"
    "\\(\\) > 1 \\? [^ ][^ ]* : )(Desc\\.Flags)")
  p2 = re.compile("(HeaderDesc\\.format\\.fourCC = Storage\\.layers"
    "\\(\\) > 1 \\? [^ ][^ ]* : )(Desc\\.FourCC)")
  for line in srcFile:
    r1 = re.sub(p1, "\\1detail::DDPF(\\2)", line)
    yield re.sub(p2, "\\1detail::D3DFORMAT(\\2)", r1)

def spirvCommonFilter(srcFile):
  already_patched = False
  include_vector = False
  report_and_abort = False
  convert_to_string = False
  convert_to_string_target = "	return std::to_string(std::forward<T>(t));"
  for line in srcFile:
    if already_patched:
      yield line
    elif include_vector:
      if line == "":
        yield "#ifdef __ANDROID__"
        yield "#include <android/log.h>"
        yield "#endif"
        yield line
      else:
        already_patched = True
        yield line
    elif line == "{" and report_and_abort:
      yield line
      yield "#ifdef __ANDROID__"
      yield ("	__android_log_assert(__FILE__, \"volcano\", \"spirv_cross "
          "error: %s\\n\", msg.c_str());")
      continue # prevent report_and_abort from being reset to False
    elif line == "#ifdef NDEBUG" and report_and_abort:
      yield "#elif defined(NDEBUG)"
    elif line == "{" and convert_to_string:
      yield line
      continue # prevent convert_to_string from being reset to False
    elif line == convert_to_string_target and convert_to_string:
      yield "	std::ostringstream converter;"
      yield "	converter << std::forward<T>(t);"
      yield "	return converter.str();"
    else:
      yield line
    include_vector = (line == "#include <vector>")
    report_and_abort = (line == "    report_and_abort(const std::string &msg)")
    convert_to_string = (line == "inline std::string convert_to_string(T &&t)")

def glfwFilter(srcFile):
  helper_patch = re.compile("#if WINVER < 0x0600")
  for line in srcFile:
    yield re.sub(helper_patch, "#ifndef DWM_BB_ENABLE",
                 line)

def sampleFilter(scope, srcFile):
  sample_patch = re.compile(("pVersionStruct->pfn(Get(Instance|Device|"
      "PhysicalDevice)ProcAddr) = vk((GetInstance|GetDevice|"
      "_layerGetPhysicalDevice)ProcAddr);$"))
  for line in srcFile:
    yield re.sub(sample_patch,
                 "pVersionStruct->pfn\\1 = %s::\\1;" % scope, line)

def helperFilter(srcFile):
  helper_patch = re.compile("static const char \\* GetPhysDevFeatureString")
  for line in srcFile:
    yield re.sub(helper_patch, "inline const char * GetPhysDevFeatureString",
                 line)

def utilsFilter(srcFile):
  utils_patch = re.compile("^#include \"vulkan/vulkan.h\"")
  for line in srcFile:
    yield re.sub(utils_patch, "#include <vulkan/vulkan.h>", line)

def windefFilter(srcFile):
  def_match = re.compile("vkGetPhysicalDeviceFormatProperties2")
  for line in srcFile:
    yield line
    if def_match.search(line) is not None:
      yield "   vkGetPhysicalDeviceImageFormatProperties2"

if __name__ == "__main__":
  FileEditor("vendor/gli/gli/core/save_dds.inl").run(gliFilter)
  FileEditor("vendor/spirv_cross/spirv_common.hpp").run(spirvCommonFilter)
  FileEditor("vendor/glfw/src/win32_platform.h").run(glfwFilter)
  vvs = "vendor/vulkansamples/submodules/Vulkan-LoaderAndValidationLayers/"
  for n in [
        "core_validation",
        "object_tracker_utils",
        "threading",
        "unique_objects",
      ]:
    scope = re.sub("_utils", "", n)
    FileEditor("%slayers/%s.cpp" % (vvs, n)).run(
      lambda srcFile: sampleFilter(scope, srcFile))
  FileEditor("%sscripts/helper_file_generator.py" % vvs).run(helperFilter)
  FileEditor("%slayers/vk_format_utils.cpp" % vvs).run(utilsFilter)
  FileEditor("%sloader/vulkan-1.def" % vvs).run(windefFilter)
