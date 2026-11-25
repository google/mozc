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

r"""A helper script to create a Bazel vendor archive for offline builds.

Examples:
  # Create a vendor archive (or pass --config=oss_macos for macOS):
  python3 build_tools/create_bazel_vendor.py \
    --config=oss_linux \
    --output_dir=/tmp/mozc_vendor

  # Extract the vendor archive and optionally verify input checksums:
  tar -xzf /tmp/mozc_vendor/bazel_vendor.tar.gz -C /tmp/mozc_vendor
  shasum -a 256 -c /tmp/mozc_vendor/vendor_dir/vendor_inputs.sha256

  # Copy MODULE.bazel.lock to the workspace root because Bazel always reads
  # <workspace_root>/MODULE.bazel.lock (no flag overrides its path), and
  # offline module resolution requires its registry file hashes to look up
  # metadata under vendor_dir/_registries without accessing bcr.bazel.build:
  cp /tmp/mozc_vendor/vendor_dir/MODULE.bazel.lock MODULE.bazel.lock

  # Build and test offline using the extracted vendor directory:
  bazelisk build package --config=oss_linux \
    --vendor_dir=/tmp/mozc_vendor/vendor_dir \
    --lockfile_mode=error \
    --repository_disable_download
  bazelisk test ... --config=oss_linux --build_tests_only \
    --vendor_dir=/tmp/mozc_vendor/vendor_dir \
    --lockfile_mode=error \
    --repository_disable_download
"""

import argparse
from collections.abc import Sequence
import gzip
import hashlib
import os
import pathlib
import platform
import shutil
import stat
import subprocess
import tarfile


ABS_SCRIPT_PATH = pathlib.Path(__file__).resolve()
# src/build_tools/create_bazel_vendor.py -> src/
ABS_MOZC_SRC_DIR = ABS_SCRIPT_PATH.parents[1]

DEFAULT_CONFIG = 'oss_linux'
VENDOR_INPUTS_SHA256_FILENAME = 'vendor_inputs.sha256'

DEFAULT_VENDOR_BAZEL_HEADER = (
    '#######################################################################'
    '########\n'
    '# This file is used to configure how external repositories are handled '
    'in vendor mode.\n'
    '# ONLY the two following functions can be used:\n'
    '#\n'
    "# ignore('@@<canonical repo name>', ...) is used to completely ignore "
    'this repo from vendoring.\n'
    '# Bazel will use the normal external cache and fetch process for this '
    'repo.\n'
    '#\n'
    "# pin('@@<canonical repo name>', ...) is used to pin the contents of "
    'this repo under the vendor\n'
    '# directory as if there is a --override_repository flag for this repo.\n'
    '# Note that Bazel will NOT update the vendored source for this repo '
    'while running vendor command\n'
    "# unless it's unpinned. The user can modify and maintain the vendored "
    'source for this repo manually.\n'
    '#######################################################################'
    '########'
)

SUPPORTED_CONFIGS = (
    'oss_linux',
    'oss_macos',
    'prod_linux',
    'prod_macos',
)

LINUX_PACKAGE_TARGETS = ('//:package',)

MACOS_PACKAGE_TARGETS = (
    '//mac:diskimage_signed',
    '//:package',
)

TEST_TARGETS = ('//...',)

MACOS_PLATFORMS = (
    'darwin_x86_64',
    'darwin_arm64',
)

COMMON_EXTRA_REPOS = (
    '@material_icon_dictionary_png',
    '@material_icon_dictionary_svg',
    '@material_icon_properties_png',
    '@material_icon_properties_svg',
    '@material_icon_tool_png',
    '@material_icon_tool_svg',
    '@ja_usage_dict',
    '@zip_code_jigyosyo',
    '@zip_code_ken_all',
)

LINUX_PROTOC_REPOS = (
    '@@protobuf++protoc+prebuilt_protoc.linux_x86_64',
    '@@protobuf++protoc+prebuilt_protoc.linux_aarch_64',
)

MACOS_EXTRA_REPOS = (
    '@google_toolbox_for_mac',
)


