#include <jvmti.h>
#include <jni.h>
#include <iostream>
#include "AllocationRecord.h"
#include "DeathRecord.h"
#include "MethodEntryRecord.h"
#include "MethodExitRecord.h"
#include "PointerUpdateRecord.h"
#include "RootRecord.h"
#include "main.h"
#include "SortAndOutputThread.h"

using namespace std;

int main(int argc, char** argv) {
  JNIEnv* jni = new JNIEnv();
  jvmtiEnv* jvmti = new jvmtiEnv();
  Record** records;
  int num_records = 5;

  for(int i = 0; i < 256; i++) {
    wantRecord[i] = 'a';
  }


  records = new Record*[num_records];
  records[0] = new AllocationRecord(1, 1, 1, "FakeType", 1, 'A');
  records[1] = new MethodEntryRecord(1, 1, 1);
  records[2] = new RootRecord(1,1);
  records[3] = new MethodExitRecord(1,1,1,1);
  records[4] = new DeathRecord(1,1);

  records[0]->setTime(0);
  records[1]->setTime(1);
  records[2]->setTime(1);
  records[3]->setTime(2);
 


  SortAndOutputThread* sortAndOutput = new SortAndOutputThread(jvmti, (std::ostream*)&cerr);
  sortAndOutput->initialize(jvmti, jni);

  sortAndOutput->doParallelSortAndOutput(records, num_records);


  records = new Record*[num_records];
  records[0] = new AllocationRecord(2, 2, 2, "FakeType2", 1, 'A');
  records[1] = new MethodEntryRecord(1, 2, 1);
  records[2] = new RootRecord(2,1);
  records[3] = new MethodExitRecord(1, 2, 1, 1);
  records[4] = new DeathRecord(2,4);

  records[0]->setTime(2);
  records[1]->setTime(3);
  records[2]->setTime(3);
  records[3]->setTime(4);

  sortAndOutput->doParallelSortAndOutput(records, num_records);

  while(sortAndOutput->getStatus() != WAITING) {
    this_thread::yield();
  }

  delete jni;
  delete jvmti;
  delete sortAndOutput;

  return 0;
}
