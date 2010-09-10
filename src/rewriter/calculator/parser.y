%token_type { double }

%left  PLUS MINUS.
%left  DIVIDE TIMES MOD.
%right POW NOT.

%include {
#include <assert.h>
#include <string.h>
#include <cmath>

// Visual C++ does not support std::isnormal, which comes from C99.
#if defined(_MSC_VER)
#include <cfloat>
namespace std {
bool isnormal(double x) {
  const int type = _fpclass(x);
  const bool is_negative_normal = (type == _FPCLASS_NN);
  const bool is_positive_normal = (type == _FPCLASS_PN);
  return (is_negative_normal || is_positive_normal);
}
}
#endif

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
      (A == 0.0 || std::isnormal(A))){
    result->value = A;
  } else {
    result->error_type = Result::RESULT_OVERFLOW;
  }
}

expr(A) ::= expr(B) MINUS  expr(C). { A = B - C; }
expr(A) ::= expr(B) PLUS   expr(C). { A = B + C; }
expr(A) ::= expr(B) TIMES  expr(C). { A = B * C; }
expr(A) ::= MINUS  expr(B). [NOT] { A = - B; }
expr(A) ::= PLUS   expr(B). [NOT] { A = B; }
expr(A) ::= expr(B) POW expr(C). { A = std::pow(B, C); }
expr(A) ::= LP expr(B) RP. { A = B; }
expr(A) ::= expr(B) DIVIDE expr(C). {
  if (C != 0.0) {
    A = B / C;
  } else {
    result->error_type = Result::DIVIDE_BY_ZERO;
  }
}
expr(A) ::= expr(B) MOD expr(C). {
  if (C != 0.0) {
    A = std::fmod(B, C);
  } else {
    result->error_type = Result::DIVIDE_BY_ZERO;
  }
}

expr(A) ::= INTEGER(B). { A = B; }