def parse_args() -> argparse.Namespace:
  """Parses command-line options."""
  parser = argparse.ArgumentParser(
      description=(
          'Creates a Bazel vendor directory and archive for offline Mozc '
          'builds.'
      )
  )
  parser.add_argument(
      '--mozc_src_dir',
      type=pathlib.Path,
      default=ABS_MOZC_SRC_DIR,
      help='Path to the Mozc src directory containing MODULE.bazel.',
  )
  parser.add_argument(
      '--output_dir',
      type=pathlib.Path,
      default=pathlib.Path('/tmp/mozc_vendor'),
      help='Output directory for vendor_dir, bazel_root, and archive.',
  )
  parser.add_argument(
      '--output',
      type=pathlib.Path,
      default=None,
      help=(
          'Output .tar.gz path (defaults to <output_dir>/bazel_vendor.tar.gz '
          'unless --no_archive is set).'
      ),
  )
  parser.add_argument(
      '--no_archive',
      action='store_true',
      help=(
          'Populate <output_dir>/vendor_dir without creating a .tar.gz '
          'archive.'
      ),
  )
  parser.add_argument(
      '--bazel_bin',
      type=str,
      default=None,
      help='Path to the bazelisk or bazel executable.',
  )
  parser.add_argument(
      '--config',
      type=str,
      default=DEFAULT_CONFIG,
      choices=SUPPORTED_CONFIGS,
      help='Bazel --config name to vendor.',
  )
  return parser.parse_args()


def find_bazel_bin(explicit_bin: str | None) -> str:
  """Finds the bazelisk or bazel executable to invoke.

  Args:
    explicit_bin: Explicit binary path from --bazel_bin, if provided.

  Returns:
    Resolved executable path or command name.

  Raises:
    FileNotFoundError: If neither bazelisk nor bazel can be found.
  """
  if explicit_bin:
    return explicit_bin
  env_bin = os.environ.get('BAZEL_BIN')
  if env_bin:
    return env_bin
  home_bazelisk = pathlib.Path.home() / 'go' / 'bin' / 'bazelisk'
  if home_bazelisk.is_file() and os.access(home_bazelisk, os.X_OK):
    return str(home_bazelisk)
  for candidate in ('bazelisk', 'bazel'):
    found = shutil.which(candidate)
    if found:
      return found
  raise FileNotFoundError('Could not find bazelisk or bazel in PATH.')


def remove_tree_writable(path: pathlib.Path) -> None:
  """Removes a directory tree after making read-only files writable."""
  if not path.exists() and not path.is_symlink():
    return
  if path.is_symlink() or path.is_file():
    path.unlink()
    return
  for root, dirs, files in os.walk(path):
    root_path = pathlib.Path(root)
    for name in dirs + files:
      entry = root_path / name
      if not entry.is_symlink():
        entry.chmod(entry.stat().st_mode | stat.S_IWUSR)
  shutil.rmtree(path)


def reset_tarinfo(tarinfo: tarfile.TarInfo) -> tarfile.TarInfo:
  """Strips host user, group, and timestamp metadata from tar entries."""
  tarinfo.uid = 0
  tarinfo.gid = 0
  tarinfo.uname = ''
  tarinfo.gname = ''
  tarinfo.mtime = 0
  return tarinfo


