#ifndef TAGTIMEMAP_H_
#define TAGTIMEMAP_H_

#include <iterator>
#include <tbb/concurrent_hash_map.h>
#include "jvmti.h"
#include "TimeStamp.h"

using namespace tbb;
using namespace std;

class TagTimeMap {
public:
  typedef concurrent_hash_map<jlong, TimeStamp> TagTable;
  class Iterator : public iterator<forward_iterator_tag, pair<jlong,TimeStamp> > {
  private:
    TagTable::const_iterator inner_iterator;

  public:
    Iterator (TagTable::const_iterator it) : inner_iterator(it) {}

    Iterator& operator++ () { ++inner_iterator; return *this;}

    Iterator operator++ (int) { return ++inner_iterator; }

    bool operator== (const Iterator lhs) {
      return this->inner_iterator == lhs.inner_iterator;
    }
   
    bool operator!= (const Iterator lhs) {
      return !(*this == lhs);
    }

    const pair<jlong,TimeStamp> operator* () { return *(this->inner_iterator); };
  };

  TagTimeMap() : tag2TimeStamp(TagTable(128)) {};

  TimeStamp zero;
  const TimeStamp& operator[] (jlong tag) const {
    TagTable::const_accessor a;
    bool wasFound = tag2TimeStamp.find(a, tag);
    return wasFound ? (a->second) : zero;
  }

  TimeStamp& operator[] (jlong tag) {
    TagTable::accessor a;
    tag2TimeStamp.insert(a, tag);
    return a->second;
  }

  void eraseTag (jlong tag) {
    // TagTable::const_accessor a;
    tag2TimeStamp.erase(tag);
  }

  TagTimeMap::Iterator begin() {
    return Iterator(tag2TimeStamp.begin());
  }

  TagTimeMap::Iterator end() {
    return Iterator(tag2TimeStamp.end());
  }

private:
  TagTable tag2TimeStamp;
  // TimeStamp zero_timestamp = {0,0};
};
#endif
