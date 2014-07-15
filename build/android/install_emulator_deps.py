#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Installs deps for using SDK emulator for testing.

The script will download system images, if they are not present, and
install and enable KVM, if virtualization has been enabled in the BIOS.
"""


import logging
import os
import shutil
import sys

from pylib import cmd_helper
from pylib import constants
from pylib.utils import run_tests_helper

# Android ARMv7 system image for the SDK.
ARMV7_IMG_URL = 'https://dl-ssl.google.com/android/repository/sysimg_armv7a-18_r01.zip'

# Android x86 system image from the Intel website:
# http://software.intel.com/en-us/articles/intel-eula-x86-android-4-2-jelly-bean-bin
X86_IMG_URL = 'http://download-software.intel.com/sites/landingpage/android/sysimg_x86-18_r01.zip'

# Android API level
API_TARGET = 'android-%s' % constants.ANDROID_SDK_VERSION


def CheckSDK():
  """Check if SDK is already installed.

  Returns:
    True if android_tools directory exists in current directory.
  """
  return os.path.exists(os.path.join(constants.EMULATOR_SDK_ROOT,
                                     'android_tools'))


def CheckARMv7Image():
  """Check if the ARMv7 system images have been installed.

  Returns:
    True if the armeabi-v7a directory is present inside the Android SDK
    checkout directory.
  """
  return os.path.exists(os.path.join(constants.EMULATOR_SDK_ROOT,
                                     'android_tools', 'sdk', 'system-images',
                                     API_TARGET, 'armeabi-v7a'))


def CheckX86Image():
  """Check if Android system images have been installed.

  Returns:
    True if android_tools/sdk/system-images directory exists.
  """
  return os.path.exists(os.path.join(constants.EMULATOR_SDK_ROOT,
                                     'android_tools', 'sdk', 'system-images',
                                     API_TARGET, 'x86'))


def CheckKVM():
  """Check if KVM is enabled.

  Returns:
    True if kvm-ok returns 0 (already enabled)
  """
  try:
    return not cmd_helper.RunCmd(['kvm-ok'])
  except OSError:
    logging.info('kvm-ok not installed')
    return False


def InstallKVM():
  """Installs KVM packages."""
  rc = cmd_helper.RunCmd(['sudo', 'apt-get', 'install', 'kvm'])
  if rc:
    logging.critical('ERROR: Did not install KVM. Make sure hardware '
                     'virtualization is enabled in BIOS (i.e. Intel VT-x or '
                     'AMD SVM).')
  # TODO(navabi): Use modprobe kvm-amd on AMD processors.
  rc = cmd_helper.RunCmd(['sudo', 'modprobe', 'kvm-intel'])
  if rc:
    logging.critical('ERROR: Did not add KVM module to Linux Kernal. Make sure '
                     'hardware virtualization is enabled in BIOS.')
  # Now check to ensure KVM acceleration can be used.
  rc = cmd_helper.RunCmd(['kvm-ok'])
  if rc:
    logging.critical('ERROR: Can not use KVM acceleration. Make sure hardware '
                     'virtualization is enabled in BIOS (i.e. Intel VT-x or '
                     'AMD SVM).')


def GetARMv7Image():
  """Download and install the ARMv7 system image."""
  logging.info('Download ARMv7 system image directory into sdk directory.')
  try:
    cmd_helper.RunCmd(['curl', '-o', '/tmp/armv7_img.zip', ARMV7_IMG_URL])
    rc = cmd_helper.RunCmd(['unzip', '-o', '/tmp/armv7_img.zip', '-d', '/tmp/'])
    if rc:
      raise Exception('ERROR: Could not download/unzip image zip.')
    sys_imgs = os.path.join(constants.EMULATOR_SDK_ROOT, 'android_tools', 'sdk',
                            'system-images', API_TARGET, 'armeabi-v7a')
    shutil.move('/tmp/armeabi-v7a', sys_imgs)
  finally:
    os.unlink('/tmp/armv7_img.zip')


def GetX86Image():
  """Download x86 system image from Intel's website."""
  logging.info('Download x86 system image directory into sdk directory.')
  try:
    cmd_helper.RunCmd(['curl', '-o', '/tmp/x86_img.zip', X86_IMG_URL])
    rc = cmd_helper.RunCmd(['unzip', '-o', '/tmp/x86_img.zip', '-d', '/tmp/'])
    if rc:
      raise Exception('ERROR: Could not download/unzip image zip.')
    sys_imgs = os.path.join(constants.EMULATOR_SDK_ROOT, 'android_tools', 'sdk',
                            'system-images', API_TARGET, 'x86')
    shutil.move('/tmp/x86', sys_imgs)
  finally:
    os.unlink('/tmp/x86_img.zip')


def main(argv):
  logging.basicConfig(level=logging.INFO,
                      format='# %(asctime)-15s: %(message)s')
  run_tests_helper.SetLogLevel(verbose_count=1)

  if not CheckSDK():
    logging.critical(
      'ERROR: android_tools does not exist. Make sure your .gclient file '
      'contains the right \'target_os\' entry. See '
      'https://code.google.com/p/chromium/wiki/AndroidBuildInstructions for '
      'more information.')
    return 1

  # Download system images only if needed.
  if CheckARMv7Image():
    logging.info('The ARMv7 image is already present.')
  else:
    GetARMv7Image()

  if CheckX86Image():
    logging.info('The x86 image is already present.')
  else:
    GetX86Image()

  # Make sure KVM packages are installed and enabled.
  if CheckKVM():
    logging.info('KVM already installed and enabled.')
  else:
    InstallKVM()


if __name__ == '__main__':
  sys.exit(main(sys.argv))
