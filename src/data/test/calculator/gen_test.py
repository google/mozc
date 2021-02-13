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

"""Generates test cases for mozc/rewriter/calculator/calculator_test.cc"""

# Usage example:
#   $ python gen_test.py --size=5000 --test_output=testset.txt
#   $ python gen_test.py --test_output=testset.txt --cc_output=test.cc
# To see other options available, type python gen_test.py --help
#
# This script randomly generates a number of test cases used in
# mozc/rewriter/calculator/calculator_test.cc. Each test case is written as a
# line in the form of expr=ans, where expr is an expression that may involve
# Japanese characters, and ans is the correct solution, in sufficiently
# accurate precision, to be calculated by mozc.  In case where expr is
# incomputable due to, e.g., overflow and/or division-by-zero, ans is
# empty. It's expected that the mozc calculator rejects such expressions.
#
# Since a lot of expressions are generated at random, to guarantee that the
# values of ans are really correct, the script itself does "test-for-test"
# using python's eval() function. Namely, after building an expression tree,
# we generate a python expression corresponding to the tree and compare its
# value with that of the direct evaluation. This test runs automatically
# inside the script, but you can also write it to a file by specifying
# --py_output option. Furthermore, passing --cc_output options enables you to
# generate C++ code, respectively, that warns about expressions if results in
# C++ differ from those of test cases.  Thus, generated test cases are
# reliable. (Having said that, there still remains precision issue. For
# example, the same python expression leads to different result on 64-bit
# Linux and Mac. Thus, the accuracy of results is checked either by absolute
# error or relative error in check program; see implementation of
# TestCaseGenerator for details.)
#
# When generating string representation of an expression tree, we should put
# parentheses very carefully because, even if we know that changing evaluation
# order never affects the result mathematically, like the associativity of +
# operator, the result may slightly change in computer due to precision.  That
# may cause an error in unit test because the mozc parser and the python
# eval() parser may build expression trees that are mathematically equivalent
# but have different order of computation. Thus, we put parentheses as safely
# as possible so that the same order of computations are guaranteed in mozc
# and python.
#
# *** NOTE ON PRECISION AND ORDER OF COMPUTATION ***
#
# Since the mozc just uses double to store values at each computational step
# and uses std::pow and std::fmod from cmath, plus since the script may
# generate long, complicated expressions with very large and very small
# values, the order of computation becomes a real issue. To see this, first
# take a look at the following two expressions from python:
#
#   math.fmod(math.pow((5231*6477/+(1252.18620676)),
#                      (+(math.fmod(6621, -1318)))),
#             (-(5389 + -8978)) + (1698 + ((3140.89975168)
#              + (-(math.fmod(97.705869081900005, -322.0))))))
#     = 3728.1155588589027
#
#   math.fmod(math.pow((5231*6477/+(1252.18620676)),
#                      (+(math.fmod(6621, -1318)))),
#             (-(5389 + -8978) + 1698 + 3140.89975168
#              + -(math.fmod(97.705869081900005, -322.0))))
#     = 3980.9098289592912
#
# Looking like the same expression but the results look totally different. You
# will find out that only difference is the order of additions because of
# parentheses and those are mathematically equivalent. If we proceed
# calculation in python, we arrive at:
#
#   big = 2.5175653147668723e+137
#   math.fmod(big, 8330.1938825980997) = 3728.1155588589027
#   math.fmod(big, 8330.1938825981015) = 3980.9098289592912
#
# The difference of the second values is really small, but since the first
# argument, big, is enormously large, the math.fmod ends with really different
# results.  Cases like this example rarely occurs, but really does. Indeed,
# the first expression was generated by this script and calculated by python,
# but mozc calculated as in the second one. To avoid such situations, the
# current script puts parentheses as carefully and safely as possible.  If
# such a case happens, it's really tough to track bugs...

import codecs
import logging
import math
import optparse
import random
import sys


class Error(Exception):
  """Base class for all the exceptions in this script."""
  pass

