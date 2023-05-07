#ifndef SGT_H
#define SGT_H

#include "Data.h"
#include "Transaction.h"
#include "two_vlock_remove_ie_reach.h"
#include <bits/stdc++.h>

class SGT {
public:
  SGT();
  ~SGT();
  Transaction &begin_transaction();
  bool read(Transaction &t, Data *d, int &x);
  bool write(Transaction &t, Data *d, int value);
  void abort(Transaction &t);
  int try_commit(Transaction &t);

  // Graph g;

private:
  atomic<int> id_counter;
};

SGT::SGT() {
  // Graph g = Graph();
  id_counter = 1;
}
SGT::~SGT() {}

SGT sgt;
vector<Data *> sharedMem;
ofstream log_file("log.txt", ios::out);
mutex log_lock;
std::mutex transLock;
// std::mutex graphLock;
std::mutex abortLock;

Transaction &SGT::begin_transaction() {
  transLock.lock();
  // cout << "start new trans\n";
  Transaction *t = new Transaction();
  t->id = id_counter++;
  // g.AddV(t->id);
  // g.PrintGraph();
  // cout << "ok\n";
  add_vertex(t->id);
  // cout << "new trans success\n";
  transLock.unlock();
  return *t;
}

bool SGT::read(Transaction &t, Data *d, int &x) {
  // std::lock_guard<std::shared_mutex> lock(d.data_lock);
  // cout << "start read\n";
  d->data_lock.lock_shared();
  // graphLock.lock();
  // cout << d->id << " read locked\n";
  // graphLock.lock();
  if (0) {
  } else {
    d->read_set.insert(t.id);
    t.read_set.insert(d);
    x = d->value;
    // add write-read dependency to the conflict graph (abort if cycle is
    // detected)
    for (auto it = d->write_set.begin(); it != d->write_set.end(); ++it) {
      bool isCycle = false;
      if (*it != t.id && !contains_edge(*it, t.id)) {
        // isCycle = g.AddE(*it, t.id);
        //   cout << "trying to add edge " << *it << " " << t.id << "\n";
        add_edge(*it, t.id);
        // cout << "added edge " << *it << " " << t.id << "\n";
        isCycle = (cycle_detect(*it, t.id) && cycle_detect(t.id, *it));
      }
      if (isCycle) {
        remove_edge(*it, t.id);
        // vector <Data *> finalOrder{};
        // for(auto it1 = t.read_set.begin(); it1 != t.read_set.end(); ++it1) {
        // 	if((*it1)->id != d->id) {
        // 		finalOrder.push_back(*it1);
        // 		(*it1)->data_lock.lock();
        // 		cout << (*it1)->id << " read abort unlocked\n";
        // 	}
        // }
        // reverse(finalOrder.begin(), finalOrder.end());
        abort(t);
        // for(auto data: finalOrder) {
        // 	data->data_lock.unlock();
        // 	cout << data->id << " read abort unlocked\n";
        // }
        // graphLock.unlock();
        d->data_lock.unlock_shared();
        // cout << d->id << " read abort unlocked\n";
        // cout << "read abort\n";
        return 0;
      } else {
        t.incoming_edges.insert(*it);
      }
    }
  }
  // graphLock.unlock();
  // cout << "end read\n";
  // graphLock.unlock();
  d->data_lock.unlock_shared();
  // cout << d->id << " read unlocked\n";
  // log_lock.lock();
  // log_file << "read " << d.id << " value " << x << " by " << t.id << endl;
  // log_lock.unlock();
  return 1;
}

bool SGT::write(Transaction &t, Data *d, int value) {
  // std::lock_guard<std::shared_mutex> lock(d.data_lock);
  // cout << "write wait\n";
  // cout << "waiting for " << d->id << " unlock\n";
  d->data_lock.lock();
  // cout << d->id << " write locked\n";
  // cout << "start write\n";
  t.write_set[d] = value;
  // cout << "end write\n";
  d->data_lock.unlock();
  // cout << d->id << " write unlocked\n";
  return 1;
}

void SGT::abort(Transaction &t) {
  // read set updation
  abortLock.lock();
  for (auto it = t.read_set.begin(); it != t.read_set.end(); ++it) {
    Data *d = *it;
    d->read_set.erase(t.id);
  }
  // conflict graph updation
  for (auto e : t.incoming_edges) {
    // g.RemoveE(e, t.id);
    remove_edge(e, t.id);
  }
  // g.RemoveV(t.id);
  abortLock.unlock();
}

