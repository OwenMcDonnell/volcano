# Copyright 2014 The Chromium Authors. All rights reserved.
# Copyright (c) 2017 the Volcano Authors. All rights reserved.
# Licensed under the GPLv3.

import argparse
import os
import subprocess
import sys

# This script prints information about the build system, the operating
# system and the iOS or Mac SDK (depending on the platform "iphonesimulator",
# "iphoneos" or "macosx" generally).
#
# In the GYP build, this is done inside GYP itself based on the SDKROOT
# variable.

def CheckOutput(args):
  return subprocess.check_output(args).decode(sys.stdout.encoding)


def FormatVersion(version):
  """Converts Xcode version to a format required for Info.plist."""
  version = version.replace('.', '')
  version = version + '0' * (3 - len(version))
  return version.zfill(4)


def FillXcodeVersion(settings):
  """Fills the Xcode version and build number into |settings|."""
  lines = CheckOutput(['xcodebuild', '-version']).splitlines()
  settings['xcode_version'] = FormatVersion(lines[0].split()[-1])
  settings['xcode_build'] = lines[-1].split()[-1]


def FillMachineOSBuild(settings):
  """Fills OS build number into |settings|."""
  settings['machine_os_build'] = CheckOutput(
      ['sw_vers', '-buildVersion']).strip()


def FillSDKPathAndVersion(settings, platform, xcode_version):
  """Fills the SDK path and version for |platform| into |settings|."""
  settings['sdk_path'] = CheckOutput([
      'xcrun', '-sdk', platform, '--show-sdk-path']).strip()
  settings['sdk_version'] = CheckOutput([
      'xcrun', '-sdk', platform, '--show-sdk-version']).strip()
  settings['sdk_platform_path'] = CheckOutput([
      'xcrun', '-sdk', platform, '--show-sdk-platform-path']).strip()
  # TODO: unconditionally use --show-sdk-build-version once Xcode 7.2 or
  # higher is required to build Chrome for iOS or OS X.
  if xcode_version >= '0720':
    settings['sdk_build'] = CheckOutput([
        'xcrun', '-sdk', platform, '--show-sdk-build-version']).strip()
  else:
    settings['sdk_build'] = settings['sdk_version']


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument("--developer_dir", required=False)
  args, unknownargs = parser.parse_known_args()
  if args.developer_dir:
    os.environ['DEVELOPER_DIR'] = args.developer_dir

  if len(unknownargs) != 1:
    sys.stderr.write(
        'usage: %s [iphoneos|iphonesimulator|macosx]\n' %
        os.path.basename(sys.argv[0]))
    sys.exit(1)

  settings = {}
  FillMachineOSBuild(settings)
  FillXcodeVersion(settings)
  FillSDKPathAndVersion(settings, unknownargs[0], settings['xcode_version'])

  for key in sorted(settings):
    print('%s="%s"' % (key, settings[key]))
