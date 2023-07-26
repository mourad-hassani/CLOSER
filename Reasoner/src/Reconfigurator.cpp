#include "Reconfigurator.h"
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

#define N_RECONFS 10000000

using namespace boost::process;

void Reconfigurator::onLearningClause(Clause *clause)
{
  learncalls++;
  ValuedClause x = {.value = -20.0f + 2.0f * (float)clause->lbd(), .clause = clause};
  clauses.insert(x);
}

void Reconfigurator::onDeletingClause(Clause *clause)
{
  // Deleted clauses will be purged later during clause DB management.
  deadClauses.insert(clause);
}

Reconfigurator::Reconfigurator(WaspFacade &w, map<string, Var> v)
    : waspFacade(w), instanceVariables(v)
{
  waspFacade.attachClauseListener(this);
  int created = mkfifo("reconfigurate", 0777);
  // if(created == -1) cout << "Error: " << strerror(errno) << endl;
  learncalls = 0;
};

Reconfigurator::~Reconfigurator() { unlink("reconfigurate"); }

void Reconfigurator::solve()
{
  ifstream istrm("reconfigurate", ios::in);
  boost::timer::cpu_timer timer;
  string line;

  for (auto var : instanceVariables)
  {
    assumptions[var.second] = false;
  }

  int reconfs = 0;
  waspFacade.setBudget(BUDGET_TIME, 330);

  while (istrm && getline(istrm, line))
  {
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
    if (result == COHERENT)
    {
      // cout << "Coherent under assumptions" << endl;
    }
    else
    {
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

void Reconfigurator::processClauseDB()
{
  size_t i = 0;
  size_t deleted = 0;

  set<Clause *> markedClauses;
  clausesThawed.clear();

  for (auto it = clauses.begin(); it != clauses.end();)
  {
    Clause *c = it->clause;

    // The clause has been deleted during solving
    if (deadClauses.count(c) > 0)
    {
      it = clauses.erase(it);
      continue;
    }

    // The clause is part of the best K and needs to be thawed for the next run.
    if (i < CLAUSE_DB_K)
    {
      waspFacade.thaw(c);
      clausesThawed.insert(c);
    }
    else
    {
      waspFacade.freeze(c);
    }

    // The clause is worse than the best N and should get purged
    if (i > CLAUSE_DB_N)
    {
      it = clauses.erase(it);
      markedClauses.insert(c);
      deleted++;
    }
    else
    {
      ++it;
    }

    ++i;
  }

  // Purge all marked clauses
  for (auto it = waspFacade.learnedClauses_begin(); it != waspFacade.learnedClauses_end(); ++it)
  {
    Clause *c = *(it.base());
    if (markedClauses.count(c) > 0)
    {
      // FIXME: Clause deletion causes SIGSEGV
      // waspFacade.deleteLearnedClause(it);
      // waspFacade.decrementFrozenClauses();
    }
  }

  // cout << "Clauses deleted: " << deleted << endl;
}

void Reconfigurator::processLearning()
{
  nbUsedClauses = 0;
  for (auto it = clauses.begin(); it != clauses.end();)
  {
    auto item = *it;
    float reward = 0.0;

    if (item.clause->usedInLearning())
    {
      if (item.clause->frozen())
      {
        reward = -20.0;
        // cout << "-20" << endl;
      }
      else
      {
        reward = 20.0;
        // cout << "20" << endl;
      }
    }
    else
    {
      if (!item.clause->frozen())
      {
        reward = -5.0;
        // cout << "-5" << endl;
      }
    }

    if (clausesThawed.count(item.clause) > 0)
    {
      if (item.clause->usedInLearning())
      {
        nbUsedClauses++;
      }
    }

    item.clause->resetUsedInLearning();
    item.value = item.value + LEARNING_RATE * (reward - item.value);

    // Replace item
    if (item.value != it->value)
    {
      it = clauses.erase(it);
      clauses.insert(item);
    }
    else
    {
      ++it;
    }
  }
  cout << "Size of thawed is: " << clausesThawed.size() << endl;
  cout << "Number of used clauses: " << nbUsedClauses << endl;
}

vector<Literal> Reconfigurator::processAssumptions(string line)
{
  vector<Literal> avec;
  // cout << "Input: " << line << endl;
  istringstream iss(line);

  string word;
  while (iss && getline(iss, word, ' '))
  {
    auto semicolon = word.find(';');
    string atom = word.substr(0, semicolon);
    string op = word.substr(semicolon + 1);
    auto search = instanceVariables.find(atom);

    if (search != instanceVariables.end())
    {
      Var var = search->second;
      if (op[0] == '+')
      {
        assumptions[var] = true;
      }
      else if (op[0] == '-')
      {
        assumptions[var] = false;
      }
      else
      {
        cerr << "WARNING: Unknown operation!" << endl;
      }
    }
    else
    {
      cerr << "WARNING: Ignoring unknown atom " << atom << endl;
    }
  }

  for (auto a : assumptions)
  {
    Literal l = Literal::createLiteralFromInt(a.second ? a.first : -a.first);
    avec.push_back(l);
  }

  return avec;
}
