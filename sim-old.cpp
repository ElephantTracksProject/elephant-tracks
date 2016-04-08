#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdlib.h>

using namespace std;

int debug = 0;

class CCNode;
class Method;

typedef map<long, CCNode *> CCNodeMap;
typedef map<int, Method *> MethodMap;
typedef vector<CCNode *> CCNodeVector;

typedef map<long, set<long> > deathMap;

class HeapObject
{
private:

  int id;
  string type;
  int bytes;
  bool live;

  int alloc_time;
  CCNode * alloc_cc;
  
  int death_time;
  CCNode * death_cc;
  
  // -- union/find stuff
  HeapObject * parent;
  int rank;

  int size;
  int num_dead;

public:

  HeapObject(int i /*, const string & ty, int sz, int a_time*/ )
    : id(i),
      type("UNKNOWN"),
      bytes(-1),
      live(true),
      alloc_time(-1),
      alloc_cc(0),
      death_time(-1),
      death_cc(0),
      parent(0), 
      rank(0),
      size(1),
      num_dead(0)
  {}

  int getId() const { return id; }
  
  const string & getType() const { return type; }

  void setAlloc(int a_time, int sz, const string & ty) {
    alloc_time = a_time;
    size = sz;
    type = ty;
  }
  
  void setDead(int d_time) { 
    live = false;
    death_time = d_time;
  }

  bool isDead() const { return live; }

  int getAllocTime() const { return alloc_time; }
  int getDeathTime() const { return death_time; }

  CCNode * getAllocCC() const { return alloc_cc; }
  void setAllocCC(CCNode * cc) { alloc_cc = cc; }
  
  CCNode * getDeathCC() const { return death_cc; }
  void setDeathCC(CCNode * cc) { death_cc = cc; }
  
  void incRank() { rank++; }
  int getRank() const { return rank; }

  void setParent(HeapObject * new_parent) {
    parent = new_parent;
  }

  HeapObject * getParent() { return parent; }

  void setSize(int new_size) { size = new_size; }
  int getSize() const { return size; }

  void setNumDead(int nd) { num_dead = nd; }
  void incNumDead() { num_dead++; }
  int getNumDead() const { return num_dead; }

  // -- Disjoint sets operations

  static HeapObject * Find(HeapObject * obj);
  static void Union(HeapObject * one, HeapObject * two);
};

HeapObject * HeapObject::Find(HeapObject * obj)
{
  HeapObject * parent = obj->getParent();
  if (parent == 0)
    return obj;

  HeapObject * root = Find(parent);
  obj->setParent(root);

  return root;
}

void HeapObject::Union(HeapObject * one, HeapObject * two)
{
  HeapObject * one_root = Find(one);
  HeapObject * two_root = Find(two);

  if (one_root == two_root)
    return;

  int new_size = one_root->getSize() + two_root->getSize();
  int new_nd = one_root->getNumDead() + two_root->getNumDead();

  int one_rank = one_root->getRank();
  int two_rank = two_root->getRank();

  if (one_rank < two_rank) {
    // -- Two becomes root
    one_root->setParent(two_root);
    two_root->setSize(new_size);
    two_root->setNumDead(new_nd);
  } else {
    if (one_rank > two_rank) {
      // -- One becomes root
      two_root->setParent(one_root);
      one_root->setSize(new_size);
      one_root->setNumDead(new_nd);
    } else {
      // -- Pick one, and increment rank
      two_root->setParent(one_root);
      one_root->setSize(new_size);
      one_root->incRank();
      one_root->setNumDead(new_nd);
    }
  }   
}

// -- Heap management

typedef map<int, HeapObject *> HeapMap;
HeapMap theHeap;

HeapObject * DemandHeapObject(int object_id)
{
  HeapObject * result;
  HeapMap::iterator f = theHeap.find(object_id);
  if (f != theHeap.end()) {
    result = (*f).second;
  } else {
    result = new HeapObject(object_id /*, "UNKNOWN", 0, 0 */);
    theHeap[object_id] = result;
  }

  return result;
} 

class Method
{
private:

  int id;
  string class_name;
  string method_name;
  string signature;

public:

  Method(int i, string cn, string mn, string sig)
    : id(i),
      class_name(cn),
      method_name(mn),
      signature(sig)
  {}

  const string & getClassName() const { return class_name; }
  const string & getMethodName() const { return method_name; }
  const string & getSignature() const { return signature; }
};


class CCNode
{
private:

