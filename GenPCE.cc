/*****************************************************************************************[GenPCE.cc]
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

#include "GenPCE.h"

void GenPCE::printVec(const std::string type, 
			     const MinVec &pa, 
			     bool print) {
  if (print) {
    std::cout << type;
    for (int j = 0; j < pa.size(); ++j) {
      std::cout << (Minisat::sign(pa[j]) ? "-" : "") << Minisat::var(pa[j]) + 1 << ' ';
    }
    std::cout << std::endl;
  }
}

void GenPCE::printVec(const std::string type, 
			     const StdVec &pa, 
			     bool print) {
  if (print) {
    std::cout << type;
    for (unsigned j = 0; j < pa.size(); ++j) {
      std::cout << (Minisat::sign(pa[j]) ? "-" : "") << Minisat::var(pa[j]) + 1 << ' ';
    }
    std::cout << std::endl;
  }
}

void GenPCE::printVec(const std::string type, 
			     const std::vector<int> &pa, 
			     bool print) {
  if (print) {
    std::cout << type;
    for (unsigned j = 0; j < pa.size(); ++j) {
      std::cout << pa[j] << ' ';
    }
    std::cout << std::endl;
  }
}

void GenPCE::extendAssignment(Solver* solver, StdVec& pa) {
  // The order of implied literals in the trail are not necessarily at the end
  if (solver->getTrail().size() > (int)pa.size()) {
    pa.clear();
    for (int i = 0; i < solver->getTrail().size(); i++)
      pa.push_back(solver->getTrail()[i]);
  }
}

void GenPCE::convert(const StdVec& pa, MinVec& assumptions) {
  assumptions.clear();
  assumptions.growTo(pa.size());
  for (unsigned i = 0; i < pa.size(); i++)
    assumptions[i] = pa[i];
}

bool GenPCE::propagate(Minisat::Solver* s, const StdVec& pa) {
  MinVec assumptions;
  convert(pa, assumptions);
  bool res = s->up(assumptions);
  return res;
}

bool GenPCE::inTrail(Solver* s, Lit p, bool lock) {
  for (int i = 0; i < s->getTrail().size(); i++) {
    Var v = var(s->getTrail()[i]);
    if (v == Minisat::var(p)){
      if(lock) s->lock_reason(v);
      return true;
    }
  }
  return false;
}

bool GenPCE::minimizeCore(Solver* s, MinVec& assumptions) {
  
  MinVec core;
  for (int i = 0; i < assumptions.size(); i++)
    core.push(~assumptions[i]);
  assumptions.clear();

  std::vector<bool> removed(core.size(), false);
  for (int i = 0; i < core.size(); i++) {
    MinVec current;
    for (int j = 0; j < core.size(); j++){
      if (removed[j] || i == j)
        continue;

      current.push(core[j]);
    }

    bool res = s->solve(current);
    if (!res) {
      removed[i] = true;
    }
  }
  for (int i = 0; i < core.size(); i++)
    if (!removed[i]) assumptions.push(~core[i]);

  return (core.size() > assumptions.size());
}


int GenPCE::minimize(Minisat::Solver *s, bool print) {
  Solver min;
  int o_vars = s->copySolver(min);
  min.setAssumptions(o_vars);

  std::vector<bool> redundant(s->nClauses(), false);
  int n_redundant = 0;
  
  for (int i = 0; i < min.nClauses(); i++) {
    if (min.is_locked(i))
      continue;

    Minisat::vec<Minisat::Lit> clause;
    min.getClause(i, clause);
    Minisat::Lit disable = Minisat::lit_Undef;
    int implications = 0;
    for (int j = 0; j < clause.size(); j++) {
      Minisat::vec<Minisat::Lit> assumptions;
      Minisat::Lit p = clause[j];
      if (Minisat::var(p) >= o_vars) continue;
      for (int z = 0; z < clause.size(); z++) {
        if (j == z) continue;
        if (Minisat::var(clause[z]) >= o_vars) {
          disable = clause[z];
          assumptions.push(clause[z]);
        } else {
          assumptions.push(~clause[z]);
        }
      }
      assert (disable != Minisat::lit_Undef);
      // Disable the remaining assumptions
      for (int w = 0; w < min.nClauses(); w++){
        Minisat::Lit x = Minisat::mkLit(w+o_vars,true);
        if (Minisat::var(disable) != Minisat::var(x)){
          if (!redundant[w])
            assumptions.push(x);  
          else
            assumptions.push(~x);  
        }
      }
      min.up(assumptions);
      if(inTrail(&min, p, locking)) implications++;
    }
    if (implications == clause.size()-1) {
      n_redundant++;
      redundant[i] = true;
    } else {
      if (locking)
        min.undo_locked();
    }
    if (locking)
      min.clear_locked();
  }

  /*
  int m_clauses = 0;

  for (unsigned i = 0; i < redundant.size(); i++) {
    if (redundant[i] && !min.is_locked(i))
      continue;

    m_clauses++;
  }
  */
  if (print) {
    //printf("c :: optimal encoding :: %d\n",min.nClauses()+min.nUnits());
    printStats("optimal encoding", &min);
    printf("c :: optimal minimal encoding :: %d\n",min.nClauses()-n_redundant+min.nUnits());


    // Print formula
    printf("p cnf %d %d\n",o_vars,min.nClauses()-n_redundant+min.nUnits());
    min.printUnits();

    for (unsigned i = 0; i < redundant.size(); i++) {
      if (redundant[i] && !min.is_locked(i))
        continue;

      min.printClause(i);
    }
  }


  return min.nClauses()-n_redundant+min.nUnits();
}