def create_pkg_config_placeholder(
    placeholder_dir: pathlib.Path, name: str
) -> None:
  """Creates a placeholder @ibus or @qt_linux repository for Linux vendoring.

  pkg_config_repository invokes pkg-config during repository fetch on Linux
  hosts. Providing a placeholder repository allows Bazel target analysis to
  succeed even when libibus-1.0-dev or qt6-base-dev is not installed on the
  host generating the vendor archive.

  Alternative considered:
    pkg_config_repository.bzl could emit a stub BUILD.bazel when gated by a
    dedicated environment variable (e.g. --repo_env=MOZC_VENDOR_MODE=1; an
    unconditional fallback on missing packages would hide real setup errors
    during normal builds). Keeping the --override_repository placeholder here
    avoids adding vendor-only branches to production repository rules.

  Args:
    placeholder_dir: Destination directory for the placeholder repository.
    name: Target and repository name ('ibus' or 'qt_linux').
  """
  libexec_dir = placeholder_dir / 'libexec'
  libexec_dir.mkdir(parents=True, exist_ok=True)
  for tool in ('moc', 'uic', 'rcc'):
    (libexec_dir / tool).touch()
  (placeholder_dir / 'REPO.bazel').touch()
  (placeholder_dir / 'BUILD.bazel').write_text(
      'load("@rules_cc//cc:cc_library.bzl", "cc_library")\n'
      'package(default_visibility = ["//visibility:public"])\n'
      f'cc_library(name = "{name}")\n'
      'exports_files(glob(["libexec/*"], allow_empty=True))\n',
      encoding='utf-8',
  )


def create_qt_mac_placeholder(
    placeholder_dir: pathlib.Path, mozc_src_dir: pathlib.Path
) -> None:
  """Creates a placeholder @qt_mac repository so hosts can vendor macOS.

  qt_mac_repository writes an empty BUILD.bazel on non-macOS hosts, and on
  macOS hosts requires a prebuilt Qt tree under third_party/qt (or
  MOZC_QT_PATH) so framework globs in BUILD.qt.bazel are non-empty. Providing a
  placeholder populated with BUILD.qt.bazel allows Bazel target analysis to
  resolve all @qt_mac targets without building Qt first.

  Alternative considered:
    qt_mac_repository.bzl could generate these stub files and BUILD.qt.bazel
    when running on non-macOS hosts or when gated by --repo_env. Keeping the
    --override_repository placeholder here keeps qt_mac_repository.bzl focused
    on real builds alongside the other local SDK placeholders.

  Args:
    placeholder_dir: Destination directory for the placeholder repository.
    mozc_src_dir: Path to the Mozc src directory.
  """
  subdirs = (
      'libexec',
      'plugins/platforms',
      'lib/QtCore.framework/Headers',
      'lib/QtGui.framework/Headers',
      'lib/QtWidgets.framework/Headers',
  )
  for subdir in subdirs:
    (placeholder_dir / subdir).mkdir(parents=True, exist_ok=True)

  placeholder_files = (
      'REPO.bazel',
      'libexec/moc',
      'libexec/uic',
      'libexec/rcc',
      'plugins/platforms/libqcocoa.dylib',
      'lib/QtCore.framework/QtCore',
      'lib/QtCore.framework/Headers/placeholder.h',
      'lib/QtGui.framework/QtGui',
      'lib/QtGui.framework/Headers/placeholder.h',
      'lib/QtWidgets.framework/QtWidgets',
      'lib/QtWidgets.framework/Headers/placeholder.h',
  )
  for rel_path in placeholder_files:
    (placeholder_dir / rel_path).touch()

  shutil.copyfile(
      mozc_src_dir / 'bazel' / 'BUILD.qt.bazel',
      placeholder_dir / 'BUILD.bazel',
  )


def create_xcode_placeholder(placeholder_dir: pathlib.Path) -> None:
  """Creates a placeholder local_config_xcode repo for non-macOS hosts.

  Analyzing macos_application targets invokes rules_swift's
  xcode_swift_toolchain, which requires a valid Xcode version in
  local_config_xcode. Providing a placeholder allows non-macOS hosts to resolve
  and vendor package dependencies without Xcode installed.

  Alternative considered:
    Unlike @ibus or @qt_mac, local_config_xcode comes from Bazel's built-in
    xcode_configure_extension rather than Mozc's MODULE.bazel, so modifying
    Mozc repository rules cannot avoid this requirement on non-macOS hosts.
    Alternatives are defining a placeholder xcode_config target in the repo for
    --xcode_version_config or hardcoding rules_swift's internal worker
    repositories in MACOS_EXTRA_REPOS; --override_repository avoids both
    polluting BUILD files and coupling to rules_swift internals.

  Args:
    placeholder_dir: Destination directory for the placeholder repository.
  """
  placeholder_dir.mkdir(parents=True, exist_ok=True)
  (placeholder_dir / 'REPO.bazel').touch()
  (placeholder_dir / 'BUILD.bazel').write_text(
      'load("@@apple_support+//xcode:xcode_config.bzl", "xcode_config")\n'
      'load("@@apple_support+//xcode:xcode_version.bzl", "xcode_version")\n'
      'package(default_visibility = ["//visibility:public"])\n'
      'xcode_version(\n'
      '    name = "placeholder_xcode",\n'
      '    version = "16.2",\n'
      '    default_ios_sdk_version = "18.2",\n'
      '    default_macos_sdk_version = "15.2",\n'
      '    default_tvos_sdk_version = "18.2",\n'
      '    default_watchos_sdk_version = "11.2",\n'
      ')\n'
      'xcode_config(\n'
      '    name = "host_xcodes",\n'
      '    default = ":placeholder_xcode",\n'
      '    versions = [":placeholder_xcode"],\n'
      ')\n',
      encoding='utf-8',
  )


