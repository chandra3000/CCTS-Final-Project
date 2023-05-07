#ifndef TRANSACTION_H
#define TRANSACTION_H

#include "Data.h"
#include <set>
#include <map>

class Transaction {
public:
  int id;
  std::map<Data *, int> write_set;
  std::set<Data *> read_set;
  std::set<int> incoming_edges;
  Transaction() {}
  ~Transaction() {}
};

#endif