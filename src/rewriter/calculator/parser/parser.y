%token_type { double }

%left  PLUS MINUS.
%left  DIVIDE TIMES MOD.
%right POW NOT.

%include {
#include <assert.h>
#ifdef OS_WINDOWS
#include <float.h>
#endif
#include <string.h>
#include <math.h>

namespace {
bool IsFinite(double x) {
#ifdef OS_WINDOWS
  return _finite(x);
#else
  return isfinite(x);
#endif
}
}  // anonymous namespace

#include "parser.h"

struct Result {
  enum ErrorType {
    NOT_ACCEPTED,
    SYNTAX_ERROR,
    DIVIDE_BY_ZERO,
    STACK_OVERFLOW,
    RESULT_OVERFLOW,
    PARSE_ERROR,
    UNKNOWN_ERROR,
    ACCEPTED
  };

  double value;
  ErrorType error_type;

  Result() : value(0), error_type(NOT_ACCEPTED) {}
  ~Result() {}

  void CheckValue(double val) {
    if (!IsFinite(val)) {
      error_type = RESULT_OVERFLOW;
    }
  }
};

}

%extra_argument { Result *result }

%parse_failure {
  result->error_type = Result::PARSE_ERROR;
}

%stack_overflow {
  result->error_type = Result::STACK_OVERFLOW;
}

%syntax_error {
  result->error_type = Result::SYNTAX_ERROR;
}

%parse_accept {
  if (result->error_type == Result::NOT_ACCEPTED) {
    result->error_type = Result::ACCEPTED;
  }
}

program ::= expr(A). {
  if (result->error_type == Result::NOT_ACCEPTED &&
      IsFinite(A)){
    result->value = A;
  } else {
    result->error_type = Result::RESULT_OVERFLOW;
  }
}

expr(A) ::= expr(B) MINUS  expr(C). {
  A = B - C;
  result->CheckValue(A);
}
expr(A) ::= expr(B) PLUS   expr(C). {
  A = B + C;
  result->CheckValue(A);
}
expr(A) ::= expr(B) TIMES  expr(C). {
  A = B * C;
  result->CheckValue(A);
}
expr(A) ::= MINUS  expr(B). [NOT] { A = - B; }
expr(A) ::= PLUS   expr(B). [NOT] { A = B; }
expr(A) ::= expr(B) POW expr(C). {
  A = pow(B, C);
  result->CheckValue(A);
}
expr(A) ::= LP expr(B) RP. { A = B; }
expr(A) ::= expr(B) DIVIDE expr(C). {
  if (C != 0.0) {
    A = B / C;
    result->CheckValue(A);
  } else {
    result->error_type = Result::DIVIDE_BY_ZERO;
  }
}
expr(A) ::= expr(B) MOD expr(C). {
  if (C != 0.0) {
    A = fmod(B, C);
    result->CheckValue(A);
  } else {
    result->error_type = Result::DIVIDE_BY_ZERO;
  }
}

expr(A) ::= INTEGER(B). { A = B; }
