# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'run_with_dll',
      'type': 'executable',
      'product_name': 'run_with_dll',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../third_party/detours/detours.gyp:detours_lib',        
      ],
      'defines': [
        'BUILD_RUN_WITH_DLL',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'run_with_dll.cc',
      ],
      'msvs_settings': {
        'VCCLCompilerTool': {
          #'BufferSecurityCheck': 'false',
          #'BasicRuntimeChecks': '0',
        },
        'VCLinkerTool': {
          #'EntryPointSymbol': 'DllMain',
          #'IgnoreAllDefaultLibraries': 'true',
          #'AdditionalOptions': [
          #  '/export:Detoured',
          #],   
          'SubSystem': '1',          
        },
      },     
    },
  ],
}