def create_updater_placeholder(
    mozc_src_dir: pathlib.Path,
) -> list[pathlib.Path]:
  """Creates a placeholder updater package if missing in product builds.

  In OSS builds this function is a no-op.

  Args:
    mozc_src_dir: Path to the Mozc src directory.

  Returns:
    Paths created by this function that should be removed on cleanup.
  """
  created_paths: list[pathlib.Path] = []
  del mozc_src_dir
  return created_paths


def find_linux_python_repos(vendor_dir: pathlib.Path) -> list[str]:
  """Returns Linux x86_64 and aarch64 rules_python repository labels."""
  prefix = 'rules_python++python+python_'
  versions: set[str] = set()
  if vendor_dir.is_dir():
    for entry in vendor_dir.iterdir():
      if entry.is_dir() and entry.name.startswith(prefix):
        rest = entry.name[len(prefix) :]
        parts = rest.split('_', 2)
        if len(parts) >= 2:
          versions.add(f'{parts[0]}_{parts[1]}')
  if not versions:
    raise RuntimeError(
        f'No {prefix}* toolchain directory found in {vendor_dir}. '
        'Verify rules_python repository naming in MODULE.bazel.'
    )
  repos: list[str] = []
  for version in sorted(versions):
    for triple in ('x86_64-unknown-linux-gnu', 'aarch64-unknown-linux-gnu'):
      repos.append(f'@@{prefix}{version}_{triple}')
  return repos


def vendor_linux_targets(
    bazel_bin: str,
    bazel_root: pathlib.Path,
    vendor_dir: pathlib.Path,
    output_dir: pathlib.Path,
    mozc_src_dir: pathlib.Path,
    config: str,
) -> None:
  """Runs bazel vendor for Linux targets and Linux host toolchains."""
  if platform.system() != 'Linux':
    raise RuntimeError(
        f'--config={config} requires a Linux host '
        f'(current host: {platform.system()}).'
    )

  ibus_placeholder_dir = output_dir / 'ibus_placeholder'
  qt_linux_placeholder_dir = output_dir / 'qt_linux_placeholder'
  remove_tree_writable(ibus_placeholder_dir)
  remove_tree_writable(qt_linux_placeholder_dir)
  create_pkg_config_placeholder(ibus_placeholder_dir, 'ibus')
  create_pkg_config_placeholder(qt_linux_placeholder_dir, 'qt_linux')

  try:
    base_cmd = [
        bazel_bin,
        f'--output_user_root={bazel_root}',
        'vendor',
        f'--vendor_dir={vendor_dir}',
        '--symlink_prefix=/',
        '--lockfile_mode=update',
        f'--config={config}',
        (
            '--override_repository=+pkg_config_repository+ibus='
            f'{ibus_placeholder_dir}'
        ),
        (
            '--override_repository=+pkg_config_repository+qt_linux='
            f'{qt_linux_placeholder_dir}'
        ),
    ]
    subprocess.run(
        [*base_cmd, *LINUX_PACKAGE_TARGETS], cwd=mozc_src_dir, check=True
    )
    subprocess.run(
        [*base_cmd, '--build_tests_only', *TEST_TARGETS],
        cwd=mozc_src_dir,
        check=True,
    )

    protoc_prefix = 'protobuf++protoc+prebuilt_protoc.'
    if not any(
        entry.is_dir() and entry.name.startswith(protoc_prefix)
        for entry in vendor_dir.iterdir()
    ):
      raise RuntimeError(
          f'No {protoc_prefix}* directory found in {vendor_dir}. '
          'Verify protobuf prebuilt_protoc naming in MODULE.bazel.'
      )

    extra_repos = [
        *COMMON_EXTRA_REPOS,
        *LINUX_PROTOC_REPOS,
        *find_linux_python_repos(vendor_dir),
    ]
    repo_flags = [f'--repo={repo}' for repo in extra_repos]
    subprocess.run(
        [
            bazel_bin,
            f'--output_user_root={bazel_root}',
            'vendor',
            f'--vendor_dir={vendor_dir}',
            '--symlink_prefix=/',
            '--lockfile_mode=update',
            *repo_flags,
        ],
        cwd=mozc_src_dir,
        check=True,
    )
  finally:
    remove_tree_writable(ibus_placeholder_dir)
    remove_tree_writable(qt_linux_placeholder_dir)


