# -*- coding: utf-8 -*-
# Copyright 2010-2021, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import hashlib
import pathlib

from absl.testing import absltest
from build_tools import create_bazel_vendor


class CreateBazelVendorTest(absltest.TestCase):

  def test_pin_vendored_repos_writes_pins_and_cleans_markers(self):
    temp_dir = pathlib.Path(self.create_tempdir().full_path)
    vendor_dir = temp_dir / 'vendor_dir'
    (vendor_dir / 'rules_cc+').mkdir(parents=True)
    (vendor_dir / '+http_archive+ja_usage_dict').mkdir(parents=True)
    (vendor_dir / '_registries').mkdir(parents=True)
    (vendor_dir / '@rules_cc+.marker').write_text('hash\n', encoding='utf-8')
    (vendor_dir / 'bazel-external').symlink_to(temp_dir)
    (vendor_dir / 'VENDOR.bazel').write_text(
        '# Custom header\n', encoding='utf-8'
    )

    pinned = create_bazel_vendor.pin_vendored_repos(vendor_dir)

    self.assertSequenceEqual(
        pinned,
        ('+http_archive+ja_usage_dict', 'rules_cc+'),
    )
    self.assertFalse((vendor_dir / '@rules_cc+.marker').exists())
    self.assertFalse((vendor_dir / 'bazel-external').is_symlink())
    vendor_bazel = (vendor_dir / 'VENDOR.bazel').read_text(encoding='utf-8')
    self.assertIn('# Custom header', vendor_bazel)
    self.assertIn('"@@+http_archive+ja_usage_dict",', vendor_bazel)
    self.assertIn('"@@rules_cc+",', vendor_bazel)
    self.assertNotIn('_registries', vendor_bazel)

  def test_write_vendor_inputs_checksums_records_module_patches_and_builds(
      self,
  ):
    temp_dir = pathlib.Path(self.create_tempdir().full_path)
    src_dir = temp_dir / 'src'
    bazel_dir = src_dir / 'bazel'
    bazel_dir.mkdir(parents=True)
    vendor_dir = temp_dir / 'vendor_dir'
    vendor_dir.mkdir()

    module_bytes = b'module(name = "mozc")\n'
    patch_bytes = b'--- a/foo\n+++ b/foo\n'
    build_bytes = b'package(default_visibility = ["//visibility:public"])\n'
    (src_dir / 'MODULE.bazel').write_bytes(module_bytes)
    (bazel_dir / 'sample.patch').write_bytes(patch_bytes)
    (bazel_dir / 'BUILD.ext.bazel').write_bytes(build_bytes)
    (bazel_dir / 'unrelated.bzl').write_text('# ignored\n', encoding='utf-8')

    checksum_path = create_bazel_vendor.write_vendor_inputs_checksums(
        vendor_dir, src_dir
    )

    expected = (
        f'{hashlib.sha256(module_bytes).hexdigest()}  MODULE.bazel\n'
        f'{hashlib.sha256(patch_bytes).hexdigest()}  bazel/sample.patch\n'
        f'{hashlib.sha256(build_bytes).hexdigest()}  bazel/BUILD.ext.bazel\n'
    )
    self.assertEqual(checksum_path, vendor_dir / 'vendor_inputs.sha256')
    self.assertEqual(checksum_path.read_text(encoding='utf-8'), expected)

  def test_write_vendor_inputs_checksums_raises_when_module_bazel_missing(self):
    temp_dir = pathlib.Path(self.create_tempdir().full_path)
    src_dir = temp_dir / 'src'
    src_dir.mkdir()
    vendor_dir = temp_dir / 'vendor_dir'
    vendor_dir.mkdir()

    with self.assertRaises(FileNotFoundError):
      create_bazel_vendor.write_vendor_inputs_checksums(vendor_dir, src_dir)


if __name__ == '__main__':
  absltest.main()
