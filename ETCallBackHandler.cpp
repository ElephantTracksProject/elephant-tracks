#include <agent_util.h>
#include <java_crw_demo.h>
#include <vector>
#include <set>
#include <map>
#include <queue>
#include <iostream>
#include <algorithm>
#include <GraphAlgorithms.h>
#include <assert.h>
#include <sstream>
#include <limits>
#include <cstddef>

#include "ETCallBackHandler.h"
#include "FlatTraceOutputter.h"
#include "main.h"
#include "RootRecord.h"
//#include <boost/graph/graph_traits.hpp>
//#include <boost/graph/adjacency_list.hpp>
//#include <boost/graph/topological_sort.hpp>
#include "TagTimeMap.h"
using namespace std;
//using namespace boost;
#define ATOMICALLY(x) enter_critical_section(theJVMTI); x exit_critical_section(theJVMTI);
//#define TRACE_NEW_OBJECTS 1
//#define TRACE_AGENT_RECORDING 1
#define ACC_STATIC 8 // from JVM Spec
#define SYNTHETIC_ID_BASE 0x10000000  // allows for 256 million classes/fields

static jvmtiEnv *theJVMTI;
jniNativeInterface *origJNIEnv;  // the original vector of JNI functions
jniNativeInterface *instJNIEnv;  // our replacement, allowing instrumentation

static int useFollowReferences = 0;

// information for getting something things from Java java/lang/reflect/Field objects
bool haveFieldDeclaringClassId = false;
jfieldID FieldDeclaringClassId;
bool haveFieldNameId = false;
jfieldID FieldNameId;
bool haveFieldTypeId = false;
jfieldID FieldTypeId;

static void JNICALL cleanerStaticWrapper (jvmtiEnv *jvmti, JNIEnv* jni, void* data) {
  // if we are doing IterateThroughHeap, don't use the weak ref cleaner
  if (useFollowReferences) {
    ETCallBackHandler* etCallBackHandler = (ETCallBackHandler*)data;
    etCallBackHandler->weakRefCleanerThreadStart(jvmti, jni);
  }
}

static tbb::atomic<jlong> nextTag; //The next object tag to use, we skip 0 since we use that to identify null

ETCallBackHandler::ETCallBackHandler (jvmtiEnv *jvmti,
                                      JavaVM*,
                                      FlatTraceOutputter* traceOutputter)
 {
  timeStampTag_time = 0;
  computeDeathTimes_time = 0;
  fakeAGC_time = 0;
  deadFinder_time = 0;
  GCEnd_time = 0;
  computeReachable_time = 0;
  deadSetAdjustment_time = 0;
  doGCEnd_time = 0;
  nativeVarEvent_time = 0;
  nextTag = 1;  // we skip 0, since they means "null"

  this->traceOutputter = traceOutputter;
  this->aJVMTI = jvmti;
  theJVMTI = jvmti;
  this->currentTime = 0;

  totalHeapReads     = 0;
  totalHeapWrites    = 0;
  totalHeapRefReads  = 0;
  totalHeapRefWrites = 0;
  totalInstCount     = 0;

  classId2slots[0] = 0;  // superclass of Object has size 0
  staticSlots = 0;       // may not be necessary, but ...
  nextSyntheticId = SYNTHETIC_ID_BASE;

  jvmti->CreateRawMonitor("heapLock"         , &heapLock         );
  jvmti->CreateRawMonitor("deadSetLock"      , &deadSetLock      );
  jvmti->CreateRawMonitor("clockLock"        , &clockLock        );
  jvmti->CreateRawMonitor("tagLock"          , &tagLock          );
  jvmti->CreateRawMonitor("countsLock"       , &countsLock       );
  jvmti->CreateRawMonitor("weakRefMon"       , &weakRefMon       );
  // jvmti->CreateRawMonitor("tag2TimeStampLock", &tag2TimeStampLock);
  // jvmti->CreateRawMonitor("arrayShadowLock"  , &arrayShadowLock  );

  GCsToProcess = 0;
  weakRefCleaner = new WeakRefCleaner(jvmti, weakRefMon, this);

}

void ETCallBackHandler::lockOn (jrawMonitorID lock) {
  jvmtiError error = aJVMTI->RawMonitorEnter(lock);
  check_jvmti_error(aJVMTI, error, "ETCallBackHandler::lockOn: Cannot enter with raw monitor");
}

void ETCallBackHandler::unlockOn (jrawMonitorID lock) {
  jvmtiError error = aJVMTI->RawMonitorExit(lock);
  check_jvmti_error(aJVMTI, error, "ETCallBackHandler::unlockOn:Cannot exit with raw monitor");
}

jthread ETCallBackHandler::getCurrentThread () {
  jthread currentThread;
  jvmtiError err = aJVMTI->GetCurrentThread(&currentThread);
  check_jvmti_error(aJVMTI, err, "getCurrentThread: could not get current thread");
  return currentThread;
}

void ETCallBackHandler::nativeMethodEntry (JNIEnv *jni,
                                           jclass,
                                           jobject thread,
                                           jint methodId,
                                           jobject receiver) {
  jthread currentThread = (jthread)thread;
	
  jlong newTime = incTime();

  jlong receiverTag = getTagAndTagIfNeeded(jni, receiver);
  jlong threadTag   = (currentThread == 0 ? 0 : getTagAndTagIfNeeded(jni, currentThread));

  this->traceOutputter->doMethodEntry(newTime, methodId, receiverTag, threadTag);
}

void ETCallBackHandler::nativeMethodExit (JNIEnv *jni, jclass klass, jobject thread, jobject exception,
                                          jint methodId, jobject receiver) {
  jthread currentThread = (jthread)thread;

  this->nativeVarEvent(jni, klass, 2, receiver, exception, NULL, NULL, NULL);

  jlong newTime = incTime();

  jlong receiverTag = getTagAndTagIfNeeded(jni, receiver     );
  jlong threadTag   = getTagAndTagIfNeeded(jni, currentThread);
  jlong excTag      = getTagAndTagIfNeeded(jni, exception    );

  this->traceOutputter->doMethodExit(newTime, methodId, receiverTag, excTag, threadTag);
}

void ETCallBackHandler::nativeException (JNIEnv *jni, jclass klass, jobject thread, jobject exception,
                                         jint methodId, jobject receiver, ExceptKind kind) {
  jthread currentThread = (jthread)thread;

  jlong newTime = incTime();

  jlong receiverTag = getTagAndTagIfNeeded(jni, receiver     );
  jlong threadTag   = getTagAndTagIfNeeded(jni, currentThread);
  jlong excTag      = getTagAndTagIfNeeded(jni, exception    );

  traceOutputter->doException(newTime, methodId, receiverTag, excTag, threadTag, kind);

  this->nativeVarEvent(jni, klass, 2, receiver, exception, NULL, NULL, NULL);
}

void ETCallBackHandler::nativeExceptionThrow (JNIEnv *jni, jclass klass, jobject thread, jobject exception,
                                              jint methodId, jobject receiver) {
  nativeException(jni, klass, thread, exception, methodId, receiver, ExceptThrow);
}

void ETCallBackHandler::nativeExceptionHandle (JNIEnv *jni, jclass klass, jobject thread, jobject exception,
                                               jint methodId, jobject receiver) {
  nativeException(jni, klass, thread, exception, methodId, receiver, ExceptHandle);
}

bool vmDead () {
  return false;	
}

ETCallBackHandler::~ETCallBackHandler () { }

jlong ETCallBackHandler::readNextTag () {
  return nextTag;  // useful for debug messages, etc.
}

jlong ETCallBackHandler::tagObject (jvmtiEnv *jvmti, jobject o, bool *newTag) {
  jlong thisTag;

  if (nextTag == 0) {
    verbose_println("!! tagObject: nextTag is 0.");
  }

  bool isNew = false;

  // EBM: This should be done under a lock and should check first that the
  // tag is not yet set. Even with the lock, it is best to use atomic access to nextTag.
  lockOn(tagLock);
  jvmtiError err = jvmti->GetTag(o, &thisTag);
  check_jvmti_error(this->aJVMTI, err, "ETCallBackHandler::tagObject: Could not get tag.");
  if (thisTag == 0) {
    err = jvmti->SetTag(o, (thisTag = nextTag++));
    check_jvmti_error(this->aJVMTI, err, "ETCallBackHandler::tagObject: Could not set tag.");
    isNew = true;
  }
  unlockOn(tagLock);

#ifdef ET_DEBUG
  {
    static set<jlong> tagsUsed;
    assert(tagsUsed.count(thisTag) == 0);
    tagsUsed.insert(thisTag);

    //Check that this tag is used for only one object.
    //It is only possible to call GetObjectsWithTags
    //in the live phase, so we cannot do this check
    //if we are in another phase.
    jvmtiPhase phase;
    err = jvmti->GetPhase(&phase);
    check_jvmti_error(jvmti, err, "ETCallBackHandler::tagObject: could not get phase.");
    if (phase == JVMTI_PHASE_LIVE) {
      jlong tags[1];
      tags[0] = thisTag;
      jint getTagCount = 0;

      err = jvmti->GetObjectsWithTags(1, tags, &getTagCount, NULL, NULL);
      check_jvmti_error(this->aJVMTI, err,
                        "ETCallBackHandler::tagObject: Could not get objects with tag: " + thisTag);
      assert(getTagCount == 1); 
    }		
  }
#endif	
  if (newTag != NULL) {
    *newTag = isNew;
  }
  return thisTag;
}

jlong ETCallBackHandler::getNextTag () {
  // sync should be enough for this (should not also need a lock)
  lockOn(tagLock);
  jlong result = nextTag++;
  unlockOn(tagLock);
  return result;
}

void ETCallBackHandler::addObjectToHeap (jlong tag, int slots) {
  heap.addNode(tag, slots);
}

void ETCallBackHandler::nativeNewObj (JNIEnv *jni, jclass klass,
                                      jobject thread, jobject obj, bool isVM, jint site) {
  verbose_println("ETCallBackHandler::nativeNewObj called.");
  if (vmDead()) {
    return;	
  }

  // tag thread first for more consistent order of P and A records in the output
  jlong threadTag = getTagAndTagIfNeeded(jni, thread);
  assert(threadTag != 0);

  // get class now so that we can check for arrays and threads
  jclass classOfObj = jni->GetObjectClass(obj);
  char *classSig;
  jvmtiError err = aJVMTI->GetClassSignature(classOfObj, &classSig, NULL);
  check_jvmti_error(aJVMTI, err,
                    "ETCallBackHandler::nativeNewObj: could not get class signature.");

  AllocKind kind = (isVM ? AllocVM : AllocArray);
  this->nativeNewObj(jni, klass, thread, obj, classSig, kind, site);

#ifdef TRACE_NEW_OBJECT
  jlong size;
  err = aJVMTI->GetObjectSize(obj, &size);
  check_jvmti_error(this->aJVMTI, err, "ETCallBackHandler::nativeNewObj: could not get object size.");

  ATOMICALLY(
             cerr << (char)kind << " " << tag << " " << size << " " << classSig << " " << threadTag << endl;
  )
#endif

#ifdef DEBUG_MULTINODE
  if (strcmp(classSig,  "LtestPrograms/MultiNode;") == 0) {
    ATOMICALLY(
    cerr << hex << "NewObj: Added thing to multiNodes: 0x" << tag << endl;
    )
    multiNodes.push_back(tag);		
  }
#endif 

  aJVMTI->Deallocate((unsigned char *)classSig);
}

void ETCallBackHandler::nativeNewObj (JNIEnv *jni,
                                      jclass,
                                      jobject thread,
                                      jobject obj,
                                      const char *className,
                                      AllocKind kind,
                                      jint site) {
  jlong threadTag = getTagAndTagIfNeeded(jni, thread);
  jlong objTag = 0;
  jvmtiError err = aJVMTI->GetTag(obj, &objTag);
  check_jvmti_error(aJVMTI, err, "ETCallBackHandler::nativeNewObject: could not get object's tag..");

  if (objTag == 0) {
    bool isNew;
    objTag = tagObject(aJVMTI, obj, &isNew);
    jlong size;
    err = aJVMTI->GetObjectSize(obj, &size);

    //the lenght field has no meaning for things that are not arrays
    int length = -1;
    if (className[0] == '[') {
      length = jni->GetArrayLength((jarray)obj);
    }
    this->traceOutputter->doAllocation(currentTime, objTag, size, length, className, threadTag, kind, site);
    if (isNew) {
      jclass objClass = jni->GetObjectClass(obj);
      jlong classTag = getTagAndTagIfNeeded(jni, objClass);
      int slots = (length >= 0 ? length : getSlotsForClass(jni, objClass, classTag));
      addObjectToHeap(objTag, slots);
    }
  }
  timeStampTag(objTag, threadTag);
}