class EvalError(Error):
  """Raised on failure of the evaluation of expression tree, e.g., overflow
  and division-by-zero, etc."""
  pass

class FatalError(Error):
  """Raised, e.g., when the evaluation result of expression tree is different
  from that of python's eval() function."""
  pass


def is_finite(x):
  """Checks if x is finite real number."""
  return not (math.isnan(x) or math.isinf(x))


class Expr(object):
  """Base class for nodes of expression tree, like number, addition, etc."""
  def __init__(self):
    pass

  def contains_operator(self):
    """Checks whether the expression has at least one operator, such as
    addition, multiplication, etc."""
    raise FatalError('Expr.contains_operator called')

  def eval(self):
    """Evaluates the expression tree hanging from this node and returns the
    result.  Raises EvalError if the evaluation fails due to overflow,
    division-by-zero, etc.
    """
    raise FatalError('Expr.eval called')

  def build_test_expr(self):
    """Returns a string representation of the tree for test cases."""
    raise FatalError('Expr.build_test_expr called')

  def build_python_expr(self):
    """Returns a python expression of the tree that can be evaluated by
    python's eval()."""
    raise FatalError('Expr.build_python_expr called')

  def build_cc_expr(self):
    """Returns a C++ expression of the tree that can be compiled by C++
    compilers."""
    raise FatalError('Expr.build_cc_expr called')


class Number(Expr):
  """Base class for number nodes (i.e., levaes).

  Attribute:
      _value: the value of the number.
  """
  def __init__(self, value):
    Expr.__init__(self)
    self._value = value

  def contains_operator(self):
    return False

  def eval(self):
    return float(self._value)


class Integer(Number):
  """Integer node, e.g., 10, 20, -30, etc."""

  def __init__(self, value):
    assert(isinstance(value, int))
    Number.__init__(self, value)

  def build_test_expr(self):
    return str(self._value)

  def build_python_expr(self):
    return '%d.0' % self._value

  def build_cc_expr(self):
    return 'static_cast<double>(%d)' % self._value


class Float(Number):
  """Floating-point number node, e.g., 3.14, 2.718, etc."""

  def __init__(self, value):
    assert(isinstance(value, float))

    # Since values are created from string in mozc, we once convert the value
    # to a string and then re-convert it to a float, for just in case.
    self._value_str = str(value)
    Number.__init__(self, float(self._value_str))

  def build_test_expr(self):
    return self._value_str

  def build_python_expr(self):
    return repr(self._value)  # E.g., 2.4 -> 2.3999999999999999

  def build_cc_expr(self):
    return repr(self._value)


class Group(Expr):
  """Grouped expression: (Expr)

  Attribute:
      _expr: an expression (Expr object) inside the parentheses.
  """
  def __init__(self, expr):
    Expr.__init__(self)
    self._expr = expr

  def contains_operator(self):
    # A pair of parentheses is not treated as an operator in mozc.
    return self._expr.contains_operator()

  def eval(self):
    return self._expr.eval()

  def build_test_expr(self):
    return '(%s)' % self._expr.build_test_expr()

  def build_python_expr(self):
    return '(%s)' % self._expr.build_python_expr()

  def build_cc_expr(self):
    return '(%s)' % self._expr.build_cc_expr()