int SGT::try_commit(Transaction &t) {
  set<Data *> tempOrder{};
  vector<Data *> finalOrder{};
  // cout << "start commit\n";
  for (auto it = t.write_set.begin(); it != t.write_set.end(); ++it) {
    Data *d = it->first;
    tempOrder.insert(d);
  }
  for (auto it = t.read_set.begin(); it != t.read_set.end(); ++it) {
    Data *d = *it;
    tempOrder.insert(d);
  }
  for (auto it = tempOrder.begin(); it != tempOrder.end(); ++it) {
    finalOrder.push_back(*it);
    (*it)->data_lock.lock();
    // cout << (*it)->id << " commit locked\n";
  }
  // graphLock.lock();
  reverse(finalOrder.begin(), finalOrder.end());

  for (auto it = t.write_set.begin(); it != t.write_set.end(); ++it) {
    Data *d = it->first;
    // d->data_lock.lock();
    // add write-write dependency to the conflict graph
    for (auto it2 = d->write_set.begin(); it2 != d->write_set.end(); ++it2) {
      bool isCycle = false;
      if (*it2 != t.id) {
        // isCycle = g.AddE(*it2, t.id);
        //   cout << "trying to add edge " << *it2 << " " << t.id
        //   << "\n";
        add_edge(*it2, t.id);
        //   cout << "added edge " << *it2 << " " << t.id << "\n";
        isCycle = (cycle_detect(*it2, t.id) && cycle_detect(t.id, *it2));
      }
      // abort if cycle is detected
      if (isCycle) {
        remove_edge(*it2, t.id);
        abort(t);
        // graphLock.unlock();
        for (auto data : finalOrder) {
          data->data_lock.unlock();
          // cout << data->id << " abort unlocked\n";
        }
        return 0;
      } else {
        t.incoming_edges.insert(*it2);
      }
    }
    // add read-write dependency to the conflict graph
    for (auto it2 = d->read_set.begin(); it2 != d->read_set.end(); ++it2) {
      bool isCycle = false;
      if (*it2 != t.id) {
        // isCycle = g.AddE(*it2, t.id);
        // cout << "trying to add edge " << *it2 << " " << t.id << "\n";
        add_edge(*it2, t.id);
        // cout << "added edge " << *it2 << " " << t.id << "\n";
        isCycle = (cycle_detect(*it2, t.id) && cycle_detect(t.id, *it2));
      }
      // abort if cycle is detected
      if (isCycle) {
        // for (auto it3 = t.write_set.begin(); it3 != it; ++it3) {
        // 	Data *d = it3->first;
        // 	d->data_lock.unlock();
        // }
        // it->first->data_lock.unlock();
        remove_edge(*it2, t.id);
        abort(t);
        // graphLock.unlock();
        for (auto data : finalOrder) {
          data->data_lock.unlock();
        //   cout << data->id << " abort unlocked\n";
        }
        return 0;
      } else {
        t.incoming_edges.insert(*it2);
      }
    }
  }

  for (auto it = t.read_set.begin(); it != t.read_set.end(); ++it) {
    Data *d = *it;
    // adding write-read dependencies to the conflict graph
    for (auto it2 = d->write_set.begin(); it2 != d->write_set.end(); ++it2) {
      bool isCycle = false;
      if (*it2 != t.id) {
        // isCycle = g.AddE(*it2, t.id);
        add_edge(*it2, t.id);
        isCycle = (cycle_detect(*it2, t.id) && cycle_detect(t.id, *it2));
      }
      // abort if cycle is detected
      if (isCycle) {
        remove_edge(*it2, t.id);
        abort(t);
        // graphLock.unlock();
        for (auto data : finalOrder) {
          data->data_lock.unlock();
        //   cout << data->id << " abort unlocked\n";
        }
        return 0;
      } else {
        t.incoming_edges.insert(*it2);
      }
    }
  }

  // commit if no cycle is detected
  for (auto it = t.write_set.begin(); it != t.write_set.end(); ++it) {
    Data *d = it->first;
    d->value += t.write_set[d];
    d->write_set.insert(t.id);
  }

  // graphLock.unlock();
  for (auto data : finalOrder) {
    data->data_lock.unlock();
    // cout << data->id << " commit unlocked\n";
  }
  return 1;
}

void initSharedData(int numData) {
  vhead = vtail = NULL;
  create_initial_vertices(1);
  for (int i = 0; i < numData; ++i) {
    Data *d = new Data(0, i);
    sharedMem.push_back(d);
  }
}

#endif