void ETCallBackHandler::nativeNewObj (JNIEnv *jni,
                                      jclass klass,
                                      jobject thread, 
                                      jstring className,
                                      jint slot,
                                      jint site) {
  // obtain the object, and the className as a char *, and pass the buck
  jobject obj;
  jvmtiError err = aJVMTI->GetLocalObject((jthread)thread, (jint)2, slot, &obj);
  check_jvmti_error(aJVMTI, err,
                    "ETCallBackHandler::nativeNewObj: Could not GetLocalObject");
  const char *cname = jni->GetStringUTFChars(className, NULL);
  this->nativeNewObj(jni, klass, thread, obj, cname, AllocNew, site);
  jni->ReleaseStringUTFChars(className, cname);
}

jlong ETCallBackHandler::getTagAndTagIfNeeded (JNIEnv* jni, jobject o) {
	
  if (jni->IsSameObject(o, NULL)) {
    //if the object is null, return 0 as its tag
    return 0;
  }

  jlong tag;
  jvmtiError err = aJVMTI->GetTag(o, &tag);
  check_jvmti_error(aJVMTI, err,
                    "ETCallBackHandler::getTagAndTagIfNeeded: Could not get tag from object");

  if (tag == 0) {
    bool isNew;
    tag = tagObject(this->aJVMTI, o, &isNew);
    if (tag == 0) {
      verbose_println("!! ETCallBackHandler::GetTagAndTagIfNeeded: got a zero tag.");
    }
#ifdef TRACE_NEW_OBJECTS
    ATOMICALLY(
    cerr << hex << "tagged object " << tag << endl;
    )
#endif
    
    if (isNew) {
      jlong size = 0;
      err = aJVMTI->GetObjectSize(o, &size);
      check_jvmti_error(this->aJVMTI, err, "ETCallBackHandler::getTagAndTagIfNeeded: could not get object size.");
      jclass classOfo = jni->GetObjectClass(o);
      char *classSig;
      err = aJVMTI->GetClassSignature(classOfo, &classSig, NULL);
      check_jvmti_error(this->aJVMTI, err,
                      "ETCallBackhandler::getTagAndTagIfNeeded: could not get class signature.");
      jthread currentThread;
      err = aJVMTI->GetCurrentThread(&currentThread);
      check_jvmti_error(aJVMTI, err, "getTagAndTagIfNeeded: could not get current thread");
      jlong threadTag;
      err = aJVMTI->GetTag(currentThread, &threadTag);
      check_jvmti_error(aJVMTI, err,
                        "ETCallBackHander::getTagAndTagIfNeeded: Could not get tag from thread");
#ifdef TRACE_NEW_OBJECT
      ATOMICALLY(
      cerr << "P " << tag << " " << size << " " << classSig << " " << threadTag << endl;
      )
#endif
      int length = -1;
      if (classSig[0] == '[') {
        length = jni->GetArrayLength((jarray)o);
      }
      this->traceOutputter->doAllocation(currentTime, tag, size, length, classSig,
                                         threadTag, AllocPreexisting, 0);
      timeStampTag(tag, threadTag);
      jlong classTag = getTagAndTagIfNeeded(jni, classOfo);
      int slots = (length >= 0 ? length : getSlotsForClass(jni, classOfo, classTag));
      addObjectToHeap(tag, slots);
      aJVMTI->Deallocate((unsigned char*)classSig);
    }
  }

  return tag;		
}

void ETCallBackHandler::timeStampTag (jlong tag, jlong thread_tag) {

  HL::Timer time;

  IF_TIMING(time.start());

  ///Time stamping null doesn't make sense, ignore it.
  // Maybe we should tell the caller not to do this?
  if (tag != 0) {
      tagTimeMap[tag] = TimeStamp(this->currentTime, thread_tag);
  }
  IF_TIMING( \
      time.stop(); \
      timeStampTag_time += time.getElapsedTime(); \
  );
}

void ETCallBackHandler::ObjectFree (jvmtiEnv*, jlong tag) {	
#ifdef ET_DEBUG
  { //Check that we don't get ObjectFree calls twice for the same object.
    static set<jlong> seenObjects;

    ATOMICALLY(
    cerr << hex << seenObjects.count(tag) << " Object ID: 0x" << tag
         << "Time stamp: " << tagTimeMap[tag] << endl;
    )

    assert(seenObjects.count(tag) == 0);
    seenObjects.insert(tag);
  }
#endif

#ifdef DEBUG_MULTINODE
  if (isMultiNode(tag)) {
    ATOMICALLY(
    cerr << hex << "ObjectFree: MultiNode Free 0x" << tag << endl;
    )
  }
#endif
  if (tag == 0) {
    // cerr << "ObjectFree: Got Zero Tag" << endl;
  } else {
    deadSet.insert(tag);
  }
}

void ETCallBackHandler::pointerSlotUpdated (JNIEnv *jni,
                                            jobject thread,
                                            jobject origin,
                                            jlong originTag,
                                            jobject newTarget,
                                            jlong newTargetTag,
                                            int fieldId,
                                            int slot,
                                            bool isArray) {
  jlong thread_tag = getTagAndTagIfNeeded(jni, thread);
  timeStampTag(originTag, thread_tag);
  jlong oldTargetTag = updateHeapAtAndStamp(originTag, slot, newTargetTag, thread_tag);
  timeStampTag(oldTargetTag, thread_tag);

  this->traceOutputter->doPointerUpdate(currentTime,
                                        oldTargetTag,
                                        originTag,
                                        newTargetTag,
                                        fieldId,
                                        getTagAndTagIfNeeded(jni, thread));

  if ((!isArray) && (fieldId == referentFieldNumber)) {
    weakRefCleaner->updateReferent(theJVMTI, jni, origin, originTag, newTarget, newTargetTag);
  }
}

void ETCallBackHandler::nativePointerUpdated (JNIEnv *jni,
                                              jclass,
                                              jobject thread,
                                              jobject origin, 
                                              jobject newTarget,
                                              jint fieldId) {

  // A null origin indicates this is an update of a static field	
  jlong originTag    = getTagAndTagIfNeeded(jni, origin   );
  jlong newTargetTag = getTagAndTagIfNeeded(jni, newTarget);
	
  int slot = fieldId2slot[fieldId];

  pointerSlotUpdated(jni, thread, origin, originTag, newTarget, newTargetTag, fieldId, slot, false);
}

void ETCallBackHandler::nativePointerUpdated (JNIEnv *jni,
                                              jclass,
                                              jobject thread,
                                              jobject origin,
                                              jobject newTarget,
                                              jlong offset) {
  jlong originTag    = getTagAndTagIfNeeded(jni, origin   );
  jlong newTargetTag = getTagAndTagIfNeeded(jni, newTarget);

#ifdef TRACE_AGENT_RECORDING
  cerr << "nativePointerUpdated called: origin 0x" << hex << originTag
       << "  newTarget 0x" << hex << newTargetTag
       << "  offset " << dec << offset
       << endl;
#endif

  // is this a static or not?
  TagAndOffset2FieldIdMap::const_iterator fieldIdIter =
    tagAndOffset2fieldId.find(make_pair(originTag, offset));
  if (fieldIdIter != tagAndOffset2fieldId.end()) {
    // static field
    int fieldId = fieldIdIter->second;
    int slot = fieldId2slot[fieldId];
#ifdef TRACE_AGENT_RECORDING
    cerr << "nativePointerUpdated: static via offset: origin 0x" << hex << originTag
         << "  offset " << dec << offset
         << "  fieldId 0x" << hex << fieldId
         << "  slot " << dec << slot
         << endl;
#endif
    pointerSlotUpdated(jni, thread, 0, 0, newTarget, newTargetTag, fieldId, slot, false);
    return;
  }

  // instance; sanity check for null
  if (origin == NULL) {
    // TODO: emit warning message
    return;
  }

  // non-null instance: examine its class
  jclass originClass = jni->GetObjectClass(origin);
  jlong classTag = getTagAndTagIfNeeded(jni, originClass);

  // is this an array?
  jboolean isArrayClass;
  jvmtiError err = theJVMTI->IsArrayClass(originClass, &isArrayClass);
  check_jvmti_error(theJVMTI,err,"ETCallBackHandler::nativePointerUpdate: Could not determine IsArrayClass");

  if (isArrayClass) {
    ArrayClassTag2IntMap::const_iterator it = arrayClassTag2baseOffset.find(classTag);
    if (it == arrayClassTag2baseOffset.end()) {
    }
    int base = it->second;

    it = arrayClassTag2indexScale.find(classTag);
    if (it == arrayClassTag2indexScale.end()) {
    }
    int scale = it->second;
    int index = (offset - base) / scale;
    int size = jni->GetArrayLength((jarray)origin);
#ifdef TRACE_AGENT_RECORDING
    cerr << "nativePointerUpdated: array via offset: origin 0x" << hex << originTag
         << "  offset " << dec << offset
         << "  base " << dec << base
         << "  scale " << dec << scale
         << "  size " << dec << size
         << endl;
#endif
    if (index < 0 || index >= size) {
      // index out of range; nothing to do
      return;
    }
    pointerSlotUpdated(jni, thread, origin, originTag, newTarget, newTargetTag, index, index, true);
    return;
  }

  // instance, but not an array: field may have been declared in a superclass
  jclass tryClass = originClass;
  int fieldId;
  while (true) {
    fieldIdIter = tagAndOffset2fieldId.find(make_pair(classTag, offset));
    if (fieldIdIter != tagAndOffset2fieldId.end()) {
      fieldId = fieldIdIter->second;
      if (tryClass != originClass) {
        // cache the mapping we found
        tagAndOffset2fieldId[make_pair(classTag, offset)] = fieldId;
      }
      break;
    }
    tryClass = jni->GetSuperclass(tryClass);
    if (tryClass == NULL) {
      // could not find field
      // TODO: add warning message
      return;
    }
    getTagAndTagIfNeeded(jni, tryClass);
  }
  int slot = fieldId2slot[fieldId];
#ifdef TRACE_AGENT_RECORDING
  cerr << "nativePointerUpdated: instance via offset: origin 0x" << hex << originTag
       << "  offset " << dec << offset
       << "  fieldId 0x" << hex << fieldId
       << "  slot " << dec << slot
       << endl;
#endif
  pointerSlotUpdated(jni, thread, origin, originTag, newTarget, newTargetTag, fieldId, slot, false);
}

static set<jlong> staticsScannedClassTags;  // also include interfaces

static jclass classClass = NULL;