class UnaryExpr(Expr):
  """Base class for unary operators.

  This is an abstract class: two functions, _unary_func and _op_symbol must be
  implemented in each subclass.

  Attribute:
      _expr: an Expr object to which the operator applied.
  """
  def __init__(self, expr):
    Expr.__init__(self)
    self._expr = expr

  def contains_operator(self):
    # Unary operators are treated as operator in mozc.
    return True

  @classmethod
  def _op_symbol(cls):
    """Returns a character representation of this operator, like + and -."""
    raise FatalError('UnaryExpr._op_symbol called')

  @classmethod
  def _unary_func(cls, x):
    """Evaluates the unary operator (i.e., this function is an underlying
    univariate function for the operator).

    Arg:
        x: a float to which the operator applied.
    """
    raise FatalError('UnaryExpr._unary_func called')

  def eval(self):
    value = self._unary_func(self._expr.eval())
    if not is_finite(value):
      raise EvalError('%s(%f) [overflow]' % (self._op_symbol(), value))
    return value

  def build_test_expr(self):
    # If the child expression is one of the following, then we can omit
    # parentheses because of precedence.
    if (isinstance(self._expr, Number) or
        isinstance(self._expr, UnaryExpr) or
        isinstance(self._expr, Group)):
      format = '%s%s'
    else:
      format = '%s(%s)'
    return format % (self._op_symbol(), self._expr.build_test_expr())

  def build_python_expr(self):
    return '%s(%s)' % (self._op_symbol(), self._expr.build_python_expr())

  def build_cc_expr(self):
    return '%s(%s)' % (self._op_symbol(), self._expr.build_cc_expr())


class PositiveSign(UnaryExpr):
  """Positive sign node: +Expr"""

  def __init__(self, expr):
    UnaryExpr.__init__(self, expr)

  @classmethod
  def _op_symbol(cls):
    return '+'

  @classmethod
  def _unary_func(cls, x):
    return x


class Negation(UnaryExpr):
  """Negation operator node: -Expr"""

  def __init__(self, expr):
    UnaryExpr.__init__(self, expr)

  @classmethod
  def _op_symbol(cls):
    return '-'

  @classmethod
  def _unary_func(cls, x):
    return -x


class BinaryExpr(Expr):
  """Base class for binary operators.

  This is an abstract class: two functions, _op_symbol and _binary_func must
  be implemented in each subclass.

  Attributes:
      _left_expr: an expression (Expr object) on left side.
      _right_expr: an expression (Expr object) on right side.
  """
  def __init__(self, left_expr, right_expr):
    Expr.__init__(self)
    self._left_expr = left_expr
    self._right_expr = right_expr

  @classmethod
  def _op_symbol(cls):
    """Returns a character representation of this operator, like + and -."""
    raise FatalError('UnaryExpr._op_symbol called')

  @classmethod
  def _binary_func(cls, x, y):
    """Evaluates the unary operator (i.e., this function is an underlying
    bivariate function for the operator)."""
    raise FatalError('BinaryExpr._binary_func called')

  def contains_operator(self):
    return True

  def eval(self):
    left_value = self._left_expr.eval()
    right_value = self._right_expr.eval()
    if not (is_finite(left_value) and is_finite(right_value)):
      raise EvalError('%f %s %f [invalid values]'
                      % (left_value, self._op_symbol(), right_value))
    value = self._binary_func(left_value, right_value)
    if not is_finite(value):
      raise EvalError('%f %s %f = %f [overflow or NaN]'
                      % (left_value, self._op_symbol(), right_value, value))
    return value

  def build_test_expr(self):
    if (isinstance(self._left_expr, Number) or
        isinstance(self._left_expr, UnaryExpr) or
        isinstance(self._left_expr, Group)):
      left_format = '%s'
    else:
      left_format = '(%s)'

    if (isinstance(self._right_expr, Number) or
        isinstance(self._right_expr, UnaryExpr) or
        isinstance(self._right_expr, Group)):
      right_format = '%s'
    else:
      right_format = '(%s)'

    format = left_format + '%s' + right_format
    return format % (self._left_expr.build_test_expr(),
                     self._op_symbol(),
                     self._right_expr.build_test_expr())

  def build_python_expr(self):
    return '(%s) %s (%s)' % (self._left_expr.build_python_expr(),
                             self._op_symbol(),
                             self._right_expr.build_python_expr())

  def build_cc_expr(self):
    return '(%s) %s (%s)' % (self._left_expr.build_cc_expr(),
                             self._op_symbol(),
                             self._right_expr.build_cc_expr())

class Addition(BinaryExpr):
  """Addition: Expr1 + Expr2"""

  @classmethod
  def _op_symbol(cls):
    return '+'

  @classmethod
  def _binary_func(cls, x, y):
    return x + y

  def __init__(self, left_expr, right_expr):
    BinaryExpr.__init__(self, left_expr, right_expr)


