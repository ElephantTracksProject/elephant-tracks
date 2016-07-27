/*
 * HashGraph.cpp
 *
 *  Created on: Dec 17, 2009
 *      Author: nricci01
 */

#include "HashGraph.h"
#include <iostream>
#include <sstream>
#include <stack>
#include <string>

HashGraph::HashGraph () { }

HashGraph::~HashGraph () { }

jlong HashGraph::updateEdge (jlong node1, int slot, jlong node2) {
    if (node1 == 0) {
        // RLV: Not really sure why slot has to be a signed int, but there's
        //      a check for negativeness so maybe slot shouldn't be an
        //      unsigned int. Thus we check if it's positive before casting to
        //      unsigned.
        if ( (slot >= 0) &&
             ((unsigned int) slot >= staticsEdges.size()) ) {
            // grow it
            int needed = (slot+1 - staticsEdges.size());
            atomic<jlong> zero;
            zero = 0;
            staticsEdges.grow_by(needed, zero);
        }
        return staticsEdges[slot].fetch_and_store(node2);
    } else {
        // wanted a const_accessor, but could not figure out how to make everything go
        Node2EdgeListMap::accessor nodeAccess;
        if (!edgeMap.find(nodeAccess, node1)) {
            // object not present!
            cerr << "HashGraph::updateEdge: object not present: 0x" << hex << node1 << endl;
            growable.insert(node1);
            addNode(node1, slot+1);
            if (!edgeMap.find(nodeAccess, node1)) {
                cerr << "HashGraph::updateEdge: failed to add object!" << endl;
                return 0;
            }
        }
        EdgeList *edges = &(nodeAccess->second);
        if ((unsigned int)slot >= (*edges).size()) {
            if (growable.find(node1) == growable.end()) {
                // complain and return
                cerr << "HashGraph::updateEdge: slot " << dec << slot << " out of range "
                    << dec << (*edges).size() << " for object 0x" << hex << node1 << endl;
                return 0;
            }
            // grow it
            int needed = (slot+1 - (*edges).size());
            atomic<jlong> zero;
            zero = 0;
            (*edges).grow_by(needed, zero);
        } else if (slot < 0) {
            cerr << "HashGraph::updateEdge: slot " << dec << slot << " negative "
                << "for object 0x" << hex << node1 << endl;
            return 0;
        }
        return (*edges)[slot].fetch_and_store(node2);
    }
}

void HashGraph::addNode (jlong node, int slots) {
  if (slots < 0) {
    growable.insert(node);
    slots = 0;
  }
  EdgeList newEdges = EdgeList(slots);
  for (int i = slots; --i >= 0; ) {
    newEdges[i] = 0;
  }
  // do this after so that only a fully initialized (zeroed) edge vector is visible;
  // use insert so that we won't over-write in concurrent situation
  edgeMap.insert(make_pair(node, newEdges));
}

bool HashGraph::removeNode (jlong node) {
  return edgeMap.erase(node);
}

pair<HashGraph::EdgeList::const_iterator, HashGraph::EdgeList::const_iterator> HashGraph::neighbors (jlong target) {
  //This is a convenient thing to return if "target" has no neighbors.
  //It needs to be static, otherwise the iterators no longer make any 
  //sense once this method returns; using them would be undefined.
  static EdgeList nullList;
  EdgeList & edges = nullList;

  Node2EdgeListMap::const_accessor acc;
  if (edgeMap.find(acc, target)) {
    edges = acc->second;
  }
  return make_pair(edges.begin(), edges.end());
}

string HashGraph::toString () const {
  stringstream ss;
  for (Node2EdgeListMap::const_iterator it = edgeMap.begin();
       it != edgeMap.end();
       it++) {
    ss << it->first << " :";
    const EdgeList& edges = it->second;
    for (EdgeList::const_iterator jt = edges.begin(); jt != edges.end(); jt++) {
      ss << " " << *jt;
    }
    ss << endl;
  }
  return ss.str();
}

vector<jlong> HashGraph::getNodes () const {
  vector<jlong> node_list;
  for (Node2EdgeListMap::const_iterator it = edgeMap.begin();
       it != edgeMap.end();
       it++) {
    node_list.push_back(it->first);
  }
  return node_list;
}

void HashGraph::computeReachable (set<jlong> reached) {
  stack<jlong> toProcess;
  for (set<jlong>::const_iterator it = reached.begin(); it != reached.end(); it++) {
    toProcess.push(*it);
  }
  while (!toProcess.empty()) {
    pair<EdgeList::const_iterator,EdgeList::const_iterator> beginEnd = neighbors(toProcess.top());
    toProcess.pop();
    for (EdgeList::const_iterator nit = beginEnd.first;
	 nit != beginEnd.second;
	 nit++) {
      if (reached.find(*nit) == reached.end()) {
	// reachable, but not yet processed or queued for processing
	reached.insert(*nit);
	toProcess.push(*nit);
      }
    }
  }
}
