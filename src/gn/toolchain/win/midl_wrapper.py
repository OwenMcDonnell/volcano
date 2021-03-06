#!/usr/bin/env python
# Copyright (c) 2017 the Volcano Authors. All rights reserved.
# Licensed under the GPLv3.
import os
import subprocess
import sys

def GetEnv(arch):
  """Gets the saved environment from a file for a given architecture."""
  # The environment is saved as an "environment block" (see CreateProcess
  # and msvs_emulation for details). We convert to a dict here.
  # Drop last 2 NULs, one for list terminator, one for trailing vs. separator.
  pairs = open(arch).read()[:-2].split('\0')
  kvs = [item.split('=', 1) for item in pairs]
  return dict(kvs)

def main(arch, outdir, tlb, h, dlldata, iid, proxy, idl, *flags):
  """Filter noisy filenames output from MIDL compile step that isn't
  quietable via command line flags.
  """
  args = ['midl', '/nologo'] + list(flags) + [
      '/out', outdir,
      '/tlb', tlb,
      '/h', h,
      '/dlldata', dlldata,
      '/iid', iid,
      '/proxy', proxy,
      idl]
  env = GetEnv(arch)
  popen = subprocess.Popen(args, shell=True, env=env, universal_newlines=True,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  out, _ = popen.communicate()
  # Filter junk out of stdout, and write filtered versions. Output we want
  # to filter is pairs of lines that look like this:
  # Processing C:\Program Files (x86)\Microsoft SDKs\...\include\objidl.idl
  # objidl.idl
  lines = out.splitlines()
  prefixes = ('Processing ', '64 bit Processing ')
  processing = set(os.path.basename(x)
                   for x in lines if x.startswith(prefixes))
  for line in lines:
    if not line.startswith(prefixes) and line not in processing:
      print(line)
  return popen.returncode

if __name__ == '__main__':
  sys.exit(main(*sys.argv[1:]))
