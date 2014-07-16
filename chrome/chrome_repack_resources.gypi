# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'action_name': 'repack_resources',
  'variables': {
    'pak_inputs': [
      #'<(SHARED_INTERMEDIATE_DIR)/chrome/chrome_unscaled_resources.pak',
      #'<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/gfx_resources.pak',
      #'<(grit_out_dir)/browser_resources.pak',
      #'<(grit_out_dir)/common_resources.pak',
      #'<(grit_out_dir)/memory_internals_resources.pak',
      #'<(grit_out_dir)/net_internals_resources.pak',
      #'<(grit_out_dir)/signin_internals_resources.pak',
      #'<(grit_out_dir)/sync_internals_resources.pak',
      #'<(grit_out_dir)/translate_internals_resources.pak',
    ],
  },
  'inputs': [
    '<(repack_path)',
    '<@(pak_inputs)',
  ],
  'outputs': [
    '<(SHARED_INTERMEDIATE_DIR)/repack/resources.pak',
  ],
  'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
}
