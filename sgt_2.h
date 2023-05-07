#ifndef SGT_2_H
#define SGT_2_H

#include "Data.h"
#include "Transaction.h"
#include "acyclic_lf.h"
#include <iostream>

class SGT {
public:
  SGT();
  ~SGT();
  Transaction &begin_transaction();
  bool read(Transaction &t, Data &d, int &x);
  bool write(Transaction &t, Data &d, int value);
  void abort(Transaction &t);
  int try_commit(Transaction &t);

  Graph g;

private:
  atomic<int> id_counter;
};

SGT::SGT() {
  Graph g = Graph();
  id_counter = 0;
}
SGT::~SGT() {}

SGT sgt;
vector<Data *> sharedMem;
ofstream log_file("log.txt", ios::out);
mutex log_lock;

Transaction &SGT::begin_transaction() {
  Transaction *t = new Transaction();
  t->id = id_counter++;
  g.AddV(t->id);
  // g.PrintGraph();
  return *t;
}

bool SGT::read(Transaction &t, Data &d, int &x) {
  std::lock_guard<std::mutex> lock(d.data_lock);
  if (d.read_set.find(t.id) != d.read_set.end()) {
    x = d.value;
  } else {
    d.read_set.insert(t.id);
    t.read_set.insert(&d);
    x = d.value;
    // add write-read dependency to the conflict graph (abort if cycle is
    // detected)
    for (auto it = d.write_set.begin(); it != d.write_set.end(); ++it) {
      bool isCycle = false;
      if (*it != t.id) {
        isCycle = g.AddE(*it, t.id);
      }
      if (isCycle) {
        abort(t);
        return 0;
      } else {
        t.incoming_edges.insert(*it);
      }
    }
  }
  log_lock.lock();
  log_file << "read " << d.id << " value " << x << " by " << t.id << endl;
  log_lock.unlock();
  return 1;
}

bool SGT::write(Transaction &t, Data &d, int value) {
  std::lock_guard<std::mutex> lock(d.data_lock);
  t.write_set[&d] = value;
  return 1;
}

void SGT::abort(Transaction &t) {
  for (auto e : t.incoming_edges) {
    g.RemoveE(e, t.id);
  }
  log_lock.lock();
  log_file << t.id << " aborted" << endl;
  log_lock.unlock();
}

int SGT::try_commit(Transaction &t) {
  for (auto it = t.write_set.begin(); it != t.write_set.end(); ++it) {
    Data *d = it->first;
    d->data_lock.lock();
    // add write-write dependency to the conflict graph
    for (auto it2 = d->write_set.begin(); it2 != d->write_set.end(); ++it2) {
      bool isCycle = false;
      if (*it2 != t.id) {
        isCycle = g.AddE(*it2, t.id);
      }
      // abort if cycle is detected
      if (isCycle) {
        for (auto it3 = t.write_set.begin(); it3 != it; ++it3) {
          Data *d = it3->first;
          d->data_lock.unlock();
        }
        it->first->data_lock.unlock();
        abort(t);
        return 0;
      } else {
        t.incoming_edges.insert(*it2);
      }
    }
    // add read-write dependency to the conflict graph
    for (auto it2 = d->read_set.begin(); it2 != d->read_set.end(); ++it2) {
      bool isCycle = false;
      if (*it2 != t.id) {
        isCycle = g.AddE(*it2, t.id);
      }
      // abort if cycle is detected
      if (isCycle) {
        for (auto it3 = t.write_set.begin(); it3 != it; ++it3) {
          Data *d = it3->first;
          d->data_lock.unlock();
        }
        it->first->data_lock.unlock();
        abort(t);
        return 0;
      } else {
        t.incoming_edges.insert(*it2);
      }
    }
  }
  // commit if no cycle is detected
  for (auto it = t.write_set.begin(); it != t.write_set.end(); ++it) {
    Data *d = it->first;
    d->value = t.write_set[d];
    d->write_set.insert(t.id);
    d->data_lock.unlock();
    log_lock.lock();
    log_file << "write " << d->id << " value " << d->value << " by " << t.id
             << endl;
    log_lock.unlock();
  }
  log_lock.lock();
  log_file << t.id << " committed" << endl;
  log_lock.unlock();
  // delete &t;
  return 1;
}

void initSharedData(int numData) {
  for (int i = 0; i < numData; ++i) {
    Data *d = new Data(0, i);
    sharedMem.push_back(d);
  }
}

void twice_value_update(int low, int high) {
  Transaction newTrans = sgt.begin_transaction();
  for (int i = low; i < high; ++i) {
    Data &d = *sharedMem[i];
    int x;
    sgt.read(newTrans, d, x);
    sgt.write(newTrans, d, 2 * x);
  }
}

void read_only_transaction() {
  Transaction newTrans = sgt.begin_transaction();
  int numOps = rand() % sharedMem.size();
  vector<bool> visited(numOps, false);
  for (int op = 0; op < numOps; ++op) {
    int dataId = rand() % 10;
    if (visited[dataId])
      continue;
    visited[dataId] = true;
    Data &d = *sharedMem[dataId];
    int x;
    sgt.read(newTrans, d, x);
  }
}

void write_only_transaction() {
  Transaction newTrans = sgt.begin_transaction();
  int numOps = rand() % sharedMem.size();
  vector<bool> visited(numOps, false);
  for (int op = 0; op < numOps; ++op) {
    int dataId = rand() % 10;
    if (visited[dataId])
      continue;
    visited[dataId] = true;
    Data &d = *sharedMem[dataId];
    sgt.write(newTrans, d, 1);
  }
}

void sum_transaction(int low, int high) {
  Transaction newTrans = sgt.begin_transaction();
  int sum = 0;
  for (int i = low; i < high; ++i) {
    Data &d = *sharedMem[i];
    int x;
    sgt.read(newTrans, d, x);
    sum += x;
  }
  std::cout << "sum: " << sum << std::endl;
  // return sum;
}

void increment_data(int low, int high, int inc) {
  Transaction newTrans = sgt.begin_transaction();
  for (int i = low; i < high; ++i) {
    Data &d = *sharedMem[i];
    int x;
    sgt.read(newTrans, d, x);
    sgt.write(newTrans, d, x + inc);
    sgt.try_commit(newTrans);
  }
}

void decrement_data(int low, int high, int dec) {
  Transaction newTrans = sgt.begin_transaction();
  for (int i = low; i < high; ++i) {
    Data &d = *sharedMem[i];
    int x;
    sgt.read(newTrans, d, x);
    sgt.write(newTrans, d, x - dec);
  }
}

#endif