void GenPCE::greedyOptimization() {

  printStats("reference encoding", reference);

  std::vector<bool> aux_vars(reference->nVars(), true);
  for (int i = 0; i < inputs.size(); ++i) {
    aux_vars[Minisat::var(inputs[i])] = false;
  }

  MinVec aux_inputs;
  MinVec copy_inputs;
  inputs.copyTo(copy_inputs);

  int iteration = 1;
  buildOptimal(false);
  int cost = minimize(optimal, false);
  std::cout << "c Iteration: " << iteration << "\t MinCls: " << cost << std::endl;
  
  while (true) {

    int begin_cost = cost;
    int pos = 0;

    for (int i = 0; i < reference->nVars(); ++i){
      if (!aux_vars[i]) 
        continue;
      
      db_assignments.clear();
      inputs.clear();
      copy_inputs.copyTo(inputs);
      inputs.push(Minisat::mkLit(i,false));
      Solver * tmp = new Solver();
      for (int j = 0 ; j < optimal->nVars(); j++)
        tmp->newVar();
      optimal = tmp;

      buildOptimal(false);
      int iter_cost = minimize(optimal, false);
      if (iter_cost < cost) {
        cost = iter_cost;
        pos = i;
      }
      //std::cout << "c Variable: " << i+1 << "\t Cost: " << iter_cost << std::endl;
    }

    std::cout << "c Iteration: " << ++iteration << "\t MinCls: " << cost << std::endl;
    if (begin_cost == cost)
      break;
    else {
      assert (aux_vars[pos]);
      aux_vars[pos] = false;
      copy_inputs.push(Minisat::mkLit(pos,false)); 
      aux_inputs.push(Minisat::mkLit(pos,false)); 
    }
  }

  Solver * tmp = new Solver();
  for (int i = 0 ; i < optimal->nVars(); i++)
    tmp->newVar();

  std::cout << "c i";
  for (int i = 0 ; i < copy_inputs.size(); i++)
    std::cout << " " << Minisat::var(copy_inputs[i])+1;
  std::cout << " 0" << std::endl;

  std::cout << "c aux";
  for (int i = 0 ; i < aux_inputs.size(); i++)
    std::cout << " " << Minisat::var(aux_inputs[i])+1;
  std::cout << std::endl;
  
  // print the final formula
  optimal = tmp;
  inputs.clear();
  db_assignments.clear();
  copy_inputs.copyTo(inputs);
  buildOptimal(false);
  minimize(optimal, true);
 
}

