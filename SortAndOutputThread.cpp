#include <algorithm>
#include <agent_util.h>
#include <assert.h>
#include "jvmti.h"
#include "jni.h"
#include "main.h"
#include "Record.h"
#include "CountsRecord.h"
#include "SortAndOutputThread.h"
#include "timer.h"

using namespace std;

static bool recordCompare (const Record *a,
			   const Record *b){
	return a->getTime() < b->getTime();		
}

static void runStaticWrapper (jvmtiEnv*,
			      JNIEnv*,
			      void *data) {
  SortAndOutputThread *sortAndOutput = (SortAndOutputThread *)data;
  sortAndOutput->run();
}


SortAndOutputThread::SortAndOutputThread (jvmtiEnv *jvmti,
					  ostream *output) :
  jvmti(jvmti),
  output(output),
  outputBuffer(NULL) {

  jvmti->CreateRawMonitor("waitingForWork", &waitingForWork);
}


void SortAndOutputThread::initialize (jvmtiEnv *jvmti,
				      JNIEnv *jni) {
  // cerr << "Finding class" << endl;
  jclass threadClass = jni->FindClass("java/lang/Thread");
  // cerr << "Allocating object" << endl;
  sortAndOutputThread = jni->NewGlobalRef(jni->AllocObject(threadClass));

  if (sortAndOutputThread == NULL) {
    cerr << "Could not allocate sortAndOutputThreadObject" << endl;
    exit(1);
  }

  // important, on some JVMs, to set the name!
  jfieldID nameField = jni->GetFieldID(threadClass, "name", "Ljava/lang/String;");
  jstring name = jni->NewStringUTF("ETSortAndOutputThread");
  jni->SetObjectField(sortAndOutputThread, nameField, name);

  jvmtiError err = jvmti->RunAgentThread(sortAndOutputThread,
                              					 runStaticWrapper,
                              					 this,
                              					 JVMTI_THREAD_NORM_PRIORITY);

  check_jvmti_error(jvmti, err, "SortAndOutputThread::initialize: Could not RunAgentThread.");
}

void SortAndOutputThread::doParallelSortAndOutput (Record **newBuffer,
						   int newBufferLength) {
  jvmtiError err = jvmti->RawMonitorEnter(waitingForWork);
  check_jvmti_error(jvmti, err, "SortAndOutputThread::doParallelSortAndOutput:"
		                "Could not enter waitingForWork");
  {
    // Just an arbitrary default value GetThreadState should never produce
    jint thread_state = 10101010;
    jvmti->GetThreadState(sortAndOutputThread, &thread_state);

    assert(outputBuffer == NULL);
    outputBuffer = newBuffer;
    bufferLength = newBufferLength;

    err = jvmti->RawMonitorNotifyAll(waitingForWork);
    check_jvmti_error(jvmti, err, "Could not NotifyAll on waitingForWork");
  }
  err = jvmti->RawMonitorExit(waitingForWork);
  check_jvmti_error(jvmti, err, "Could not exit waitingForWork");
}

void SortAndOutputThread::waitForCurrentJobToFinish () {
  jvmti->RawMonitorEnter(waitingForWork);
  jvmti->RawMonitorExit(waitingForWork);
}

void SortAndOutputThread::run () {
  jvmtiError err;

  err = jvmti->RawMonitorEnter(waitingForWork);
  check_jvmti_error(jvmti, err, "SortAndOutputThread::run: Could not enter waitingForWork");

  while(true) {
    jvmtiError err = jvmti->RawMonitorWait(waitingForWork, 0);
    check_jvmti_error(jvmti, err, "SortAndOutputThread::run: Could Not wait on waitingForWork");
    
    stable_sort(&outputBuffer[0], &outputBuffer[bufferLength], recordCompare);

    for (unsigned int i = 0; i < bufferLength; ++i) {
      Record *rec = outputBuffer[i];

      if (wantRecord['C'] && rec->getKind() == 'C') {
	// see if we can combine with a later C record;
	// we must stop if we run out of buffer,
	// or if we encounter another record that will be output,
	// or if we find a C record

	for (unsigned int j = i+1; j < bufferLength; ++j) {
	  Record *other = outputBuffer[j];
	  char okind = other->getKind();
	  if (okind == 'C') {
	    // can combine, so do it, and skip doing output
	    ((CountsRecord *)other)->combineFrom((CountsRecord *)rec);
	    outputBuffer[i] = NULL;
	    goto skip;

	  } else if (wantRecord[(unsigned int)okind]) {
	    // a wanted record so stop and output the record
	    break;

	  }
	}
      }
        
      rec->streamTo(*output);
    skip:
      delete rec;
    }

    delete [] outputBuffer;
    outputBuffer = NULL;
  }
}
