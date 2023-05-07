#include "Data.h"
#include "Transaction.h"
#include "sgt.h"
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

using namespace std;

SGT::SGT() {
  Graph g = Graph();
  id_counter = 0;
}
SGT::~SGT() {}

SGT sgt;
vector<Data *> sharedMem{};
ofstream log_file("log.txt", ios::out);
mutex log_lock;
mutex temp;

Transaction &SGT::begin_transaction() {
  Transaction *t = new Transaction();
  t->id = id_counter++;
  std::cout << "transaction id:" << t->id << std::endl;
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
        // cout << "here1\n";
        isCycle = g.AddE(*it, t.id);
        // cout << "here2\n";
      }
      if (isCycle) {
        abort(t);
        return 0;
      } else {
        t.incoming_edges.insert(*it);
      }
      // cout << "here3\n";
    }
  }
  // cout << "here4\n";
  log_lock.lock();
  log_file << "read " << d.id << " value " << x << " by " << t.id << endl;
  log_lock.unlock();
  return 1;

}

bool SGT::write(Transaction &t, Data &d, int value) {
  std::lock_guard<std::mutex> lock(d.data_lock);
  for (auto it = d.read_set.begin(); it != d.read_set.end(); ++it) {
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

void SGT::try_commit(Transaction &t) {
  // commit if no cycle is detected
  for (auto it = t.write_set.begin(); it != t.write_set.end(); ++it) {
    Data *d = it->first;
    std::lock_guard<std::mutex> lock(d->data_lock);
    d->value = t.write_set[d];
    d->write_set.insert(t.id);
    log_lock.lock();
    log_file << "write " << d->id << " value " << d->value << " by " << t.id
             << endl;
    log_lock.unlock();
  }
  log_lock.lock();
  log_file << t.id << " committed" << endl;
  log_lock.unlock();
  // delete &t;
  return;
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
    // temp.lock();
    // cout << newTrans.id << " " << i << " here1" << endl;
    // temp.unlock();
    bool status1 = sgt.read(newTrans, d, x);
    if (!status1) {
      return;
    }
    // temp.lock();
    // cout << newTrans.id << " " << i << " here2" << endl;
    // temp.unlock();
    bool status2 = sgt.write(newTrans, d, x + inc);
    if (!status2) {
      return;
    }
    // temp.lock();
    // cout << newTrans.id << " " << "Success" << endl;
    // temp.unlock();
  }
  sgt.try_commit(newTrans);
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

int main() {
  // Transaction t1,t2;

  // Data d(2, 1);
  // int x1,x2;
  // t1 = sgt.begin_transaction();
  // t2 = sgt.begin_transaction();

  // sgt.read(t1, d, x1);
  // sgt.read(t2,d, x2);
  // sgt.write(t1, d, 1);
  // sgt.write(t2, d, 2);
  // sgt.try_commit(t1);
  // sgt.try_commit(t2);

  int numTrans = 100;
  int numData = 100;
  std::thread trans[numTrans];

  initSharedData(numData);

  for (int i = 0; i < numTrans; ++i)
    trans[i] = std::thread(increment_data, 2, 14, 5);
  for (int i = 0; i < numTrans; ++i)
    trans[i].join();

  log_file.close();
  sgt.g.PrintGraph();
  return 0;
}