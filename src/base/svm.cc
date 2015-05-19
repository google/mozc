// Copyright 2010-2012, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/svm.h"
#include "base/base.h"

#ifdef OS_WINDOWS
#define NO_MINMAX
#endif

#include <cstring>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

namespace mozc {
namespace {
static const double kEPS = 0.1;
static const double kINF = 1e+37;

size_t GetDimension(const vector<vector<pair<int, double> > > &x) {
  int result = 0;
  for (size_t i = 0; i < x.size(); ++i) {
    for (size_t j = 0; j < x[i].size(); ++j) {
      result = max(x[i][j].first, result);
    }
  }
  return static_cast<size_t>(result + 1);
}
}  // namespace

bool SVM::Train(const vector<double> &y,  // label
                const vector<vector<pair<int, double> > > &x,
                double C,       // toralence parameter
                vector<double> *w) {
  if (x.size() != y.size()) {
    LOG(ERROR) << "Invalid data size";
    return false;
  }

  const size_t n = GetDimension(x);
  const size_t l = y.size();
  size_t active_size = l;
  double PGmax_old = kINF;
  double PGmin_old = -kINF;
  vector<double> QD(l);
  vector<size_t> index(l);
  vector<double> alpha(l);

  w->resize(n);
  fill(w->begin(), w->end(), 0.0);
  fill(alpha.begin(), alpha.end(), 0.0);

  for (size_t i = 0; i < l; ++i) {
    index[i] = i;
    QD[i] = 0;
    for (size_t j = 0; j < x[i].size(); ++j) {
      QD[i] += (x[i][j].second * x[i][j].second);
    }
  }

  static const size_t kMaxIteration = 5000;
  for (size_t iter = 0; iter < kMaxIteration; ++iter) {
    double PGmax_new = -kINF;
    double PGmin_new = kINF;
    random_shuffle(index.begin(), index.begin() + active_size);

    for (size_t s = 0; s < active_size; ++s) {
      const size_t i = index[s];
      double G = 0;

      for (size_t j = 0; j < x[i].size(); ++j) {
        G += (*w)[x[i][j].first] * x[i][j].second;
      }

      G = G * y[i] - 1;
      double PG = 0.0;

      if (alpha[i] == 0.0) {
        if (G > PGmax_old) {
          active_size--;
          swap(index[s], index[active_size]);
          s--;
          continue;
        } else if (G < 0.0) {
          PG = G;
        }
      } else if (alpha[i] == C) {
        if (G < PGmin_old) {
          active_size--;
          swap(index[s], index[active_size]);
          s--;
          continue;
        } else if (G > 0.0) {
          PG = G;
        }
      } else {
        PG = G;
      }

      PGmax_new = max(PGmax_new, PG);
      PGmin_new = min(PGmin_new, PG);

      if (fabs(PG) > 1.0e-12) {
        const double alpha_old = alpha[i];
        alpha[i] = min(max(alpha[i] - G/QD[i], 0.0), C);
        const double d = (alpha[i] - alpha_old)* y[i];
        for (size_t j = 0; j < x[i].size(); ++j) {
          (*w)[x[i][j].first] += d * x[i][j].second;
        }
      }
    }

    if (iter % 4 == 0) {
      cout << "." << flush;
    }

    if ((PGmax_new - PGmin_new) <= kEPS) {
      if (active_size == l) {
        break;
      } else {
        active_size = l;
        PGmax_old = kINF;
        PGmin_old = -kINF;
        continue;
      }
    }

    PGmax_old = PGmax_new;
    PGmin_old = PGmin_new;
    if (PGmax_old <= 0) {
      PGmax_old = kINF;
    }
    if (PGmin_old >= 0) {
      PGmin_old = -kINF;
    }
  }

  cout << endl;

  return true;
}
}  // namespace mozc