  int method_id;
  int thread_id;
  CCNode * parent;
  CCNodeVector children;

  int calls;

  int first_call;
  int last_call;

  int alloc_bytes;
  int alloc_objects;
  int dead_bytes;
  int dead_objects;

  int total_alloc_bytes;
  int total_alloc_objects;
  int total_dead_bytes;
  int total_dead_objects;

  int alloc_rank;

public:

  static int count;

  CCNode(int id, int thread_id, int time, CCNode * par)
    : method_id(id),
      thread_id(thread_id),
      parent(par),
      calls(0),
      first_call(time),
      last_call(time),
      alloc_bytes(0),
      alloc_objects(0),
      dead_bytes(0),
      dead_objects(0),
      total_alloc_bytes(0),
      total_alloc_objects(0),
      total_dead_bytes(0),
      total_dead_objects(0),
      alloc_rank(0)
  {
    count++;
  }

  CCNode * demand_child(int id, int thread_id, int time)
  {
    for (CCNodeVector::iterator p = children.begin();
	 p != children.end();
	 p++)
      {
	CCNode * child = (*p);
	if (child->method_id == id) {
	  child->setLastCall(time);
	  return child;
	}
      }

    // -- Not Found
    CCNode * new_child = new CCNode(id, thread_id, time, this);
    children.push_back(new_child);
    return new_child;
  }

  int getMethodId() const { return method_id; }
  
  int getThreadId() const { return thread_id; }

  CCNode * getParent() const { return parent; }

  const CCNodeVector & getChildren() const { return children; }

  void incCalls() { calls++; }
  int getCalls() const { return calls; }

  void setLastCall(int time) { last_call = time; }
  int getLastCall() const { return last_call; }
  int getFirstCall() const { return first_call; }

  void incAllocBytes(int bytes) { alloc_bytes += bytes; }
  int getAllocBytes() const { return alloc_bytes; }

  void incAllocObjects() { alloc_objects++; }
  int getAllocObjects() const { return alloc_objects; }

  void incDeadBytes(int bytes) { dead_bytes += bytes; }
  int getDeadBytes() const { return dead_bytes; }

  void incDeadObjects() { dead_objects++; }
  int getDeadObjects() const { return dead_objects; }

  void incTotalAllocBytes(int total) { total_alloc_bytes += total; }
  int getTotalAllocBytes() const { return total_alloc_bytes; }

  void incTotalAllocObjects(int total) { total_alloc_objects += total; }
  int getTotalAllocObjects() const { return total_alloc_objects; }

  void incTotalDeadBytes(int total) { total_dead_bytes += total; }
  int getTotalDeadBytes() const { return total_dead_bytes; }

  void incTotalDeadObjects(int total) { total_dead_objects += total; }
  int getTotalDeadObjects() const { return total_dead_objects; }

  void setAllocRank(int rank) { alloc_rank = rank; }
  int getAllocRank() const { return alloc_rank; }
};


int CCNode::count = 0;
MethodMap allMethods;

int thread_number = 0;
map<int, int> threadIdNumbering;

void computeTotals(CCNode * node)
{
  // -- Start with this node's values

  node->incTotalAllocBytes(node->getAllocBytes());
  node->incTotalAllocObjects(node->getAllocObjects());
  node->incTotalDeadBytes(node->getDeadBytes());
  node->incTotalDeadObjects(node->getDeadObjects());

  // -- Compute totals for any children, then add up
  
  const CCNodeVector & children = node->getChildren();
  for (CCNodeVector::const_iterator p = children.begin();
       p != children.end();
       p++)
    {
      CCNode * child = (*p);
      computeTotals(child);

      node->incTotalAllocBytes(child->getTotalAllocBytes());
      node->incTotalAllocObjects(child->getTotalAllocObjects());
      node->incTotalDeadBytes(child->getTotalDeadBytes());
      node->incTotalDeadObjects(child->getTotalDeadObjects());
    }
}

string getMethodFullName(CCNode * node)
{
  Method * meth = allMethods[node->getMethodId()];
  string result;
  if (meth)
    result = meth->getClassName() + "::" + meth->getMethodName();
  else
    result = "ENTRY";

  return result;
}