void ETCallBackHandler::scanStaticsOf (jvmtiEnv *jvmti, JNIEnv *jni, queue<jclass> toScan) {
  jvmtiError err;
  while (!toScan.empty()) {
    jclass cls = toScan.front();
    toScan.pop();

    jlong clsTag = getTag(jni, cls);

    // push superclass, if necessary
    jclass super = jni->GetSuperclass(cls);
    if (super != NULL) {
      jlong superTag = getTag(jni, super);
      if (staticsScannedClassTags.count(superTag) == 0) {
        staticsScannedClassTags.insert(superTag);
        toScan.push(super);
      }
    }

    jint status;
    err = jvmti->GetClassStatus(cls, &status);
    check_jvmti_error(jvmti, err, "scanStaticsOf: could not get class status");

    if ((status & JVMTI_CLASS_STATUS_ARRAY) != 0) {
      // array classes have no static fields
      continue;
    } else if ((status & JVMTI_CLASS_STATUS_PRIMITIVE) != 0) {
      // not sure if int.class, etc., have static fields:
      // do nothing (fall through)
    } else if ((status & JVMTI_CLASS_STATUS_PREPARED) == 0) {
      // if a class it not prepared, we cannot get its methods, implemented
      // interfaces, etc., so we must skip it
      continue;
    }

    // push implemented interfaces, as needed
    jint numIntfs;
    jclass *intfs;
    err = jvmti->GetImplementedInterfaces(cls, &numIntfs, &intfs);
    check_jvmti_error(jvmti, err, "scanStaticsOf: Could not get implemented interfaces");

    for (int i = 0; i < numIntfs; ++i) {
      jclass intf = intfs[i];
      jlong intfTag = getTag(jni, intf);

      if (staticsScannedClassTags.count(intfTag) == 0) {
        staticsScannedClassTags.insert(intfTag);
        toScan.push(intf);
      }
    }
    err = jvmti->Deallocate((unsigned char *)intfs);
    check_jvmti_error(jvmti, err, "scanStaticsOf: Could not deallocate interfaces");

    // now scan fields of this class/interface
    jint numFields;
    jfieldID *fields;
    err = jvmti->GetClassFields(cls, &numFields, &fields);
    check_jvmti_error(jvmti, err, "scanStaticsOf: Could not get class fields");

    string className = getClassName(jni, cls, clsTag);
    int classId = className2classId[className];
    FieldsInfo finfo = classId2fieldsInfo[classId];

    for (int i = 0; i < numFields; ++i) {
      jfieldID fid = fields[i];

      jint mods;
      err = jvmti->GetFieldModifiers(cls, fid, &mods);
      check_jvmti_error(jvmti, err, "scanStaticsOf: Could not get field modifiers");

      if ((mods & ACC_STATIC) == 0) {
        continue;
      }

      char *fname;
      char *fsig;
      err = jvmti->GetFieldName(cls, fid, &fname, &fsig, NULL);
      check_jvmti_error(jvmti, err, "scanStaticsOf: Could not get field name");

      int agentFieldId = -1;

      char kind = fsig[0];
      if (kind == 'L' || kind == '[') {
        string fieldName((const char *)fname);
        for (FieldsInfo::const_iterator fiit = finfo.begin(); fiit != finfo.end(); ++fiit) {
          if (fiit->first == fieldName) {
            agentFieldId = -fiit->second;
            break;
          }
        }
      }
      err = jvmti->Deallocate((unsigned char *)fname);
      check_jvmti_error(jvmti, err, "scanStaticsOf: Could not deallocate field name");
      err = jvmti->Deallocate((unsigned char *)fsig);
      check_jvmti_error(jvmti, err, "scanStaticsOf: Could not deallocate field signature");

      if (agentFieldId < 0) {
        continue;
      }

      FieldId2slotMap::const_iterator slotIt = fieldId2slot.find(agentFieldId);
      if (slotIt == fieldId2slot.end()) {
        continue;
      }
      int slot = fieldId2slot[agentFieldId];
      jobject newTarget = jni->GetStaticObjectField(cls, fid);

      jlong newTargetTag = 0;
      if (newTarget != NULL) {
        err = jvmti->GetTag(newTarget, &newTargetTag);
        check_jvmti_error(jvmti, err, "scanStaticsOf: Could not get netTarget tag");
      }

      jlong oldTargetTag = this->updateHeapAtAndStamp(0, 
					      	      slot,
					      	      newTargetTag,
					              getTagAndTagIfNeeded(jni, getCurrentThread()));

      if (newTargetTag != oldTargetTag) {
        this->traceOutputter->doSyntheticUpdate(0, oldTargetTag, 0, newTargetTag, agentFieldId);
      }
    }
    err = jvmti->Deallocate((unsigned char *)fields);
    check_jvmti_error(jvmti, err, "scanStaticsOf: Could not deallocate fields");
  }
}

void ETCallBackHandler::handleInitialObject (jvmtiEnv *jvmti, JNIEnv *jni, jobject obj, jlong tag) {
  if (obj == NULL) {
    return;  // shouldn't happen, but for safety ...
  }

  jclass cls = jni->GetObjectClass(obj);

  jlong classTag = getTag(jni, cls);

  jlong size;
  jvmtiError err = jvmti->GetObjectSize(obj, &size);
  check_jvmti_error(jvmti, err, "handleInitialObject: could not get object size");

  string type = getClassSignature(jni, cls, classTag);

  if (type[0] == '[') {

    jobjectArray array = (jobjectArray)obj;

    jsize length = jni->GetArrayLength(array);

    this->traceOutputter->doAllocation(0, tag, size, length, type, 0, AllocInitialHeap, 0);

    char kind = type[1];
    if (kind != '[' && kind != 'L') {
      // array of primitive: no fields
      this->addObjectToHeap(tag, 0);
      return;
    }

    // array of references
    this->addObjectToHeap(tag, length);
    for (int i = 0; i < length; ++i) {
      jobject element = jni->GetObjectArrayElement(array, i);
      jlong elementTag = getTag(jni, element);
      if (element != NULL) {
        jlong oldElementTag = this->updateHeapAt(tag, i, elementTag);
        if (oldElementTag != elementTag) {
          this->traceOutputter->doSyntheticUpdate(0, oldElementTag, tag, elementTag, i);
        }
      }
    }
    return;

  }

  if (classClass == NULL) {
    jclass cc = jni->FindClass("java/lang/Class");
    classClass = (jclass)jni->NewGlobalRef(cc);
  }

  // for Class objects, scan statics
  if (jni->IsInstanceOf(obj, classClass)) {
    if (staticsScannedClassTags.count(tag) == 0) {
      staticsScannedClassTags.insert(tag);
      queue<jclass> staticsToProcess; // will hold classes and interfaces whose statics need processing
      staticsToProcess.push((jclass)obj);
      scanStaticsOf(jvmti, jni, staticsToProcess);
    }
  }

  // scalar object case
  int slots = getSlotsForClass(jni, cls, classTag);  // insures tables are filled in :-)
  this->addObjectToHeap(tag, slots);

  this->traceOutputter->doAllocation(0, tag, size, 0, type, 0, AllocInitialHeap, 0);

  for (jclass currClass = cls; currClass != NULL; currClass = jni->GetSuperclass(currClass)) {
    // process instance fields

    jlong clsTag = getTag(jni, currClass);

    int numFields;
    jfieldID *fields;
    err = jvmti->GetClassFields(currClass, &numFields, &fields);
    check_jvmti_error(jvmti, err, "handleInitialObject: could not get class fields");

    string className = getClassName(jni, currClass, clsTag);
    int classId = className2classId[className];
    FieldsInfo finfo = classId2fieldsInfo[classId];

    for (int i = 0; i < numFields; ++i) {
      jfieldID fid = fields[i];

      jint mods;
      err = jvmti->GetFieldModifiers(currClass, fid, &mods);
      check_jvmti_error(jvmti, err, "handleInitialObject: could not get field modifiers");

      if ((mods & ACC_STATIC) != 0) {
        continue;
      }

      int agentFieldId = -1;
      pair<jlong,jfieldID> classAndJFieldID = make_pair(clsTag, fid);
      TagAndJFieldID2AgentId::iterator fit =
        tagAndJFieldID2AgentId.find(classAndJFieldID);
      if (fit != tagAndJFieldID2AgentId.end()) {
        agentFieldId = fit->second;
      } else {

        char *fname;
        char *fsig;
        err = jvmti->GetFieldName(currClass, fid, &fname, &fsig, NULL);
        check_jvmti_error(jvmti, err, "handleInitialObject: could not get field name and signature");

        string fieldName((const char *)fname);
        for (FieldsInfo::const_iterator fiit = finfo.begin(); fiit != finfo.end(); ++fiit) {
          if (fiit->first == fieldName) {
            agentFieldId = fiit->second;
            break;
          }
        }
        tagAndJFieldID2AgentId[classAndJFieldID] = agentFieldId;
        err = jvmti->Deallocate((unsigned char *)fname);
        check_jvmti_error(jvmti, err, "handleInitialObject: could not deallocate field name");
        err = jvmti->Deallocate((unsigned char *)fsig);
        check_jvmti_error(jvmti, err, "handleInitialObject: could not deallocate field signature");
      }

      if (agentFieldId < 0) {
        continue;
      }

      FieldId2slotMap::const_iterator slotIt = fieldId2slot.find(agentFieldId);
      if (slotIt == fieldId2slot.end()) {
        continue;
      }
      int slot = fieldId2slot[agentFieldId];
      jobject newTarget = jni->GetObjectField(obj, fid);

      jlong newTargetTag = getTag(jni, newTarget);

      jlong oldTargetTag = this->updateHeapAt(tag, slot, newTargetTag);

      if (newTargetTag != oldTargetTag) {
        this->traceOutputter->doSyntheticUpdate(0, oldTargetTag, tag, newTargetTag, agentFieldId);
      }
    }
  }
}

void ETCallBackHandler::JNIPointerUpdated (JNIEnv *jni, jobject obj, jclass klass,
                                           jfieldID fieldId, jobject newVal) {
  // NOTE: if obj is NULL then klass should be non-null
  // and this is an update of a static field of klass;
  // if obj is non-null, klass is ignored.
  jthread thread = getCurrentThread();
  jclass cls;
  jclass declClass;
  if (obj == NULL) {
    cls = klass;
    declClass = klass;
  } else {
    cls = jni->GetObjectClass(obj);
    jvmtiError err = theJVMTI->GetFieldDeclaringClass(cls, fieldId, &declClass);
    check_jvmti_error(theJVMTI, err, "ETCallBackHandler:: Could not GetFieldDeclaringClass");
  }
  jlong classTag = getTagAndTagIfNeeded(jni, cls);
  if (obj != NULL) {
    getSlotsForClass(jni, cls, classTag);  // insures tables are filled in :-)
  }

  pair<jlong,jfieldID> classAndJFieldID = make_pair(classTag, fieldId);
  int fid = -1;
  TagAndJFieldID2AgentId::const_iterator fit =
    tagAndJFieldID2AgentId.find(classAndJFieldID);
  if (fit != tagAndJFieldID2AgentId.end()) {
    fid = fit->second;
  } else {
    string className = getAgentClassName(jni, declClass);
    int classId = className2classId[className];
    FieldsInfo finfo = classId2fieldsInfo[classId];

    jobject fldObj = jni->ToReflectedField(declClass, fieldId, false);
    string fieldName = getFieldName(jni, fldObj);

    for (FieldsInfo::const_iterator it = finfo.begin(); it != finfo.end(); ++it) {
      if (it->first == fieldName) {
        fid = it->second;
        tagAndJFieldID2AgentId[classAndJFieldID] = fid;
#ifdef TRACE_AGENT_RECORDING
        cerr << "nativePointerUpdated from JNI SetObjectField; field id = 0x" << hex << fid << endl;
#endif
        break;
      }
    }
  }

  assert(fid != -1);
  nativePointerUpdated(jni, cls, thread, obj, newVal, fid);
}

/*A pointer update, based only on tags;
  Used by the WeakRefCleaner, who isn't so sure about objects */
void ETCallBackHandler::syntheticPointerUpdated (JNIEnv* jni,
                                                 jlong originTag,
                                                 jlong newTargetTag,
                                                 jlong time) {
  int fieldId = referentFieldNumber;
  int referentSlot = fieldId2slot[fieldId];
  jlong oldTargetTag;
  if (time == 0) {
    oldTargetTag = updateHeapAt(originTag, referentSlot, newTargetTag);
    // consider this to be a present action;
    // otherwise, it is in the past and we should not update time stamps
    timeStampTag(originTag, getTagAndTagIfNeeded(jni, getCurrentThread()));
    time = currentTime;
  } else {
    oldTargetTag =  updateHeapAtAndStamp(originTag,
				 	 referentSlot,
				 	 newTargetTag,
				 	 getTagAndTagIfNeeded(jni, getCurrentThread()));
  }

  if (useFollowReferences) {
    this->traceOutputter->doPointerUpdate(time, oldTargetTag, originTag, newTargetTag, fieldId, 0);
  } else {
    this->traceOutputter->doSyntheticUpdate(time, oldTargetTag, originTag, newTargetTag, fieldId);
  }
}

void ETCallBackHandler::nativeUninitPutfield (JNIEnv *jni, jclass klass, jobject thread,
                                              jobject newTarget, jint objSlot, jint fieldId) {
  jobject obj;
  jvmtiError err = aJVMTI->GetLocalObject((jthread)thread, (jint)2, objSlot, &obj);
  check_jvmti_error(aJVMTI, err, "ETCallBackHandler::nativeUninitPutfield: Could not getLocalObject");
  this->nativePointerUpdated (jni, klass, thread, obj, newTarget, fieldId);
}


//TODO: Break this into two methods
jlong ETCallBackHandler::updateHeapAtAndStamp (jlong originTag,
				       	       int slot,
				       	       jlong newTargetTag,
				               jlong threadTag) {
  jlong result = heap.updateEdge(originTag, slot, newTargetTag);
  timeStampTag(result, threadTag);
  return result;
}

jlong ETCallBackHandler::updateHeapAt (jlong originTag,
				       int slot,
				       jlong newTargetTag) {
  jlong result = heap.updateEdge(originTag, slot, newTargetTag);
  return result;
}



void ETCallBackHandler::nativeStampThis (JNIEnv *jni, jclass, jstring) {
  jthread thread;
  jvmtiError err = aJVMTI->GetCurrentThread(&thread);
  check_jvmti_error(aJVMTI, err, "nativeStampThis: could not get current thread");
  // jlong threadTag = getTagAndTagIfNeeded(jni, thread);

  jobject obj;
  err = aJVMTI->GetLocalObject((jthread)thread, (jint)2, (jint)0, &obj);
  check_jvmti_error(aJVMTI, err, "ETCallBackHandler::nativeStampThis: could not GetLocalObject");
  jlong objTag = 0;
  err = aJVMTI->GetTag(obj, &objTag);
  check_jvmti_error(aJVMTI, err, "ETCallBackHandler::nativeStampThis: could not GetTag");
  if (objTag == 0) {
    objTag = getTagAndTagIfNeeded(jni, obj);  // outputs a P trace entry, if necessary, and adds to heap
  }
  timeStampTag(objTag, getTagAndTagIfNeeded(jni, thread));
}