def vendor_macos_targets(
    bazel_bin: str,
    bazel_root: pathlib.Path,
    vendor_dir: pathlib.Path,
    output_dir: pathlib.Path,
    mozc_src_dir: pathlib.Path,
    config: str,
) -> None:
  """Runs bazel vendor for Intel and Apple Silicon macOS targets."""
  qt_placeholder_dir = output_dir / 'qt_mac_placeholder'
  remove_tree_writable(qt_placeholder_dir)
  create_qt_mac_placeholder(qt_placeholder_dir, mozc_src_dir)

  xcode_placeholder_dir: pathlib.Path | None = None
  if platform.system() != 'Darwin':
    xcode_placeholder_dir = output_dir / 'xcode_placeholder'
    remove_tree_writable(xcode_placeholder_dir)
    create_xcode_placeholder(xcode_placeholder_dir)

  created_updater_paths = create_updater_placeholder(mozc_src_dir)

  try:
    for arch in MACOS_PLATFORMS:
      apple_platform = f'@build_bazel_apple_support//platforms:{arch}'
      base_cmd = [
          bazel_bin,
          f'--output_user_root={bazel_root}',
          'vendor',
          f'--vendor_dir={vendor_dir}',
          '--symlink_prefix=/',
          '--lockfile_mode=update',
          f'--config={config}',
          '--macos_cpus=x86_64,arm64',
          (
              '--override_repository=+qt_mac_repository+qt_mac='
              f'{qt_placeholder_dir}'
          ),
          f'--host_platform={apple_platform}',
          f'--platforms={apple_platform}',
      ]
      if xcode_placeholder_dir is not None:
        base_cmd.append(
            '--override_repository='
            'bazel_tools+xcode_configure_extension+local_config_xcode='
            f'{xcode_placeholder_dir}'
        )
      subprocess.run(
          [*base_cmd, *MACOS_PACKAGE_TARGETS], cwd=mozc_src_dir, check=True
      )
      subprocess.run(
          [*base_cmd, '--build_tests_only', *TEST_TARGETS],
          cwd=mozc_src_dir,
          check=True,
      )

    extra_repos = [*COMMON_EXTRA_REPOS, *MACOS_EXTRA_REPOS]
    repo_flags = [f'--repo={repo}' for repo in extra_repos]
    subprocess.run(
        [
            bazel_bin,
            f'--output_user_root={bazel_root}',
            'vendor',
            f'--vendor_dir={vendor_dir}',
            '--symlink_prefix=/',
            '--lockfile_mode=update',
            *repo_flags,
        ],
        cwd=mozc_src_dir,
        check=True,
    )
  finally:
    remove_tree_writable(qt_placeholder_dir)
    if xcode_placeholder_dir is not None:
      remove_tree_writable(xcode_placeholder_dir)
    for path in created_updater_paths:
      if path.is_file() or path.is_symlink():
        path.unlink()
      elif path.is_dir():
        try:
          path.rmdir()
        except OSError:
          pass


