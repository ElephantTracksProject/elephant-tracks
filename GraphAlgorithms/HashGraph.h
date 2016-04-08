/*
 * HashGraph.h
 *
 *  Created on: Dec 17, 2009
 *      Author: nricci01
 */

#ifndef HASHGRAPH_H_
#define HASHGRAPH_H_

#include "tbb/atomic.h"
#include "tbb/concurrent_hash_map.h"
#include "tbb/concurrent_unordered_set.h"
#include "tbb/concurrent_vector.h"
#include <set>
#include <map>
#include <vector>
#include <utility>
#include <string>

#include <jni.h>

using namespace std;
using namespace tbb;

/*This is a graph data structure for use by ElephantTracks.
  It is not a fully general graph structure, because it makes some assumptions about how
  it will be used.
  In particular, removeNode(n) does not clean up any incoming edges to n, just the out going edges.
  We can get away with this in ET because only dead objects are ever removed, and if n is dead, anything 
  pointing to n also must be dead, and therefore the incoming edge will eventually be removed.
  This saves time/space tracking incoming edges. */

class HashGraph {
public:
  typedef concurrent_vector<atomic<jlong> > EdgeList;
  typedef concurrent_hash_map<jlong,EdgeList> Node2EdgeListMap;

  HashGraph ();
  virtual ~HashGraph ();
  jlong updateEdge (jlong node1, int slot, jlong node2);
  void addNode (jlong node, int slots);
  bool removeNode (jlong);
  pair<EdgeList::const_iterator, EdgeList::const_iterator> neighbors (jlong target);
  string toString () const;
  vector<jlong> getNodes () const;
  void computeReachable (set<jlong> starting);
private:
  Node2EdgeListMap edgeMap;
  EdgeList staticsEdges;
  concurrent_unordered_set<jlong> growable;
};

#endif /* HASHGRAPH_H_ */
