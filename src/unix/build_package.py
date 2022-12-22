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

"""Script to make a zip file of Mozc built files."""

import argparse
import os
import shutil
import tempfile


class LocationRules:
  """Rules to locate the build targets to the install paths."""

  def __init__(self, args):
    self.location_rules = {
        'ibus_mozc': args.ibus_mozc_path,
        'icons.zip': '/tmp/icons.zip',
        'mozc.xml': os.path.join(args.ibus_component_dir, 'mozc.xml'),
        'mozc_server': os.path.join(args.mozc_dir, 'mozc_server'),
        'mozc_tool': os.path.join(args.mozc_dir, 'mozc_tool'),
        'mozc_renderer': os.path.join(args.mozc_dir, 'mozc_renderer'),
    }

  def GetDestName(self, src: str) -> str:
    basename = os.path.basename(src)
    return self.location_rules.get(basename, basename).lstrip('/')


def ParseArguments() -> argparse.Namespace:
  """Parse command line options."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--inputs', nargs='+')
  # For mozc_server, mozc_tool, mozc_renderer
  parser.add_argument('--mozc_dir', default='/usr/lib/mozc')
  # For icons
  parser.add_argument('--mozc_icon_dir', default='/usr/share/icons/mozc')
  # For mozc.xml
  parser.add_argument('--ibus_component_dir', default='/usr/lib/ibus/component')
  # For ibus-engine-mozc. It's not dir but a file path.
  parser.add_argument('--ibus_mozc_path',
                      default='/usr/lib/ibus-mozc/ibus-engine-mozc')
  # For ibus icons.
  parser.add_argument('--ibus_mozc_install_dir', default='/usr/share/ibus-mozc')
  parser.add_argument('--output')
  return parser.parse_args()


def main():
  args = ParseArguments()
  location_rules = LocationRules(args)
  with tempfile.TemporaryDirectory() as tmp_dir:
    for src in args.inputs:
      dest_name = location_rules.GetDestName(src)
      dest_path = os.path.join(tmp_dir, dest_name)
      os.makedirs(os.path.dirname(dest_path), exist_ok=True)
      shutil.copy(src, dest_path)

    basename = os.path.splitext(args.output)[0]
    shutil.make_archive(basename, format='zip', root_dir=tmp_dir)


if __name__ == '__main__':
  main()
