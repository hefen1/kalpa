# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'memory_watcher_profiler',
      'type': 'shared_library',
      'product_name': 'memory_watcher',
      'dependencies': [
      ],
      'defines': [
        'BUILD_MEMORY_WATCHER',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'dll_main_memory_watcher.cc',
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
        },
      },     
    },
  ],
}
