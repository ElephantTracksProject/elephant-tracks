#ifndef GRAPHALGORITHMS_H_
#define GRAPHALGORITHMS_H_ 
#include <map>
#include <set>
#include <iostream>
#include "HashGraph.h"
#include "../TagTimeMap.h"

using namespace std;


class GADiedBeforeComparator {
public:
  GADiedBeforeComparator (TagTimeMap o2t):
    oid2time(o2t) { }

  bool operator() (jlong obj1, jlong obj2) {
    return oid2time[obj1].time < oid2time[obj2].time;
  }
private:
  TagTimeMap oid2time;
};
#endif // GRAPHALGORITHMS_H_
