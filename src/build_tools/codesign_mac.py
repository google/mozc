# -*- coding: utf-8 -*-
# Copyright 2010-2011, Google Inc.
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

__author__ = "komatsu"

"""
This script code-signs the target files with the specified keychain.

Exapmle:
/home/komatsu/bin/update_codesign.py --target /path/to/target
"""

import optparse
import os
import sys


def RunOrDie(command):
  """Run the command, or die if it failed."""
  print "Running: " + command
  if os.system(command) != 0:
    print >> sys.stderr, "=========="
    print >> sys.stderr, "ERROR: " + command
    print >> sys.stderr, "=========="
    sys.exit(1)

def Codesign(target, sign, flags):
  """Run the codesign command with the arguments."""
  RunOrDie(" ".join(["/usr/bin/codesign -f",
                     "--sign \"%s\"" % sign,
                     flags,
                     target]))

def Verify(target):
  """Run the codesign command with the -vvv option."""
  # codesign returns false if the target was not signed.
  result = os.system(" ".join(["/usr/bin/codesign",
                     "-vvv",
                     target]))
  return (result == 0)

def UnlockKeychain(keychain, password=None):
  """Run the security command with the unlock-keychain option."""
  command = ["/usr/bin/security unlock-keychain"]
  if password:
    command.append("-p " + password)
  command.append(keychain)
  RunOrDie(" ".join(command))



def IsGYPBuild():
  """Return true if this script is called from XCode from GYP."""
  return bool(os.getenv("BUILD_WITH_GYP"))

def ParseOption():
  """Parse command line options."""
  parser = optparse.OptionParser()
  parser.add_option("--target", dest="target")
  parser.add_option("--sign", dest="sign", default="Google Inc (test)")
  parser.add_option("--keychain", dest="keychain",
                    default="mac/MacSigning.keychain")
  parser.add_option("--password", dest="password",
                    default="GoogleJapaneseInput")
  parser.add_option("--noautoconf", dest="autoconf",
                    action="store_false", default=True)
  parser.add_option("--release", dest="release", action="store_true",
                    default=False)
  parser.add_option("--verify", dest="verify", action="store_true",
                    default=False)
  (options, unused_args) = parser.parse_args()

  if not options.target:
    print "Error: --target should be specified."
    print parser.print_help()
    sys.exit(1)

  return options


def DumpEnviron():
  print "=== os.environ ==="
  for key in sorted(os.environ):
    print "%s = %s" % (key, os.getenv(key))
  print "=================="


def main():
  opts = ParseOption()
  if opts.verify:
    Verify(opts.target)
    return


  sign = opts.sign
  keychain = os.path.abspath(opts.keychain)
  flags = "--keychain " + keychain
  # Unlock Keychain for codesigning.
  UnlockKeychain(keychain, opts.password)

  Codesign(opts.target, sign, flags)

if __name__ == "__main__":
  main()