bool GenPCE::solve(Solver * s, Solver * s_opt, 
                          const assignment &assign, Lit p) {
  assignment next;
  next.core = assign.core;
  next.pa = assign.pa;

  next.core.push_back(p);
  next.pa.push_back(p);

  MinVec assumptions;

  bool res = propagate(s_opt, next.core);
  if (!res){
    convert(next.core, assumptions);
    bool result = s_opt->solve(assumptions);
    assert (!result);

    s_opt->addClause__(s_opt->conflict);
    return false;
  }

  MinVec up_lits;
  MinVec search_lits;

  extendAssignment(s_opt, next.pa);

  assumptions.clear();
  convert(next.core, assumptions);

  for (int i = 0; i < s_opt->getTrail().size(); ++i)
    up_lits.push(s_opt->getTrail()[i]);

  printVec("c :: assumptions :: ", assumptions, print);
  bool result = s->solve(assumptions);
  assert (assumptions.size() > 0);
  

  if (result) {
    assignment_heap.push(next);
  } else {
    assert (s->conflict.size() > 0);
    if (mus) {
      if (minimizeCore(s, s->conflict)) {
        n_minimize_core++;
      }
    }
    
#if 0
    // Sanity check: check if the clause already exists
    StdVec duplicate;
    for (int i = 0; i < s->conflict.size(); i++)
      duplicate.push_back(s->conflict[i]);

    assert (duplicate.size() == s->conflict.size());
    std::sort(duplicate.begin(), duplicate.end());
    if (db_clauses.find(duplicate) == db_clauses.end()) {
      db_clauses.insert(duplicate);
      printVec("c :: Learned clause :: ",s->conflict, print);
      s_opt->addClause__(s->conflict);
    } else { 
      assert (0);
    }
#else
    printVec("c :: Learned clause :: ",s->conflict, print);
    s_opt->addClause__(s->conflict);  
#endif
    return false;
  }
  return true;
}

void GenPCE::buildOptimal(bool print) {

  unsigned n_assignments = 0;
  std::vector<bool> seen(reference->nVars(), false);
  std::vector<int> clear;

  // Random order
  std::srand(seed);
  std::vector<Lit> random_inputs;
  for (int i = 0; i < inputs.size(); i++)
    random_inputs.push_back(inputs[i]);
  
  if (random)
    std::random_shuffle(random_inputs.begin(), random_inputs.end(), randomGenerator);

  if (print)
    printVec("c :: inputs :: ", random_inputs, true);

  assignment assign;
  assignment_heap.push(assign);

  while (assignment_heap.size() > 0) {
    assignment current;
    current.core = assignment_heap.top().core;
    current.pa = assignment_heap.top().pa;
    assignment_heap.pop();
    
    bool res = propagate(optimal, current.core);
    if (!res) continue;

    extendAssignment(optimal, current.pa);
    
    for (unsigned i = 0; i < current.pa.size(); i++) {
      seen[Minisat::var(current.pa[i])] = true; 
      clear.push_back(Minisat::var(current.pa[i]));
    }

    for (unsigned i = 0; i < random_inputs.size(); i++) {
      Minisat::Lit p = random_inputs[i];
      if (seen[Minisat::var(p)])
        continue;

      // TODO: fix this version since this is too costly.
      // Cache duplicates
      std::vector<int> p_pos;
      std::vector<int> p_neg;

      for (unsigned i = 0; i < current.core.size(); i++) {
        p_pos.push_back(toInt(current.core[i]));
        p_neg.push_back(toInt(current.core[i]));
      }

      p_pos.push_back(Minisat::var(p)+1);
      p_neg.push_back(-(Minisat::var(p)+1));
      std::sort(p_pos.begin(), p_pos.end());
      std::sort(p_neg.begin(), p_neg.end());

      bool pos_status = true;
      bool neg_status = true;

      if (db_assignments.find(p_pos) == db_assignments.end() && 
          neg_status) {
        db_assignments.insert(p_pos); 
        pos_status = solve(reference, optimal, current, p);
        n_assignments++;
      }

      if (db_assignments.find(p_neg) == db_assignments.end() && 
          pos_status) {
        db_assignments.insert(p_neg); 
        neg_status = solve(reference, optimal, current, ~p);
        n_assignments++;
      }

    }

    for (unsigned i = 0; i < clear.size(); i++)
      seen[clear[i]]= false;
    clear.clear();
  }

  if (print) {
    std::cout << "c :: clause minimization :: " << n_minimize_core << std::endl;
    std::cout << "c :: assignments analyzed :: " << n_assignments << std::endl;  
    printStats("reference encoding", reference);
  
    if (minimal || locking) minimize(optimal, true);
    else {
      //printf("c :: optimal encoding :: %d\n",optimal->nClauses()+optimal->nUnits());    
      printStats("optimal encoding", optimal);
      optimal->printFormula();  
    }
  }
}