void printTree(CCNode * node, int depth)
{
  if (node->getTotalAllocBytes() == 0 and
      node->getTotalAllocObjects() == 0)
    return;

  for (int i = 0; i < depth; i++)
    cout << ".";

  if (depth > 0)
    cout << " ";

  cout << getMethodFullName(node) << " ";

  cout << node->getTotalAllocBytes() << " "
       << node->getAllocRank() << " "
       << node->getTotalAllocObjects() << "  "
       << node->getCalls() << endl;

  const CCNodeVector & children = node->getChildren();
  for (CCNodeVector::const_iterator p = children.begin();
       p != children.end();
       p++)
    {
      CCNode * child = (*p);
      printTree(child, depth+1);
    }
}

void collectNodes(CCNode * node, CCNodeVector & all)
{
  all.push_back(node);
  const CCNodeVector & children = node->getChildren();
  for (CCNodeVector::const_iterator p = children.begin();
       p != children.end();
       p++)
    {
      CCNode * child = (*p);
      collectNodes(child, all);
    }
}  

bool compareNodes(CCNode * one, CCNode * two)
{
  return one->getTotalAllocBytes() < two->getTotalAllocBytes();
}

void rankNodes(CCNode * root)
{
  CCNodeVector all;
  collectNodes(root, all);
  sort(all.begin(), all.end(), compareNodes);

  int rank = all.size();
  for (CCNodeVector::const_iterator p = all.begin();
       p != all.end();
       p++)
    {
      CCNode * child = (*p);
      child->setAllocRank(rank);
      rank--;
    }
}

void printStack(CCNode * node)
{
  CCNode * cur = node;
  while (cur) {
    cout << "  " << getMethodFullName(cur) << "(0x" << hex << cur->getMethodId() << dec << ")" << endl;
    cur = cur->getParent();
  }
}

void emitPath(CCNode * node, ofstream & out)
{
  CCNode * parent = node->getParent();
  if (parent != 0)
    emitPath(parent, out);

  out << "\t" << getMethodFullName(node);
}

void emitTreeMapTM3Rec(ofstream & out, CCNode * node)
{
  int abytes = node->getTotalAllocBytes();
  int dbytes = node->getTotalDeadBytes();
  
  if (abytes > 0 or dbytes > 0) {
    const CCNodeVector & children = node->getChildren();
    
    // -- Compute total size of children
    int child_total_alloc = 0;
    int child_total_dead = 0;
    int num_children = 0;
    for (CCNodeVector::const_iterator p = children.begin();
      p != children.end();
      p++)
    {
      CCNode * child = (*p);
      int child_alloc = child->getTotalAllocBytes();
      int child_dead =  child->getTotalDeadBytes();
      child_total_alloc += child_alloc;
      child_total_dead  += child_dead;
      if (child_alloc > 0 or child_dead > 0)
        num_children++;
    }
    
    // -- Output entry for this node
    out << (abytes+dbytes) << "\t";
    out << node->getCalls() << "\t";
    out << "0" << "\t";
    out << node->getThreadId() << "\t";
    out << node->getFirstCall() << "\t";
    emitPath(node, out);
    out << endl;
    
    // -- output synthetic node representing the this node's contribution (if necessary)
    if (num_children > 0) {
      int alloc_diff = abytes - child_total_alloc;
      int dead_diff = dbytes - child_total_dead;
      if (alloc_diff > 0) {
        out << alloc_diff << "\t";
        out << node->getCalls() << "\t";
        out << "0" << "\t";
        out << node->getThreadId() << "\t";
        out << node->getFirstCall() << "\t";
        emitPath(node, out);
        out << "\talloc";
        out << endl;
      }
      if (dead_diff > 0) {
        out << dead_diff << "\t";
        out << node->getCalls() << "\t";
        out << "0" << "\t";
        out << node->getThreadId() << "\t";
        out << node->getFirstCall() << "\t";
        emitPath(node, out);
        out << "\tdead";
        out << endl;
      }
    }
    
    // -- Output the children
    for (CCNodeVector::const_iterator p = children.begin();
      p != children.end();
      p++)
    {
      CCNode * child = (*p);
      emitTreeMapTM3Rec(out, child);
    }
  }
}

void emitTreeMapTM3(ofstream & out, CCNode * root)
{
  out << "Bytes\tCalls\tLifetime\tThread\tFirst call" << endl;
  out << "INTEGER\tINTEGER\tINTEGER\tINTEGER\tINTEGER" << endl;
  
  emitTreeMapTM3Rec(out, root);
}

/* emitTreeML

   Emit the calling context tree in TreeML format, for use in prefuse
*/

void emitTreeMLRec(ofstream & out, CCNode * node, int depth);

