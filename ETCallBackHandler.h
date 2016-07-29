#ifndef MEARTHCALLBACKHANDLER_H_
#define MEARTHCALLBACKHANDLER_H_

#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "jvmti.h"
#include "tbb/atomic.h"
#include "tbb/concurrent_unordered_map.h"

#include "main.h"
#include "HashGraph.h"
#include "TagTimeMap.h"
#include <string>
#include <map>
#include <queue>
#include <iostream>
#include <fstream>
#include <tbb/concurrent_hash_map.h>
#include "HashGraph.h"
#include "WeakRefCleaner.h"

using namespace tbb;
using namespace std;

class WeakRefCleaner;
class FlatTraceOutputter;

class ETCallBackHandler {

public:
  ETCallBackHandler (jvmtiEnv *jvmti, JavaVM* vm,  FlatTraceOutputter* traceOutputter);

  void nativeNewObj (JNIEnv *env,
                     jclass klass,
                     jobject thread,
                     jobject o,
                     bool isVM,
                     jint site);

  void nativeNewObj (JNIEnv *env,
                     jclass klass,
                     jobject thread,
                     jobject o,
                     const char* classSig,
                     AllocKind kind,
                     jint site);

  void nativeNewObj (JNIEnv *env,
                     jclass klass,
                     jobject thread,
                     jstring className,
                     jint slot,
                     jint site);

  jthread getCurrentThread (); 

  void nativeMethodEntry (JNIEnv *jni_env,
                          jclass klass,
                          jthread thread,
                          jint methodId,
                          jobject receiver);

  void nativeMethodExit (JNIEnv *jni_env,
                         jclass klass,
                         jobject thread,
                         jobject exception,
                         jint methodId,
                         jobject receiver);

  void nativeExceptionThrow (JNIEnv *jni_env,
                             jclass klass,
                             jobject thread,
                             jobject exception,
                             jint methodId,
                             jobject receiver);

  void nativeExceptionHandle (JNIEnv *jni_env,
                              jclass klass,
                              jobject thread,
                              jobject exception,
                              jint methodId,
                              jobject receiver);

  void nativeArrayStore (JNIEnv *jni,
                         jclass klass,
                         jthread thread,
                         jobject array,
                         jint index,
                         jobject storee);

 void nativePointerUpdated (JNIEnv *env,
                            jclass klass,
                            jobject thread,
                            jobject origin,
                            jobject newTarget,
                            jlong offset);

  void nativePointerUpdated (JNIEnv *env,
                             jclass klass,
                             jobject oldTarget,
                             jobject origin,
                             jobject newTarget,
                             jint fieldId);

  void scanStaticsOf (jvmtiEnv *jvmti,
                      JNIEnv *jni,
                      queue<jclass> toScan);

  void handleInitialObject (jvmtiEnv *jvmti,
                            JNIEnv *jni,
                            jobject obj,
                            jlong tag);

  void JNIPointerUpdated (JNIEnv *env,
                          jobject obj,
                          jclass klass,
                          jfieldID fieldId,
                          jobject newVal);

  void syntheticPointerUpdated (JNIEnv *jni,
                                jlong originTag,
                                jlong newTargetTag,
                                jlong time);

  void nativePointerMovedOff (JNIEnv *env,
                              jclass klass,
                              jobject oldTarget,
                              jobject origin,
                              jobject newTarget);

  void syntheticPointerMovedOff (JNIEnv *jni,
                                 jlong oldTargetTag,
                                 jlong originTag,
                                 jlong newTargetTag,
                                 jlong time);

  void noteReferentUpdate (JNIEnv *env,
                           jclass klass,
                           jobject origin,
                           jobject oldTarget,
                           jobject newTarget);

  void nativeStampThis (JNIEnv *jni,
                        jclass klass,
                        jstring className);

  void nativeVarEvent (JNIEnv *jni,
                       jclass klass,
                       jint numObjs,
                       jobject o0,
                       jobject o1,
                       jobject o2,
                       jobject o3,
                       jobject o4);

  void nativeUninitPutfield (JNIEnv *jni,
                             jclass klass,
                             jobject thread,
                             jobject newTarget,
                             jint objSlot,
                             jint fieldId);

  jboolean nativeDoSystemArraycopy (JNIEnv *jni,
                                    jclass klass,
                                    jobject src,
                                    jint srcIdx,
                                    jobject dst,
                                    jint dstIdx,
                                    jint cnt);

  void nativeCounts (JNIEnv *jni,
                     jclass klass,
                     jint heapReads,
                     jint heapWrites,
                     jint heapRefReads,
                     jint heapRefWrites,
                     jint instCount);

  void nativeNewObj (JNIEnv *env,
                     jclass klass,
                     jobject thread,
                     jobject obj,
                     const char *className,
                     AllocKind kind);

