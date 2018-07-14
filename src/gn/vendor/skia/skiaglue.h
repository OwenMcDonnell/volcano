/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 *
 * skiaglue contains helper functions for calling skia. Instead of defining
 * these functions in a relevant class like memory::Buffer, they are in a
 * separate struct so that skia is an optional dependency instead of required.
 */

#include <src/memory/memory.h>

// skiaglue reads or writes images using skia.
typedef struct skiaglue {
  skiaglue(command::CommandPool& cpool) : cpool(cpool){};

  // cpool is the CommandPool for read/write/transition and the Device dev
  // inside it for allocating a Buffer and Image.
  command::CommandPool& cpool;

  // loadImage loads imgFilename as a VK_FORMAT_R8G8B8A8_UNORM
  // image in stage (a Buffer). Then copies is set up for a later copy from
  // 'stage' to 'img'. img.info is populated by loadImage so that img is ready
  // for an img.ctorError call, though many fields are not touched (so your app
  // can set things up before loadImage if desired.)
  int loadImage(const char* imgFilename, memory::Buffer& stage,
                memory::Image& img);

  // writePNG does not use imgFilenameFound, copy1, or info. It just writes
  // a PNG-encoded image at the requested outFilename.
  int writePNG(memory::Image& image, std::string outFilename);

  // writeDDS does not use imgFilenameFound, copy1, or info. It just writes
  // a DDS-encoded texture at the requested outFilename.
  int writeDDS(memory::Image& image, std::string outFilename);

  // imgFilenameFound is populated by loadImage().
  std::string imgFilenameFound;
  // copies is populated by loadImage().
  std::vector<VkBufferImageCopy> copies;
} skiaglue;