void emitTreeML(ofstream & out, CCNode * root)
{
  out << "<tree>" << endl;
  out << " <declarations>" << endl;
  out << "  <attributeDecl name=\"name\" type=\"String\"/>" << endl;
  out << "  <attributeDecl name=\"class\" type=\"String\"/>" << endl;
  out << "  <attributeDecl name=\"bytes\" type=\"Long\"/>" << endl;
  out << "  <attributeDecl name=\"alloc\" type=\"Long\"/>" << endl;
  out << "  <attributeDecl name=\"dead\" type=\"Long\"/>" << endl;
  out << "  <attributeDecl name=\"objects\" type=\"Long\"/>" << endl;
  out << "  <attributeDecl name=\"objalloc\" type=\"Long\"/>" << endl;
  out << "  <attributeDecl name=\"objdead\" type=\"Long\"/>" << endl;
  out << "  <attributeDecl name=\"thread\" type=\"Long\"/>" << endl;
  out << " </declarations>" << endl;
  
  emitTreeMLRec(out, root, 1);
  
  out << "</tree>" << endl;
}

void emitTreeMLRec(ofstream & out, CCNode * node, int depth)
{
  int abytes = node->getTotalAllocBytes();
  int dbytes = node->getTotalDeadBytes();
  
  if (abytes + dbytes > 0) {
    
    const CCNodeVector & children = node->getChildren();
    bool is_leaf = (children.size() == 0);
    
    if (is_leaf) {
      // -- Leaf
      out << "<leaf>" << endl;
    } else {
      out << "<branch>" << endl;
    }
    
    Method * meth = allMethods[node->getMethodId()];
    string nm;
    string classnm;
    if (meth) {
      nm = meth->getMethodName();
      classnm = meth->getClassName();
    } else {
      nm = "ENTRY";
      classnm = "";
    }
    
    int i = nm.find("<init>");
    if (i != string::npos)
      nm.replace(i, i+6, "&lt;init&gt;");
    int j = nm.find("<clinit>");
    if (j != string::npos)
      nm.replace(j, j+8, "&lt;clinit&gt;");
    out << "<attribute name=\"name\" value=\"" << nm << "\"/>" << endl;
    out << "<attribute name=\"class\" value=\"" << classnm << "\"/>" << endl;
    out << "<attribute name=\"bytes\" value=\"" << (abytes+dbytes) << "\"/>" << endl;
    out << "<attribute name=\"alloc\" value=\"" << abytes << "\"/>" << endl;
    out << "<attribute name=\"dead\" value=\"" << dbytes << "\"/>" << endl;
    out << "<attribute name=\"objects\" value=\"" << (node->getAllocObjects() + node->getDeadObjects()) << "\"/>" << endl;
    out << "<attribute name=\"objalloc\" value=\"" << node->getAllocObjects() << "\"/>" << endl;
    out << "<attribute name=\"objdead\" value=\"" << node->getDeadObjects() << "\"/>" << endl;
    out << "<attribute name=\"thread\" value=\"" << threadIdNumbering[node->getThreadId()] << "\"/>" << endl;
    
    for (CCNodeVector::const_iterator p = children.begin();
      p != children.end();
      p++)
    {
      CCNode * child = (*p);
      emitTreeMLRec(out, child, depth+1);
    }
    
    if (is_leaf) {
      // -- Leaf
      out << "</leaf>" << endl;
    } else {
      out << "</branch>" << endl;
    }
  }
}

// -- Death records
//    A mapping from time to the set of objects that die at that time

deathMap DeathRecords;

void processDeathTimes(long time, CCNode * currentMethod)
{
  deathMap::iterator f = DeathRecords.find(time);
  if (f != DeathRecords.end()) {
    set<long> & deaths = (*f).second;

    if (deaths.size() > 100)
      cout << "Got " << deaths.size() << " death records for method " << getMethodFullName(currentMethod) << endl;

    for (set<long>::iterator i = deaths.begin();
	 i != deaths.end();
	 i++)
      {
	long object_id = *i;
	HeapObject * heapObject = DemandHeapObject(object_id);
	heapObject->setDead(time);
	(HeapObject::Find(heapObject))->incNumDead();

	currentMethod->incDeadBytes(heapObject->getSize());
	currentMethod->incDeadObjects();
	heapObject->setDeathCC(currentMethod);
      }
  }
}

// -- Global counters

int total_alloc_size = 0;
int record_count = 0;
int no_alloc = 0;