void ETCallBackHandler::nativeVarEvent (JNIEnv *jni,
                                        jclass, jint numObjs,
                                        jobject o0,
                                        jobject o1,
                                        jobject o2,
                                        jobject o3,
                                        jobject o4) {
  jlong objTag;
  HL::Timer timer;

  IF_TIMING (timer.start());

  jthread currentThread;
  jvmtiError err = aJVMTI->GetCurrentThread(&currentThread);
  check_jvmti_error(aJVMTI,err, "ETCallBackHandler::nativeVarEvent: could not get current thread.");

  jlong threadTag = getTagAndTagIfNeeded(jni, currentThread);

  //EBM: does it matter that currentTime might change as we go?
  //The lack of breaks here is deliberate, we want the fall throgh behavior
  switch (numObjs) {
  case 5:
    objTag = getTagAndTagIfNeeded(jni, o4);
    timeStampTag(objTag, threadTag);
    this->traceOutputter->doVarEvent(currentTime, objTag, threadTag);
  case 4:
    objTag = getTagAndTagIfNeeded(jni, o3);
    timeStampTag(objTag, threadTag);
    this->traceOutputter->doVarEvent(currentTime, objTag, threadTag);
  case 3: 
    objTag = getTagAndTagIfNeeded(jni, o2);
    timeStampTag(objTag, threadTag);
    this->traceOutputter->doVarEvent(currentTime, objTag, threadTag);
  case 2:
    objTag = getTagAndTagIfNeeded(jni, o1);
    timeStampTag(objTag, threadTag);
    this->traceOutputter->doVarEvent(currentTime, objTag, threadTag);
  case 1:
    objTag = getTagAndTagIfNeeded(jni, o0);
    timeStampTag(objTag, threadTag);
    this->traceOutputter->doVarEvent(currentTime, objTag, threadTag);
    break;
  default:
    //cerr << "ETCallBackHandler::nativeVarEvent: Unxpected number of objects " << numObjs
    //     << ", valid values are 1-5." << endl;
    exit(-1);
  }

  IF_TIMING ( \
    timer.stop(); \
    nativeVarEvent_time += timer.getElapsedTime(); \
  );
}

// NOTE: We code handling if SystemArrayCopy to do the work here because
// we could not find a way to check the assignability into the target
// array in all cases, and thus need to let the exception happen in order
// to stop the copy loop at the right place in all cases.  Note also that
// in the copy-to-same-array case, the exception cannot arise, so the caller
// cannot detect that we have in fact copied in the other direction.

jboolean ETCallBackHandler::nativeDoSystemArraycopy(JNIEnv *jni, jclass, jobject,
                                                    jobject src, jint srcIdx,
                                                    jobject dst, jint dstIdx, jint cnt) {
  if (jni->IsSameObject(src, NULL) ||
      jni->IsSameObject(dst, NULL)) {
    // punt null object case to the native
    return JNI_FALSE;
  }
  jclass srcClass = jni->GetObjectClass(src);
  jclass dstClass = jni->GetObjectClass(dst);
  jvmtiError err;
  jboolean srcIsArray;
  err = aJVMTI->IsArrayClass(srcClass, &srcIsArray);
  check_jvmti_error(aJVMTI, err, "nativeDoSystemArraycopy: Could not IsArrayClass on srcClass");
  jboolean dstIsArray;
  err = aJVMTI->IsArrayClass(dstClass, &dstIsArray);
  check_jvmti_error(aJVMTI, err, "nativeDoSystemArraycopy: Could not IsArrayClass on dstClass");
  if ((!srcIsArray) || (!dstIsArray)) {
    // punt non-array case to the native
    return JNI_FALSE;
  }
  char *srcSig;
  err = aJVMTI->GetClassSignature(srcClass, &srcSig, NULL);
  check_jvmti_error(aJVMTI, err, "nativeNoteSystemArraycopy: Could not get src class signature");
  char *dstSig;
  err = aJVMTI->GetClassSignature(dstClass, &dstSig, NULL);
  check_jvmti_error(aJVMTI, err, "nativeNoteSystemArraycopy: Could not get dst class signature");
  char srcEltKind = srcSig[1];
  char dstEltKind = dstSig[1];
  aJVMTI->Deallocate((unsigned char *)srcSig);
  aJVMTI->Deallocate((unsigned char *)dstSig);
  if (((srcEltKind != 'L') && (srcEltKind != '[')) ||
      ((dstEltKind != 'L') && (dstEltKind != '['))) {
    // punt non-object array case to the native
    return JNI_FALSE;
  }
  jobjectArray srcArray = (jobjectArray)src;
  jobjectArray dstArray = (jobjectArray)dst;
  jsize srcLen = jni->GetArrayLength(srcArray);
  jsize dstLen = jni->GetArrayLength(srcArray);
  if (srcIdx < 0 || (srcIdx+cnt) > srcLen ||
      dstIdx < 0 || (dstIdx+cnt) > dstLen ||
      cnt < 0) {
    // punt improper chunks case to the native
    return JNI_FALSE;
  }
  int incr = 1;
  if (jni->IsSameObject(srcArray, dstArray) && srcIdx < dstIdx) {
    srcIdx += cnt-1;
    dstIdx += cnt-1;
    incr = -1;
  }
  for (int i = 0; i < cnt; ++i) {
    jobject srcElement = jni->GetObjectArrayElement(srcArray, srcIdx);
    // TODO(nricci01) Do we need this? The return value was not being used.
    jni->GetObjectArrayElement(dstArray, dstIdx);
    // Note: our handler for SetObjectArrayElement will log the update!
    jni->SetObjectArrayElement(dstArray, dstIdx, srcElement);
    if (jni->ExceptionCheck()) {
      return JNI_TRUE;  // let the exception happen
    }
    srcIdx += incr;
    dstIdx += incr;
  }
  return JNI_TRUE;
}

bool ETCallBackHandler::diedBefore (jlong obj1, jlong obj2) {
  // assert(tag2TimeStamp.count(obj1) != 0);
  // assert(tag2TimeStamp.count(obj2) != 0);
  return tagTimeMap[obj1].time < tagTimeMap[obj2].time;
}

bool ETCallBackHandler::isUnreachable (jlong obj) {
  return (deadSet.count(obj) != 0);
}

// Caller must hold locks on heap, tag2timeStamp
void ETCallBackHandler::computeObjectDeathTimes () {

  HL::Timer timer;

  IF_TIMING (timer.start());
  // TODO TimeStamp lastTime = (TimeStamp){-1L, 0}; // This assumes two's complement to find max long.
  TimeStamp lastTime(std::numeric_limits<int>::max(), 0);
  set<jlong> completed;

  vector<jlong> unreachableStack;

  // copy the dead set to a list
  unreachableStack.reserve(deadSet.size());
  for (set<jlong>::iterator it = deadSet.begin(); it != deadSet.end(); it++) {
    unreachableStack.push_back(*it);
  }

  // sort the list by time; we will use the list as the algorothm's stack
  DiedBeforeComparator comp(*this);
  std::sort(unreachableStack.begin(), unreachableStack.end(), comp);

  while (!unreachableStack.empty()) {
    jlong obj = unreachableStack.back();
    unreachableStack.pop_back();

    pair<set<jlong>::iterator, bool> result = completed.insert(obj);
    if (result.second == false) continue;

    // We should have a time stamp for everything here...
    // map<jlong,jlong>::iterator it = tag2TimeStamp.find(obj);
    // assert(it != tag2TimeStamp.end());
    TimeStamp objTime = tagTimeMap[obj];

    if (objTime.time <= lastTime.time) {
      lastTime = objTime;
      pair<HashGraph::EdgeList::const_iterator, HashGraph::EdgeList::const_iterator> beginEnd = heap.neighbors(obj);
      for (HashGraph::EdgeList::const_iterator neighborIt = beginEnd.first;
           neighborIt != beginEnd.second;
           ++neighborIt) {
        jlong neighbor = *neighborIt;
        /* map<jlong,jlong>::iterator jt = tag2TimeStamp.find(neighbor);

        if (jt == tag2TimeStamp.end()) {
          ATOMICALLY(
            cerr << hex << "Neighbor not found: 0x" << neighbor << " of object " << obj << endl;
          )
          assert(false);
        }*/

        TimeStamp neighborTime = tagTimeMap[neighbor];
        if (isUnreachable(neighbor) && neighborTime.time < lastTime.time) {
          tagTimeMap[neighbor] = lastTime;
          unreachableStack.push_back(neighbor);	
        }
      }
    }	
  }

  IF_TIMING ( \
    timer.stop(); \
    computeDeathTimes_time += timer.getElapsedTime(); \
  );
}

/*
// Time stamp object o, and return its tag
jlong ETCallBackHandler::timeStampObject (jvmtiEnv *jvmti, jobject o) {
  jlong tag;

  jvmtiError err = jvmti->GetTag(o, &tag);
  check_jvmti_error(jvmti, err, "timeStampObject: Could not get tag.");

  timeStampTag(tag);

  return tag;
}*/

void ETCallBackHandler::nativeArrayStore (JNIEnv *jni,
                                          jclass,
                                          jobject thread,
                                          jobject array,
                                          jint index,
                                          jobject storee) {

  jlong arrayTag  = getTagAndTagIfNeeded(jni, array );
  jlong storeeTag = getTagAndTagIfNeeded(jni, storee);
  jlong threadTag = getTagAndTagIfNeeded(jni, thread);

  jlong oldTargetTag = updateHeapAtAndStamp(arrayTag, index, storeeTag, threadTag);

  this->traceOutputter->doPointerUpdate(currentTime,
					oldTargetTag,
                                        arrayTag,
                                        storeeTag,
                                        index,
                                        threadTag);
}

void ETCallBackHandler::nativeCounts (JNIEnv*,
                                      jclass,
                                      jint heapReads,
                                      jint heapWrites,
                                      jint heapRefReads,
                                      jint heapRefWrites,
                                      jint instCount) {
  lockOn(countsLock);
  totalHeapReads     += heapReads;
  totalHeapWrites    += heapWrites;
  totalHeapRefReads  += heapRefReads;
  totalHeapRefWrites += heapRefWrites;
  totalInstCount     += instCount;
  unlockOn(countsLock);
  this->traceOutputter->doCounts(currentTime,
				 heapReads,
				 heapWrites,
				 heapRefReads,
				 heapRefWrites,
				 instCount);
}


void ETCallBackHandler::GCStart (jvmtiEnv*) {
  verbose_println("Start GC...");
}

void ETCallBackHandler::GCEnd (jvmtiEnv *jvmti) {

  if (useFollowReferences) {
    ++GCsToProcess;

    jvmti->RawMonitorEnter (weakRefMon);
    jvmti->RawMonitorNotify(weakRefMon);
    jvmti->RawMonitorExit  (weakRefMon);

    verbose_println("End GC");
  }
}


