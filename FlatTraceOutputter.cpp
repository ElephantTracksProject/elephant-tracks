#include <iostream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <jvmti.h>
#include <agent_util.h>
#include <limits.h>
#include <assert.h>
#include <set>
#include "AllocationRecord.h"
#include "CountsRecord.h"
#include "DeathRecord.h"
#include "ETCallBackHandler.h"
#include "ExceptionRecord.h"
#include "ExceptionalExitRecord.h"
#include "FlatTraceOutputter.h"
#include "main.h"
#include "MethodEntryRecord.h"
#include "MethodNameRecord.h"
#include "MethodExitRecord.h"
#include "NewObjectRecord.h"
#include "PointerUpdateRecord.h"
#include "Record.h"
#include "RootRecord.h"
#include "SortAndOutputThread.h"

using namespace std;

FlatTraceOutputter::FlatTraceOutputter (jvmtiEnv* jvmti,
					ostream *out,
					void (*fullcb)(void) )
  : combiningRecords_time(0),
    totalSortTime(0),
    outputRecords_time(0),
    output(out),
    aJVMTI(jvmti)
    {
  initialize(fullcb, DEFAULT_MAX_RECORDS);
}

FlatTraceOutputter::FlatTraceOutputter (jvmtiEnv* jvmti,
					ostream *out,
					unsigned int recordLimit,
					void (*fullcb)(void))
  : 
    totalSortTime(0),
    output(out),
    aJVMTI(jvmti)
{
  initialize(fullcb, recordLimit);
}

/*Set up some monitors, zero some counters, allocate some buffers */
void FlatTraceOutputter::initialize (void (*fullcb)(void),
				     unsigned int recordLimit) {
  jvmtiError err;

  err = aJVMTI->CreateRawMonitor("yieldLock", &yieldLock);
  check_jvmti_error(aJVMTI, err, "FlatTraceOutputter::FlatTraceOutputter Could not create yieldLock");

  err = aJVMTI->CreateRawMonitor("fullMon", &fullMon);
  check_jvmti_error(aJVMTI, err, "FlatTraceOutputter::FlatTraceOutputter Could not create fullMon");

  err = aJVMTI->CreateRawMonitor("stdoutLock", &stdoutLock);
  check_jvmti_error(aJVMTI, err, "FlatTraceOutputter::FlatTraceOutputter Could not create stdoutLock");

  //We need to keep some slack at the end of this buffer
  //Because we will copy death records onto the end of it for sorting
  realBufferSize = (unsigned int)(recordLimit * 5);
  deathBufferSize = 4*recordLimit;

  assert(recordLimit + deathBufferSize == realBufferSize);

  outputBuffer = new Record *[realBufferSize];
  deathBuffer = new Record *[deathBufferSize];
  maxRecords = recordLimit;
  bufferEnd = 0;
  recFinishedCount = 0;
  deathBufferEnd = 0;
  gcsStarted = 0;
  gcsFinished = 0;
  sortAndOutputThread = new SortAndOutputThread(aJVMTI, output);

  // -- Counters
  countMethodEntries = 0;
  countMethodExits = 0;
  countThrows = 0;
  countHandles = 0;
  countAllocations = 0;
  countPointerUpdates = 0;
  countDeaths = 0;
  countVarEvents = 0;
  countCounts = 0;

  this->fullcb = fullcb;
}

FlatTraceOutputter::~FlatTraceOutputter () { }

void FlatTraceOutputter::doMethodEntry (jlong time,
					int methodId,
					jlong receiver,
					jlong thread) {
  Record *entryRecord = NULL;

  entryRecord = new MethodEntryRecord(methodId, receiver, thread);
  entryRecord->setTime(time);
  addRecordToList(entryRecord);
  countMethodEntries++;
}