// -- Multi-threaded stack
typedef map<int, CCNode *> StackMap;
StackMap theStack;

//    Map from thread objects to the context in which start() was called
StackMap threadStarts;
int thread_start_method_id = 0;

void read_trace_file(ifstream & in, CCNode * root)
{
  HeapObject * heapObject;
  HeapObject * targetObject;

  // CCNode * current_node = root;
  theStack[0] = root;
  threadStarts[0] = root;
  CCNode * node;
  int depth = 0;
  int time = 0;
  bool in_death_records = false;
  int last_thread_id = 0;

  while ( ! in.eof()) {
    char kind;
    string line;
    int object_id;
    int dtime;
    int size;
    string type;
    int method_id;
    string class_name;
    string method_name;
    string signature;
    int old_target;
    int new_target;
    int thread_id;

    Method * method;
    
    if (record_count > 400000000) {
      debug = 1;
    }

    in >> kind;

    switch (kind) {
    case 'A':
      {
        in >> hex >> object_id;
        in >> hex >> size;
        in >> type;
        in >> hex >> thread_id;
        
        CCNode * curContext = 0;
        
        if (thread_id == object_id) {
          // -- Spawning a new thread -- make it a child of the root
          curContext = root; // theStack[last_thread_id];        
          CCNode * newContext = curContext->demand_child(method_id, thread_id, time);
          newContext->incCalls();
          theStack[thread_id] = newContext;
          if (debug > 0)
            cout << "Spawn thread 0x" << hex << thread_id << dec << endl;
        }
        
        curContext = theStack[thread_id];
        
        curContext->incAllocBytes(size);
        curContext->incAllocObjects();
        heapObject = DemandHeapObject(object_id);
        heapObject->setAlloc(time, size, type);
        heapObject->setAllocCC(curContext);
        
        if (debug > 0) {
          cout << "Allocate 0x" << hex << object_id << " size 0x" << size << dec << " at time " << time << endl;
          if (debug > 1) printStack(curContext);
	}	

	last_thread_id = thread_id;
      }
      break;
    case 'D':
      in >> hex >> object_id;
      in >> hex >> dtime;
      // in >> hex >> thread_id;
      break;
    case 'U':
      in >> hex >> old_target;
      in >> hex >> object_id;
      in >> hex >> new_target;
      if (new_target != 0) {
	heapObject = DemandHeapObject(object_id);
	targetObject = DemandHeapObject(new_target);
	HeapObject::Union(heapObject, targetObject);
	if (debug > 0) {
	  cout << "Pointer from 0x" << hex << object_id << " to 0x" << new_target << dec << " at time " << time << endl;
	  if (debug > 1) {
	    CCNode * curContext = theStack[last_thread_id];
	    printStack(curContext);
	  }
	}
      }
      break;
    case 'N':
      in >> hex >> method_id;
      in >> class_name;
      in >> method_name;
      in >> signature;
      method = new Method(method_id, class_name, method_name, signature);
      allMethods[method_id] = method;
      if (class_name == "java/lang/Thread" and method_name == "start") {
        thread_start_method_id = method_id;
        if (true) { // debug > 0) {
          cout << "Found Thread.start() == 0x" << hex << thread_start_method_id << dec << endl;
        }
      }
      break;
    case 'M':
      {
        in >> hex >> method_id;
        in >> hex >> object_id;
        in >> hex >> thread_id;
        
        CCNode * curContext = theStack[thread_id];
        
        bool new_thread = false;
        if (curContext == 0) {
          // -- Spawning a new thread -- look up the place where the thread was started
          //    Relies on the fact that the thread_id is the same as the object_id of
          //    the Thread object instance.
          new_thread = true;
          curContext = threadStarts[thread_id];
          if (curContext) {
          // if (debug > 0)
            cout << "Spawn thread 0x" << hex << thread_id << dec << " -- started at " << getMethodFullName(curContext) << endl;
            cout << "   in context" << endl;
            printStack(curContext);
          } else {
            cout << "Problem: no threadStart for thread id 0x" << hex << thread_id << dec << endl;
            curContext = root;
          }
        }
        
        processDeathTimes(time, curContext);
        time++;
        depth++;
        curContext = curContext->demand_child(method_id, thread_id, time);
        curContext->incCalls();
        theStack[thread_id] = curContext;
        
        if (debug > 0 or new_thread) {
          cout << "Enter " << getMethodFullName(curContext) << " 0x" << hex << method_id << " thread 0x" << thread_id << " at time " << time << endl;
          if (debug > 1) printStack(curContext);
	}
	
	if (method_id == thread_start_method_id) {
	  // -- Found a new thread start
	  threadStarts[object_id] = curContext;
	  thread_number++;
	  threadIdNumbering[object_id] = thread_number;
	  if (true) {
	    cout << "Found Thread.start at " << endl;
	    printStack(curContext);
	  }
	}
        
        last_thread_id = thread_id;
      }
      break;
    case 'E':
      {
        in >> hex >> method_id;
        in >> hex >> object_id;
        in >> hex >> thread_id;
        
        CCNode * curContext = theStack[thread_id];

        if (debug > 0) {
          cout << "Exit  " << getMethodFullName(curContext) << " 0x" << hex << method_id << " thread 0x" << thread_id << " at time " << time << endl;
          if (debug > 1) printStack(curContext);
	}	

        processDeathTimes(time, curContext);
        time++;
        
        CCNode * returning = curContext;
        int cur_id = returning->getMethodId();
        int orig_depth = depth;
        while (returning and returning->getMethodId() != method_id) {
          returning = returning->getParent();
          depth--;
        }
        
        if (returning == 0) {
          cout << "THIS IS BAD: looking for " 
               << hex << "0x" << method_id << " but found 0x" 
               << cur_id << dec << endl;
          printStack(returning);
          // current_node unchanged
          depth = orig_depth;
        } else {
          // cout << "Return " << getMethodFullName(current_node) << "(" << hex << current_node->getMethodId() << dec << ")" << endl;
          returning->setLastCall(time);
          theStack[thread_id] = returning->getParent();
          depth--;
        }
        
        last_thread_id = thread_id;
      }
      break;
    default:
      cout << "UNKNOWN" << endl;
      break;
    }

    getline(in, line);

    record_count++;
    if (record_count % 1000000 == 0) {
      cerr << "At " << record_count << endl;
    }
  }
}