  void nativeRecordStaticFieldBase (JNIEnv *jni,
                                            jclass klass,
                                            jobject base,
                                            jobject field);

  void nativeRecordStaticFieldOffset (JNIEnv *jni,
                                              jclass klass,
                                              jlong offset,
                                              jobject field);

  void nativeRecordObjectFieldOffset (JNIEnv *jni,
                                              jclass klass,
                                              jlong offset,
                                              jobject field);

  void nativeRecordArrayBaseOffset (JNIEnv *jni,
                                            jclass klass,
                                            jint offset,
                                            jclass arrayClass);

  void nativeRecordArrayIndexScale (JNIEnv *jni,
                                            jclass klass,
                                            jint scale,
                                            jclass arrayClass);

  jboolean nativeDoSystemArraycopy (JNIEnv *jni,
                                            jclass klass,
                                            jobject thread,
					    jobject src,
                                            jint srcIdx,
                                            jobject dst,
                                            jint dstIdx,
                                            jint cnt);

  void ObjectFree (jvmtiEnv *jvmti,
                   jlong tag);

  void GCStart (jvmtiEnv *jvmti);

  void GCEnd (jvmtiEnv *jvmti);

  // Sometimes we need to do JNI calls that are not intercepted by the agent.
  jniNativeInterface* getUninstrumentedJNI ();

  void shadowGCEnd (jvmtiEnv *jvmti);

  void VMInit (jvmtiEnv *jvmti,
               JNIEnv *jni,
               jthread thread);

  void VMDeath (jvmtiEnv *jvmti);

  void fakeAGC (jvmtiEnv *jvmti);

  ~ETCallBackHandler ();

  void RecordClassInfo (int classId,
                        const char *className,
                        int superId);

  void RecordFieldInfo (int classId,
                        const char *fieldName,
                        int fieldId,
                        int isStatic);

  int getSlotsForClass (JNIEnv *jni,
                        jclass klass,
                        jlong classTag);

  int handleClassWithoutId (JNIEnv *jni,
                            jclass klass,
                            jlong classTag,
                            string className);

  jint scanLiveObject (jlong tag);

  void weakRefCleanerThreadStart (jvmtiEnv *jvmti,
                                  JNIEnv* jni);

  jint scanAllObjectsCB (jlong class_tag,
                         jlong size,
                         jlong* tag_ptr,
                         jint length);

  bool diedBefore (jlong obj1,
                   jlong obj2);


  jlong updateHeapAt (jlong originTag,
                      int slot,
                      jlong newTargetTag);



  jlong updateHeapAtAndStamp (jlong originTag,
                      	      int slot,
                      	      jlong newTargetTag,
		      	      jlong threadTag);

  jlong getNextTag ();

  jlong readNextTag ();

  jlong getTagAndTagIfNeeded (JNIEnv* jni,
                              jobject o);

  tbb::atomic<int> GCsToProcess;

  jlong totalHeapReads;
  jlong totalHeapWrites;
  jlong totalHeapRefReads;
  jlong totalHeapRefWrites;
  jlong totalInstCount;

private:
  double timeStampTag_time;
  double computeDeathTimes_time;
  double fakeAGC_time;
  double deadFinder_time;
  double GCEnd_time;
  double computeReachable_time;
  double deadSetAdjustment_time;
  double doGCEnd_time;
  double nativeVarEvent_time;

  typedef concurrent_hash_map<unsigned long, unsigned long> TagTable;

  void nativePointerUpdate (JNIEnv *env,
                            jclass klass,
                            jthread thread,
                            jobject origin,
                            jobject newTarget,
                            jint fieldID);

  void pointerSlotUpdated (JNIEnv *jni,
                                   jobject thread,
                                   jobject origin,
                                   jlong originTag,
                                   jobject newTarget,
                                   jlong newTargetTag,
                                   int fieldId,
                                   int slot,
                                   bool isArray);

  void nativeException (JNIEnv *jni,
                                jclass klass,
                                jobject thread,
                                jobject exception,
                                jint methodId,
                                jobject receiver,
                                ExceptKind kind);

  jclass getFieldDeclaringClass (JNIEnv *jni,
                                         jobject field);

  string getFieldName (JNIEnv *jni,
                               jobject field);

  string getFieldDesc (JNIEnv *jni,
                               jobject field);

  string getAgentClassName (JNIEnv *jni,
                                    jclass klass);

  jlong getTag (JNIEnv *jni,
                        jobject obj,
                        bool tagIfNecessary = false);

  string getClassSignature (JNIEnv *jni,
                                    jclass klass,
                                    jlong classTag,
                                    bool tagIfNecessary = false);

  string getClassName (JNIEnv *jni,
                               jclass klass,
                               jlong classTag,
                               bool tagIfNecessary = false);

  //TagTable tag2TimeStamp;
  TagTimeMap tagTimeMap;