void ETCallBackHandler::shadowGCEnd (jvmtiEnv*) {

  HL::Timer timer;
  IF_TIMING (timer.start());
  stringstream verbose_output;

  verbose_output << "...end GC -- got " << deadSet.size() << " dead objects.";

  verbose_println(verbose_output.str());

  HL::Timer deadSetAdjustment_timer;
  IF_TIMING (deadSetAdjustment_timer.start());

  // Before computing death times, adjust dead set
  // (Some VMs do not report weakly reachable objects as reachable, apparently!)
  set<jlong> removed;
  weakRefCleaner->adjustDeadSet(deadSet, removed);

  for (set<jlong>::iterator it = removed.begin();
       it != removed.end();
       it++) {
    cerr << "shadowCGEnd: removed 0x" << hex << *it << " from dead set" << endl;
  }

  HL::Timer computeReachable_timer;
  IF_TIMING ( \
    deadSetAdjustment_timer.stop(); \
    deadSetAdjustment_time += deadSetAdjustment_timer.getElapsedTime(); \
    computeReachable_timer.start(); \
  );

  heap.computeReachable(removed);
  for (set<jlong>::iterator it = removed.begin();
       it != removed.end();
       it++) {
    if (deadSet.count(*it) != 0) {
      cerr << "shadowCGEnd: additionally removed 0x" << hex << *it << " from dead set" << endl;
    }
    deadSet.erase(*it);
  }

  IF_TIMING ( \
    computeReachable_timer.stop();
    computeReachable_time += computeReachable_timer.getElapsedTime(); \
  );

  

  // We put these in a data structure to avoid holding a lock while entering the outputter.
  map<jlong, TimeStamp> deadObjsWithTime;
  
  computeObjectDeathTimes();

  verbose_println("Output death records");

  for (set<jlong>::iterator it = deadSet.begin(); it != deadSet.end(); it++) {

    //If we are creating new tags here, something has gone wrong...
    // assert(tag2TimeStamp.count(*it) != 0);
    deadObjsWithTime[*it] = tagTimeMap[*it];

    heap.removeNode(*it);

    tagTimeMap.eraseTag(*it);
  }

  deadSet.clear();

  if (!useFollowReferences) {
    weakRefCleaner->checkForDeadReferents(deadObjsWithTime);
  }

  for (map<jlong, TimeStamp>::iterator it = deadObjsWithTime.begin();
       it != deadObjsWithTime.end();
       it++) {
    traceOutputter->doDeath(it->first, it->second);
  }

  HL::Timer doGCEnd_timer;
  IF_TIMING (doGCEnd_timer.start());

  traceOutputter->doGCEnd();

  IF_TIMING ( \
    doGCEnd_timer.stop(); \
    doGCEnd_time += doGCEnd_timer.getElapsedTime(); \
    timer.stop(); \
    GCEnd_time += timer.getElapsedTime(); \
  );
}

jclass instDefineClass (JNIEnv *jni, const char *name, jobject loader, const jbyte *buf, jsize bufLen) {
#ifdef TRACE_NEW_OBJECTS
  ATOMICALLY(
  cerr << "Call of JNI DefineClass; class = " << name << endl;
  )
#endif
  return origJNIEnv->DefineClass(jni, name, loader, buf, bufLen);
}

jclass instFindClass (JNIEnv *jni, const char *name) {
#ifdef TRACE_NEW_OBJECTS
  ATOMICALLY(
  cerr << "Call of JNI FindClass; class = " << name << endl;
  )
#endif
  return origJNIEnv->FindClass(jni, name);
}

jobject instAllocObject (JNIEnv *jni, jclass klass) {
#ifdef TRACE_NEW_OBJECTS
  jvmtiError err;
  char *classSig;
  err = theJVMTI->GetClassSignature(klass, &classSig, NULL);
  check_jvmti_error(theJVMTI, err, "instAllocObject: Could not get class signature");
  ATOMICALLY(
  cerr << "Call of JNI AllocObject; class = " << classSig << endl;
  )
  theJVMTI->Deallocate((unsigned char*)classSig);
#endif
  jobject obj = origJNIEnv->AllocObject(jni, klass);
  ((ETCallBackHandler *)cbHandler)->getTagAndTagIfNeeded(jni, obj);
  return obj;
}

jobject instNewObject (JNIEnv *jni, jclass klass, jmethodID methodID, ...) {
#ifdef TRACE_NEW_OBJECTS
  jvmtiError err;
  char *classSig;
  err = theJVMTI->GetClassSignature(klass, &classSig, NULL);
  check_jvmti_error(theJVMTI, err, "instNewObject: Could not get class signature");
  ATOMICALLY(
  cerr << "Call of JNI NewObject; class = " << classSig << endl;
  )
  theJVMTI->Deallocate((unsigned char*)classSig);
#endif
  va_list argptr;
  va_start(argptr, methodID);
  // we will find the object at the start of the constructor
  jobject obj = origJNIEnv->NewObjectV(jni, klass, methodID, argptr);
  va_end(argptr);
  return obj;
}

jobject instNewObjectA (JNIEnv *jni, jclass klass, jmethodID methodID, const jvalue *args) {
#ifdef TRACE_NEW_OBJECTS
  jvmtiError err;
  char *classSig;
  err = theJVMTI->GetClassSignature(klass, &classSig, NULL);
  check_jvmti_error(theJVMTI, err, "instNewObjectA: Could not get class signature");
  ATOMICALLY(
  cerr << "Call of JNI NewObjectA; class = " << classSig << endl;
  )
  theJVMTI->Deallocate((unsigned char*)classSig);
#endif
  // we will find the object at the start of the constructor
  jobject obj = origJNIEnv->NewObjectA(jni, klass, methodID, args);
  return obj;
}

jobject instNewObjectV (JNIEnv *jni, jclass klass, jmethodID methodID, va_list args) {
#ifdef TRACE_NEW_OBJECTS
  jvmtiError err;
  char *classSig;
  err = theJVMTI->GetClassSignature(klass, &classSig, NULL);
  check_jvmti_error(theJVMTI, err, "instNewObjectV: Could not get class signature");
  ATOMICALLY(
  cerr << "Call of JNI NewObjectV; class = " << classSig << endl;
  )
  theJVMTI->Deallocate((unsigned char*)classSig);
#endif
  // we will find the object at the start of the constructor
  jobject obj = origJNIEnv->NewObjectV(jni, klass, methodID, args);
  return obj;
}

jstring instNewString (JNIEnv *jni, const jchar *unicodeChars, jsize len) {
#ifdef TRACE_NEW_OBJECTS
  ATOMICALLY(
  cerr << "Call of JNI NewString" << endl;
  )
#endif
  jstring newString = origJNIEnv->NewString(jni, unicodeChars, len);
  ((ETCallBackHandler *)cbHandler)->getTagAndTagIfNeeded(jni, newString);
  return newString;
}

jstring instNewStringUTF (JNIEnv *jni, const char *bytes) {
#ifdef TRACE_NEW_OBJECTS
  ATOMICALLY(
  cerr << "Call of JNI NewStringUTF" << endl;
  )
#endif
  jstring newString = origJNIEnv->NewStringUTF(jni, bytes);
  ((ETCallBackHandler *)cbHandler)->getTagAndTagIfNeeded(jni, newString);
  return newString;
}

jint instThrowNew (JNIEnv *jni, jclass klass, const char *message) {
#ifdef TRACE_NEW_OBJECTS
  jvmtiError err;
  char *classSig;
  err = theJVMTI->GetClassSignature(klass, &classSig, NULL);
  check_jvmti_error(theJVMTI, err, "instThrowNew: Could not get class signature");
  ATOMICALLY(
  cerr << "Call of JNI ThrowNew; class = " << classSig << endl;
  )
  theJVMTI->Deallocate((unsigned char*)classSig);
#endif
  jint value = origJNIEnv->ThrowNew(jni, klass, message);
  if (value == 0) {
    jthrowable exc = origJNIEnv->ExceptionOccurred(jni);
    ((ETCallBackHandler *)cbHandler)->getTagAndTagIfNeeded(jni, exc);
  }
  return value;
}

void instSetStaticObjectField (JNIEnv *jni, jclass klass, jfieldID fieldId, jobject newVal) {
  cbHandler->JNIPointerUpdated(jni, NULL, klass, fieldId, newVal);
  origJNIEnv->SetStaticObjectField(jni, klass, fieldId, newVal);
}

void instSetObjectField (JNIEnv *jni, jobject obj, jfieldID fieldId, jobject newVal) {
  if (!origJNIEnv->IsSameObject(jni, obj, NULL)) {
    cbHandler->JNIPointerUpdated(jni, obj, NULL, fieldId, newVal);
  }
  origJNIEnv->SetObjectField(jni, obj, fieldId, newVal);
}

void instSetObjectArrayElement (JNIEnv *jni, jobjectArray array, jsize index, jobject newVal) {
    if (!origJNIEnv->IsSameObject(jni, array, NULL)) {
        jsize len = origJNIEnv->GetArrayLength(jni, array);
        // if (((unsigned)index) < len) {
        // TODO: So why exactly are we casting to unsigned here?
        if (index < len) {
            jclass oclass = origJNIEnv->GetObjectClass(jni, array);
            origJNIEnv->SetObjectArrayElement(jni, array, index, newVal);
            if (!jni->ExceptionCheck()) {  // do not log if the update fails
                jthread thread;
                jvmtiError err = theJVMTI->GetCurrentThread(&thread);
                check_jvmti_error(theJVMTI, err, "ETCallBackHandler::instSetObjectArrayElementi:: Could not GetCurrentThread");
                // TODO: check error code
                cbHandler->nativePointerUpdated(jni, oclass, thread, array, newVal, index);
            }
            return;
        }
    }
    // line below will always fail, but we need to have it fail since it would have before
    origJNIEnv->SetObjectArrayElement(jni, array, index, newVal);
}

// jobjectArray NewObjectArray (JNIEnv* jni, jsize length, jclass elementClass, jobject initialElement)
// jbooleanArray NewBooleanArray (JNIEnv *jni, jsize length);
// jbyteArray    NewByteArray    (JNIEnv *jni, jsize length);
// jcharArray    NewCharArray    (JNIEnv *jni, jsize length);
// jshortArray   NewShortArray   (JNIEnv *jni, jsize length);
// jintArray     NewIntArray     (JNIEnv *jni, jsize length);
// jlongArray    NewLongArray    (JNIEnv *jni, jsize length);
// jfloatArray   NewFloatArray   (JNIEnv *jni, jsize length);
// jdoubleArray  NewDoubleArray  (JNIEnv *jni, jsize length);
// jobject NewDirectByteBuffer (JNIEnv *jni, void *address, jlong capacity);

void ETCallBackHandler::RecordClassInfo (int classId, const char *className, int superId) {
  // NOTE!  Classes can appear "out of order", i.e., subclass before superclass
  // Therefore we fill information lazily
#ifdef TRACE_AGENT_RECORDING
  cerr << "Recording class info: " << endl;;
  cerr << "  class 0x" << hex << classId << " -> super 0x" << hex << superId << endl;
#endif
  classId2superId  [classId] = superId;
#ifdef TRACE_AGENT_RECORDING
  cerr << "  class 0x" << hex << classId << " -> name |" << className << "|" << endl;
#endif
  classId2className[classId] = className;
  ClassName2ClassIdMap::const_iterator classNameAndId = className2classId.find(className);
  if (classNameAndId == className2classId.end()) {
#ifdef TRACE_AGENT_RECORDING
    cerr << "  name |" << className << "| -> classId 0x" << hex << classId << endl;
#endif
    className2classId[className] = classId;
    if (strcmp(className, "java/lang/ref/Reference") == 0) {
#ifdef TRACE_AGENT_RECORDING
      cerr << "  *** Reference class id = 0x" << hex << classId << endl;
#endif
      ReferenceClassId = classId;
    }
  }
}

void ETCallBackHandler::RecordFieldInfo (int classId, const char *fieldName, int id, int isStatic) {
#ifdef TRACE_AGENT_RECORDING
  cerr << "Recording field info: classId 0x" << hex << classId << ", name |" << fieldName << "|"
       << " field id 0x" << id << (isStatic ? " S" : " I") << endl;
#endif
  ClassId2FieldsInfoMap::const_iterator fldInfoIter = classId2fieldsInfo.find(classId);
  if (fldInfoIter == classId2fieldsInfo.end()) {
#ifdef TRACE_AGENT_RECORDING
    cerr <<  "  new class id to fields info entry" << endl;
#endif
    pair<ClassId2FieldsInfoMap::iterator,bool> result =
      classId2fieldsInfo.insert(pair<int,FieldsInfo>(classId,FieldsInfo()));
    fldInfoIter = result.first;
  }
  (fldInfoIter->second).push_back(make_pair(fieldName,(isStatic ? -id : id)));
#ifdef TRACE_AGENT_RECORDING
  cerr << "  recorded |" << fieldName << "| x " << hex << (isStatic ? "-0x" : "0x") << id << endl;
#endif

  if (isStatic) {
    // record now because:
    // 1) we can (we have the necessary information, unlike for instance fields); and
    // 2) we must, since statics can be accessed as soon as the class is loaded!
    int slot = staticSlots++;  // note: uses fetch_and_add (:-)
    fieldId2slot[id] = slot;
#ifdef TRACE_AGENT_RECORDING
    cerr << "  static |" << fieldName << "| x " << hex << id << " assigned slot " << dec << slot << endl;
#endif
  }

  if ((classId == ReferenceClassId) &&
      strcmp(fieldName, "referent") == 0) {
#ifdef TRACE_AGENT_RECORDING
    cerr << "  *** Reference.referent field id = 0x" << hex << id << endl;
#endif
    referentFieldNumber = id;
  }
}

