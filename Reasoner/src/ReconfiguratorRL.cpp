#include "ReconfiguratorRL.h"
#include "WaspFacade.h"

#include <boost/process.hpp>
#include <boost/timer/timer.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <random>

#define N_RECONFS 10000000

using namespace boost::process;

void ReconfiguratorRL::onLearningClause(Clause *clause) {
  vector<Literal> literals;
  for(int i=0; i<clause->size(); i++) {
    literals.push_back(clause->getAt(i));
  }
  learncalls++;
  ValuedClauseRL x = { .value = -20.0f + 2.0f * (float)clause->lbd(), .clause = clause, .literals = literals, .freeze = false, .n_t = 0 };
  clauses.insert(x);
}

void ReconfiguratorRL::onDeletingClause(Clause *clause) {
  // Deleted clauses will be purged later during clause DB management.
  deadClauses.insert(clause);
}

ReconfiguratorRL::ReconfiguratorRL(WaspFacade &w, map<string, Var> v)
    : waspFacade(w), instanceVariables(v) {
  waspFacade.attachClauseListener(this);
  int created = mkfifo("reconfigurate", 0777);
  // if(created == -1) cout << "Error: " << strerror(errno) << endl;
  learncalls = 0;
};

ReconfiguratorRL::~ReconfiguratorRL() { unlink("reconfigurate"); }

void ReconfiguratorRL::solve() {
  ifstream istrm("reconfigurate", ios::in);
  boost::timer::cpu_timer timer;
  string line;

  for (auto var : instanceVariables) {
    assumptions[var.second] = false;
  }

  int reconfs = 0;
  waspFacade.setBudget(BUDGET_TIME, 330);

  while (istrm && getline(istrm, line)) {
    // reconf limit for experiments
    ++reconfs;
    if (reconfs > N_RECONFS)
      break;


    vector<Literal> conflict;

    // Process Assumptions
    vector<Literal> assumptions_vec = processAssumptions(line);

    // Unfreeze top k, delete anything over n
    processClauseDB();

    // cout << "Number of not frozen clauses: " << waspFacade.getNbNotFrozenClauses() << endl;

    // Hand control over to solver
    unsigned int result = waspFacade.solve(assumptions_vec, conflict);
    if (result == COHERENT) {
      // cout << "Coherent under assumptions" << endl;
    } else {
      assert(result == INCOHERENT);
      // cout << "Incoherent under assumptions" << endl;
    }
    // waspFacade.printAnswerSet();
    // cout << "Clauses stored: " << clauses.size() << endl;

    // Learning
    processLearning();

    // cout << "Clauses learned: " << learncalls << endl;
  }
  cout << timer.format() << endl;
}

void ReconfiguratorRL::processClauseDB() {
  size_t i = 0;
  size_t deleted = 0;

  set<Clause*> markedClauses;
  clausesThawed.clear();

  for (auto it = clauses.begin(); it != clauses.end(); ) {
    Clause* c = it->clause;
    auto item = *it;

    // The clause has been deleted during solving
    if(deadClauses.count(c) > 0) {
      it = clauses.erase(it);
      continue;
    }
    // The clause is part of the best K and needs to be thawed for the next run.
    if(i < (int)(0.9*CLAUSE_DB_K)) {
      waspFacade.thaw(c);
      item.n_t += 1;
      item.freeze = false;
      clausesThawed.insert(c);
    } else if(i < CLAUSE_DB_N) {
      if(stochasticValue(0.09)) {
        waspFacade.thaw(c);
        item.n_t += 1;
        item.freeze = false;
        clausesThawed.insert(c);
      } else {
        waspFacade.freeze(c);
        item.freeze = true;
      }
    } else {
      waspFacade.freeze(c);
      item.freeze = true;
    }
    // The clause is worse than the best N and should get purged
    if(i > CLAUSE_DB_N) {
      it = clauses.erase(it);
      markedClauses.insert(c);
      deleted++;
    } else {
      it = clauses.erase(it);
      clauses.insert(item);
      // ++it;
    }

    ++i;
  }


  // Purge all marked clauses
  for (auto it = waspFacade.learnedClauses_begin(); it != waspFacade.learnedClauses_end(); ++it) {
    Clause *c = *(it.base());
    if(markedClauses.count(c) > 0) {
      // FIXME: Clause deletion causes SIGSEGV
      // waspFacade.deleteLearnedClause(it);
      // waspFacade.decrementFrozenClauses();
    }
  }

  // cout << "Clauses deleted: " << deleted << endl;
}

void ReconfiguratorRL::processLearning() {
  nbUsedClauses = 0;
  for (auto it = clauses.begin(); it != clauses.end(); ) {
    auto item = *it;
    float reward = 0.0;

    if(clausesThawed.count(item.clause) > 0) {
      if (item.clause->usedInLearning()) {
        reward = 20;
        nbUsedClauses++;
      } else {
        reward = -20;
      }
    } 
    // if(clausesThawed.find(item.clause) != clausesThawed.end() && item.clause->usedInLearning() && !item.clause->frozen()) nbUsedClauses++;

    item.clause->resetUsedInLearning();
    if(clausesThawed.count(item.clause) > 0) {
      item.value = item.value + (reward - item.value) * LEARNING_RATE;
    }

    // Replace item
    if (item.value != it->value) {
      it = clauses.erase(it);
      clauses.insert(item);
    } else {
      ++it;
    }
  }
  cout << "Size of thawed is: " << clausesThawed.size() << endl;
  cout << "Number of used clauses: " << nbUsedClauses << endl;
}

vector<Literal> ReconfiguratorRL::processAssumptions(string line) {
  vector<Literal> avec;
  // cout << "Input: " << line << endl;
  istringstream iss(line);

  string word;
  while (iss && getline(iss, word, ' ')) {
    auto semicolon = word.find(';');
    string atom = word.substr(0, semicolon);
    string op = word.substr(semicolon + 1);
    auto search = instanceVariables.find(atom);

    if (search != instanceVariables.end()) {
      Var var = search->second;
      if (op[0] == '+') {
        assumptions[var] = true;
      } else if (op[0] == '-') {
        assumptions[var] = false;
      } else {
        cerr << "WARNING: Unknown operation!" << endl;
      }
    } else {
      cerr << "WARNING: Ignoring unknown atom " << atom << endl;
    }
  }

  for (auto a : assumptions) {
    Literal l = Literal::createLiteralFromInt(a.second ? a.first : -a.first);
    avec.push_back(l);
  }

  return avec;
}

bool ReconfiguratorRL::stochasticValue(float n) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dis(0.0, 1.0);
  if(dis(gen) < n) return true;
  return false;
}