void FlatTraceOutputter::doMethodExit (jlong time,
				       int methodId,
				       jlong receiver,
				       jlong exception,
				       jlong thread) {
  Record *exitRecord = NULL;

  if (exception == 0) {
    exitRecord = new MethodExitRecord(methodId, receiver, thread);
  }
  else {
    exitRecord = new ExceptionalExitRecord(methodId, receiver, exception, thread);
  }

  exitRecord->setTime(time);
  addRecordToList(exitRecord);
  countMethodExits++;
}

void FlatTraceOutputter::doException (jlong time,
				      int methodId,
				      jlong receiver,
				      jlong exception,
				      jlong thread,
				      ExceptKind kind) {
  Record *excRecord = NULL;

  excRecord = new ExceptionRecord(methodId, receiver, exception, thread, kind);
  excRecord->setTime(time);
  addRecordToList(excRecord);
  if (kind == ExceptThrow) {
    countThrows++;
  } else {
    countHandles++;
  }
}

void FlatTraceOutputter::doPointerUpdate (jlong time,
					  jlong oldTarget,
					  jlong origin,
					  jlong newTarget,
					  int fieldId,
					  jlong thread
					  ) {
  Record *updateRecord = NULL;
	
  updateRecord = new PointerUpdateRecord(oldTarget,
					 origin,
					 newTarget,
					 fieldId,
					 thread
					 );
  updateRecord->setTime(time);	
  addRecordToList(updateRecord);
  countPointerUpdates++;
}

void FlatTraceOutputter::doSyntheticUpdate (jlong time,
					    jlong oldTarget,
					    jlong origin,
					    jlong newTarget,
					    int fieldId) {
  Record *updateRecord = NULL;
	
  updateRecord = new PointerUpdateRecord(oldTarget,
					 origin,
					 newTarget,
					 fieldId,
					 0
					 );
  updateRecord->setTime(time);	
  addRecordToDeathList(updateRecord);
  countPointerUpdates++;
}

void FlatTraceOutputter::doAllocation (jlong time,
				       jlong objectTag,
				       jlong size,
				       int length,
				       string type,
				       jlong thread,
				       AllocKind kind,
				       jint site) {
  Record *allocationRecord = NULL;

  allocationRecord = new AllocationRecord(objectTag, size, length, type, thread, kind, site);
  allocationRecord->setTime(time);
  addRecordToList(allocationRecord);
  countAllocations++;
}

void FlatTraceOutputter::doNewObject (jlong time,
				      jlong objectTag,
				      jlong classID,
				      jlong thread) {
  Record *newObjectRecord = NULL;

  newObjectRecord = new NewObjectRecord(objectTag, classID, thread);
  newObjectRecord->setTime(time);
  addRecordToList(newObjectRecord);
}

void FlatTraceOutputter::doRoot (jlong time,
				 jlong obj) {
  Record *rootRecord = NULL;

  rootRecord = new RootRecord(obj,0);
  rootRecord->setTime(time);
  addRecordToList(rootRecord);
}

void FlatTraceOutputter::doVarEvent (jlong time,
				     jlong obj,
				     jlong thread) {
  Record *rootRecord = NULL;

  rootRecord = new RootRecord(obj, thread);
  rootRecord->setTime(time);
  addRecordToList(rootRecord);

  countVarEvents++;
}

void FlatTraceOutputter::doCounts (jlong countsTime,
				   int heapReads,
				   int heapWrites,
				   int heapRefReads,
				   int heapRefWrites,
				   int instCount) {
  Record *countsRecord = NULL;

  countsRecord = new CountsRecord(countsTime, heapReads, heapWrites, heapRefReads, heapRefWrites, instCount);
  addRecordToList(countsRecord);

  countCounts++;
}

void FlatTraceOutputter::doDeath (jlong tag,
				  TimeStamp deathTime) {
  Record *deathRecord = NULL;
	
  deathRecord = new DeathRecord(tag, deathTime.time, deathTime.thread);
  addRecordToDeathList(deathRecord);

  countDeaths++;
}


