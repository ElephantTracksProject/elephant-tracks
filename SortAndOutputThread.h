#ifndef SORTANDOUTPUTTHREAD_H_
#define SORTANDOUTPUTTHREAD_H_
#include <iostream>
#include <jvmti.h>
#include <jni.h>

using namespace std;

// Forward declarations
class Record;

class SortAndOutputThread {
public:
  SortAndOutputThread (jvmtiEnv* jvmti,
		       ostream* output);
  
  /* There is some initialization that cannot be done until after VMInit.
     Thus this method.*/
  void initialize (jvmtiEnv* jvmti,
		   JNIEnv* jni);

  void doParallelSortAndOutput (Record **newBuffer,
				int newBufferLength);

  void run ();

  // Called during VM shut down; we want to wait for the thread to finish its last
  // job before we terminate the process.
  void waitForCurrentJobToFinish ();



private:
  jvmtiEnv* jvmti;
  jrawMonitorID waitingForWork;
  ostream* output;
  Record** outputBuffer;
  unsigned int bufferLength;
  jobject sortAndOutputThread;
};

#endif // SORTANDOUTPUTTHREAD_H_
