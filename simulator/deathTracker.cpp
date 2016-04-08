
#include "cctree.h"
#include "heap.h"

int debug = 0;

// -- Death records
//    A mapping from time to the set of objects that die at that time

typedef map<long, set<long> > deathMap;

deathMap DeathRecords;

//list of all objects allocated
vector<long> objects;


void processDeathTimes(long time, CCNode * currentMethod)
{
  deathMap::iterator f = DeathRecords.find(time);
  if (f != DeathRecords.end()) {
    set<long> & deaths = (*f).second;

    if (deaths.size() > 100)
      cout << "Got " << deaths.size() << " death records for method " << currentMethod->getMethodFullName() << endl;

    for (set<long>::iterator i = deaths.begin();
	 i != deaths.end();
	 i++)
      {
	long object_id = *i;
	HeapObject * heapObject = HeapObject::DemandHeapObject(object_id);
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
int thread_number = 0;
map<int, int> threadIdNumbering;

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
  int last_epoch_time = 0;
  int last_thread_id = 0;

  while (in.good()) {
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

    in >> kind;
    // in >> hex >> time;

    /*
    if (in_death_records && (kind != 'D')) {
      in_death_records = false;
      last_epoch_time = time;
    }
    */

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
        heapObject = HeapObject::DemandHeapObject(object_id);
        heapObject->setAlloc(time, size, type);
        heapObject->setAllocCC(curContext);
        
        if (debug > 0) {
          cout << "Allocate 0x" << hex << object_id << " size 0x" << size << dec << " at time " << time << endl;
          if (debug > 1) curContext->printStack();
	}	

	last_thread_id = thread_id;
      }
      objects.push_back(object_id);
      break;
    case 'D':
      {
        in >> hex >> object_id;
        
        heapObject = HeapObject::DemandHeapObject(object_id);
        heapObject->setDead(time);
        (HeapObject::Find(heapObject))->incNumDead();
        
        CCNode * curContext = theStack[last_thread_id];
        
        curContext->incDeadBytes(heapObject->getSize());
        curContext->incDeadObjects();
        heapObject->setDeathCC(curContext);
      }
      break;
    case 'U':
      in >> hex >> old_target;
      in >> hex >> object_id;
      in >> hex >> new_target;
      if (new_target != 0) {
	heapObject = HeapObject::DemandHeapObject(object_id);
	targetObject = HeapObject::DemandHeapObject(new_target);
	HeapObject::Union(heapObject, targetObject);
	if (debug > 0) {
	  cout << "Pointer from 0x" << hex << object_id << " to 0x" << new_target << dec << " at time " << time << endl;
	  if (debug > 1) {
	    CCNode * curContext = theStack[last_thread_id];
	    curContext->printStack();
	  }
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
            cout << "Spawn thread 0x" << hex << thread_id << dec << " -- started at " << curContext->getMethodFullName() << endl;
            cout << "   in context" << endl;
            curContext->printStack();
          } else {
            cout << "Problem: no threadStart for thread id 0x" << hex << thread_id << dec << endl;
            curContext = root;
          }
        }
        
        time++;
        depth++;
        
        curContext = curContext->demand_child(method_id, thread_id, time);
        curContext->incCalls();
        theStack[thread_id] = curContext;
        
        if (debug > 0 or new_thread) {
          cout << "Enter " << curContext->getMethodFullName() << " 0x" << hex << method_id << " thread 0x" << thread_id << " at time " << time << endl;
          if (debug > 1) curContext->printStack();
	}
	
	if (method_id == thread_start_method_id) {
	  // -- Found a new thread start
	  threadStarts[object_id] = curContext;
	  thread_number++;
	  threadIdNumbering[object_id] = thread_number;
	  if (true) {
	    cout << "Found Thread.start at " << endl;
	    curContext->printStack();
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
	  cout << hex <<  "E " << method_id << " " << object_id << " " << thread_id << endl;
          //cout << "Exit  " << curContext->getMethodFullName() << " 0x" << hex << method_id << " thread 0x" << thread_id << " at time " << time << endl;
	  cout << "Exit: Looking for " << "0x" << hex << method_id <<  " reciever:  0x" << object_id <<  " thread: 0x" << thread_id << endl;
          if (debug > 1) curContext->printStack();
	}	

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
          returning->printStack();
          // current_node unchanged
          depth = orig_depth;
        } else {
          // cout << "Return " << current_node->getMethodFullName() << "(" << hex << current_node->getMethodId() << dec << ")" << endl;
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

void read_name_file(ifstream & name_file)
{
  while ( ! name_file.eof()) {
    char kind;
    int method_id;
    string class_name;
    string method_name;
    string signature;
    
    name_file >> kind;

    if (kind == 'N') {
      name_file >> hex >> method_id;
      name_file >> class_name;
      name_file >> method_name;
      name_file >> signature;
      Method * method = new Method(method_id, class_name, method_name, signature);
      if (class_name == "java/lang/Thread" and method_name == "start") {
        thread_start_method_id = method_id;
      }
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
  read_name_file(name_file);

  /*
  cout << "Read death records..." << endl;
  ifstream deaths_file;
  deaths_file.open(argv[3]);
  read_deaths_file(deaths_file);
  */

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
  // cout << "Total methods: " << allMethods.size() << endl;
  cout << "Total CCNodes: " << CCNode::count << endl;

  root->computeTotals();
  root->rankNodes();
  // printTree(root, 0);
  
  ofstream treemap;
  treemap.open("jbb.tm3");
  root->emitTreeMapTM3(treemap);
  treemap.close();
  
  ofstream treeml;
  treeml.open("jbb.xml");
  root->emitTreeML(treeml);
  treeml.close();

  map<CCNode*,map<CCNode*, int> > allocDied;
  for(vector<long>::iterator it = objects.begin(); it != objects.end(); it++){
	HeapObject* obj = HeapObject::DemandHeapObject(*it);
	CCNode* allocCC = obj->getAllocCC();
	CCNode* deathCC = obj->getDeathCC();		

	if(allocDied.count(allocCC) == 0 || allocDied[allocCC].count(deathCC) == 0){
		allocDied[allocCC][deathCC] = 1;
	}
	else{
		allocDied[allocCC][deathCC] = allocDied[allocCC][deathCC] + 1;
	}	

  }

  int thresh = objects.size()/100;

  for(map<CCNode*, map<CCNode*, int> >::iterator it = allocDied.begin();
      it != allocDied.end();
      it++  )
  {
	CCNode* allocCC = it->first;
	map<CCNode*, int> deathCCS = it->second;	


	int sum = 0;
	for(map<CCNode*, int>::iterator jt = deathCCS.begin();
            jt != deathCCS.end();
	    jt++)
        {
		CCNode* deathCC = jt->first;
		int numDied = jt->second;
		sum  += numDied;
	}
	
	if(sum > thresh){
		cout << hex << allocCC->getMethodId() << " " << allocCC->getMethodFullName() << " (" << dec << sum << " objects): ";
		int maxDeathNum = -1;
		for(map<CCNode*, int>::iterator jt = deathCCS.begin();
		    jt != deathCCS.end();
		    jt++){
		        if(jt->second > maxDeathNum){
				maxDeathNum = jt->second;
			}
		   }
		cout << maxDeathNum << "(" << 100.0 * (float)maxDeathNum/sum << "%)" ;
		cout << " # of death CCS: " << dec << deathCCS.size() << endl;
	}
	
  }


  /*
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
  */
}

