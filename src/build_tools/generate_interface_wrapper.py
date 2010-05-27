# -*- coding: utf-8 -*-
# Copyright 2010, Google Inc.
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

"""Generates wrapper classes from a header file generated from IDL.

Example:
  python tools\generate_interface_wrapper.py --source=msctf.h
    --output=win32\GIMEJP10\msctf_wrapper-inl.h
    --include_guard_base=MOZC_WIN32_GIMEJP10

"""

__author__ = "mazda"

from optparse import OptionParser
import os
import re
from string import Template


class Function(object):
  """A class for storing a function definition."""

  def __init__(self, name, ret):
    self.name = name
    self.ret = ret
    self.argv = []
    self.body = ""

  def __repr__(self):
    return ("%s %s (\n"
            "      %s) {\n"
            "%s\n"
            "  }") % (self.ret, self.name, ",\n      ".join(self.argv),
                      self.body)

  def __str__(self):
    return self.__repr__()


class Interface(object):
  """A class for storing an interface definition."""

  def __init__(self, name, base):
    self.name = name
    self.base = base
    self.functions = []

  def __repr__(self):
    return ("class %s : public %s {\n"
            " public:\n"
            "  %s\n"
            "};\n") % (self.name, self.base,
                       "\n".join(str(f) for f in self.functions))

  def __str__(self):
    return self.__repr__()

  def StartTemplate(self):
    """Generates a template class to start inheritance chain."""
    return ("template <typename T>\n"
            "class %sT : public %sT2<T, %s> {\n"
            " public:\n"
            "%s\n"
            "};\n") % (self.name, self.base, self.name,
                       "\n\n".join(str(f) for f in self.functions))

  def MiddleTemplate(self):
    """Generates a template class to relay inheritance chain."""
    return ("template <typename T, typename S>\n"
            "class %sT2 : public %sT2<T, S> {\n"
            " public:\n"
            "%s\n"
            "};\n") % (self.name, self.base,
                       "\n\n".join(str(f) for f in self.functions))

  def EndTemplate(self):
    """Generates a template class to end inheritance chain."""
    return ("template <typename T>\n"
            "class IUnknownT2<T, %s> : public %s {};\n") % (self.name,
                                                            self.name)


# snippet used for generating all interface function bodies.
function_body_template = Template(
    "#ifdef NO_LOGGING\n"
    "    HRESULT result =\n"
    "          static_cast<T*>(this)->${function_name}Impl(${argv});\n"
    "    return result;\n"
    "#else\n"
    "    __try {\n"
    "      HRESULT result =\n"
    "          static_cast<T*>(this)->${function_name}Impl(${argv});\n"
    "      return result;\n"
    "    } __except (UnhandledExceptionFilter(GetExceptionInformation())) {\n"
    "      ::OutputDebugString(\n"
    "          L\"exception @ ${class_name}::${function_name}\");\n"
    "      ::DebugBreak();\n"
    "      return E_FAIL;\n"
    "    }\n"
    "#endif  // _DEBUG\n")


def LineToArgv(line):
  """Converts raw line input to an argument declaration."""
  # delete unnecessary tokens
  argv = line.replace(") = 0;", "").replace("\n", "").replace(",", "")
  # delete special syntax such as __RPC__deref_out_opt
  argv = re.sub(r"__[^ ()]+(\(.+\))?", "", argv)
  # delete comment
  argv = re.sub(r"/\*.+\*/", "", argv)
  # compress redundant spaces
  argv = re.sub(r" +", " ", argv)
  # delete a space at the top
  return re.sub(r"^ +", "", argv)


def IncludeGuard(base, filename):
  """Returns an include guard macro."""
  return "%s_%s_" % (base, filename.replace("-", "_").replace(".", "_").upper())


def ParseFile(filename):
  """Parses a header file generated from IDL and returns interfaces."""
  source = file(filename)
  interfaces = []
  for line in source:
    class_match = re.search(
        r"(?P<class_name>[^ ]+) : public (?P<base_class>[^ \n\r]+)", line)
    if not class_match:
      continue
    interface = Interface(class_match.group("class_name"),
                          class_match.group("base_class"))
    for line in source:
      function_match = re.search(
          r"(?P<return_value>virtual HRESULT STDMETHODCALLTYPE)"
          r" (?P<function_name>[^\(]+)\(", line)
      if not function_match:
        if "}" in line:
          interfaces.append(interface)
          break
        else:
          continue
      function = Function(function_match.group("function_name"),
                          "  " + function_match.group("return_value"))
      for line in source:
        if re.match(r"^ +$", line):
          mapping = {"class_name": interface.name,
                     "function_name": function.name,
                     "argv": ",".join(re.search(r"(?P<var>[a-zA-Z]+$)",
                                                arg).group("var")
                                      for arg in function.argv)}
          function.body = function_body_template.substitute(mapping)
          interface.functions.append(function)
          break
        function.argv.append(LineToArgv(line))
  return interfaces


def GenerateFile(filename, interfaces, header_buard_base):
  """Write interface wapper template classes to a file."""
  output = file(filename, "w")
  include_guard = IncludeGuard(header_buard_base, filename.split(os.sep)[-1])
  header = ("// Generated by generate_interface_wrapper.py.  DO NOT EDIT!\n"
            "\n"
            "#ifndef %s\n"
            "#define %s\n"
            "\n") % (include_guard, include_guard)
  footer = ("\n"
            "#endif  // %s\n") % include_guard

  output.write(header)
  output.write("template <typename T, typename S> class IUnknownT2;\n\n")
  for interface in interfaces:
    output.write(interface.StartTemplate())
    output.write("\n")
    output.write(interface.MiddleTemplate())
    output.write("\n")
    output.write(interface.EndTemplate())
    output.write("\n")
  output.write(footer)


def ParseOption():
  """Parse command line options."""
  parser = OptionParser()
  parser.add_option("--source", dest="source")
  parser.add_option("--output", dest="output")
  parser.add_option("--include_guard_base", dest="include_guard_base")
  (options, unused_args) = parser.parse_args()
  return (options, parser)


def main():
  (opt, parser) = ParseOption()
  if not (opt.source and opt.output and opt.include_guard_base):
    parser.print_help()
    print "SOURCE, OUTPUT and HEADER_GUARD_BASE need to be specified."
    return 1

  interfaces = ParseFile(opt.source)
  GenerateFile(os.path.normpath(opt.output), interfaces, opt.include_guard_base)
  return 0


if __name__ == "__main__":
  main()