bool GenPCE::checkOptimal(bool naive) {

  unsigned n_assignments = 0;
  db_assignments.clear();
  std::vector<StdVec> assignments;
  StdVec empty;
  assignments.push_back(empty);

  // Random order
  std::srand(seed);
  std::vector<Lit> random_inputs;
  for (int i = 0; i < inputs.size(); i++)
    random_inputs.push_back(inputs[i]);
  std::vector<bool> seen(inputs.size(), false);
  std::vector<int> clear;

  if (random)
    std::random_shuffle(random_inputs.begin(), random_inputs.end(), randomGenerator);

  printVec("c :: inputs :: ", random_inputs, print);
  
  while (assignments.size() > 0) {

    StdVec current = assignments.back();
    assignments.pop_back();

    bool res = propagate(reference, current);
    if (!res) continue;
    
    StdVec implied = current;
    extendAssignment(reference, implied);

    for (unsigned i = 0; i < implied.size(); i++) {
      seen[Minisat::var(implied[i])] = true; 
      clear.push_back(Minisat::var(implied[i]));
    }

    for (unsigned i = 0; i < random_inputs.size(); i++) {
      Minisat::Lit p = random_inputs[i];
      if (seen[Minisat::var(p)])
      	continue;

      bool res_pos = true;
      bool res_neg = true;

      if (!naive) {
        // TODO: fix the previous version since this is too costly.
        // Cache duplicates
        std::vector<int> p_pos;
        std::vector<int> p_neg;

        for (unsigned i = 0; i < current.size(); i++) {
          p_pos.push_back(toInt(current[i]));
          p_neg.push_back(toInt(current[i]));
        }

        p_pos.push_back(Minisat::var(p)+1);
        p_neg.push_back(-(Minisat::var(p)+1));
        std::sort(p_pos.begin(), p_pos.end());
        std::sort(p_neg.begin(), p_neg.end());

        if (db_assignments.find(p_pos) == db_assignments.end()) {
          db_assignments.insert(p_pos); 
          MinVec pos; convert(current, pos); pos.push(p);
          printVec("c :: assumptions :: ", pos, print);
          res_pos = reference->solve(pos);
          n_assignments++;
        }

        if (db_assignments.find(p_neg) == db_assignments.end()) {
          db_assignments.insert(p_neg); 
          MinVec neg; convert(current, neg); neg.push(~p);
          printVec("c :: assumptions :: ", neg, print);
          res_pos = reference->solve(neg);
          n_assignments++;
        }
      } else {
        MinVec pos; convert(current, pos); pos.push(p);
	MinVec neg; convert(current, neg); neg.push(~p);
      
	printVec("c :: assumptions :: ", pos, print);
	res_pos = reference->solve(pos);
	printVec("c :: assumptions :: ", neg, print);
	res_neg = reference->solve(neg);
	n_assignments += 2;

      }    

      if (!res_pos || !res_neg)
      	return false;

      if (res_pos) {
        StdVec rpos = current; rpos.push_back(p);
        assignments.push_back(rpos);
      }

      if (res_neg) {
        StdVec rneg = current; rneg.push_back(~p);
        assignments.push_back(rneg);
      }
    }

    for (unsigned i = 0; i < clear.size(); i++)
      seen[clear[i]] = false;
    clear.clear();
  }
  std::cout << "c :: assignments analyzed :: " << n_assignments << std::endl;  
  return true;
}

void GenPCE::printStats(const std::string type, Solver * s) {
  std::cout << "c :: " << type << " :: variables :: " << s->nRealVars();
  std::cout << " :: clauses :: " << s->nClauses()+s->nUnits() << std::endl;
}