class Subtraction(BinaryExpr):
  """Subtraction: Expr1 - Expr2"""

  def __init__(self, left_expr, right_expr):
    BinaryExpr.__init__(self, left_expr, right_expr)

  @classmethod
  def _op_symbol(cls):
    return '-'

  @classmethod
  def _binary_func(cls, x, y):
    return x - y


class Multiplication(BinaryExpr):
  """Multiplication: Expr1 * Expr2"""

  def __init__(self, left_expr, right_expr):
    BinaryExpr.__init__(self, left_expr, right_expr)

  @classmethod
  def _op_symbol(cls):
    return '*'

  @classmethod
  def _binary_func(cls, x, y):
    return x * y


class Division(BinaryExpr):
  """Division: Expr1 / Expr2"""

  def __init__(self, left_expr, right_expr):
    BinaryExpr.__init__(self, left_expr, right_expr)

  @classmethod
  def _op_symbol(cls):
    return '/'

  @classmethod
  def _binary_func(cls, x, y):
    try:
      return x / y
    except ZeroDivisionError:
      raise EvalError('%f / %f  [division by zero]' % (x, y))


class Power(BinaryExpr):
  """Power: Expr1 ^ Expr2 or Expr1 ** Expr2"""

  def __init__(self, left_expr, right_expr):
    BinaryExpr.__init__(self, left_expr, right_expr)

  @classmethod
  def _op_symbol(cls):
    return '^'

  @classmethod
  def _binary_func(cls, x, y):
    try:
      return math.pow(x, y)
    except OverflowError:
      raise EvalError('%f ^ %f [overflow]' % (x, y))
    except ValueError:
      raise EvalError('%f ^ %f [math.pow error]' % (x, y))

  def build_test_expr(self):
    # Note: we cannot use the default implementation of this function from
    # BinaryExpr because mozc interprets, e.g., -3^2 as -(3^2), which means
    # that if left expr is a number but negative, we need to put parentheses.
    if not (isinstance(self._left_expr, Group)):
      format = '(%s)^'
    else:
      format = '%s^'

    if not (isinstance(self._right_expr, Number) or
            isinstance(self._right_expr, UnaryExpr) or
            isinstance(self._right_expr, Group)):
      format += '(%s)'
    else:
      format += '%s'

    return format % (self._left_expr.build_test_expr(),
                     self._right_expr.build_test_expr())

  def build_python_expr(self):
    return 'math.pow(%s, %s)' % (self._left_expr.build_python_expr(),
                                 self._right_expr.build_python_expr())

  def build_cc_expr(self):
    return 'pow(%s, %s)' % (self._left_expr.build_cc_expr(),
                            self._right_expr.build_cc_expr())


class Modulo(BinaryExpr):
  """Modulo for floats: Expr1 % Expr2, or fmod(Expr1, Expr2)"""

  def __init__(self, left_expr, right_expr):
    BinaryExpr.__init__(self, left_expr, right_expr)

  @classmethod
  def _op_symbol(cls):
    return '%'

  @classmethod
  def _binary_func(cls, x, y):
    try:
      return math.fmod(x, y)
    except ValueError:  #  Raised by calling math.fmod(x, 0.0)
      raise EvalError('%f %% %f [math.fmod error]' % (x, y))

  def build_python_expr(self):
    return 'math.fmod(%s, %s)' % (self._left_expr.build_python_expr(),
                                  self._right_expr.build_python_expr())

  def build_cc_expr(self):
    return 'fmod(%s, %s)' % (self._left_expr.build_cc_expr(),
                             self._right_expr.build_cc_expr())


