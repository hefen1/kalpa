# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_vtable_verify%': 0,
  },
  'targets': [
    # Only executables and not libraries should depend on the
    # allocator target; only the application (the final executable)
    # knows what allocator makes sense.
    {
      'target_name': 'allocator',
      'type': 'static_library',
      # Make sure the allocation library is optimized to
      # the hilt in official builds.
      'variables': {
        'optimize': 'max',
      },
      'include_dirs': [
        '.',
        '../..',
      ],
      'direct_dependent_settings': {
        'configurations': {
          'Common_Base': {
            'msvs_settings': {
              'VCLinkerTool': {
                'IgnoreDefaultLibraryNames': ['libcmtd.lib', 'libcmt.lib'],
                'AdditionalDependencies': [
                  '<(SHARED_INTERMEDIATE_DIR)/allocator/libcmt.lib'
                ],
              },
            },
          },
        },
        'conditions': [
          ['OS=="win"', {
            'defines': [
              'PERFTOOLS_DLL_DECL=',
            ],
          }],
        ],
      },
      'sources': [
        # Generated for our configuration from tcmalloc's build
        # and checked in.
        'allocator_shim.cc',
        'allocator_shim.h',
        'debugallocation_shim.cc',
        'generic_allocators.cc',
        'win_allocator.cc',
      ],
      # sources! means that these are not compiled directly.
      'sources!': [
        # Included by allocator_shim.cc for maximal inlining.
        'generic_allocators.cc',
        'win_allocator.cc',

        # Included by debugallocation_shim.cc.


        # We simply don't use these, but list them above so that IDE
        # users can view the full available source for reference, etc.
      ],
      'dependencies': [
        '../third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      'msvs_settings': {
        # TODO(sgk):  merge this with build/common.gypi settings
        'VCLibrarianTool': {
          'AdditionalOptions': ['/ignore:4006,4221'],
        },
        'VCLinkerTool': {
          'AdditionalOptions': ['/ignore:4006'],
        },
      },
      'configurations': {
        'Debug_Base': {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'RuntimeLibrary': '0',
            },
          },
          'variables': {
            # Provide a way to force disable debugallocation in Debug builds,
            # e.g. for profiling (it's more rare to profile Debug builds,
            # but people sometimes need to do that).
            'disable_debugallocation%': 0,
          },
          'conditions': [
            # TODO(phajdan.jr): Also enable on Windows.
            ['disable_debugallocation==0 and OS!="win"', {
              'defines': [
                # Use debugallocation for Debug builds to catch problems early
                # and cleanly, http://crbug.com/30715 .
                'TCMALLOC_FOR_DEBUGALLOCATION',
              ],
            }],
          ],
        },
      },
      'conditions': [
        ['OS=="linux" and clang_type_profiler==1', {
          'dependencies': [
            'type_profiler_tcmalloc',
          ],
          # It is undoing dependencies and cflags_cc for type_profiler which
          # build/common.gypi injects into all targets.
          'dependencies!': [
            'type_profiler',
          ],
          'cflags_cc!': [
            '-fintercept-allocation-functions',
          ],
        }],
        ['OS=="win"', {
          'defines': [
            'PERFTOOLS_DLL_DECL=',
          ],
          'defines!': [
            # tcmalloc source files unconditionally define this, remove it from
            # the list of defines that common.gypi defines globally.
            'NOMINMAX',
          ],
          'dependencies': [
            'libcmt',
          ],
          'include_dirs': [
          ],
          'sources!': [

            # included by allocator_shim.cc
            'debugallocation_shim.cc',

            # heap-profiler/checker/cpuprofiler

          ],
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="solaris" or OS=="android"', {
          'sources!': [

            # TODO(willchan): Support allocator shim later on.
            'allocator_shim.cc',

            # TODO(willchan): support jemalloc on other platforms
            # jemalloc files

          ],
          # We enable all warnings by default, but upstream disables a few.
          # Keep "-Wno-*" flags in sync with upstream by comparing against:
          # http://code.google.com/p/google-perftools/source/browse/trunk/Makefile.am
          'cflags': [
            '-Wno-sign-compare',
            '-Wno-unused-result',
          ],
          'cflags!': [
            '-fvisibility=hidden',
          ],
          'link_settings': {
            'ldflags': [
              # Don't let linker rip this symbol out, otherwise the heap&cpu
              # profilers will not initialize properly on startup.
              '-Wl,-uIsHeapProfilerRunning,-uProfilerStart',
              # Do the same for heap leak checker.
              '-Wl,-u_Z21InitialMallocHook_NewPKvj,-u_Z22InitialMallocHook_MMapPKvS0_jiiix,-u_Z22InitialMallocHook_SbrkPKvi',
              '-Wl,-u_Z21InitialMallocHook_NewPKvm,-u_Z22InitialMallocHook_MMapPKvS0_miiil,-u_Z22InitialMallocHook_SbrkPKvl',
              '-Wl,-u_ZN15HeapLeakChecker12IgnoreObjectEPKv,-u_ZN15HeapLeakChecker14UnIgnoreObjectEPKv',
          ]},
        }],
        # Need to distinguish a non-SDK build for Android WebView
        # due to differences in C include files.
        ['OS=="android" and android_webview_build==1', {
          'defines': ['ANDROID_NON_SDK_BUILD'],
        }],
        [ 'use_vtable_verify==1', {
          'cflags': [
            '-fvtable-verify=preinit',
          ],
        }],
        [ 'linux_keep_shadow_stacks==1', {
          'sources': [
          ],
          'cflags': [
            '-finstrument-functions',
          ],
          'defines': [
            'KEEP_SHADOW_STACKS',
          ],
        }],
        [ 'linux_use_heapchecker==0', {
          # Do not compile and link the heapchecker source.
          'sources!': [
          ],
          # Disable the heap checker in tcmalloc.
          'defines': [
            'NO_HEAP_CHECK',
           ],
        }],
        ['order_profiling != 0', {
          'target_conditions' : [
            ['_toolset=="target"', {
              'cflags!': [ '-finstrument-functions' ],
            }],
          ],
        }],
      ],
    },
    {
      # This library is linked in to src/base.gypi:base and allocator_unittests
      # It can't depend on either and nothing else should depend on it - all
      # other code should use the interfaced provided by base.
      'target_name': 'allocator_extension_thunks',
      'type': 'static_library',
      'sources': [
        'allocator_extension_thunks.cc',
        'allocator_extension_thunks.h',
      ],
      'toolsets': ['host', 'target'],
      'include_dirs': [
        '../../'
      ],
      'conditions': [
        ['OS=="linux" and clang_type_profiler==1', {
          # It is undoing dependencies and cflags_cc for type_profiler which
          # build/common.gypi injects into all targets.
          'dependencies!': [
            'type_profiler',
          ],
          'cflags_cc!': [
            '-fintercept-allocation-functions',
          ],
        }],
      ],
    },
   ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'libcmt',
          'type': 'none',
          'actions': [
            {
              'action_name': 'libcmt',
              'inputs': [
                'prep_libc.py',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/allocator/libcmt.lib',
              ],
              'action': [
                'python',
                'prep_libc.py',
                '$(VCInstallDir)lib',
                '<(SHARED_INTERMEDIATE_DIR)/allocator',
                '<(target_arch)',
              ],
            },
          ],
        },
        {
          'target_name': 'allocator_unittests',
          'type': 'executable',
          'dependencies': [
            'allocator',
            'allocator_extension_thunks',
            '../../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '.',
            '../..',
          ],
          'sources': [
            'allocator_unittests.cc',
            '../profiler/alternate_timer.cc',
            '../profiler/alternate_timer.h',
          ],
        },
        {
          'target_name': 'tcmalloc_unittest',
          'type': 'executable',
          'sources': [
            'tcmalloc_unittest.cc',
          ],
          'include_dirs': [
            '../..',
            # For constants of TCMalloc.
          ],
          'dependencies': [
            '../../testing/gtest.gyp:gtest',
            '../base.gyp:base',
            'allocator',
          ],
        },
      ],
    }],
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'allocator_extension_thunks_win64',
          'type': 'static_library',
          'sources': [
            'allocator_extension_thunks.cc',
            'allocator_extension_thunks.h',
          ],
          'toolsets': ['host', 'target'],
          'include_dirs': [
            '../../'
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
    ['OS=="linux" and clang_type_profiler==1', {
      # Some targets in this section undo dependencies and cflags_cc for
      # type_profiler which build/common.gypi injects into all targets.
      'targets': [
        {
          'target_name': 'type_profiler',
          'type': 'static_library',
          'dependencies!': [
            'type_profiler',
          ],
          'cflags_cc!': [
            '-fintercept-allocation-functions',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'type_profiler.cc',
            'type_profiler.h',
            'type_profiler_control.h',
          ],
          'toolsets': ['host', 'target'],
        },
        {
          'target_name': 'type_profiler_tcmalloc',
          'type': 'static_library',
          'dependencies!': [
            'type_profiler',
          ],
          'cflags_cc!': [
            '-fintercept-allocation-functions',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'type_profiler_tcmalloc.cc',
            'type_profiler_tcmalloc.h',
          ],
        },
        {
          'target_name': 'type_profiler_unittests',
          'type': 'executable',
          'dependencies': [
            '../../testing/gtest.gyp:gtest',
            '../base.gyp:base',
            'allocator',
            'type_profiler_tcmalloc',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'type_profiler_control.cc',
            'type_profiler_control.h',
            'type_profiler_unittests.cc',
          ],
        },
        {
          'target_name': 'type_profiler_map_unittests',
          'type': 'executable',
          'dependencies': [
            '../../testing/gtest.gyp:gtest',
            '../base.gyp:base',
            'allocator',
          ],
          'dependencies!': [
            'type_profiler',
          ],
          'cflags_cc!': [
            '-fintercept-allocation-functions',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'type_profiler_map_unittests.cc',
          ],
        },
      ],
    }],
  ],
}
