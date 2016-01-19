/*****************************************************************************************[GenPCE.h]
GenPCE -- Copyright (c) 2016, Ruben Martins, Martin Brain, Liana Hadarean, Daniel Kroening
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
************************************************************************************************/

#ifndef GenPCE_h
#define GenPCE_h

#include "core/Solver.h"
#include <algorithm>
#include <set>
#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include <queue>
#include <cstdlib>
#include <map>

typedef Minisat::Solver Solver;
typedef Minisat::vec<Minisat::Lit> MinVec;
typedef std::vector<Minisat::Lit> StdVec;
typedef Minisat::Lit Lit;
typedef Minisat::Var Var;

class GenPCE {
 public:
	
  GenPCE(Solver *_reference, Solver *_optimal, MinVec& _inputs,
		bool _minimal, bool _mus, bool _locking, bool _random, 
    bool _greedy, int _seed, bool _print) {
    reference = _reference;
    optimal = _optimal;
    _inputs.copyTo(inputs);
    minimal = _minimal;
    mus = _mus;
    print = _print;
    locking = _locking;
    n_minimize_core = 0;
    random = _random;
    seed = _seed;
    greedy = _greedy;
  }
  virtual ~GenPCE(){};

  bool checkOptimal(bool naive = false); 
  void buildOptimal(bool print = true);
  void greedyOptimization();

  static int randomGenerator(int i) { return std::rand()%i; }

 protected:
  struct assignment {
    StdVec pa;
    StdVec core;
  };

  struct GreaterThanBySize {
    bool operator()(const assignment& a, const assignment& b) const {
      if (a.core.size() == b.core.size())
      	return a.pa.size() > b.pa.size();
      else
      	return a.core.size() > b.core.size();
    }
  };

  std::priority_queue<assignment, std::vector<assignment>, GreaterThanBySize> 
    assignment_heap;

  void extendAssignment(Solver* solver, StdVec& pa);
  void convert(const StdVec& pa, MinVec& assumptions);
  bool propagate(Minisat::Solver* s, const StdVec& pa);
  bool inTrail(Solver* s, Lit p, bool lock = true);

  void printVec(const std::string type, const MinVec &pa, bool print = false);
  void printVec(const std::string type, const StdVec &pa, bool print = false);
  void printVec(const std::string type, const std::vector<int> &pa, bool print = false);

  bool solve(Solver * s, Solver * s_opt, const assignment &assign, Lit p);
  int minimize(Solver *s, bool print = false);
  bool minimizeCore(Solver* s, MinVec& assumptions);

  void printStats(const std::string type, Solver * s);

  int toInt(Lit p) { 
    int x = Minisat::var(p)+1; 
    if (Minisat::sign(p)) x = -x;
    return x;
  }

  Solver * reference;
  Solver * optimal;

  bool minimal;
  bool mus;
  bool print;
  bool locking;
  bool random;
  int seed;
  bool greedy;

  MinVec inputs;
  std::set<StdVec> db_clauses;
  std::set<std::vector<int> > db_assignments;

  unsigned n_minimize_core;

};

#endif
