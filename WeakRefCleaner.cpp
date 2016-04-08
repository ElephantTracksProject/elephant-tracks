#include "WeakRefCleaner.h"

#include <iostream>
#include <algorithm>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <agent_util.h>
#include <java_crw_demo.h>
#include "main.h"
#include "TimeStamp.h"

using namespace std;

WeakRefCleaner::WeakRefCleaner (jvmtiEnv* jvmti, jrawMonitorID mon, ETCallBackHandler *etcbh) {
  weakCleanMon = mon;
  etCallBackHandler = etcbh;
  jvmti->CreateRawMonitor("weakRefListLock", &weakRefListLock);	
}

void WeakRefCleaner::cleanerThreadStart (jvmtiEnv *jvmti, JNIEnv *jni) {
  jvmtiError err;

  jclass refClass = jni->FindClass("java/lang/ref/Reference");
  jfieldID referentFieldID = jni->GetFieldID(refClass, "referent", "Ljava/lang/Object;");

  int gcCount;
  while (true) {
    verbose_println("Top of cleaner loop.");
    err = jvmti->RawMonitorEnter(weakCleanMon);
    check_jvmti_error(jvmti, err,
                      "WeakRefCleaner::cleanerThreadStart: could not enter weakCleanMon");
    // the .load() below emphasizes the atomic/ordered load
    while ((gcCount = etCallBackHandler->GCsToProcess.load()) == 0) {
      err = jvmti->RawMonitorWait(weakCleanMon, 0);
      check_jvmti_error(jvmti, err,
                        "WeakRefCleaner::cleanerThreadStart: could not wait on weakCleanMon");
    }
    err = jvmti->RawMonitorExit(weakCleanMon);
    check_jvmti_error(jvmti, err,
                      "WeakRefCleaner::cleanerThreadStart: could not exit weakCleanMon");

    jvmti->RawMonitorEnter(weakRefListLock);

    map<jlong,jweak>::iterator it = weakObjects.begin();
    while (it != weakObjects.end()) {
      jlong weakTag = it->first;
      jweak weakObj = it->second;
      jobject strongObj = jni->NewLocalRef(weakObj);
      if (jni->IsSameObject(strongObj, NULL)) {

        // object is gone: delete from both maps, and delete weak global refs and local ref
        // jlong refTag  = weakReferentTags[weakObj];
        jweak refWeak = weakReferents   [weakObj];
        weakObjects.erase(it++);
        weakReferentTags.erase(weakObj);
        weakReferents   .erase(weakObj);
        jni->DeleteWeakGlobalRef(weakObj);
        jni->DeleteWeakGlobalRef(refWeak);
        jni->DeleteLocalRef(strongObj);

        continue;
      }

      // object exists; check referent field
      jobject referent = jni->GetObjectField(strongObj, referentFieldID);
      if (jni->IsSameObject(referent, NULL)) {

        // referent is NULL
        // since we don't record null referents, this is a change from before
        etCallBackHandler->syntheticPointerUpdated(jni, weakTag, 0, 0);
        // delete from both maps, and delete weak global refs and local refs
        // jlong refTag  = weakReferentTags[weakObj];
        jweak refWeak = weakReferents   [weakObj];
        weakObjects.erase(it++);
        weakReferentTags.erase(weakObj);
        weakReferents   .erase(weakObj);
        jni->DeleteWeakGlobalRef(weakObj);
        jni->DeleteWeakGlobalRef(refWeak);
        jni->DeleteLocalRef(strongObj);
        jni->DeleteLocalRef(referent);
        continue;
      }

      jlong referentTag = etCallBackHandler->getTagAndTagIfNeeded(jni, referent);
      jlong oldTag = weakReferentTags[weakObj];
      if (referentTag != oldTag) {
        // this case does not happen, as far as we know, but for safety's sake ...
        etCallBackHandler->syntheticPointerUpdated(jni, weakTag, referentTag, 0);
        jweak refWeak = jni->NewWeakGlobalRef(referent);
        weakReferentTags[weakObj] = referentTag;
        weakReferents   [weakObj] = refWeak;
      }

      // delete local refs and bump iterator
      jni->DeleteLocalRef(strongObj);
      jni->DeleteLocalRef(referent);
      it++;
    }
    
    jvmti->RawMonitorExit(weakRefListLock);

    etCallBackHandler->GCsToProcess -= gcCount;

    verbose_println("Entered WeakClean");
  }
}