int ETCallBackHandler::getSlotsForClass (JNIEnv *jni, jclass klass, jlong classTag) {
  // NOTE: because we have to assign slots lazily, we do so here
#ifdef TRACE_AGENT_RECORDING
  cerr << "getSlotsForClass tag = 0x" << hex << classTag << endl;
#endif

  ClassTag2SlotsMap::const_iterator tag2slotsIt = classTag2slots.find(classTag);
  if (tag2slotsIt != classTag2slots.end()) {
    int slots = tag2slotsIt->second;
#ifdef TRACE_AGENT_RECORDING
    cerr << "  found: " << dec << slots << endl;
#endif
    return slots;
  }

  char *classSig;
  jvmtiError err = aJVMTI->GetClassSignature(klass, &classSig, NULL);
  check_jvmti_error(aJVMTI, err,
                    "ETCallBackHandler::getSlotsForClass: could not get class signature.");
  // TODO: check the string itself
  string className = string(classSig, 1, strlen(classSig)-2);  // skip the L and omit the ;
  aJVMTI->Deallocate((unsigned char *)classSig);
#ifdef TRACE_AGENT_RECORDING
  cerr << "  class name is |" << className << "|" << endl;
#endif
  
  ClassName2ClassIdMap::const_iterator idIter = className2classId.find(className);
  if (idIter == className2classId.end()) {
    cerr << "  class id not found for |" << className << "|" << endl;
    // We see this when the JVM did not tell us about a class.
    // In principle, it should not happen, but in practice it does ...
    // Usually the class won't be loaded later, but because a class can
    // occur in multiple class loaders, it is probably possible.
    // (We don't really deal effectively with multiple loadings of the
    // same class at present.)
    handleClassWithoutId(jni, klass, classTag, className);
    idIter = className2classId.find(className);
    if (idIter == className2classId.end()) {
      cerr << "getSlotsForClass: handleClassWithoutId failed!" << endl;
      return 0;
    }
  }
  int classId = idIter->second;
#ifdef TRACE_AGENT_RECORDING
  cerr << "  found class id 0x" << hex << classId << endl;
#endif

  ClassId2SlotsMap::const_iterator slotsIt = classId2slots.find(classId);
  if (slotsIt != classId2slots.end()) {
    int slots = slotsIt->second;
#ifdef TRACE_AGENT_RECORDING
    cerr << "  found: " << dec << slots << endl;
#endif
    classTag2slots[classTag] = slots;
    return slots;
  }

  jclass superKlass = jni->GetSuperclass(klass);
  if (superKlass == NULL) {
    // klass is java/lang/Object so return 0
    return 0;
  }

  jlong superTag = getTagAndTagIfNeeded(jni, superKlass);
  int superSlots = this->getSlotsForClass(jni, superKlass, superTag);
#ifdef TRACE_AGENT_RECORDING
  cerr << "  super slots for 0x" << hex << classId << " = " << dec << superSlots << endl;
  cerr << "  back in handling of 0x" << hex << classTag << endl;
#endif

  // need to recheck since table may have been updated in the meantime;
  // also, getTagAndTagIfNeeded can cause a 'recursive' call
  tag2slotsIt = classTag2slots.find(classTag);
  if (tag2slotsIt != classTag2slots.end()) {
    int slots = tag2slotsIt->second;
#ifdef TRACE_AGENT_RECORDING
    cerr << "  found on recheck: " << dec << slots << endl;
#endif
    return slots;
  }

  ClassId2FieldsInfoMap::const_iterator fieldsIt = classId2fieldsInfo.find(classId);
  if (fieldsIt != classId2fieldsInfo.end()) {
    // have some fields to register
    for (FieldsInfo::iterator fieldIt = fieldsIt->second.begin();
         fieldIt != fieldsIt->second.end();
         fieldIt++) {
      int fieldId = fieldIt->second;
      if (fieldId > 0) {
        // instance field (statics are done as soon as we see them, in RecordFieldInfo)
        fieldId2slot[fieldId] = superSlots++;
#ifdef TRACE_AGENT_RECORDING
        cerr << "  instance 0x" << hex << fieldId << "(" << fieldIt->first << ") -> " << dec << superSlots-1 << endl;
#endif
      }
    }
  }
  classId2slots [classId ] = superSlots;
  classTag2slots[classTag] = superSlots;
#ifdef TRACE_AGENT_RECORDING
  cerr << "  class total for 0x" << hex << classId << " = " << dec << superSlots << endl;
#endif
  return superSlots;
}

int ETCallBackHandler::handleClassWithoutId (JNIEnv *jni, jclass klass, jlong, string className) {
  // first, get superclass info
  jclass superKlass = jni->GetSuperclass(klass);
  jlong superTag = 0;
  int superSlots = 0;
  string superName = "";
  if (superKlass != NULL) {
    superTag = getTagAndTagIfNeeded(jni, superKlass);
    superSlots = getSlotsForClass(jni, superKlass, superTag);
    char *superSig;
    jvmtiError err = theJVMTI->GetClassSignature(superKlass, &superSig, NULL);
    check_jvmti_error(theJVMTI,err, "ETCallBackHandler::handleClassWithoutId: Could not get class signature.");
    superName = string(superSig+1, strlen(superSig)-2);  // omit the leading L and trailing ;
    theJVMTI->Deallocate((unsigned char *)superSig);
  }

  // TODO(nricci01) This elimiantes an unused variable warning; fix
  // by deleting and refactoring the ifdef.
  if(superSlots >= 0) {
  #ifdef TRACE_AGENT_RECORDING
     cerr << "  super slots for no-id |" << className << "| = " << dec << superSlots << endl;
     cerr << "  back in handling of |" << className << "|" << endl;
  #endif
  }
  // At this point we have dealt with the whole superclass chain, which
  // may have needed the same treatment recursively!

  // Pretend that the rewriter told us about this class.
  // (But note: synthetic ids are numerically distinguishable from regular ones!
  int superId = (superTag == 0 ? 0 : className2classId[superName]);
  int classId = nextSyntheticId++;
#ifdef TRACE_AGENT_RECORDING
  cerr << "  synthetic info for class |" << className << "|  id = 0x" << hex << classId  << endl;
#endif
  RecordClassInfo(classId, className.c_str(), superId);
  stringstream classLine;
  classLine << "C 0x" << hex << classId << " " << className << " 0x" << superId;
  sendInfo(theJVMTI, jni, classLine.str());

  int count;
  jfieldID *fields;
  jvmtiError err = theJVMTI->GetClassFields(klass, &count, &fields);
  check_jvmti_error(theJVMTI, err, "ETCallBackHandler::handleClassWithoutId: GetClassField error");
  for (int i = 0; i < count; ++i) {
    jfieldID field = fields[i];
    char *name;
    char *sig;
    err = theJVMTI->GetFieldName(klass, field, &name, &sig, NULL);
    // TODO: check err
    jint mods;
    err = theJVMTI->GetFieldModifiers(klass, field, &mods);
    // TODO: check err
    bool isStatic = ((mods & ACC_STATIC) != 0);
    int fid = nextSyntheticId++;
    // Pretend the rewriter told us about this field.
    // As with classes, the ids are numerically distinguishable from 'real' ones.
#ifdef TRACE_AGENT_RECORDING
    cerr << "  synthetic info for field; class |" << className << "|  field |"
         << name << "|  id = 0x" << hex << fid << (isStatic ? "  S" : "  I")  << endl;
#endif
    char kind = sig[0];
    if (kind == 'L' || kind == '[') {
      // record only pointer fields (no others need heap slots)
      RecordFieldInfo(classId, name, fid, isStatic);
    }
    stringstream fieldLine;
    fieldLine << "F " << (isStatic ? "S" : "I") << " 0x" << hex << fid << " " << name
              << " 0x" << classId << " " << className << " " << sig;
    sendInfo(theJVMTI, jni, fieldLine.str());
    theJVMTI->Deallocate((unsigned char *)name);
    theJVMTI->Deallocate((unsigned char *)sig);
  }
  theJVMTI->Deallocate((unsigned char *)fields);

  //TODO(nricci01) Why does this have an int return?
  return 0;
}

jclass ETCallBackHandler::getFieldDeclaringClass (JNIEnv *jni, jobject field) {
  if (!haveFieldDeclaringClassId) {
    jclass fieldClass = jni->FindClass("java/lang/reflect/Field");
    FieldDeclaringClassId = jni->GetFieldID(fieldClass, "clazz", "Ljava/lang/Class;");
    haveFieldDeclaringClassId = true;
  }
  return (jclass)jni->GetObjectField(field, FieldDeclaringClassId);
}

string ETCallBackHandler::getFieldName (JNIEnv *jni, jobject field) {
  if (!haveFieldNameId) {
    jclass fieldClass = jni->FindClass("java/lang/reflect/Field");
    FieldNameId = jni->GetFieldID(fieldClass, "name", "Ljava/lang/String;");
    haveFieldNameId = true;
  }
  jstring name = (jstring)jni->GetObjectField(field, FieldNameId);
  const char *nameUTF = jni->GetStringUTFChars(name, NULL);
  string result = string(nameUTF);
  jni->ReleaseStringUTFChars(name, nameUTF);
  return result;
}

string ETCallBackHandler::getFieldDesc (JNIEnv *jni, jobject field) {
  if (!haveFieldTypeId) {
    jclass fieldClass = jni->FindClass("java/lang/reflect/Field");
    FieldTypeId = jni->GetFieldID(fieldClass, "type", "Ljava/lang/Class;");
    haveFieldTypeId = true;
  }
  jclass type = (jclass)jni->GetObjectField(field, FieldTypeId);
  char *signature;
  jvmtiError err = theJVMTI->GetClassSignature(type, &signature, NULL);
  check_jvmti_error(theJVMTI, err, "ETCallBackHandler::getFieldDesc: GetClasSignature error.");
  string result = string(signature);
  theJVMTI->Deallocate((unsigned char *)signature);
  return result;
}

string ETCallBackHandler::getAgentClassName (JNIEnv*, jclass klass) {
  char *signature;
  jvmtiError err = theJVMTI->GetClassSignature(klass, &signature, NULL);
  check_jvmti_error(theJVMTI, err, "ETCallBackHandler::getAgentClassName: GetClasSignature error.");
  char *cstart = signature;
  int len = strlen(signature);
  if (signature[0] == 'L') {
    ++cstart;  // skip the L
    len -= 2;  // remove the L and the ;
  }
  string result = string(cstart, len);
  theJVMTI->Deallocate((unsigned char *)signature);
  return result;
}

typedef concurrent_unordered_map<jlong,string> Tag2StringMap;
static Tag2StringMap classTag2sig;
static Tag2StringMap classTag2name;

jlong ETCallBackHandler::getTag (JNIEnv *jni, jobject obj, bool tagIfNecessary) {

  if (obj == NULL) {
    return 0;
  }

  jlong tag;
  jvmtiError err = theJVMTI->GetTag(obj, &tag);
  check_jvmti_error(theJVMTI, err, "ETCallBackHandler::getTag: could not get tag");

  if (tag != 0) {
    return tag;
  }

  if (tagIfNecessary) {
    return getTagAndTagIfNeeded(jni, obj);
  }

  fatal_error("ETCallBackHandler::getTag: tag required but not present");
  return 0; // Unreachable
}

// may supply either or both of klass and classTag, but obeying these rules:
// if previously cached, classTag is adequate, otherwise requires klass
// if not previously tagged, requires tagIfNecessary
string ETCallBackHandler::getClassSignature (JNIEnv *jni, jclass klass, jlong classTag, bool tagIfNecessary) {

  jvmtiError err;

  if (classTag == 0) {
    classTag = getTag(jni, klass, tagIfNecessary);
    if (classTag == 0) {
      fatal_error("ETCallBackHandler::getClassSignature: class argument is NULL");
    }
  }

  // probe cache
  Tag2StringMap::const_iterator it = classTag2sig.find(classTag);
  if (it != classTag2sig.end()) {
    return it->second;
  }

  if (klass == NULL) {
    fatal_error("ETCallBackHandler::getClassSignature: class is NULL");
  }

  char *sig;
  err = theJVMTI->GetClassSignature(klass, &sig, NULL);
  check_jvmti_error(theJVMTI, err, "ETCallBackHandler::getClassSignature: could not get class signature");

  string signature(sig);

  err = theJVMTI->Deallocate((unsigned char *)sig);
  check_jvmti_error(theJVMTI, err, "ETCallBackHandler::getClassSignature: could not deallocate signature");

  // insert signature into cache
  classTag2sig[classTag] = signature;

  // insert name into cache
  char kind = signature[0];
  int start = 0;
  int count = signature.size();
  if (kind == 'L') {
    start = 1;
    count -= 2;
  }
  string name(signature, start, count);
  classTag2name[classTag] = name;

  return signature;
}

