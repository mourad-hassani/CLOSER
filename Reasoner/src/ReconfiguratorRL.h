#ifndef RECONFIGURATORRL_H
#define RECONFIGURATORRL_H

#include "ClauseListener.h"
#include "WaspFacade.h"

#include <map>
#include <memory>
#include <set>

using namespace std;

// Maximum size of the clause database
#define CLAUSE_DB_N 300
// Number of always unfrozen top clauses
#define CLAUSE_DB_K 100


// Learning Parameters
#define LEARNING_RATE 0.1

struct ValuedClauseRL {
  float value;
  Clause* clause;
  vector<Literal> literals;
  bool freeze;
  int n_t;

  friend bool operator<(const ValuedClauseRL& l, const ValuedClauseRL& r) {
    return l.value < r.value;
  }
};

class ReconfiguratorRL : public ClauseListener {
public:
  ReconfiguratorRL(WaspFacade &w, map<string, Var> v);
  ~ReconfiguratorRL();
  void onLearningClause(Clause *clause);
  void onDeletingClause(Clause *clause);
  void solve();

private:
  WaspFacade &waspFacade;
  multiset<ValuedClauseRL> clauses;
  set<Clause*> deadClauses;
  map<string, Var> instanceVariables;

  map<Var, bool> assumptions;

  vector<Literal> processAssumptions(string line);
  void processClauseDB();
  void processLearning();
  bool stochasticValue(float n);
  string relaxedAssumptions();
  unordered_set<Var> computeRelaxedAnswerSet();

  unsigned int learncalls;

  float exp_rate = 0.1;
  int nbUsedClauses = 0;
  set<Clause*> clausesThawed;
};
#endif /* end of include guard: RECONFIGURATOR_H */
