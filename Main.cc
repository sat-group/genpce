/*****************************************************************************************[Main.cc]
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

#include <assert.h>
#include <iostream>

#include <zlib.h>

#include "core/Dimacs.h"
#include "core/Solver.h"
#include "GenPCE.h"

typedef Minisat::Solver Solver;
typedef Minisat::BoolOption BoolOption;
typedef Minisat::vec<Minisat::Lit> MinVec;

void printFileStats(int argc, char **argv, 
                    Solver &reference, Solver &optimal,
                    MinVec &inputs) {
  printf("c :: reference filename :: %s\n",argv[1]);
  printf("c :: reference encoding :: %d\n",reference.nClauses());
  if (argc > 2) {
    printf("c :: original filename :: %s\n",argv[2]);    
    printf("c :: original encoding :: %d\n",optimal.nClauses());
  }
  printf("c i ");
  for (int i = 0; i < inputs.size(); i++) {
    printf("%d ",Minisat::var(inputs[i])+1);
  }
  printf("0\n");
}

int parse_DIMACS(int argc, char **argv, Solver &reference, Solver &optimal) {

  gzFile gz_reference = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
  if (gz_reference == NULL) {
    std::cerr << "Could not open file : "
	      << ((argc == 1) ? "<stdin>" : argv[1])
	      << std::endl;
    return 1;
  }

  Minisat::parse_DIMACS(gz_reference, reference);
  gzclose(gz_reference);

  if (argc > 2) {
    gzFile gz_optimal = gzopen(argv[2], "rb");
    if (gz_optimal == NULL) {
      std::cerr << "Could not open file : " << argv[2]
		<< std::endl;
      return 1;
    }
    Minisat::parse_DIMACS(gz_optimal, optimal);
    gzclose(gz_optimal);
  }

  // Both solvers should have the same vocabulary
  while (optimal.nVars() < reference.nVars())
    optimal.newVar();

  return 0;
}

void updateVariables (Solver& reference, MinVec& inputs) {
  reference.loadIO(inputs);
}

int main (int argc, char **argv) {

  Solver reference;
  Solver optimal;
  MinVec inputs;

  BoolOption mus("GenPCE", "mus", "Minimizes the unsat core.\n",false);
  BoolOption minimal_lock("GenPCE","minimal-lock",
			  "Minimizes the encoding by locking reasons.\n",false);
  BoolOption minimal("GenPCE", "minimal", "Minimizes the encoding.\n",false);
  BoolOption check("GenPCE", "optimal", 
                   "Checks if an encoding is optimal.\n",false);
  BoolOption check_naive("GenPCE", "optimal-naive", 
			 "Checks if an encoding is optimal.\n",false);
  BoolOption print("GenPCE", "print", "Prints debug information.\n",false);
  BoolOption random("GenPCE", "random", "Uses a random seed.\n",false);
  Minisat::IntOption seed("GenPCE", "seed",
			  "Random seed number.\n",91648253, Minisat::IntRange(1, INT32_MAX));
  BoolOption greedy("GenPCE", "greedy", "Minimises the encoding with a greedy auxiliarly variable manager.\n",false);

  Minisat::parseOptions(argc, argv, true);

  if (argc == 1) {
    std::cerr << "Reading from standard input... Use '--help' for help."
	      << std::endl
	      << std::flush;
  }

  if (parse_DIMACS(argc, argv, reference, optimal)) return 1;
  updateVariables(reference, inputs);
  printFileStats(argc, argv, reference, optimal, inputs);

  GenPCE finder(&reference, &optimal, inputs, 
                       minimal, mus, minimal_lock, random, 
                       greedy, (int)seed, print);
  if (check || check_naive) {
    if (finder.checkOptimal(check_naive)) {
      std::cout << "c :: OPTIMAL ENCODING" << std::endl;
      return 10;
    } else {
      std::cout << "c :: NOT OPTIMAL ENCODING" << std::endl; 
      return 20;
    }
  } else {
    if (greedy)
      finder.greedyOptimization();
    else 
      finder.buildOptimal(true);
  }

  return 10;
}
