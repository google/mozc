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

"""This script code-signs the target files with the specified keychain.

Exapmle:
/home/komatsu/bin/update_codesign.py --target /path/to/target
"""
import optparse
import os
import subprocess
import sys


def RunOrDie(command):
  """Run the command, or die if it failed."""
  try:
    output = subprocess.check_output(command, shell=True)
    print("==========", file=sys.stderr)
    print(output.decode("utf-8"), file=sys.stderr)
  except subprocess.CalledProcessError as e:
    print("==========", file=sys.stderr)
    output = e.output
    if isinstance(output, bytes):
      output = output.decode("utf-8")
    print(output, file=sys.stderr)
    print("==========", file=sys.stderr)
    sys.exit(1)


def Codesign(target, sign=None, keychain=None):
  """Run the codesign command with the arguments."""

  sign = (sign or GetIdentifier(""))
  keychain = (keychain or GetKeychain(""))
  if not sign or not keychain:
    return

  flags = "--keychain " + os.path.abspath(keychain)
  RunOrDie(" ".join(["/usr/bin/codesign",
                     "-f --verbose",
                     "--deep",  # Recursive signs
                     "--timestamp --options=runtime",  # For notarization
                     "--sign \"%s\"" % sign,
                     flags,
                     target]))


def Verify(target):
  """Run the codesign command with the -vvv option."""
  # codesign returns false if the target was not signed.
  result = os.system(" ".join(["/usr/bin/codesign", "-vvv", target]))
  return result == 0


def UnlockKeychain(keychain, password=None):
  """Run the security command with the unlock-keychain option."""
  command = ["/usr/bin/security unlock-keychain"]
  if password:
    command.append("-p " + password)
  command.append(keychain)
  RunOrDie(" ".join(command))


def GetIdentifier(default):
  """Return the identifier for the keychain."""
  return default


def GetKeychain(default):
  """Return the keychain for the keychain."""
  return os.path.abspath(default)


def GetCodeSignFlags():
  """Return a list of code sign flags considering the build environment."""
  identity = GetIdentifier("")
  keychain = GetKeychain("")

  if identity and keychain:
    flags = "--deep --timestamp --options=runtime --keychain %s" % keychain
    return [
        "CODE_SIGN_IDENTITY=%s" % identity,
        "OTHER_CODE_SIGN_FLAGS=%s" % flags,
        # CODE_SIGN_INJECT_BASE_ENTITLEMENTS=NO is the configuration to address
        # the error of "The executable requests the
        #               com.apple.security.get-task-allow entitlement."
        "CODE_SIGN_INJECT_BASE_ENTITLEMENTS=NO",
    ]
  else:
    return []


def ParseOption():
  """Parse command line options."""
  parser = optparse.OptionParser()
  parser.add_option("--target", dest="target")
  parser.add_option("--sign", dest="sign", default="Google Inc (test)")
  parser.add_option("--keychain", dest="keychain",
                    default="mac/MacSigning.keychain")
  parser.add_option("--password", dest="password",
                    default="GoogleJapaneseInput")
  parser.add_option("--release", dest="release", action="store_true",
                    default=False)
  parser.add_option("--verify", dest="verify", action="store_true",
                    default=False)
  parser.add_option("--notarize_only", dest="notarize_only",
                    action="store_true", default=False)
  parser.add_option("--output", dest="output")
  (options, unused_args) = parser.parse_args()

  if not options.target:
    print("Error: --target should be specified.")
    print(parser.print_help())
    sys.exit(1)

  return options


def DumpEnviron():
  print("=== os.environ ===")
  for key in sorted(os.environ):
    print("%s = %s" % (key, os.getenv(key)))
  print("==================")


def Notarize(target):
  """Execute the notariazation commands."""


def main():
  opts = ParseOption()
  if opts.verify:
    Verify(opts.target)
    return

  DumpEnviron()

  if not opts.notarize_only:
    # Call Codesign with the release keychain.
    keychain = GetKeychain(opts.keychain)
    RunOrDie(" ".join(["/usr/bin/security", "find-identity", keychain]))

    # Unlock Keychain for codesigning.
    UnlockKeychain(keychain, opts.password)

    Codesign(opts.target, keychain=keychain)

  Notarize(opts.target)

  # Output something to make the processes trackable.
  if opts.output:
    with open(opts.output, "w") as output:
      output.write("Done")

if __name__ == "__main__":
  main()
