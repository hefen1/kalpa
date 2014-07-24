# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'profiler_dll',
      'type': 'shared_library',
	  'product_name': 'profiler',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_static',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../ipc/ipc.gyp:ipc',
		'../../third_party/detours/detours.gyp:detours_lib',
		'../../third_party/detours/detours.gyp:detoured',
      ],
      'defines': [
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'call_stack.cc',
        'call_stack.h',
        'dll_main.cc',
        'handle_watcher.cc',
        'handle_watcher.h',
        'log_client.cc',
        'log_client.h',
        'profiler_data.cc',
        'profiler_data.h',
        'profiler_init.cc',
        'profiler_init.h',
        'profiler_ipc_messages.cc',
        'profiler_ipc_messages.h',
        'profiler_main.cc',
        'profiler_main.h',
        'profiler_process.cc',
        'profiler_process.h',
        'profiler_thread.cc',
        'profiler_thread.h',
        'profiler_stub.cc',
        'profiler_stub.h',
        'profiler_hook.cc',
        'profiler_hook.h',
        'result_codes.h',
        'nt_loader.cc',
        'nt_loader.h',
      ],
    },
  ],
}