void FlatTraceOutputter::initializeOutputThread (jvmtiEnv* jvmti,
						 JNIEnv* jni) {
  sortAndOutputThread->initialize(jvmti, jni);
}


void FlatTraceOutputter::doGCEnd () {
  stringstream verbose_string;
 
  verbose_string << "GCEND: Processing "
       << "   A " << countAllocations 
       << "   D " << countDeaths
       << "   U " << countPointerUpdates
       << "   M " << countMethodEntries
       << "   E " << countMethodExits 
       << "   V " << countVarEvents
       << "   C " << countCounts
       << endl;

  verbose_println(verbose_string.str());

  memcpy (&outputBuffer[recFinishedCount], deathBuffer, deathBufferEnd * sizeof(Record*));
  unsigned int combinedBufferSize =  recFinishedCount + deathBufferEnd;

  sortAndOutputThread->doParallelSortAndOutput(outputBuffer, combinedBufferSize);

  outputBuffer = new Record *[realBufferSize];
  
  (*output) << dec;
  bufferEnd = 0;
  recFinishedCount = 0;
  deathBufferEnd = 0;
}



void FlatTraceOutputter::addRecordToDeathList (Record* rec) {
  unsigned int index = deathBufferEnd++;
  if (index >= deathBufferSize) {
    cerr << "FlatTraceOutputter::addRecordToDeathList: Error, death buffer full." << endl;
    exit(1);
  }
  deathBuffer[index] = rec;
}

