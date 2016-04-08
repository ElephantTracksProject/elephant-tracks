#ifndef FLATTRACEOUTPUTTER_H_
#define FLATTRACEOUTPUTTER_H_
#include <jvmti.h>
#include <map>
#include <vector>
#include <string>
#include "jni.h"
#include "tbb/atomic.h"
#include "Record.h"

using namespace std;

#define DEFAULT_MAX_RECORDS 20000000

// Forward Declarations
class SortAndOutputThread;

class FlatTraceOutputter 
{
public:
  FlatTraceOutputter (jvmtiEnv* jvmti,
		      ostream *out,
		      void (*fullcb)(void));

  FlatTraceOutputter (jvmtiEnv* jvmti,
		      ostream *out,
		      unsigned int recordLimit,
		      void (*fullcb)(void));

  ~FlatTraceOutputter ();

  void doMethodEntry (jlong time,
		      int methodId,
		      jlong receiver,
		      jlong thread);

  void doMethodExit (jlong time,
		     int methodId,
		     jlong receiver,
		     jlong exception,
		     jlong thread);

  void doException (jlong time,
		    int methodId,
		    jlong receiver,
		    jlong exception,
		    jlong thread,
		    ExceptKind kind);

  void doAllocation (jlong time,
		     jlong objectTag,
		     jlong size,
		     int length,
		     string type,
		     jlong thread,
		     AllocKind kind,
		     jint site);

  void doNewObject (jlong time,
		    jlong objectTag,
		    jlong classID,
		    jlong thread);


  void doPointerUpdate (jlong time,
			jlong oldTarget,
			jlong origin,
			jlong newTarget,
			int fieldId,
			jlong thread
			);

  void doSyntheticUpdate (jlong time,
			  jlong oldTarget,
			  jlong origin,
			  jlong newTarget,
			  int fieldId);

  void doVarEvent (jlong time,
		   jlong obj,
		   jlong thread);

  void doCounts (jlong countsTime,
		 int heapReads,
		 int heapWrites,
		 int heapRefReads,
		 int heapRefWrites,
		 int instCount);

  void doDeath (jlong tag,
		TimeStamp deathTime);

  void doGCEnd ();

  void doRoot (jlong time,
	       jlong obj);

  // void flush();

  void initializeOutputThread (jvmtiEnv* jvmti,
			       JNIEnv* jni);

  // Called during VMDeath; wait until we are done outputting any remaing records
  // before ending the process.
  void waitToFinishOutput ();


  // TODO(NPR) performance timer; delete or put behind a debug flag or something.
  double combiningRecords_time;
  double totalSortTime;
  double outputRecords_time;
private:
    
  ostream* output;
  jvmtiEnv*  aJVMTI;

  void initialize (void (*fullcb)(void),
		   unsigned int recordLimit);

  Record **outputBuffer;
  Record **deathBuffer;
  //We keep some slack at the end of the output buffer
  //because we copy death records there to sort.
  //Real buffer size represents the actual allocated size.
  unsigned int realBufferSize;
  unsigned int deathBufferSize;
  //Pointer into outputBuffer, where we can add a new record
  atomic<unsigned int> bufferEnd;
  atomic<unsigned int> deathBufferEnd;
  //How many records have we finished adding to the buffer?
  atomic <unsigned int> recFinishedCount;
  unsigned int maxRecords; //Do a call back if we exceed this number of records
  void (*fullcb)(void);

  vector<char> debugList;
  // jlong currentTime;
  jrawMonitorID  yieldLock;
  jrawMonitorID  fullMon;
  jrawMonitorID  stdoutLock;
  void addRecordToList (Record* rec);
  void addRecordToDeathList (Record* rec);


  atomic<unsigned int> gcsStarted;
  atomic<unsigned int> gcsFinished;

  SortAndOutputThread* sortAndOutputThread;

  // -- Counters
  jlong countMethodEntries;
  jlong countMethodExits;
  jlong countThrows;
  jlong countHandles;
  jlong countAllocations;
  jlong countPointerUpdates;
  jlong countDeaths;
  jlong countVarEvents;
  jlong countCounts;
};

#endif /*FLATTRACEOUTPUTTER_H_*/
