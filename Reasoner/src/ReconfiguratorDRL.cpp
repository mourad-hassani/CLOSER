#include "ReconfiguratorDRL.h"
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

int generate_id()
{
  static int id = 0;
  return ++id;
}

void ReconfiguratorDRL::onLearningClause(Clause *clause)
{
  vector<Literal> literals;
  for (int i = 0; i < clause->size(); i++)
  {
    literals.push_back(clause->getAt(i));
  }
  learncalls++;
  ValuedClauseDRL x = {.id = generate_id(), .value = -20.0f + 2.0f * (float)clause->lbd(), .clause = clause, .literals = literals, .freeze = false, .toThaw = false, .n_t = 0, .nbUsedInLearning = 0, .itemToReplace = 0, .changed = false};
  clauses.insert(x);
  sendMessage("onLearningClause," + std::to_string(x.id) + "," + std::to_string(x.clause->lbd()));
  int index = std::stoi(receiveMessage());
  if (index != -1)
  {
    if ((x.clause->lbd() > cache_lbd - 5) && (x.clause->lbd() < cache_lbd + 5))
    {
      clausesThawed[index] = clause;
    }
  }
}

void ReconfiguratorDRL::onDeletingClause(Clause *clause)
{
  deadClauses.insert(clause);
}

ReconfiguratorDRL::ReconfiguratorDRL(WaspFacade &w, map<string, Var> v)
    : waspFacade(w), instanceVariables(v)
{
  waspFacade.attachClauseListener(this);
  int created = mkfifo("reconfigurate", 0777);
  learncalls = 0;
};

ReconfiguratorDRL::~ReconfiguratorDRL() { unlink("reconfigurate"); }

void ReconfiguratorDRL::solve()
{
  openMessenger();
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
    ++reconfs;
    if (reconfs > N_RECONFS)
      break;

    vector<Literal> conflict;
    vector<Literal> assumptions_vec = processAssumptions(line);
    processClauseDB();

    unsigned int result = waspFacade.solve(assumptions_vec, conflict);
    if (result == COHERENT)
    {
    }
    else
    {
      assert(result == INCOHERENT);
    }
    int reward = 0;
    for (auto it = clauses.begin(); it != clauses.end(); it++)
    {
      auto item = *it;
      if (item.clause->usedInLearning())
      {
        auto itr = std::find(clausesThawed.begin(), clausesThawed.end(), it->clause);
        if (itr != clausesThawed.end())
        {
          reward++;
        }
      }
    }
    if (started)
    {
      sendMessage("Start Reward");
      receiveMessage();
      sendMessage("reward," + std::to_string(reward));
      receiveMessage();
      processLearning();
    }
    sendMessage("Start");
    receiveMessage();
    started = true;
    if (started)
    {
      getAction();
    }
  }
  ::cout << timer.format() << endl;
  sendMessage("Terminate");
  receiveMessage();
  closeMessenger();
}

void ReconfiguratorDRL::processClauseDB()
{
  sendMessage("Start delete");
  receiveMessage();
  size_t i = 0;
  size_t deleted = 0;

  for (auto it = clauses.begin(); it != clauses.end();)
  {
    Clause *c = it->clause;
    auto item = *it;
    if (deadClauses.count(c) > 0)
    {
      it = clauses.erase(it);
      auto itr = std::find(clausesThawed.begin(), clausesThawed.end(), it->clause);

      if (itr != clausesThawed.end())
      {
        clausesThawed[std::distance(clausesThawed.begin(), itr)] = NULL;
      }
      sendMessage("onDeletingClause," + std::to_string(item.id));
      receiveMessage();
      continue;
    }
    auto itr = std::find(clausesThawed.begin(), clausesThawed.end(), it->clause);
    if (itr != clausesThawed.end())
    {
      waspFacade.thaw(it->clause);
      item.n_t += 1;
      item.freeze = false;
    }
    else
    {
      waspFacade.freeze(c);
      item.freeze = true;
    }
    it = clauses.erase(it);
    clauses.insert(item);
    ++i;
  }
  int nbThawed = 0;
  for (int i = 0; i < CLAUSE_DB_K; i++)
  {
    if (clausesThawed[i] != NULL)
    {
      nbThawed++;
    }
  }
  std::cout << "Size of Thawed : " << nbThawed << std::endl;
}

void ReconfiguratorDRL::processLearning()
{
  sendMessage("Start Request");
  receiveMessage();
  nbUsedClauses = 0;
  // This is used to send only one request
  int first_iterations = NB_REQUESTS;
  for (auto it = clauses.begin(); it != clauses.end();)
  {
    auto item = *it;
    if (item.clause->usedInLearning())
    {
      item.changed = true;
      item.clause->resetUsedInLearning();
      item.nbUsedInLearning++;
      auto itr = std::find(clausesThawed.begin(), clausesThawed.end(), it->clause);
      if (itr != clausesThawed.end())
      {
        nbUsedClauses++;
      }
      if (first_iterations > 0 && itr == clausesThawed.end())
      {
        sendMessage("requested," + std::to_string(item.id) + "," + std::to_string(item.nbUsedInLearning));
        requestedClausesId[NB_REQUESTS - first_iterations] = item.id;
        receiveMessage();
        first_iterations--;
      }
    }

    if (item.changed == true)
    {
      item.changed = false;
      it = clauses.erase(it);
      clauses.insert(item);
    }
    else
    {
      ++it;
    }
  }
  ::cout << "Number of used clauses: " << nbUsedClauses << endl;
}

void ReconfiguratorDRL::getAction()
{
  sendMessage("getAction,");
  cache_lbd = std::stoi(receiveMessage());
  for (int i = 0; i < clausesThawed.size(); i++)
  {
    if (clausesThawed[i] != NULL)
    {
      if ((clausesThawed[i]->lbd() >= cache_lbd + 5) && (clausesThawed[i]->lbd() <= cache_lbd - 5))
      {
        clausesThawed[i] = NULL;
      }
    }
  }
}

vector<Literal> ReconfiguratorDRL::processAssumptions(string line)
{
  vector<Literal> avec;
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

bool ReconfiguratorDRL::stochasticValue(float n)
{
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dis(0.0, 1.0);
  if (dis(gen) < n)
    return true;
  return false;
}

void ReconfiguratorDRL::openMessenger()
{
  // Create a socket
  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket < 0)
  {
    cerr << "Error opening socket" << endl;
    exit(1);
  }

  // Set up the server address
  port_number = 1234;
  bzero((char *)&server_address, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(port_number);

  // Bind the socket to the server address
  if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
  {
    cerr << "Error on binding" << endl;
    exit(1);
  }

  // Listen for incoming connections
  listen(server_socket, 5);

  // Accept a connection
  client_length = sizeof(client_address);
  client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_length);
  if (client_socket < 0)
  {
    cerr << "Error on accept" << endl;
    exit(1);
  }
}

void ReconfiguratorDRL::sendMessage(std::string message)
{
  std::cout << "sent: " + message << std::endl;
  int n = write(client_socket, message.c_str(), message.length());
  if (n < 0)
  {
    cerr << "Error writing to socket" << endl;
    exit(1);
  }
}

char *ReconfiguratorDRL::receiveMessage()
{
  bzero(buffer, 256);
  int n = read(client_socket, buffer, 255);
  if (n < 0)
  {
    cerr << "Error reading from socket" << endl;
    exit(1);
  }
  std::string my_string = buffer;
  std::cout << "received: " + my_string << std::endl;
  return buffer;
}

void ReconfiguratorDRL::closeMessenger()
{
  // Close the socket
  ::close(client_socket);
  ::close(server_socket);
}