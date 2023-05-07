#ifndef DATA_H
#define DATA_H

#include <bits/stdc++.h>
#include <set>
#include <mutex>
#include <vector>

using namespace std;

class Data {
public:
  std::shared_mutex data_lock;
  int value;
  int id;
  std::set<int> read_set;
  std::set<int> write_set;
  Data(int v, int i) : value(v), id(i) {}
};

// std::vector<Data*> data_map;

#endif