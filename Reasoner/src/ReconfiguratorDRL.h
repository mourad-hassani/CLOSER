#ifndef RECONFIGURATORDRL_H
#define RECONFIGURATORDRL_H

#include "ClauseListener.h"
#include "WaspFacade.h"

#include <map>
#include <memory>
#include <set>

#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <array>
#include <algorithm>

using namespace std;

// Maximum size of the clause database
#define CLAUSE_DB_N 100
// Number of always unfrozen top clauses
#define CLAUSE_DB_K 100

// Learning Parameters
#define LEARNING_RATE 0.1

// Number of requests
#define NB_REQUESTS 20

struct ValuedClauseDRL
{
  int id;
  float value;
  Clause *clause;
  vector<Literal> literals;
  bool freeze;
  bool toThaw;
  int n_t;
  int nbUsedInLearning;
  int itemToReplace;
  bool changed;

  friend bool operator<(const ValuedClauseDRL &l, const ValuedClauseDRL &r)
  {
    return l.id < r.id;
  }
};

class ReconfiguratorDRL : public ClauseListener
{
public:
  ReconfiguratorDRL(WaspFacade &w, map<string, Var> v);
  ~ReconfiguratorDRL();
  void onLearningClause(Clause *clause);
  void onDeletingClause(Clause *clause);
  void solve();

private:
  WaspFacade &waspFacade;
  multiset<ValuedClauseDRL> clauses;
  set<Clause *> deadClauses;
  map<string, Var> instanceVariables;

  int server_socket, client_socket, port_number;
  socklen_t client_length;
  char buffer[256];
  struct sockaddr_in server_address, client_address;
  std::array<int, NB_REQUESTS> requestedClausesId = {};
  bool started = false;
  int cache_lbd = 0;

  map<Var, bool> assumptions;

  vector<Literal> processAssumptions(string line);
  void processClauseDB();
  void processLearning();
  bool stochasticValue(float n);
  string relaxedAssumptions();
  unordered_set<Var> computeRelaxedAnswerSet();
  void openMessenger();
  void sendMessage(std::string message);
  char *receiveMessage();
  void closeMessenger();
  void getAction();

  unsigned int learncalls;

  float exp_rate = 0.1;
  int nbUsedClauses = 0;
  std::array<Clause *, CLAUSE_DB_K> clausesThawed = {};
  std::array<int, CLAUSE_DB_K> clausesThawedId = {};
};
#endif /* end of include guard: RECONFIGURATOR_H */