string ETCallBackHandler::getClassName (JNIEnv *jni, jclass klass, jlong classTag, bool tagIfNecessary) {

  if (classTag == 0) {
    classTag = getTag(jni, klass, tagIfNecessary);
    if (classTag == 0) {
      fatal_error("ETCallBackHandler::getClassName: class argument is NULL");
    }
  }

  // probe cache
  Tag2StringMap::const_iterator it = classTag2name.find(classTag);
  if (it != classTag2name.end()) {
    return it->second;
  }

  if (klass == NULL) {
    fatal_error("ETCallBackHandler::getClassSignature: class is NULL");
  }

  getClassSignature(jni, klass, classTag, tagIfNecessary);  // populate table

  return classTag2name[classTag];
}

void ETCallBackHandler::nativeRecordStaticFieldBase (JNIEnv *jni,
                                                     jclass /*klass*/,
                                                     jobject base,
                                                     jobject field) {
  string fieldType = getFieldDesc(jni, field);
  char first = fieldType[0];
  if (first != 'L' && first != '[') {
    // only interested in object fields
    return;
  }

  jlong baseTag = getTagAndTagIfNeeded(jni, base);

  // get the name of the class, as the agent would produce it to us
  jclass declaring = getFieldDeclaringClass(jni, field);
  string className = getAgentClassName(jni, declaring);

  int classId = className2classId[className];  // TODO: check more carefully here

  // get the name of the field
  string fieldName = getFieldName(jni, field);

#ifdef TRACE_AGENT_RECORDING
  cerr << "nativeRecordStaticFieldBase: class |" << className << "| (0x" << hex << classId << ")"
       << "  name |" << fieldName << "|  baseTag = 0x" << baseTag << endl;
#endif

  ClassAndField classAndField = make_pair(classId, fieldName);

  staticFieldBase[classAndField] = baseTag;

  // check if there is a corresponding offset
  ClassAndField2OffsetMap::const_iterator offsetIter = fieldOffset.find(classAndField);
  if (offsetIter == fieldOffset.end()) {
    // no work to do on the field id map yet
    return;
  }

  jlong offset = offsetIter->second;

  FieldsInfo fieldsInfo = classId2fieldsInfo[classId];
  for (FieldsInfo::iterator fldIter = fieldsInfo.begin();
       fldIter != fieldsInfo.end();
       fldIter++) {
    if (fldIter->first == fieldName) {
      int fieldId = fldIter->second;
      tagAndOffset2fieldId[make_pair(baseTag, offset)] = -fieldId;
#ifdef TRACE_AGENT_RECORDING
      cerr << "Recorded base tag 0x" << hex << baseTag << " x offset " << dec << offset
           << " -> agent field id 0x" << hex << fieldId << endl;
#endif
      return;
    }
  }
  // TODO: do something if the field is not found
  cerr << "nativeRecordStaticFieldBase: could not find field id!" << endl;
}

void ETCallBackHandler::nativeRecordStaticFieldOffset (JNIEnv *jni,
                                                       jclass /*klass*/,
                                                       jlong offset,
                                                       jobject field) {

  string fieldType = getFieldDesc(jni, field);
  char first = fieldType[0];
  if (first != 'L' && first != '[') {
    // only interested in object fields
    return;
  }

  // get the name of the class, as the agent would produce it to us
  jclass declaring = getFieldDeclaringClass(jni, field);
  string className = getAgentClassName(jni, declaring);

  int classId = className2classId[className];  // TODO: check more carefully here

  // get the name of the field
  string fieldName = getFieldName(jni, field);

#ifdef TRACE_AGENT_RECORDING
  cerr << "nativeRecordStaticFieldOffset: class |" << className << "| (0x" << hex << classId << ")"
       << "  name |" << fieldName << "|  offset = " << dec << offset << endl;
#endif

  ClassAndField classAndField = make_pair(classId, fieldName);

  fieldOffset[classAndField] = offset;

  // check if there is a corresponding base
  ClassAndField2BaseTagMap::const_iterator baseIter = staticFieldBase.find(classAndField);
  if (baseIter == staticFieldBase.end()) {
    // no work to do on the field id map yet
    return;
  }

  jlong baseTag = baseIter->second;

  FieldsInfo fieldsInfo = classId2fieldsInfo[classId];
  for (FieldsInfo::iterator fldIter = fieldsInfo.begin();
       fldIter != fieldsInfo.end();
       fldIter++) {
    if (fldIter->first == fieldName) {
      int fieldId = fldIter->second;
      tagAndOffset2fieldId[make_pair(baseTag, offset)] = -fieldId;
#ifdef TRACE_AGENT_RECORDING
      cerr << "Recorded base tag 0x" << hex << baseTag << " x offset " << dec << offset
           << " -> agent field id 0x" << hex << fieldId << endl;
#endif
      return;
    }
  }
  // TODO: do something if the field is not found
  cerr << "nativeRecordStaticFieldOffset: could not find field id!" << endl;
}

void ETCallBackHandler::nativeRecordObjectFieldOffset (JNIEnv *jni,
                                                       jclass /*klass*/,
                                                       jlong offset,
                                                       jobject field) {

  string fieldType = getFieldDesc(jni, field);
  char first = fieldType[0];
  if (first != 'L' && first != '[') {
    // only interested in object fields
    return;
  }

  // get the name of the class, as the agent would produce it to us
  jclass declaring = getFieldDeclaringClass(jni, field);
  string className = getAgentClassName(jni, declaring);

  int classId = className2classId[className];  // TODO: check more carefully here

  // get the name of the field
  string fieldName = getFieldName(jni, field);

  ClassAndField classAndField = make_pair(classId, fieldName);

#ifdef TRACE_AGENT_RECORDING
  cerr << "nativeRecordObjectFieldOffset: class |" << className << "| (0x" << hex << classId << ")"
       << "  name |" << fieldName << "|  offset = " << dec << offset << endl;
#endif

  fieldOffset[classAndField] = offset;

  jlong classTag = getTagAndTagIfNeeded(jni, declaring);

  FieldsInfo fieldsInfo = classId2fieldsInfo[classId];
  for (FieldsInfo::iterator fldIter = fieldsInfo.begin();
       fldIter != fieldsInfo.end();
       fldIter++) {
    if (fldIter->first == fieldName) {
      int fieldId = fldIter->second;
      tagAndOffset2fieldId[make_pair(classTag, offset)] = fieldId;
#ifdef TRACE_AGENT_RECORDING
      cerr << "Recorded class tag 0x" << hex << classTag << " x offset " << dec << offset
           << " -> agent field id 0x" << hex << fieldId << endl;
#endif
      return;
    }
  }
  // TODO: do something if the field is not found
  cerr << "nativeRecordObjectFieldOffset: could not find field id!" << endl;
}

void ETCallBackHandler::nativeRecordArrayBaseOffset (JNIEnv *jni,
                                                     jclass /*klass*/,
                                                     jint offset,
                                                     jclass arrayClass) {

  char *classSig;
  jvmtiError err = aJVMTI->GetClassSignature(arrayClass, &classSig, NULL);
  check_jvmti_error(aJVMTI, err,
                    "ETCallBackHandler::nativeRecordArrayBaseOffset: could not get class signature.");

  char second = classSig[1];  // skip leading '['
  if (second != 'L' && second != '[') {
    // only interested in object fields
#ifdef TRACE_AGENT_RECORDING
    cerr << "nativeRecordArrayBaseOffset: ignoring class |" << classSig
         << "|  (offset = " << dec << offset << ")" << endl;
#endif
    aJVMTI->Deallocate((unsigned char *)classSig);
    return;
  }

  jlong classTag = getTagAndTagIfNeeded(jni, arrayClass);
  // who cares if we insert this more than once?  The value should be the same ...
  arrayClassTag2baseOffset[classTag] = offset;
#ifdef TRACE_AGENT_RECORDING
  cerr << "nativeRecordArrayBaseOffset: class |" << classSig << "|  tag 0x" << hex << classTag
       << "  offset = " << dec << offset << endl;
#endif
  aJVMTI->Deallocate((unsigned char *)classSig);
}

void ETCallBackHandler::nativeRecordArrayIndexScale (JNIEnv *jni,
                                                     jclass /*klass*/,
                                                     jint scale,
                                                     jclass arrayClass) {

  char *classSig;
  jvmtiError err = aJVMTI->GetClassSignature(arrayClass, &classSig, NULL);
  check_jvmti_error(aJVMTI, err,
                    "ETCallBackHandler::nativeRecordArrayIndexScale: could not get class signature.");

  char second = classSig[1];  // skip leading '['
  if (second != 'L' && second != '[') {
    // only interested in object fields
#ifdef TRACE_AGENT_RECORDING
    cerr << "nativeRecordArrayIndexScale: ignoring class |" << classSig
         << "|  (scale = " << dec << scale << ")" << endl;
#endif
    aJVMTI->Deallocate((unsigned char *)classSig);
    return;
  }

  jlong classTag = getTagAndTagIfNeeded(jni, arrayClass);
  // who cares if we insert this more than once?  The value should be the same ...
  arrayClassTag2indexScale[classTag] = scale;
#ifdef TRACE_AGENT_RECORDING
  cerr << "nativeRecordArrayIndexScale: class |" << classSig << "|  tag 0x" << hex << classTag
       << "  scale = " << dec << scale << endl;
#endif
  aJVMTI->Deallocate((unsigned char *)classSig);
}

static void scanInitialHeap (
    jvmtiEnv* jvmti,
    JNIEnv *jni,
    ETCallBackHandler *etcb);  // forward declaration

void ETCallBackHandler::VMInit (jvmtiEnv *jvmti, JNIEnv *jni, jthread /*thread*/) {

  jvmtiError err;

  // do this before allocating and starting threads
  if (engageCallbacks) {
    scanInitialHeap(jvmti, jni, this);
  }

  incTime();

  err = jvmti->GetJNIFunctionTable(&origJNIEnv);
  check_jvmti_error(jvmti, err, "ETCallBackHandler::VMInit: Could not get JNI function table.");

  err = jvmti->GetJNIFunctionTable(&instJNIEnv);
  check_jvmti_error(jvmti, err, "ETCallBackHandler::VMInit: Could not get JNI function table (2).");

  if (engageCallbacks) {
    instJNIEnv->DefineClass           = instDefineClass;
    instJNIEnv->FindClass             = instFindClass;
    instJNIEnv->AllocObject           = instAllocObject;
    instJNIEnv->NewObject             = instNewObject;
    instJNIEnv->NewObjectA            = instNewObjectA;
    instJNIEnv->NewObjectV            = instNewObjectV;
    instJNIEnv->NewString             = instNewString;
    instJNIEnv->NewStringUTF          = instNewStringUTF;
    instJNIEnv->ThrowNew              = instThrowNew;
    instJNIEnv->SetStaticObjectField  = instSetStaticObjectField;
    instJNIEnv->SetObjectField        = instSetObjectField;
    instJNIEnv->SetObjectArrayElement = instSetObjectArrayElement;
  }

  err = jvmti->SetJNIFunctionTable(instJNIEnv);
  check_jvmti_error(jvmti, err, "ETCallBackHandler::VMInit: Could not set JNI function table.");

  // The three lines before are for testing the callback
  // jclass ETclass = jni->FindClass("ElephantTracks");
  // jfieldID tester = jni->GetStaticFieldID(ETclass, "updateTest", "Ljava/lang/Object;");
  // instJNIEnv->SetStaticObjectField(jni, ETclass, tester, NULL);

  traceOutputter->initializeOutputThread(jvmti, jni);

  //Start the cleaner thread
  //is there a better way to do this?
  jclass threadClass = jni->FindClass("java/lang/Thread");

  //Run agent thread needs a jthread
  //For at least some JVMs the name must be initialized
  //We prefer to avoid calling an <init> method, so initialize it directly
  jthread weakCleanerThread = jni->AllocObject(threadClass);
  jfieldID nameField = jni->GetFieldID(threadClass, "name", "Ljava/lang/String;");
  jstring name = jni->NewStringUTF("ETWeakRefCleaner");
  jni->SetObjectField(weakCleanerThread, nameField, name);

  err = jvmti->RunAgentThread(weakCleanerThread, cleanerStaticWrapper, this,
                              JVMTI_THREAD_NORM_PRIORITY);
  check_jvmti_error(jvmti, err, "ETCallBackHandler::VMInit: Could not start WeakRefCleaner thread.");
}

static vector<jlong> initialLive;

static jlong initialTag (jlong *tag_ptr) {
  if (tag_ptr == NULL) {
    return 0;
  }

  jint tag = *(tag_ptr);
  if (tag != 0) {
    return tag;
  }

  tag = (*tag_ptr = nextTag++);
  initialLive.push_back(tag);
  return tag;
}

