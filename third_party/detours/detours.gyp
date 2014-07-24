# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'detours_lib',
      'type': 'static_library',
      'product_name': 'detours',
      'sources': [
        'src/creatwth.cpp',
        'src/detours.cpp',
		'src/detours.h',
		'src/detoured.h',
        'src/disasm.cpp',
		'src/image.cpp',
		'src/modules.cpp',
      ],
	  'defines': [
		'DETOURS_X86=1',
		'_X86_',
	  ],      
	  'dependencies': [
      ],
      'include_dirs': [
        '../..',
      ],
    },
	{
      'target_name': 'detoured',
      'type': 'shared_library',
      'sources': [
        'src/detoured.cpp',
        'src/detoured.h',
      ],
      'dependencies': [
      ],
      'include_dirs': [
        '../..',
      ],
      'msvs_settings': {
		'VCCLCompilerTool': {
          #'BufferSecurityCheck': 'false',
          #'BasicRuntimeChecks': '0',
		},
        'VCLinkerTool': {
          #'EntryPointSymbol': 'DllMain',
          #'IgnoreAllDefaultLibraries': 'true',
          'AdditionalOptions': [
            '/export:Detoured',
          ],
        },
      },
    },
  ],
}