  map<string,bool> methodSet;
  map<string,long> method2id;
  jvmtiEnv *aJVMTI;
  set<jlong> deadSet;
  FlatTraceOutputter* traceOutputter;

  // data structures for tracking updates by field

  // agent field id -> object slot in shadow heap
  typedef concurrent_unordered_map<int,int> FieldId2slotMap;
  FieldId2slotMap fieldId2slot;

  // agent class id -> vector(field name x agent field id)
  typedef vector<pair<string,int> > FieldsInfo;
  typedef concurrent_unordered_map<int,FieldsInfo> ClassId2FieldsInfoMap;
  ClassId2FieldsInfoMap classId2fieldsInfo;

  // agent class id -> #slots
  typedef concurrent_unordered_map<int,int> ClassId2SlotsMap;
  ClassId2SlotsMap classId2slots;

  // class name -> agent class id
  typedef concurrent_unordered_map<string,int> ClassName2ClassIdMap;
  ClassName2ClassIdMap className2classId;

  // agent class id -> class name
  typedef concurrent_unordered_map<int,string> ClassId2ClassNameMap;
  ClassId2ClassNameMap classId2className;

  // agent class id -> agent superclass id
  typedef concurrent_unordered_map<int,int> ClassId2SuperIdMap;
  ClassId2SuperIdMap classId2superId;

  // class tag -> #slots
  typedef concurrent_unordered_map<jlong,int> ClassTag2SlotsMap;
  ClassTag2SlotsMap classTag2slots;

  // number of static slots seen so far
  tbb::atomic <int> staticSlots;

  // used for assigning ids when a class was not presented to the rewriter (ugh!)
  tbb::atomic <int> nextSyntheticId;

  // data structures for dealing with updates via Unsafe putObject, etc.

  // base tag x field name -> static base for a field
  typedef pair<jlong,string> ClassAndField;
  typedef concurrent_unordered_map<ClassAndField,jlong> ClassAndField2BaseTagMap;
  ClassAndField2BaseTagMap staticFieldBase;

  // base/class tag x field name -> offset for a field (static or instance)
  typedef concurrent_unordered_map<ClassAndField,jlong> ClassAndField2OffsetMap;
  ClassAndField2OffsetMap fieldOffset;

  // base/class tag x offset -> agent field id
  typedef pair<jlong,jlong> TagAndOffset;
  typedef concurrent_unordered_map<TagAndOffset,int> TagAndOffset2FieldIdMap;
  TagAndOffset2FieldIdMap tagAndOffset2fieldId;

  // array class tag -> base for indexing
  typedef concurrent_unordered_map<jlong,int> ArrayClassTag2IntMap;
  ArrayClassTag2IntMap arrayClassTag2baseOffset;

  // array class -> scale for indexing
  ArrayClassTag2IntMap arrayClassTag2indexScale;

  typedef pair<jlong,jfieldID> ClassTagAndJFieldID;
  typedef concurrent_unordered_map<ClassTagAndJFieldID,int> TagAndJFieldID2AgentId;
  TagAndJFieldID2AgentId tagAndJFieldID2AgentId;

  tbb::atomic<jlong> currentTime;

  jlong incTime () {
    return ++(this->currentTime);
  }

  void timeStampTag (jlong objectTag, jlong threadTag);

  bool seenBefore (string method);

  // jlong timeStampObject (jvmtiEnv *jvmti,
  //                       jobject o);

  HashGraph heap;

  void computeObjectDeathTimes ();

  bool isUnreachable (jlong obj);

  bool inDeathList;

  ofstream deathChunkFile;

  jlong tagObject (jvmtiEnv *jvmti,
                   jobject o,
                   bool *newTag = NULL);

  void addObjectToHeap (jlong tag,
                        int slots);

  void lockOn (jrawMonitorID lock);

  void unlockOn (jrawMonitorID lock);

  //Cleans up weak references that may have been nulled by the GC
  WeakRefCleaner *weakRefCleaner;

  jrawMonitorID heapLock;
  jrawMonitorID vertexMapLock;
  jrawMonitorID deadSetLock;
  jrawMonitorID clockLock;
  jrawMonitorID tagLock;
  jrawMonitorID countsLock;
  jrawMonitorID weakRefMon;
#ifdef DEBUG_MULTINODE
  vector<jlong> multiNodes;
  bool isMultiNode (jlong);
#endif 

};

class DiedBeforeComparator {
public:
  DiedBeforeComparator (ETCallBackHandler &etcb) : handler(etcb) { }
  
  bool operator () (jlong obj1, jlong obj2) {
    return handler.diedBefore(obj1, obj2);
  }
  
private:
  ETCallBackHandler &handler;
};

#endif /*MEARTHCALLBACKHANDLER_H_*/

// Local Variables:
// mode:C++
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