def create_vendor_archive(args: argparse.Namespace) -> None:
  """Runs bazel vendor for the selected config and packages vendor_dir."""
  mozc_src_dir = args.mozc_src_dir.resolve()
  if not (mozc_src_dir / 'MODULE.bazel').is_file():
    raise FileNotFoundError(f'MODULE.bazel not found in {mozc_src_dir}')
  if not (mozc_src_dir / 'BUILD.bazel').is_file():
    raise FileNotFoundError(
        f'BUILD.bazel not found in {mozc_src_dir}. Export a Bazel workspace '
        'before running create_bazel_vendor.py.'
    )

  output_dir = args.output_dir.resolve()
  if output_dir.is_relative_to(mozc_src_dir):
    raise ValueError(
        f'--output_dir ({output_dir}) must be outside --mozc_src_dir '
        f'({mozc_src_dir}) so Bazel //... expansion does not scan '
        'generated BUILD.bazel files.'
    )
  archive_path = (
      args.output.resolve()
      if args.output is not None
      else output_dir / 'bazel_vendor.tar.gz'
  )
  if not args.no_archive and archive_path.is_relative_to(mozc_src_dir):
    raise ValueError(
        f'--output ({archive_path}) must be outside --mozc_src_dir '
        f'({mozc_src_dir}).'
    )
  output_dir.mkdir(parents=True, exist_ok=True)
  vendor_dir = output_dir / 'vendor_dir'
  bazel_root = output_dir / 'bazel_root'
  bazel_bin = find_bazel_bin(args.bazel_bin)

  remove_tree_writable(vendor_dir)
  if not args.no_archive and archive_path.exists():
    archive_path.unlink()

  lockfile_path = mozc_src_dir / 'MODULE.bazel.lock'
  original_lockfile = (
      lockfile_path.read_bytes() if lockfile_path.is_file() else None
  )
  if lockfile_path.exists():
    lockfile_path.unlink()

  try:
    if args.config.endswith('_linux'):
      vendor_linux_targets(
          bazel_bin=bazel_bin,
          bazel_root=bazel_root,
          vendor_dir=vendor_dir,
          output_dir=output_dir,
          mozc_src_dir=mozc_src_dir,
          config=args.config,
      )
    else:
      vendor_macos_targets(
          bazel_bin=bazel_bin,
          bazel_root=bazel_root,
          vendor_dir=vendor_dir,
          output_dir=output_dir,
          mozc_src_dir=mozc_src_dir,
          config=args.config,
      )
    shutil.copyfile(lockfile_path, vendor_dir / 'MODULE.bazel.lock')
  finally:
    subprocess.run(
        [bazel_bin, f'--output_user_root={bazel_root}', 'shutdown'],
        cwd=mozc_src_dir,
        check=False,
    )
    if original_lockfile is not None:
      lockfile_path.write_bytes(original_lockfile)
    elif lockfile_path.exists():
      lockfile_path.unlink()

  pin_vendored_repos(vendor_dir)
  write_vendor_inputs_checksums(vendor_dir, mozc_src_dir)

  if not args.no_archive:
    archive_path.parent.mkdir(parents=True, exist_ok=True)
    tmp_archive_path = (
        archive_path.parent / f'.{archive_path.name}.{os.getpid()}.tmp'
    )
    try:
      with tmp_archive_path.open('wb') as raw_file:
        with gzip.GzipFile(
            filename='', mode='wb', compresslevel=6, fileobj=raw_file, mtime=0
        ) as gz_file:
          with tarfile.open(mode='w', fileobj=gz_file) as tar:
            tar.add(vendor_dir, arcname='vendor_dir', filter=reset_tarinfo)
      tmp_archive_path.replace(archive_path)
    except BaseException:
      tmp_archive_path.unlink(missing_ok=True)
      raise
    print(f'Created {archive_path}')