class RandomExprBuilder(object):
  """Random expression tree builder.

  Randomly builds an expression tree using recursive calls.

  Attributes:
      _max_abs_val: maximum absolute values of each number in expression.
      _factories: a list of factories used to constract expression nodes.
  """
  def __init__(self, max_abs_val):
    self._max_abs_val = max_abs_val

    # Default factory is only self._build_expr, which means that only number
    # nodes are generated; see the implementation of self._build_expr.
    self._factories = [lambda n: self._build_expr(n)]

  def add_operators(self, string):
    """Adds operators for node candidates.

    For example, to generate expressions with four arithmetic operations (+,
    -, *, /), call this method like: builder.add_operators('add,sub,mul,div')

    Arg:
        string: operator names, each separated by comma, without spaces. The
          correspondence between operators and their names are as follows:
          group: (expr)
            pos: +expr
            neg: -expr
            add: expr1 + expr2
            sub: expr1 - expr2
            mul: expr1 * expr2
            div: expr1 / expr2
            pow: expr1 ^ expr2
            mod: expr1 % expr2
    """
    factories = {'group': lambda n: Group(self._build_expr(n)),
                 'pos': lambda n: PositiveSign(self._build_expr(n)),
                 'neg': lambda n: Negation(self._build_expr(n)),
                 'add': lambda n: Addition(self._build_expr(n),
                                           self._build_expr(n)),
                 'sub': lambda n: Subtraction(self._build_expr(n),
                                              self._build_expr(n)),
                 'mul': lambda n: Multiplication(self._build_expr(n),
                                                 self._build_expr(n)),
                 'div': lambda n: Division(self._build_expr(n),
                                           self._build_expr(n)),
                 'pow': lambda n: Power(self._build_expr(n),
                                        self._build_expr(n)),
                 'mod': lambda n: Modulo(self._build_expr(n),
                                         self._build_expr(n))}
    for op in string.split(','):
      self._factories.append(factories[op])

  def build(self, max_depth):
    """Builds a random expression tree.

    This method generates only those expressions that include at least one
    operator, because mozc doesn't calculates expressions without operator,
    e.g. (123)=.

    Arg:
        max_depth: the maximum possible height of tree.
    """
    for i in range(100):
      expr = self._build_expr(max_depth)
      if expr.contains_operator():
        return expr
    else:
      raise FatalError('Failed to build expression containing '
                       'at least one operator many times')

  def _build_expr(self, max_depth):
    """Implementation of recursive tree construction algorithm."""
    if max_depth == 0:  # Leaf
      if random.randint(0, 1) == 0:
        return Integer(random.randint(-self._max_abs_val, self._max_abs_val))
      else:
        return Float(random.uniform(-self._max_abs_val, self._max_abs_val))

    # Select a node factory at random and generate childs.
    factory = random.choice(self._factories)
    return factory(max_depth - 1)