void FlatTraceOutputter::addRecordToList (Record *rec) {
  jvmtiError err;
#ifdef ET_DEBUG
  jthread t;
  aJVMTI->GetCurrentThread(&t);
  int loops = 0;
#endif

  while (true) {	
#ifdef ET_DEBUG
    loops++;
#endif

    unsigned int index = bufferEnd++;
    if (index < maxRecords) {
      outputBuffer[index] = rec;
      ++recFinishedCount;

#ifdef ET_DEBUG
      if (loops > 1) {
	aJVMTI->RawMonitorEnter(stdoutLock);
	cout << "(index < maxRecords): succeeded on try " << loops << " " << t << endl;
	aJVMTI->RawMonitorExit(stdoutLock);
      }
#endif
      break;

    } else if (index == maxRecords) {
      // Because we always get index via an atomic fetch and add,
      // There should be no possible way for two threads to get
      // into this case at the same time; thus this does not dead lock
			
      // Yield lock is basically a hack to make a JNI thread yield,
      // since we may need to wait for other threads to finish inserting 
      // their records.
      // Probably there is a better way to do this.
      err = aJVMTI->RawMonitorEnter(yieldLock);
      check_jvmti_error(aJVMTI, err, "FlatTraceOutputter::addRecordToList: could not acquire yield lock");

      while (recFinishedCount < index) {
				
#ifdef ET_DEBUG
	aJVMTI->RawMonitorEnter(stdoutLock);
	cout << "(index == maxRecords): Waiting in loop..." << t << endl;
	aJVMTI->RawMonitorExit(stdoutLock);
#endif

	err = aJVMTI->RawMonitorWait(yieldLock, 1);
	check_jvmti_error(aJVMTI, err, "FlatTraceOutputter::addRecordToList: could not wait on yield lock");
      }
		
      err = aJVMTI->RawMonitorExit(yieldLock);
      check_jvmti_error(aJVMTI, err, "FlatTraceOutputter:addRecordToList: could not exit yieldlock");
			
      ++gcsStarted;
#ifdef ET_DEBUG
      int gcnum = gcsStarted;
      aJVMTI->RawMonitorEnter(stdoutLock);
      cout << "(index == maxRecords): gc " << gcnum << ", loops=" << loops << ", trying to acquire fullMon. " << t << endl;
      aJVMTI->RawMonitorExit(stdoutLock);
#endif

      err = aJVMTI->RawMonitorEnter(fullMon);
      check_jvmti_error(aJVMTI, err, "FlatTraceOutputter:: could not gain monitor ownership of fullMon");

#ifdef ET_DEBUG
      aJVMTI->RawMonitorEnter(stdoutLock);
      cout << "(index == maxRecords): got fullMon " << t << endl;
      aJVMTI->RawMonitorExit(stdoutLock);
#endif

      // Do the fullcb, this will trigger a GC.
      // We will get a bunch of death call backs, and a GCEnd event.
      // This is kind of non-modular, this module needs to know that this will cause a GCEvent.
      // Will refactor when time permits
      (*fullcb)();

#ifdef ET_DEBUG
      aJVMTI->RawMonitorEnter(stdoutLock);
      cout << "(index == maxRecords): back from fullcb; about to NotifyAll " << t << endl;
      aJVMTI->RawMonitorExit(stdoutLock);
#endif

      //When this returns, the GC is done, our buffers have been emptied.
      //Other threads waiting to add records to it can go ahead.
      ++gcsFinished;

      assert(gcsStarted == gcsFinished);

      err = aJVMTI->RawMonitorNotifyAll(fullMon);
      check_jvmti_error(aJVMTI, err, "FlatTraceOutputter::addRecordToList: could not notify all listeners to fullMon");

#ifdef ET_DEBUG
      aJVMTI->RawMonitorEnter(stdoutLock);
      cout << "(index == maxRecords): about to exit fullMon " << t << endl;
      aJVMTI->RawMonitorExit(stdoutLock);
#endif 

      err = aJVMTI->RawMonitorExit(fullMon);
      check_jvmti_error(aJVMTI, err, "FlatTraceOutputter::addRecordToList: could not exit fullMon");
      continue;

    } else {

#ifdef ET_DEBUG
      aJVMTI->RawMonitorEnter(stdoutLock);
      cout << "Else (index > maxRecords): Trying to enter fullmon " << t << " loops=" << loops << endl;
      aJVMTI->RawMonitorExit(stdoutLock);
#endif

      err = aJVMTI->RawMonitorEnter(fullMon);
      check_jvmti_error(aJVMTI, err, "FlatTraceOutputter::addRecordToList: could not acqure fullMon (else clause)");

#ifdef ET_DEBUG
      aJVMTI->RawMonitorEnter(stdoutLock);
      cout << "Else (index > maxRecords: Full mon acquired " << t << endl;
      aJVMTI->RawMonitorExit(stdoutLock);
#endif 

      while (gcsStarted != gcsFinished) {

#ifdef ET_DEBUG	
	aJVMTI->RawMonitorEnter(stdoutLock);
	cout << "Else (index > maxRecords): Waiting for fullMon... " << t <<  endl;
	aJVMTI->RawMonitorExit(stdoutLock);
#endif
				
	err = aJVMTI->RawMonitorWait(fullMon,1000);
	check_jvmti_error(aJVMTI, err, "FlatTraceOutputter::addRecordToList: could not wait on fullMon");

#ifdef ET_DEBUG
	aJVMTI->RawMonitorEnter(stdoutLock);
	cout << "Else (index > maxRecords): Back from wait, about to exit " << t<<  endl;
	aJVMTI->RawMonitorExit(stdoutLock);
#endif

      }

		
      err = aJVMTI->RawMonitorExit(fullMon);
      check_jvmti_error(aJVMTI, err, "FlatTraceOutputter::addRecordToList: could not exit fullMon (else clause)");

      // try again
#ifdef ET_DEBUG
      aJVMTI->RawMonitorEnter(stdoutLock);
      cout << "Else (index > maxRecords): about to try again " << t<<  endl;
      aJVMTI->RawMonitorExit(stdoutLock);
#endif
      continue;		
    }
  }
}


void FlatTraceOutputter::waitToFinishOutput () {
  sortAndOutputThread->waitForCurrentJobToFinish();
}