// not used by FollowReferences
static jint initialScanObjectCB (
    jlong /*class_tag*/,
    jlong /*size*/,
    jlong *tag_ptr,
    jint /*length*/,
    void* /*user_data*/) {
  initialTag(tag_ptr);
  return JVMTI_VISIT_OBJECTS;
}

static jint initialScanHeapReferenceCB (
    jvmtiHeapReferenceKind /*kind*/,
    const jvmtiHeapReferenceInfo* /*info*/,
    jlong /*class_tag*/,
    jlong /*referrer_class_tag*/,
    jlong /*size*/,
    jlong *tag_ptr,
    jlong *referrer_tag_ptr,
    jint /*length*/,
    void* /*user_data*/) {
  initialTag(referrer_tag_ptr);
  initialTag(tag_ptr);
  return JVMTI_VISIT_OBJECTS;
}

static jint initialScanArrayPrimitiveValueCB (
    jlong /*class_tag*/,
    jlong /*size*/,
    jlong *tag_ptr,
    jint /*element_count*/,
    jvmtiPrimitiveType /*element_type*/,
    const void* /*elements*/,
    void * /*user_data*/) {
  initialTag(tag_ptr);
  
  // It does not really matter what we return,
  // since a primitive array should have no neighbors
  return JVMTI_VISIT_OBJECTS;
}

static jint initialScanStringPrimitiveValueCB (
    jlong /*class_tag*/,
    jlong /*size*/,
    jlong *tag_ptr,
    const jchar* /*value*/,
    jint /*value_length*/,
    void* /*user_data*/) {
  initialTag(tag_ptr);

  return JVMTI_VISIT_OBJECTS;
}

static void scanInitialHeap (
    jvmtiEnv* jvmti,
    JNIEnv *jni,
    ETCallBackHandler *etcb) {

  initialLive.clear();  // shouldn't be necessary, but safe and clear

  bool doHeapIteration = true;

  jvmtiHeapCallbacks heapCbs;
  memset(&heapCbs, 0, sizeof(heapCbs));

  jvmtiError err;
  if (doHeapIteration) {
    heapCbs.heap_iteration_callback         = initialScanObjectCB;
    err = jvmti->IterateThroughHeap(0, NULL, &heapCbs, (void*)etcb);
    check_jvmti_error(jvmti, err, "ETCallBackHandler::scanInitialHeap could not IterateThroughHeap");
  } else {
    heapCbs.heap_reference_callback         = initialScanHeapReferenceCB;
    heapCbs.array_primitive_value_callback  = initialScanArrayPrimitiveValueCB;
    heapCbs.string_primitive_value_callback = initialScanStringPrimitiveValueCB;
    err = jvmti->FollowReferences(0, NULL, NULL, &heapCbs, (void*)etcb);
    check_jvmti_error(jvmti, err, "ETCallBackHandler::scanInitialHeap could not FollowReferences");
  }

  int numLive = initialLive.size();
  jlong *liveTags;
  err = jvmti->Allocate(numLive * sizeof(jlong), (unsigned char **)&liveTags);
  // TODO check error

  jlong *nextLiveTag = liveTags;
  for (vector<jlong>::const_iterator it = initialLive.begin(); it != initialLive.end(); ++it) {
    *nextLiveTag++ = *it;
  }
  initialLive.clear();  // release space

  jint numObjects;
  jobject *objects;
  jlong *tags;
  err = jvmti->GetObjectsWithTags((jint)numLive, liveTags, &numObjects, &objects, &tags);
  // TODO check error

  int numTags = (int)nextTag;
  int *indexOf;
  err = jvmti->Allocate(numTags * sizeof(int), (unsigned char **)&indexOf);
  // TODO check error

  for (int tag = 0; tag < numTags; ++tag) {
    indexOf[tag] = -1;
  }

  for (int idx = 0; idx < numObjects; ++idx) {
    int tag = (int)(tags[idx]);
    if (tag < 0 || tag >= numTags) {
      cerr << "tag out of range!  0x" << hex << tag << endl;
    } else {
      indexOf[tag] = idx;
    }
  }

  for (int tag = 0; tag < numTags; ++tag) {
    int idx = indexOf[tag];
    if (idx < 0) {
      continue;
    } else if (idx >= numObjects) {
      cerr << "Found bad idx!  Value is 0x" << hex << idx
           << "  tag is 0x" << tag
           << "  num tags is 0x" << numTags << endl;
      continue;
    }
    jclass klass = jni->GetObjectClass(objects[idx]);

    jlong classTag;
    err = jvmti->GetTag(klass, &classTag);
    // TODO check error

    char *classSig;
    err = jvmti->GetClassSignature(klass, &classSig, NULL);
    // TODO check error

    /*
    cerr << "Initial object, tag = 0x" << hex << tags[idx]
         << "  class tag = 0x" << classTag
         << "  class = |" << classSig << "|"
         << endl;
    */

    jvmti->Deallocate((unsigned char *)classSig);
    etcb->handleInitialObject(jvmti, jni, objects[idx], tags[idx]);
  }
  jvmti->Deallocate((unsigned char *)indexOf);
  jvmti->Deallocate((unsigned char *)liveTags);
  jvmti->Deallocate((unsigned char *)objects);
  jvmti->Deallocate((unsigned char *)tags);
}

static jint scanLiveObjectsCBStaticWrapper (
    jvmtiHeapReferenceKind /*reference_kind*/,
    const jvmtiHeapReferenceInfo* /*reference_info*/,
    jlong /*class_tag*/,
    jlong /*referrer_class_tag*/,
    jlong /*size*/,
    jlong* tag_ptr,
    jlong* referrer_tag_ptr,
    jint /*length*/,
    void* user_data) {

  ETCallBackHandler* cbHandler = (ETCallBackHandler*)user_data;
  cbHandler->scanLiveObject(*tag_ptr);
  if (referrer_tag_ptr != NULL) {
    cbHandler->scanLiveObject(*referrer_tag_ptr);
  }
  return JVMTI_VISIT_OBJECTS;
}

static jint scanStringPrimitiveCBStaticWrapper (
    jlong /*class_tag*/, 
    jlong /*size*/, 
    jlong* tag_ptr, 
    const jchar* /*value*/, 
    jint /*value_length*/, 
    void* user_data) {
#ifdef ET_DEBUG
  ATOMICALLY(
  cerr << hex << "scanStringPrimitiveCBStaticWrapper called. Tag: " << *tag_ptr << endl;
  )
#endif

  ETCallBackHandler* cbHandler = (ETCallBackHandler*)user_data;
  return cbHandler->scanLiveObject(*tag_ptr);
}

static jint scanArrayPrimitiveCBStaticWrapper (
    jlong /*class_tag*/, 
    jlong /*size*/, 
    jlong *tag_ptr, 
    jint /*element_count*/, 
    jvmtiPrimitiveType /*element_type*/, 
    const void* /*elements*/, 
    void *user_data) {
#ifdef ET_DEBUG
  ATOMICALLY(
  cerr << hex << "scanArrayPrimitiveCBStaticWrapper caled. Tag: " << *tag_ptr << endl;
  )
#endif

  ETCallBackHandler* cbHandler = (ETCallBackHandler*)user_data;
  return cbHandler->scanLiveObject(*tag_ptr);
}

static jint heapIterationCBStaticWrapper (
    jlong /*class_tag*/,
    jlong /*size*/,
    jlong *tag_ptr,
    jint /*length*/,
    void *user_data) {
  ETCallBackHandler* cbHandler = (ETCallBackHandler*)user_data;
  return cbHandler->scanLiveObject(*tag_ptr);
}

//Follow References will fill this set.
//Because there is no good way to get a return
//from it due to the JVMTI api, we will have to 
//have it fill this and remember to clear it.
//in fakeAGC
static set<jlong> liveObjects;
jint ETCallBackHandler::scanLiveObject (jlong tag) {

  if (tag != 0) {
    liveObjects.insert(tag);
  }
  return JVMTI_VISIT_OBJECTS;
}

void ETCallBackHandler::VMDeath (jvmtiEnv *jvmti) {
  fakeAGC(jvmti);

  traceOutputter->waitToFinishOutput();

  IF_TIMING ( \
    cerr << "Time spent in nativeVarEvent: " << nativeVarEvent_time << endl; \
    cerr << "Time spent in timeStampTag: " << timeStampTag_time << endl; \
    cerr << "Time spent in fakeAGC: " << fakeAGC_time << endl; \
    cerr << "  Time spent in finding dead objects: " << deadFinder_time << endl; \
    cerr << "  Time spend in GCEnd: " << GCEnd_time << endl; \
    cerr << "    Time sepnt in computDeathTimes: " << computeDeathTimes_time  << endl; \
    cerr << "    Time spent in computeReachable: " << computeReachable_time << endl; \
    cerr << "    Time spent adjusting deads set: " << deadSetAdjustment_time << endl; \
    cerr << "    Time spent doGCEnd: " << doGCEnd_time << endl; \
    cerr << "      Time spent in sort: " << traceOutputter->totalSortTime << endl; \
    cerr << "      Time spent in record output " << traceOutputter->outputRecords_time << endl; \
    cerr << "        Time spent combining records: " << traceOutputter->combiningRecords_time << endl; \
  );
}

void ETCallBackHandler::fakeAGC (jvmtiEnv *jvmti) {
  jvmtiError err;
  jvmtiHeapCallbacks heapCbs;
  vector<jlong> objsToFree;

  HL::Timer fakeAGC_timer;
  IF_TIMING (fakeAGC_timer.start());

  verbose_println("ETCallBackHandler: fakeAGC entered");
  lockOn(deadSetLock);
  lockOn(heapLock);
  // lockOn(tag2TimeStampLock);

  memset(&heapCbs, 0, sizeof(heapCbs));

  // TODO(nricci01) This comment is outdated, fix it.
  //Essentially the problem this function solves is that there may be dead objects
  //still hanging around in the heap, but we will never see the deallocation for them,
  //because the VM is dead, and we will get no more events.
  //So, first we scan all the live objects and time stamp them,
  //Then scan all objects, concluding that ones that do not have the current time stamp are dead.
  //Then we do an artificial GC.

  HL::Timer deadFinder_timer;
  IF_TIMING (deadFinder_timer.start())

  if (useFollowReferences) {

    heapCbs.heap_reference_callback = scanLiveObjectsCBStaticWrapper; // these used only if following refs
    heapCbs.string_primitive_value_callback = scanStringPrimitiveCBStaticWrapper;
    heapCbs.array_primitive_value_callback = scanArrayPrimitiveCBStaticWrapper;

    err = jvmti->FollowReferences(0, NULL, NULL, &heapCbs, (void*)this);
    check_jvmti_error(jvmti, err, "ETCallBackHandler::fakeAGC could not follow references");

  } else {

    err = jvmti->ForceGarbageCollection();
    check_jvmti_error(jvmti, err, "ETCallBackHandler::fakeAGC could not force a GC");

    heapCbs.heap_iteration_callback = heapIterationCBStaticWrapper;   // used only if iterating
    err = jvmti->IterateThroughHeap(0, NULL, &heapCbs, (void*)this);
    check_jvmti_error(jvmti, err, "ETCallBackHandler::fakeAGC could not iterate over heap");
  }

  IF_TIMING ( \
    deadFinder_timer.stop(); \
    deadFinder_time += deadFinder_timer.getElapsedTime(); \
  );

  for (TagTimeMap::Iterator it = tagTimeMap.begin();
       it != tagTimeMap.end();
       it++) {
    jlong tag = (*it).first;
    if (liveObjects.count(tag) == 0) { //object is not live
      objsToFree.push_back(tag);
    }	
  } 

  liveObjects.clear();

  for (vector<jlong>::iterator it = objsToFree.begin(); it != objsToFree.end(); it++) {
    ObjectFree(jvmti, *it);
  }
  this->shadowGCEnd(jvmti);

  // unlockOn(tag2TimeStampLock);
  unlockOn(heapLock);
  unlockOn(deadSetLock);

  IF_TIMING ( \
    fakeAGC_timer.stop(); \
    fakeAGC_time += fakeAGC_timer.getElapsedTime(); \
  );
  verbose_println("ETCallBackHandler: fakeAGC exit"); \
}

void ETCallBackHandler::weakRefCleanerThreadStart (jvmtiEnv *jvmti, JNIEnv *jni) {
  weakRefCleaner->cleanerThreadStart(jvmti, jni);
}

#ifdef DEBUG_MULTINODE
bool ETCallBackHandler::isMultiNode (jlong tag) {

  for (vector<jlong>::iterator it = multiNodes.begin(); it != multiNodes.end(); it++) {
    if (*it == tag) {
      return true;
    }
  }

  return false;
}
#endif

// Local Variables:
// mode:C++
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