class TestCaseGenerator(object):
  """Test case generator for expression trees.  Generates test cases for
  expressions by mixinig japanese fonts to expressions.

  Attributes:
      _filename: filename of output file
      _file: file object to which test cases are written.
      _num_total_cases: total number of written expressions.
      _num_computable_cases: number of computable expressions.
  """

  # Character map used to generate test expression including Japanese.
  _EQUIVALENT_CHARS = {'+': ['+', u'＋'],
                       '-': ['-', u'−', u'ー'],
                       '*': ['*', u'＊'],
                       '/': ['/', u'／', u'・'],
                       '^': ['^'],
                       '%': ['%', u'％'],
                       '(': ['(', u'（'],
                       ')': [')', u'）'],
                       '0': ['0', u'０'],
                       '1': ['1', u'１'],
                       '2': ['2', u'２'],
                       '3': ['3', u'３'],
                       '4': ['4', u'４'],
                       '5': ['5', u'５'],
                       '6': ['6', u'６'],
                       '7': ['7', u'７'],
                       '8': ['8', u'８'],
                       '9': ['9', u'９']}

  def __init__(self, test_filename, py_filename = '', cc_filename = ''):
    """
    Arg:
        filename: output file. When it's empty, generates nothing.
    """
    self._num_total_cases = 0
    self._num_computable_cases = 0

    # Initialize output file
    self._test_filename = test_filename
    if test_filename:
      self._test_file = codecs.getwriter('utf-8')(open(test_filename, 'wb'))
    else:
      # Replace the generating function by a dummy
      self.add_test_case_for = lambda expr: None
      self._test_file = None

    # Initialize python code
    if py_filename:
      self._py_file = codecs.getwriter('utf-8')(open(py_filename, 'wb'))
      self._py_file.write('import math\n\n')
    else:
      self._add_py_code_for = lambda py_expr, expected: None
      self._py_file = None

    # Initialize cc code
    if cc_filename:
      self._cc_file = codecs.getwriter('utf-8')(open(cc_filename, 'wb'))
      self._cc_file.write('// Automatically generated by '
                          'mozc/src/data/test/calculator/gen_test.py\n\n'
                          '#include <cmath>\n'
                          '#include <iostream>\n'
                          '#include <string>\n\n'
                          'using namespace std;\n\n')
    else:
      # Replace the generating function by a dummy
      self._add_cc_code_for = lambda cc_expr, expectecd: None
      self._cc_file = None

  def __del__(self):
    if self._test_file:
      self._test_file.close()
      logging.info('%d test cases were written to %s, '
                   'of which %d can be calculated.',
                   self._num_total_cases, self._test_filename,
                   self._num_computable_cases)

    if self._py_file:
      self._py_file.close()

    if self._cc_file:
      self._cc_file.write('int main(int, char**) {\n')
      for i in range(self._num_total_cases):
        self._cc_file.write('  test%d();\n' % i)
      self._cc_file.write('  return 0;\n'
                          '}\n')
      self._cc_file.close()

  @staticmethod
  def _mix_japanese_string(string):
    """Randomly transforms half-width characters to full-width."""
    result = u''
    for char in string:
      if char in TestCaseGenerator._EQUIVALENT_CHARS:
        equiv_chars = TestCaseGenerator._EQUIVALENT_CHARS[char]
        result += random.choice(equiv_chars)
      else:
        result += char
    return result

  def add_test_case_for(self, expr):
    """Appends the code that checks whether the evaluation result of given
    expr coincides with the epxected result.

    Args:
        expr: Expr object
    """
    test_expr = self._mix_japanese_string(expr.build_test_expr())
    try:
      # Raises an exceeption on failure, like divison-by-zero, etc.
      value = expr.eval()

      # If the above evaluation was a success, the same value should be
      # calculated by python's eval. This check should be done in absolute
      # error because the script runs on the same machine.
      py_expr = expr.build_python_expr()
      py_value = eval(py_expr)
      if not (abs(value - py_value) < 1e-8):
        logging.critical('Calculation differs from python: %f != %f (python)\n'
                         'test: %s\n'
                         '  py: %s', value, py_value, test_expr, py_expr)
        raise FatalError('Expression tree evaluation error')

      self._num_computable_cases += 1
      self._test_file.write(u'%s=%.8g\n' % (test_expr, value))
      self._add_py_code_for(py_expr, value)
      self._add_cc_code_for(expr.build_cc_expr(), value)
    except EvalError:
      self._test_file.write(u'%s=\n' % test_expr)
      self._add_cc_code_for(expr.build_cc_expr(), None)

    self._num_total_cases += 1

  def _add_py_code_for(self, py_expr, expected):
    """Appends python code that checks whether the evaluation result of given
    expr coincides with the epxected result.

    If expected is None, it indicates that the evaluation of expr results in
    error (like overflow and division-by-zero). Currently, just generates
    comments for such cases.

    In generated scrpt, the accuracy is verified either in absolute error or
    relative error, because there's a possibility that different machines
    generate different values due to precision. For example, if the expected
    value is very large, we cannot expect that error is less than a certain
    small threshold. In this case, however, the relative error would work.

    Args:
        py_expr: string of python expression.
        expected: expected value of the expression (float)
    """
    if expected:
      self._py_file.write('expr = u"%s"\n' % py_expr)
      self._py_file.write('expected = %s\n' % repr(expected))
      self._py_file.write('val = eval(expr)\n')
      self._py_file.write('err = abs(val - expected)\n')
      self._py_file.write('if (err > 1e-8 and\n')
      self._py_file.write('    err > 1e-2 * abs(expected)):\n')
      self._py_file.write('  print repr(val), "!=", repr(expected)\n')
      self._py_file.write('  print "expr =", expr\n\n')
    else:
      self._py_file.write('# Incomputable\n'
                          '# %s\n\n' % py_expr)

  def _add_cc_code_for(self, cc_expr, expected):
    """Appends the code that checks whether the evaluation result of given
    expr coincides with the epxected result.

    If expected is None, it indicates that the evaluation of expr results in
    error (like overflow and division-by-zero). Currently, just generates
    comments for such cases.

    In generated code, the accuracy is verified either in absolute error or
    relative error; see _add_py_code_for for details.

    Args:
        cc_expr: string of C++ expression.
        expected: expected value of the expression (float)
    """
    self._cc_file.write('void test%d() {\n' % self._num_total_cases)
    if expected:
      self._cc_file.write('  const string expr = string("%s");\n' % cc_expr)
      self._cc_file.write('  const double expected = %s;\n' % repr(expected))
      self._cc_file.write('  const double val = %s;\n' % cc_expr)
      self._cc_file.write('  const double err = abs(val - expected);\n')
      self._cc_file.write('  if (err > 1e-8 || err > 1e-2 * abs(expected))\n')
      self._cc_file.write('    cerr << val << " != " << expected\n')
      self._cc_file.write('         << "expr = " << expr << endl;\n')
    else:
      self._cc_file.write('  // Incomputable\n'
                          '  // %s\n' % cc_expr)
    self._cc_file.write('}\n\n')


