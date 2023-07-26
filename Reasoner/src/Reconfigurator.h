#ifndef RECONFIGURATOR_H
#define RECONFIGURATOR_H

#include "ClauseListener.h"
#include "WaspFacade.h"

#include <map>
#include <memory>
#include <set>

using namespace std;

// Maximum size of the clause database
#define CLAUSE_DB_N 100
// Number of always unfrozen top clauses
#define CLAUSE_DB_K 100

// Learning Parameters
#define LEARNING_RATE 0.6

struct ValuedClause
{
  float value;
  Clause *clause;

  friend bool operator<(const ValuedClause &l, const ValuedClause &r)
  {
    return l.value < r.value;
  }
};

class Reconfigurator : public ClauseListener
{
public:
  Reconfigurator(WaspFacade &w, map<string, Var> v);
  ~Reconfigurator();
  void onLearningClause(Clause *clause);
  void onDeletingClause(Clause *clause);
  void solve();

private:
  WaspFacade &waspFacade;
  multiset<ValuedClause> clauses;
  set<Clause *> deadClauses;
  map<string, Var> instanceVariables;

  map<Var, bool> assumptions;

  vector<Literal> processAssumptions(string line);
  void processClauseDB();
  void processLearning();
  string relaxedAssumptions();
  unordered_set<Var> computeRelaxedAnswerSet();

  unsigned int learncalls;

  int nbUsedClauses = 0;
  set<Clause *> clausesThawed;
};
#endif /* end of include guard: RECONFIGURATOR_H */
