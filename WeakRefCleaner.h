#ifndef WEAKREFCLEANER_H_
#define WEAKREFCLEANER_H_

#include <map>
#include <jvmti.h>
#include <jni.h>
#include <HashGraph.h>
#include "ETCallBackHandler.h"

using namespace std;

class ETCallBackHandler;
class TimeStamp;

class WeakRefCleaner {

public:
  WeakRefCleaner (jvmtiEnv *jvmti, jrawMonitorID mon, ETCallBackHandler *etcbh);

  void updateReferent (jvmtiEnv* jvmti, JNIEnv *jni,
                       jobject origin, jlong originTag,
                       jobject referent, jlong referentTag);

  void cleanerThreadStart (jvmtiEnv *jvmti, JNIEnv *jni);

  void adjustDeadSet (set<jlong> deadSet, set<jlong> removed);

  void checkForDeadReferents (const map<jlong, TimeStamp> deadObjTime);

private:
  map<jlong,jweak> weakObjects;
  map<jweak,jlong> weakReferentTags;
  map<jweak,jweak> weakReferents;
  jrawMonitorID weakCleanMon;
  jrawMonitorID weakRefListLock;

  ETCallBackHandler* etCallBackHandler;

};
#endif

// Local Variables:
// mode:C++
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