void read_deaths_file(ifstream & deaths_file)
{
  while ( ! deaths_file.eof()) {
    char kind;
    int object_id;
    int dtime;

    deaths_file >> kind;

    if (kind == 'D') {
      deaths_file >> hex >> object_id;
      deaths_file >> hex >> dtime;

      DeathRecords[dtime].insert(object_id);
    }
  }
}

int main(int argc, char * argv[])
{
  if (argc < 3) {
    cout << "Usage: cctree <tracefile> <namesfile> <deathsfile>" << endl;
    exit(0);
  }

  CCNode * root = new CCNode(0, 0, 0, 0);

  cout << "Read names..." << endl;

  ifstream name_file;
  name_file.open(argv[2]);
  read_trace_file(name_file, root);

  cout << "Read death records..." << endl;
  ifstream deaths_file;
  deaths_file.open(argv[3]);
  read_deaths_file(deaths_file);

  cout << "Read events..." << endl;

  ifstream in;
  in.open(argv[1]);

  /*
  epoch_one = new Epoch(0);
  epoch_two = 0;
  */

  // methodAtTime = new CCNode * [350000000];

  read_trace_file(in, root);

  cout << "Total bytes: " << total_alloc_size << endl;
  cout << "Total methods: " << allMethods.size() << endl;
  cout << "Total CCNodes: " << CCNode::count << endl;

  computeTotals(root);
  rankNodes(root);
  // printTree(root, 0);
  
  ofstream treemap;
  treemap.open("jbb.tm3");
  emitTreeMapTM3(treemap, root);
  treemap.close();
  
  ofstream treeml;
  treeml.open("jbb.xml");
  emitTreeML(treeml, root);
  treeml.close();

  int histogram[20];
  for (int i = 0; i < 20; i++) histogram[i] = 0;

  for (HeapMap::iterator i = theHeap.begin();
       i != theHeap.end();
       i++)
    {
      HeapObject * obj = (*i).second;

      long lifetime = obj->getDeathTime() - obj->getAllocTime();
      if (lifetime > 0) {
	int place = 0;
	while (lifetime > 0) {
	  lifetime = lifetime/2;
	  place++;
	}
	if (place > 20) place = 20;
	histogram[place-1]++;
      }
    }
  cout << "Total objects: " << theHeap.size() << endl;
  for (int i = 0; i < 20; i++) {
    cout << "2^" << i << ": " << histogram[i] << endl;
  } 
}