def parse_options():
  parser = optparse.OptionParser()
  parser.add_option('--size', type = 'int', dest = 'size', default = 100,
                    metavar = 'NUM', help = 'Number of tests to be generated')
  parser.add_option('--max_depth', type = 'int', dest = 'max_depth',
                    default = 5, metavar = 'NUM',
                    help = 'Max depth of equation tree (default = 5)')
  parser.add_option('--max_abs', type = 'float', dest = 'max_abs',
                    default = 1e+4, metavar = 'FLOAT',
                    help = 'Max absolute value of each number (default = 1e+4)')
  parser.add_option('--operators', dest = 'operators', metavar = 'OPs',
                    default = 'group,pos,neg,add,sub,mul,div,pow,mod',
                    help = 'Operators used by expression generator')
  parser.add_option('--test_output', dest = 'test_output',
                    default = '', metavar = 'FILE',
                    help = 'Output test data file')
  parser.add_option('--py_output', dest = 'py_output',
                    default = '', metavar = 'FILE',
                    help = 'Output python script for verification (optional)')
  parser.add_option('--cc_output', dest = 'cc_output',
                    default = '', metavar = 'FILE',
                    help = 'Output C++ src for verification (optional)')
  options, args = parser.parse_args()

  if not options.test_output:
    logging.error('--test_output is empty')
    sys.exit(-1)

  return options


def main():
  sys.stdin = codecs.getreader('utf-8')(sys.stdin)
  sys.stdout = codecs.getwriter('utf-8')(sys.stdout)
  sys.stderr = codecs.getwriter('utf-8')(sys.stderr)
  random.seed()
  logging.basicConfig(level = logging.INFO,
                      format = '%(levelname)s: %(message)s')
  options = parse_options()

  if not options.test_output:
    logging.error('--test_output is empty')
    sys.exit(-1)

  builder = RandomExprBuilder(options.max_abs)
  builder.add_operators(options.operators)
  test_case_generator = TestCaseGenerator(options.test_output,
                                          options.py_output,
                                          options.cc_output)

  for i in range(options.size):
    expr = builder.build(options.max_depth)
    test_case_generator.add_test_case_for(expr)


if __name__ == '__main__':
  main()
