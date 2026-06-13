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

"""Collects artifacts (logs) from failing tests with a Bazel BEP JSONL file.

Reads the Build Event Protocol stream written by
`bazel test --build_event_json_file=<path>` and copies the outputs of every
non-passing test attempt into a destination directory, preserving the
`<package>/<target>` layout so the result can be uploaded as a CI artifact.

Despite the `--build_event_json_file` flag name, the output is JSONL (a.k.a.
newline-delimited JSON): one self-contained JSON event object per line, *not* a
single JSON array. It is therefore parsed line by line with `json.loads()`;
calling `json.loads()` on the whole file would fail.

This intentionally resolves the absolute `file://` URIs recorded in each
`testResult` event instead of walking the `bazel-testlogs` convenience symlink:
that symlink is not produced reliably when the test command triggers platform
transitions, and its location depends on the compilation mode. The URIs in the
BEP are always correct for the run that produced them, so this works the same
on Linux, macOS and Windows.
"""

import argparse
import json
import pathlib
import shutil
from urllib import parse
from urllib import request


def _relative_dest(src: pathlib.Path, label: str) -> pathlib.Path:
  """Computes the destination path under the output dir for a source file.

  Prefers the segment after `testlogs/` (e.g. `base/update_util_test/test.log`)
  so the layout matches what `bazel-testlogs` would have produced. Falls back to
  deriving it from the target label if `testlogs` is somehow absent.

  Args:
    src: Absolute path to the source test output file (e.g. a `test.log`).
    label: Bazel target label of the test (e.g. `//base:update_util_test`),
      used only as a fallback when `src` has no `testlogs` segment.

  Returns:
    The path, relative to the output directory, at which `src` should be
    copied.
  """
  parts = src.parts
  if "testlogs" in parts:
    return pathlib.Path(*parts[parts.index("testlogs") + 1:])
  return pathlib.Path(label.lstrip("/").replace(":", "/")) / src.name


def collect(bep_path: pathlib.Path, out_dir: pathlib.Path) -> int:
  """Copies failing-test outputs referenced by `bep_path` into `out_dir`.

  Args:
    bep_path: Path to the JSONL Build Event Protocol file produced by
      `bazel test --build_event_json_file=<path>`.
    out_dir: Destination directory into which the collected logs are copied.
      Parent directories are created as needed.

  Returns:
    The number of test log files copied into `out_dir`.
  """
  copied = 0
  with open(bep_path, encoding="utf-8") as stream:
    # The BEP output is JSONL: one independent JSON event per line. Parse each
    # line on its own rather than the whole file at once.
    for line in stream:
      line = line.strip()
      if not line:
        continue
      event = json.loads(line)
      result = event.get("testResult")
      if result is None or result.get("status") == "PASSED":
        continue
      label = event.get("id", {}).get("testResult", {}).get("label", "")
      for output in result.get("testActionOutput", []):
        if output.get("name") not in ("test.log", "test.xml"):
          continue
        uri = output.get("uri")
        if not uri:
          continue
        src = pathlib.Path(request.url2pathname(parse.urlparse(uri).path))
        if not src.is_file():
          continue
        dest = out_dir / _relative_dest(src, label)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)
        copied += 1
  return copied


def main() -> None:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      "--bep", required=True, type=pathlib.Path,
      help="Path to the --build_event_json_file output (JSONL).")
  parser.add_argument(
      "--out", required=True, type=pathlib.Path,
      help="Destination directory for the collected logs.")
  args = parser.parse_args()

  if not args.bep.is_file():
    # No BEP means the test command never ran; nothing to collect.
    print(f"BEP file {args.bep} not found; nothing to collect.")
    return

  copied = collect(args.bep, args.out)
  print(f"Collected {copied} test log file(s) into {args.out}")


if __name__ == "__main__":
  main()