void WeakRefCleaner::updateReferent (jvmtiEnv *jvmti, JNIEnv *jni,
                                     jobject origin, jlong originTag,
                                     jobject referent, jlong referentTag) {
  // Note: caller (ETCallBackHandler) will see to updating the heap model
  verbose_println("WeakRefCleaner::updateReferent called.");

  jvmti->RawMonitorEnter(weakRefListLock);

  map<jlong,jweak>::iterator it = weakObjects.find(originTag);
  if (it == weakObjects.end()) {

    // entirely possible, even normal, for object not to be recorded yet
    if (referentTag != 0) { // ignore if initial referent is null
      jweak weak    = jni->NewWeakGlobalRef(origin);
      jweak refWeak = jni->NewWeakGlobalRef(referent);
      weakObjects[originTag] = weak;
      weakReferentTags[weak] = referentTag;
      weakReferents   [weak] = refWeak;
    }

  } else {
    // already set; as far as we know, should always be changing to null, but ...
    jweak weak = it->second;
    // jobject strong = jni->NewLocalRef(weak);
    jlong oldTag     = weakReferentTags[weak];
    jweak oldRefWeak = weakReferents   [weak];

    if (oldTag != referentTag) {  // no work needed if no change
      if (referentTag == 0) {

        // being set to null: clean out of our data structures
        weakObjects.erase(it);
        weakReferentTags.erase(weak);
        weakReferents   .erase(weak);
        jni->DeleteWeakGlobalRef(weak);
        jni->DeleteWeakGlobalRef(oldRefWeak);

      } else {

        // being set to a different value (shouldn't happen, but ...)
        jweak refWeak = jni->NewWeakGlobalRef(referent);
        weakReferents   [weak] = refWeak;
        weakReferentTags[weak] = referentTag;
        jni->DeleteWeakGlobalRef(oldRefWeak);

      }
    }
  }

  jvmti->RawMonitorExit(weakRefListLock);
}

void WeakRefCleaner::adjustDeadSet (set<jlong> deadSet, set<jlong> removed) {
  // It seems that at least some JVMs do not report eweakly reachable objects
  // when fllowing references or iterating over the heap.  This removes those
  // actually-reachable objects from the dead set.

  jvmtiEnv *jvmti = cachedJvmti;

  jvmti->RawMonitorEnter(weakRefListLock);

  JNIEnv *jni = getJNI();

  map<jlong,jweak>::iterator it = weakObjects.begin();
  while (it != weakObjects.end()) {
    jlong weakTag = it->first;
    jweak weakObj = it->second;
    
    if (deadSet.count(weakTag) != 0) {
      // if this object is thought dead, let's see if it really is ...

      jobject strong = jni->NewLocalRef(weakObj);
      if (jni->IsSameObject(strong, NULL)) {
        // It's really dead, so leave dead set as it is.
        // Clean info out of weak data structures.
        // jlong refTag  = weakReferentTags[weakObj];
        jweak refWeak = weakReferents   [weakObj];
        weakObjects.erase(it++);
        weakReferentTags.erase(weakObj);
        weakReferents   .erase(weakObj);
        jni->DeleteWeakGlobalRef(weakObj);
        jni->DeleteWeakGlobalRef(refWeak);
        jni->DeleteLocalRef(strong);
        continue;
      }
      
      // it's live, so remove from dead set
      deadSet.erase(weakTag);
      removed.insert(weakTag);
      jni->DeleteLocalRef(strong);
      continue;
      
    }
    it++;
  }

  map<jweak,jweak>::iterator rt = weakReferents.begin();
  while (rt != weakReferents.end()) {
    jweak weakObj = rt->first;
    jlong refTag  = weakReferentTags[weakObj];
    jweak refWeak = rt->second;
    
    if (deadSet.count(refTag) != 0) {
      // if this object is thought dead, let's see if it really is ...

      jobject strong = jni->NewLocalRef(refWeak);
      if (jni->IsSameObject(strong, NULL)) {
        // It's really dead, so leave dead set as it is.
        // We'll deal with our own data structure later
        jni->DeleteLocalRef(strong);
        rt++;
        continue;
      }
      
      // it's live, so remove from dead set
      deadSet.erase(refTag);
      removed.insert(refTag);
      jni->DeleteLocalRef(strong);
      continue;
      
    }
    rt++;
  }

  jvmti->RawMonitorExit(weakRefListLock);
}

void WeakRefCleaner::checkForDeadReferents (const map<jlong, TimeStamp> deadObjTime) {
  jvmtiEnv *jvmti = cachedJvmti;

  jvmti->RawMonitorEnter(weakRefListLock);

  JNIEnv *jni = getJNI();

  map<jlong,jweak>::iterator it = weakObjects.begin();
  while (it != weakObjects.end()) {

    jlong weakTag = it->first;
    jweak weakObj = it->second;
    jlong refTag  = weakReferentTags[weakObj];
    jweak refWeak = weakReferents   [weakObj];

    // check whether any of the two objects died
    // (note: the weak object (weakObj) should be live if adjustDeadSet was called, but ...)
    map<jlong, TimeStamp>::const_iterator woi = deadObjTime.find(weakTag);
    map<jlong, TimeStamp>::const_iterator roi = deadObjTime.find(refTag );

    int wdead = (woi != deadObjTime.end());
    int rdead = (roi != deadObjTime.end());

    jlong rdtime = (rdead ? roi->second.time : 0);
    jlong wdtime = (wdead ? woi->second.time : 0);

    if (rdtime < wdtime) {
      // both died, but referent was earlier: add an update record
      // referent died, but not weak object: add an update record
      etCallBackHandler->syntheticPointerUpdated(jni, weakTag, 0, rdtime);
    }

    if (wdead + rdead) {
      // if either is dead, remove from data structures and iterate
      weakObjects.erase(it++);
      weakReferentTags.erase(weakObj);
      weakReferents   .erase(weakObj);
      jni->DeleteWeakGlobalRef(weakObj);
      jni->DeleteWeakGlobalRef(refWeak);
      continue;

    }

    // if both are live, just iterate
    it++;
  }

  jvmti->RawMonitorExit(weakRefListLock);

}

// Local Variables:
// mode:C++
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