def pin_vendored_repos(vendor_dir: pathlib.Path) -> Sequence[str]:
  """Pins all vendored repositories in VENDOR.bazel and cleans host markers.

  Bazel hashes the host JVM's os.name, os.arch, and repository rule environment
  into @<repo>.marker files. When a vendor archive is generated on one host
  (e.g. Linux amd64) and consumed on another (e.g. macOS), unpinned
  repositories fail the marker check and attempt network downloads. Pinning
  all vendored repositories in VENDOR.bazel instructs Bazel to use the vendored
  directories directly without marker validation, while local SDK repositories
  not present in vendor_dir (@qt_mac, local_config_xcode, etc.) still run
  normally on the target host.

  Args:
    vendor_dir: Path to the populated Bazel vendor directory.

  Returns:
    Sorted sequence of pinned canonical repository directory names.
  """
  for entry in list(vendor_dir.iterdir()):
    if (
        entry.is_file()
        and not entry.is_symlink()
        and entry.name.startswith('@')
        and entry.name.endswith('.marker')
    ):
      entry.unlink()
    elif entry.is_symlink() and entry.name == 'bazel-external':
      entry.unlink()

  repo_names = tuple(
      sorted(
          entry.name
          for entry in vendor_dir.iterdir()
          if entry.is_dir()
          and not entry.is_symlink()
          and entry.name != '_registries'
      )
  )

  vendor_bazel_path = vendor_dir / 'VENDOR.bazel'
  if vendor_bazel_path.is_file():
    header_lines = [
        line
        for line in vendor_bazel_path.read_text(encoding='utf-8').splitlines()
        if line.strip().startswith('#')
    ]
    header = (
        '\n'.join(header_lines) if header_lines else DEFAULT_VENDOR_BAZEL_HEADER
    )
  else:
    header = DEFAULT_VENDOR_BAZEL_HEADER

  lines = [header]
  if repo_names:
    lines.append('')
    lines.append('pin(')
    for repo_name in repo_names:
      lines.append(f'    "@@{repo_name}",')
    lines.append(')')
  vendor_bazel_path.write_text('\n'.join(lines) + '\n', encoding='utf-8')
  return repo_names


def write_vendor_inputs_checksums(
    vendor_dir: pathlib.Path, mozc_src_dir: pathlib.Path
) -> pathlib.Path:
  """Writes vendor_inputs.sha256 for MODULE.bazel and bazel/ overlays.

  Because pinning repositories in VENDOR.bazel skips Bazel's .marker checks,
  recording SHA-256 checksums allows CI and release builds to verify inputs
  with `shasum -a 256 -c`.

  Args:
    vendor_dir: Path to the Bazel vendor directory.
    mozc_src_dir: Path to the Mozc src directory.

  Returns:
    Path to the written vendor_inputs.sha256 file.

  Raises:
    FileNotFoundError: If MODULE.bazel does not exist in mozc_src_dir.
  """
  module_path = mozc_src_dir / 'MODULE.bazel'
  if not module_path.is_file():
    raise FileNotFoundError(f'MODULE.bazel not found in {mozc_src_dir}')

  bazel_dir = mozc_src_dir / 'bazel'
  extra_paths: list[pathlib.Path] = []
  if bazel_dir.is_dir():
    extra_paths.extend(sorted(bazel_dir.glob('*.patch')))
    extra_paths.extend(sorted(bazel_dir.glob('BUILD.*.bazel')))

  input_paths = [module_path, *extra_paths]
  checksum_lines = [
      f'{hashlib.sha256(path.read_bytes()).hexdigest()}  '
      f'{path.relative_to(mozc_src_dir).as_posix()}\n'
      for path in input_paths
      if path.is_file()
  ]
  checksum_path = vendor_dir / VENDOR_INPUTS_SHA256_FILENAME
  checksum_path.write_text(''.join(checksum_lines), encoding='utf-8')
  return checksum_path


def main() -> None:
  create_vendor_archive(parse_args())


if __name__ == '__main__':
  main()
