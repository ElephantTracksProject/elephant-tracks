#include "main.h"
#include "gzstream.h"
#include "ExternalClassInstrumenter.h"
#include "ClassInstrumenter.h"
#include "ETCallBackHandler.h"
#include "FlatTraceOutputter.h"
#include <iostream>
#include <fstream>
#include <jni.h>
#include <jvmti.h>
#include <agent_util.h>
#include <java_crw_demo.h>

// #define LOG_PHASE_CHANGES 1

using namespace std;

JavaVM *theVM;
jvmtiEnv* cachedJvmti;

unsigned char wantRecord[256];
bool includeDeathChunks = true;
int ReferenceClassId = -1;
int referentFieldNumber = -1;

void setCapabilities (jvmtiEnv *env);
void setEvents (jvmtiEnv *jvmti);
bool wantToInstrument (const char* name,
                       jclass klass,
                       const unsigned char* klass_data,
                       jint klass_data_len);
map<string,string> getOptions (char* optString);

static jrawMonitorID theBigLock;

static const char *ETclassName = "ElephantTracks";
// static const char *ETclassName = "net/redline/ElephantTracks";
static jfieldID engagedField;

// This is slightly complicated:
// outputFile and one of ofs/ogz will point to the same stream (one will be null)
// but I need the ogz/ofs pointer later to call "close" (since ostream does not have that method).
// So we check which is null. I did not want to mess around with down casting.
static ostream   *outputFile = NULL;
static ofstream  *ofs        = NULL;
static ogzstream *ogz        = NULL;

static HL::Timer programTimer;

static ClassInstrumenter* instrumenter;
static Phase vmInitialized = Phase::ONLOAD;
static jclass etClass_g = NULL; 
// TODO static char *etjar_g = NULL;

bool engageCallbacks = true;
bool countCalls = false;

bool verbose = false;
void verbose_println (string s) {
    if (verbose) {
        cout << s << endl;
    }
}

bool showFields = false;  // whether to show the field being updated in pointer moved records

void
// TODO add_et_jar_to_bootclasspath( jvmtiEnv *jvmti )
// TODO {
// TODO     jvmtiError error;
// TODO    
// TODO     error = jvmti->ddToBootstrapClassLoaderSearch( (const char*)etjar_g);
// TODO     check_jvmti_error(jvmti, error, "Cannot add to boot classpath");
// TODO }

void enter_critical_section (jvmtiEnv *jvmti) {
    jvmtiError error = jvmti->RawMonitorEnter(theBigLock);
    check_jvmti_error(jvmti, error, "Cannot enter with raw monitor");
}

void exit_critical_section (jvmtiEnv *jvmti) {
    jvmtiError error = jvmti->RawMonitorExit(theBigLock);
    check_jvmti_error(jvmti, error, "Cannot exit with raw monitor");
}

void setCallbacks (jvmtiEnv *jvmti);
void instrumentClass( jvmtiEnv *jvmti,
                      JNIEnv* env,
                      jclass class_being_redefined,
                      jobject loader,
                      const char* name,
                      jobject protection_domain,
                      jint class_data_len,
                      const unsigned char* class_data,
                      jint* new_class_data_len,
                      unsigned char** new_class_data )
{
#ifdef LOG_PHASE_CHANGES
    cerr << "instrumentClass: class loaded - " << name << endl;
#endif
    if (wantToInstrument(name, class_being_redefined, class_data, class_data_len)) {
        /*Just pass on the call to the instrumenter */
        instrumenter->instrumentClass(jvmti, env, class_being_redefined, loader, name,
                                      protection_domain, class_data_len, class_data,
                                      new_class_data_len, new_class_data);
    }
}

ETCallBackHandler* cbHandler;

void sendInfo (jvmtiEnv *jvmti, JNIEnv *jni, string s) {
  char first = s[0];
  ClassInstrumenter::SendKind kind =
    (first == 'C' ? ClassInstrumenter::SynthClassInfo :
                    ClassInstrumenter::SynthFieldInfo);
  instrumenter->sendInfo(jvmti, jni, kind, s.c_str());
}


static void traceBufferFull (void) {
  verbose_println("TraceBufferFull got called.");
  cbHandler->fakeAGC(cachedJvmti);
}

static void cbGCEnd (jvmtiEnv *jvmti) {
  cbHandler->GCEnd(jvmti);
}

static void nativePointerUpdated (JNIEnv *env,
                                  jclass klass,
                                  jobject origin,
                                  jobject newTarget,
                                  jint fieldId) {
  if (vmInitialized == Phase::LIVE) {
    jthread thread = cbHandler->getCurrentThread();
    cbHandler->nativePointerUpdated(env, klass, thread, origin, newTarget, fieldId);
  }
}

static void nativePointerUpdated2 (JNIEnv *env,
                                   jclass klass,
                                   jobject origin,
                                   jobject newTarget,
                                   jlong offset) {
  if (vmInitialized == Phase::LIVE) {
    jthread thread = cbHandler->getCurrentThread();
    cbHandler->nativePointerUpdated(env, klass, thread, origin, newTarget, offset);
  }
}

static void nativeReferentUpdated (JNIEnv *env,
                                   jclass klass,
                                   jobject obj,
                                   jobject referent) {
  if (vmInitialized == Phase::LIVE) {
    jthread thread = cbHandler->getCurrentThread();
    cbHandler->nativePointerUpdated(env, klass, thread, obj, referent, referentFieldNumber);
  }
}

static void nativeArrayStore (JNIEnv *jni,
                              jclass klass,
                              jobject array,
                              jint index,
                              jobject storee) {
  if (vmInitialized == Phase::LIVE) {
    jthread thread = cbHandler->getCurrentThread();
    cbHandler->nativeArrayStore(jni, klass, thread, array, index, storee);
  }
}

static void nativeUninitPutfield (JNIEnv *jni,
                                  jclass klass,
                                  jobject newTarget,
                                  jint objSlot,
                                  jint fieldId) {
  jthread thread = cbHandler->getCurrentThread();
  cbHandler->nativeUninitPutfield(jni, klass, thread, newTarget, objSlot, fieldId);
}

static void nativeNewObj (JNIEnv *jni,
                          jclass klass,
                          jobject o) {
  jthread thread = cbHandler->getCurrentThread();
  cbHandler->nativeNewObj(jni, klass, thread, o, false, 0);
}

static void nativeNewObj2 (JNIEnv *jni,
                           jclass klass,
                           jstring className,
                           jint slot,
                           jint site) {
  verbose_println("main::nativeNewObj2 called");
  jthread thread = cbHandler->getCurrentThread();
  cbHandler->nativeNewObj(jni, klass, thread, className, slot, site);
}

static void nativeNewArray (JNIEnv *jni,
                            jclass klass,
                            jobject o,
                            jint site) {
  jthread thread = cbHandler->getCurrentThread();
  cbHandler->nativeNewObj(jni, klass, thread, o, false, site);
}

static void cbVMAlloc (jvmtiEnv*,
                       JNIEnv* jni_env,
                       jthread thread,
                       jobject o,
                       jclass object_klass,
                       jlong) {
  cbHandler->nativeNewObj(jni_env, object_klass, thread, o, true, 0);
}

void nativeMethodEntry (JNIEnv *jni,
                        jclass klass,
                        jint methodId,
                        jobject receiver,
                        jboolean isStatic) {
  if (vmInitialized == Phase::LIVE) {
    jthread thread = cbHandler->getCurrentThread();
    if ((!isStatic) && jni->IsSameObject(receiver, NULL)) {
      jvmtiError err = cachedJvmti->GetLocalObject(thread, (jint)2, (jint)0, &receiver);
      check_jvmti_error(cachedJvmti, err, "main:nativeMethodEntry: could not fetch uninitialized receiver");
    }
    cbHandler->nativeMethodEntry(jni, klass, thread, methodId, receiver);
  }
}

void nativeMethodExit (JNIEnv *jni,
                       jclass klass,
                       jobject exception,
                       jint methodId,
                       jobject receiver,
                       jboolean isStatic) {
  if (vmInitialized == Phase::LIVE) {
    jthread thread = cbHandler->getCurrentThread();
    if ((!isStatic) && jni->IsSameObject(receiver, NULL)) {
      jvmtiError err = cachedJvmti->GetLocalObject(thread, (jint)2, (jint)0, &receiver);
      check_jvmti_error(cachedJvmti, err, "main:nativeMethodExit: could not fetch uninitialized receiver");
    }
    cbHandler->nativeMethodExit(jni, klass, thread, exception, methodId, receiver);
  }
}

void nativeExceptionThrow (JNIEnv *jni,
                           jclass klass,
                           jobject exception,
                           jint methodId,
                           jobject receiver,
                           jboolean isStatic) {
  if (vmInitialized >= Phase::LIVE) {
    jthread thread = cbHandler->getCurrentThread();
    if ((!isStatic) && jni->IsSameObject(receiver, NULL)) {
      jvmtiError err = cachedJvmti->GetLocalObject(thread, (jint)2, (jint)0, &receiver);
      check_jvmti_error(cachedJvmti, err, "main:nativeExceptionThrow: could not fetch uninitialized receiver");
    }
    cbHandler->nativeExceptionThrow(jni, klass, thread, exception, methodId, receiver);
  }
}

void nativeExceptionHandle (JNIEnv *jni,
                            jclass klass,
                            jobject exception,
                            jint methodId,
                            jobject receiver,
                            jboolean isStatic) {
  if (vmInitialized >= Phase::LIVE) {
    jthread thread = cbHandler->getCurrentThread();
    if ((!isStatic) && jni->IsSameObject(receiver, NULL)) {
      jvmtiError err = cachedJvmti->GetLocalObject(thread, (jint)2, (jint)0, &receiver);
      check_jvmti_error(cachedJvmti, err, "main:nativeExceptionHandle: could not fetch uninitialized receiver");
    }
    cbHandler->nativeExceptionHandle(jni, klass, thread, exception, methodId, receiver);
  }
}

void nativeStampThis (JNIEnv *jni,
                      jclass klass,
                      jstring className) {
  cbHandler->nativeStampThis(jni, klass, className);
}

void nativeVarEvent (JNIEnv *jni,
                     jclass klass,
                     jint numObjs,
                     jobject o0,
                     jobject o1,
                     jobject o2,
                     jobject o3,
                     jobject o4) {
  if (vmInitialized == Phase::LIVE) {
    cbHandler->nativeVarEvent(jni, klass, numObjs, o0, o1, o2, o3, o4);
  }
}

void nativeRecordStaticFieldBase (JNIEnv *jni,
                                  jclass klass,
                                  jobject base,
                                  jobject field) {
  if (vmInitialized == Phase::LIVE) {
    cbHandler->nativeRecordStaticFieldBase(jni, klass, base, field);
  }
}

void nativeRecordStaticFieldOffset (JNIEnv *jni,
                                    jclass klass,
                                    jlong offset,
                                    jobject field) {
  if (vmInitialized == Phase::LIVE) {
    cbHandler->nativeRecordStaticFieldOffset(jni, klass, offset, field);
  }
}

void nativeRecordObjectFieldOffset (JNIEnv *jni,
                                    jclass klass,
                                    jlong offset,
                                    jobject field) {
  if (vmInitialized == Phase::LIVE) {
    cbHandler->nativeRecordObjectFieldOffset(jni, klass, offset, field);
  }
}

void nativeRecordArrayBaseOffset (JNIEnv *jni,
                                  jclass klass,
                                  jint offset,
                                  jclass arrayClass) {
  if (vmInitialized == Phase::LIVE) {
    cbHandler->nativeRecordArrayBaseOffset(jni, klass, offset, arrayClass);
  }
}

void nativeRecordArrayIndexScale (JNIEnv *jni,
                                  jclass klass,
                                  jint scale,
                                  jclass arrayClass) {
  if (vmInitialized == Phase::LIVE) {
    cbHandler->nativeRecordArrayIndexScale(jni, klass, scale, arrayClass);
  }
}

jboolean nativeDoSystemArraycopy (JNIEnv *jni,
                                  jclass klass,
                                  jobject src,
                                  jint srcIdx,
                                  jobject dst,
                                  jint dstIdx,
                                  jint cnt) {
  if (vmInitialized == Phase::LIVE) {
    jthread thread = cbHandler->getCurrentThread();
    return cbHandler->nativeDoSystemArraycopy(jni, klass, thread, src, srcIdx, dst, dstIdx, cnt);
  } else {
    return JNI_FALSE;
  }
}

void nativeCounts( JNIEnv *jni,
                   jclass klass,
                   jint heapReads,
                   jint heapWrites,
                   jint heapRefReads,
                   jint heapRefWrites,
                   jint instCount )
{
    if (vmInitialized == Phase::LIVE) {
        cbHandler->nativeCounts( jni,
                                 klass,
                                 heapReads,
                                 heapWrites,
                                 heapRefReads,
                                 heapRefWrites,
                                 instCount );
    }
}

extern "C" {
  jboolean Java_ElephantTracks_getCountCalls (JNIEnv*,
                                              jclass) {
    return (countCalls ? JNI_TRUE : JNI_FALSE);
  }
}

#define NUM_NATIVES 21

static bool registered = false;

static void registerNatives (JNIEnv *jni,
                             jclass klass) {
  if (registered) {
    return;
  }

  if (etClass_g == NULL) {
    etClass_g = static_cast<jclass>(jni->NewGlobalRef(klass));
  }

  JNINativeMethod jni_pointerUpdated;
  JNINativeMethod jni_pointerUpdated2;
  JNINativeMethod jni_referentUpdated;
  JNINativeMethod jni_arrayStore;
  JNINativeMethod jni_uninitPutfield;
  JNINativeMethod jni_newObj;
  JNINativeMethod jni_newObj2;
  JNINativeMethod jni_newArray;
  JNINativeMethod jni_methodEntry;
  JNINativeMethod jni_methodExit;
  JNINativeMethod jni_exceptionThrow;
  JNINativeMethod jni_exceptionHandle;
  JNINativeMethod jni_stampThis;
  JNINativeMethod jni_varEvent;
  JNINativeMethod jni_recordStaticFieldBase;
  JNINativeMethod jni_recordStaticFieldOffset;
  JNINativeMethod jni_recordObjectFieldOffset;
  JNINativeMethod jni_recordArrayBaseOffset;
  JNINativeMethod jni_recordArrayIndexScale;
  JNINativeMethod jni_doSystemArraycopy;
  JNINativeMethod jni_counts;
  JNINativeMethod native_methods[NUM_NATIVES];

  jni_pointerUpdated.name = (char*)"_pointerUpdated";
  jni_pointerUpdated.signature = (char*)"(Ljava/lang/Object;Ljava/lang/Object;I)V";
  jni_pointerUpdated.fnPtr = (void*)nativePointerUpdated;
    
  jni_pointerUpdated2.name = (char*)"_pointerUpdated";
  jni_pointerUpdated2.signature = (char*)"(Ljava/lang/Object;Ljava/lang/Object;J)V";
  jni_pointerUpdated2.fnPtr = (void*)nativePointerUpdated2;
    
  jni_referentUpdated.name = (char*)"_referentUpdated";
  jni_referentUpdated.signature = (char*)"(Ljava/lang/Object;Ljava/lang/Object;)V";
  jni_referentUpdated.fnPtr = (void*)nativeReferentUpdated;
    
  jni_arrayStore.name = (char*)"_arrayStore";
  jni_arrayStore.signature = (char*)"(Ljava/lang/Object;ILjava/lang/Object;)V";
  jni_arrayStore.fnPtr = (void*)nativeArrayStore;

  jni_uninitPutfield.name = (char*)"_uninitPutfield";
  jni_uninitPutfield.signature = (char*)"(Ljava/lang/Object;II)V";
  jni_uninitPutfield.fnPtr = (void*)nativeUninitPutfield;
    
  jni_newObj.name = (char*)"_newobj";
  jni_newObj.signature = (char*)"(Ljava/lang/Object;)V";
  jni_newObj.fnPtr = (void*)nativeNewObj;
    
  jni_newObj2.name = (char*)"_newobj";
  jni_newObj2.signature = (char*)"(Ljava/lang/String;II)V";
  jni_newObj2.fnPtr = (void*)nativeNewObj2;
    
  jni_newArray.name = (char*)"_newarray";
  jni_newArray.signature = (char*)"(Ljava/lang/Object;I)V";
  jni_newArray.fnPtr = (void*)nativeNewArray;
    
  jni_methodEntry.name = (char*)"_methodEntry";
  jni_methodEntry.signature = (char*)"(ILjava/lang/Object;Z)V";
  jni_methodEntry.fnPtr = (void*)nativeMethodEntry;
    
  jni_methodExit.name = (char*)"_methodExit";
  jni_methodExit.signature = (char*)"(Ljava/lang/Object;ILjava/lang/Object;Z)V";
  jni_methodExit.fnPtr = (void*)nativeMethodExit;
   
  jni_exceptionThrow.name = (char*)"_exceptionThrow";
  jni_exceptionThrow.signature = (char*)"(Ljava/lang/Object;ILjava/lang/Object;Z)V";
  jni_exceptionThrow.fnPtr = (void*)nativeExceptionThrow;
   
  jni_exceptionHandle.name = (char*)"_exceptionHandle";
  jni_exceptionHandle.signature = (char*)"(Ljava/lang/Object;ILjava/lang/Object;Z)V";
  jni_exceptionHandle.fnPtr = (void*)nativeExceptionHandle;
   
  jni_stampThis.name = (char*)"_stampThis";
  jni_stampThis.signature = (char*)"(Ljava/lang/String;)V";
  jni_stampThis.fnPtr = (void*)nativeStampThis;

  jni_varEvent.name = (char*)"_varEvent";
  jni_varEvent.signature = (char*)
    "(ILjava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V";
  jni_varEvent.fnPtr = (void*)nativeVarEvent;
    
  jni_recordStaticFieldBase.name = (char*)"_recordStaticFieldBase";
  jni_recordStaticFieldBase.signature = (char*)"(Ljava/lang/Object;Ljava/lang/reflect/Field;)V";
  jni_recordStaticFieldBase.fnPtr = (void*)nativeRecordStaticFieldBase;

  jni_recordStaticFieldOffset.name = (char*)"_recordStaticFieldOffset";
  jni_recordStaticFieldOffset.signature = (char*)"(JLjava/lang/reflect/Field;)V";
  jni_recordStaticFieldOffset.fnPtr = (void*)nativeRecordStaticFieldOffset;

  jni_recordObjectFieldOffset.name = (char*)"_recordObjectFieldOffset";
  jni_recordObjectFieldOffset.signature = (char*)"(JLjava/lang/reflect/Field;)V";
  jni_recordObjectFieldOffset.fnPtr = (void*)nativeRecordObjectFieldOffset;

  jni_recordArrayBaseOffset.name = (char*)"_recordArrayBaseOffset";
  jni_recordArrayBaseOffset.signature = (char*)"(ILjava/lang/Class;)V";
  jni_recordArrayBaseOffset.fnPtr = (void*)nativeRecordArrayBaseOffset;

  jni_recordArrayIndexScale.name = (char*)"_recordArrayIndexScale";
  jni_recordArrayIndexScale.signature = (char*)"(ILjava/lang/Class;)V";
  jni_recordArrayIndexScale.fnPtr = (void*)nativeRecordArrayIndexScale;

  jni_doSystemArraycopy.name = (char*)"_doSystemArraycopy";
  jni_doSystemArraycopy.signature = (char*)"(Ljava/lang/Object;ILjava/lang/Object;II)Z";
  jni_doSystemArraycopy.fnPtr = (void*)nativeDoSystemArraycopy;

  jni_counts.name = (char*)"_counts";
  jni_counts.signature = (char*)"(IIIII)V";
  jni_counts.fnPtr = (void*)nativeCounts;

  native_methods[ 0] = jni_pointerUpdated;
  native_methods[ 1] = jni_pointerUpdated2;
  native_methods[ 2] = jni_referentUpdated;
  native_methods[ 3] = jni_arrayStore;
  native_methods[ 4] = jni_uninitPutfield;
  native_methods[ 5] = jni_newObj;
  native_methods[ 6] = jni_newObj2;
  native_methods[ 7] = jni_newArray;
  native_methods[ 8] = jni_methodEntry;
  native_methods[ 9] = jni_methodExit;
  native_methods[10] = jni_exceptionThrow;
  native_methods[11] = jni_exceptionHandle;
  native_methods[12] = jni_stampThis;;
  native_methods[13] = jni_varEvent;
  native_methods[14] = jni_recordStaticFieldBase;
  native_methods[15] = jni_recordStaticFieldOffset;
  native_methods[16] = jni_recordObjectFieldOffset;
  native_methods[17] = jni_recordArrayBaseOffset;
  native_methods[18] = jni_recordArrayIndexScale;
  native_methods[19] = jni_doSystemArraycopy;
  native_methods[20] = jni_counts;
    
  int rc = jni->RegisterNatives(etClass_g, native_methods, NUM_NATIVES);
  if (rc != 0) {
    fatal_error("ElephantTracksAgent: ERROR: JNI: Cannot register native methods for %s, rc: %d\n",
                ETclassName, rc);
  }

  registered = true;
}

static void cbVMStart( jvmtiEnv *,
                       JNIEnv *jni )
{
    // entering the JVMTI "Start" phase
    // all JNI calls are legal
    // *some* JVMTI calls are legal

    IF_TIMING (programTimer.start());

#ifdef LOG_PHASE_CHANGES
    cerr << "main: VMStart" << endl;
#endif

    jclass et = NULL;
    et = jni->FindClass(ETclassName);
    if (et == NULL) {
        fatal_error("ERROR: JNI: Cannot find ElephantTracks with FindClass.\n");
    }
    registerNatives(jni, et);

    if (engageCallbacks) {
        engagedField = jni->GetStaticFieldID(etClass_g, "engaged", "I");
        if (engagedField == NULL) {
            fatal_error("ERROR: JNI: Cannot get field from ElephantTracks\n");
        }
        jni->SetStaticIntField( etClass_g,
                                engagedField,
                                static_cast<std::underlying_type<Phase>::type>(Phase::START) );
    }
}

static void cbVMInit (jvmtiEnv *jvmti,
                      JNIEnv *env,
                      jthread thread) {
  // entering the JVMTI "Live" phase
  // all JNI calls are legal
  // all JVMTI calls are legal
#ifdef LOG_PHASE_CHANGES
  cerr << "main: VMInit" << endl;
#endif
  cbHandler->VMInit(jvmti, env, thread);
  vmInitialized = Phase::LIVE;
  if (engageCallbacks) {
      env->SetStaticIntField( etClass_g,
                              engagedField,
                              static_cast<std::underlying_type<Phase>::type>(Phase::LIVE) );
  } 
  jmethodID liveHook = env->GetStaticMethodID(etClass_g, "liveHook", "()V");
  if (env->ExceptionOccurred() == NULL) {
    env->CallStaticVoidMethod(etClass_g, liveHook);
  } else {
    env->ExceptionClear();
  }
}

// not currently used; for debugging support
static inline void cbNativeMethodBind (jvmtiEnv *jvmti,
                                JNIEnv*,
                                jthread,
                                jmethodID method,
                                void*,
                                void**) {
  if (vmInitialized >= Phase::LIVE) {
    char *name;
    char *signature;
    jvmti->GetMethodName(method, &name, &signature, NULL);
    jclass declClass;
    jvmti->GetMethodDeclaringClass(method, &declClass);
    char *classSig;
    jvmti->GetClassSignature(declClass, &classSig, NULL);
    cerr << "Native bind: " << classSig << " "<< name << signature << endl;
    jvmti->Deallocate((unsigned char *)name);
    jvmti->Deallocate((unsigned char *)signature);
    jvmti->Deallocate((unsigned char *)classSig);
  } else {
    cerr << "Native bind: early " << hex << "0x" << (long long)method << endl;
  }
}

// support routine for other modules
JNIEnv *getJNI() {
  // JNIEnv pointers are per-thread, so we do NOT cache this globally
  // Most functions are provided with this, but sometimes we need it "out of thin air"
  JNIEnv *jni;
  theVM->GetEnv((void **)&jni, JNI_VERSION_1_6);
  return jni;
}


jint Agent_OnLoad( JavaVM *vm,
                   char *options,
                   void * )
{
#ifdef LOG_PHASE_CHANGES
    cerr << "main: AgentOnload entered" << endl;
#endif
    verbose_println("main:: AgentOnload entered");
    theVM = vm;

    map<string,string> optionMap = getOptions(options);

    if (optionMap["verbose"] == "T" || optionMap["verbose"] == "t") {
        verbose = true;
    }

    if (optionMap["showFields"] == "T" || optionMap["showFields"] == "t") {
        showFields = true;
    }

    if (optionMap["zip"] == "T") {

        ogz = new ogzstream();
        ogz->open(((optionMap["traceFile"]) + ".gz").c_str());

        if (ogz->fail()) {
            cerr << "Error opening output file " << optionMap["traceFile"] << " for writing." << endl;
            exit(1);
        }

        outputFile = ogz;

    } else {

        ofs = new ofstream();
        ofs->open((optionMap["traceFile"]).c_str());
        if (ofs->fail()) {
            cerr << "Error opening output file " << optionMap["traceFile"] << " for writing." << endl;
            exit(1);
        }

        outputFile = ofs;

    }

    unsigned int maxRecordsToBuffer = atoi(optionMap["bufferSize"].c_str());

    string which = optionMap["whichRecs"];
    int whichDefault = (which[0] == '-');
    for (int i = 0; i < 256; ++i) {
        wantRecord[i] = whichDefault;
    }
    int whichSet = 1;
    for (unsigned int i = 0; i < which.length(); ++i) {
        if (which[i] == '-') {
            whichSet ^= 1;
        } else {
            wantRecord[(unsigned int)which[i]] = whichSet;
        }
    }

    if (optionMap.count("engageCallbacks") && optionMap["engageCallbacks"].compare("false") == 0) {
        engageCallbacks = false;
    }

    if (optionMap.count("countCalls") && optionMap["countCalls"].compare("true") == 0) {
        countCalls = true;
    }

    if (optionMap.count("deathChunks") && optionMap["deathChunks"].compare("false") == 0) {
        includeDeathChunks = false;
    }

    jvmtiEnv *jvmti;
    theVM->GetEnv((void **)&jvmti, JVMTI_VERSION_1_1);
    cachedJvmti = jvmti;

    FlatTraceOutputter *outputter =
        new FlatTraceOutputter(jvmti, outputFile, maxRecordsToBuffer, &traceBufferFull);

    cbHandler = new ETCallBackHandler(jvmti, vm, outputter);

    instrumenter =
        new ExternalClassInstrumenter(jvmti,
                optionMap["javaPath"],
                optionMap["classPath"],
                optionMap["classReWriter"],
                optionMap["namesFile"],
                verbose,
                optionMap["experimental"],
                optionMap,
                cbHandler);

    jvmtiError error = jvmti->CreateRawMonitor("agent data", &theBigLock);
    check_jvmti_error(jvmti, error, "Cannot create raw monitor");

    setCapabilities(jvmti);
    setEvents(jvmti);	
    setCallbacks(jvmti);

    error = jvmti->SetNativeMethodPrefix("$$ET$$");
    check_jvmti_error(jvmti, error, "Could not set native method prefix");

    // TODO error = jvmti->GetSystemProperty("ETJAR", &etjar_g);
    // TODO check_jvmti_error(jvmti, error, "Cannot get ETJAR property value");
    // TODO if ( etjar_g == NULL || etjar_g[0] == 0 ) {
    // TODO     fatal_error("ERROR: ETJAR property not found\n");
    // TODO }
    // cerr << "ETJAR:" << etjar_g << endl;
    // error = jvmti->AddToSystemClassLoaderSearch( (const char*)etjar_g);
    // error = jvmti->AddToBootstrapClassLoaderSearch( (const char*)etjar_g);
    // check_jvmti_error(jvmti, error, "Cannot add.");

#ifdef LOG_PHASE_CHANGES
    // cerr << "    Class ID for " << ETclassName << " is loaded." << endl;
#endif
    return JNI_OK;	
}

static void cbVMDeath (jvmtiEnv *jvmti,
                       JNIEnv *env) {
  // entering the JVMTI "Dead" phase
  // no more events will be sent
  // at this point, JNI and JVMTI calls are legal,
  //   but after it only a subset of each are ok
        
#ifdef LOG_PHASE_CHANGES
  cerr << "main: VMDeath" << endl;
#endif
  vmInitialized = Phase::DEAD;
  if (engageCallbacks) {
    env->SetStaticIntField( etClass_g,
                            engagedField,
                            static_cast<std::underlying_type<Phase>::type>(Phase::DEAD) );
  }
  jmethodID deathHook = env->GetStaticMethodID(etClass_g, "deathHook", "()V");
  if (env->ExceptionOccurred() == NULL) {
    env->CallStaticVoidMethod(etClass_g, deathHook);
  } else {
    env->ExceptionClear();
  }
		 
  /* Clear out all callbacks. */
  jvmtiEventCallbacks callbacks;
  (void)memset(&callbacks, 0, sizeof(callbacks));
  jvmtiError error = jvmti->SetEventCallbacks(&callbacks, (jint)sizeof(callbacks));
  check_jvmti_error(jvmti, error, "Cannot set jvmti callbacks");

  cbHandler->VMDeath(jvmti);

  if(ogz != NULL){
	ogz->close();
  }
  if(ofs != NULL){
	ofs->close();
  }
  delete instrumenter;

  IF_TIMING ( \
    programTimer.stop(); \
    cout << "Total elephantTracks time " << programTimer.getElapsedTime() << endl; \
  );
  cerr << "Elephant Tracks Completed." << endl;
}   


void setCallbacks (jvmtiEnv *jvmti) {
  jvmtiEventCallbacks callbacks;

  (void)memset(&callbacks, 0, sizeof(callbacks));
  callbacks.ClassFileLoadHook = &(instrumentClass);
  callbacks.VMStart = &(cbVMStart);
  callbacks.GarbageCollectionFinish = &(cbGCEnd);
  callbacks.VMInit = &(cbVMInit);
  callbacks.VMDeath = &(cbVMDeath);
  if (engageCallbacks) {
    callbacks.VMObjectAlloc = &(cbVMAlloc);
  }
  //callbacks.NativeMethodBind = &(cbNativeMethodBind);
	
  jvmtiError error = jvmti->SetEventCallbacks(&callbacks, (jint)sizeof(callbacks));
  check_jvmti_error(jvmti, error, "Cannot set jvmti callbacks");	
}

void setCapabilities (jvmtiEnv *env) {
  jvmtiCapabilities capa;
	
  (void)memset(&capa, 0, sizeof(capa));
     
  capa.can_generate_all_class_hook_events = 1;
  capa.can_generate_frame_pop_events = 1;

  capa.can_tag_objects = 1;
  capa.can_generate_vm_object_alloc_events = 1;
  capa.can_generate_garbage_collection_events = 1;

  // Need local var access to get at uninit new objects, etc.
  capa.can_access_local_variables = 1;

  capa.can_generate_garbage_collection_events = 1;
  capa.can_set_native_method_prefix = 1;
  //capa.can_generate_native_method_bind_events = 1;

  jvmtiError error = env->AddCapabilities(&capa);
  check_jvmti_error(env, error, "Unable to get necessary JVMTI capabilities.");
}

void setEvents (jvmtiEnv *jvmti) {
  jvmtiError error =
    jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                    JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, (jthread)NULL);

  check_jvmti_error(jvmti, error,
                    "Cannot set event notification JVMTI_EVENT_CLASS_FILE_LOAD_HOOK");
         
  error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_START, (jthread)NULL);
  check_jvmti_error(jvmti, error, "Cannot set event notification JVMTI_EVENT_VM_START");
	 
  error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_GARBAGE_COLLECTION_START,
                                          (jthread)NULL);
  check_jvmti_error(jvmti, error,
                    "Cannot set event notification JVMTI_EVENT_GARBAGE_COLLECTION_START");
	 
  error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH,
                                          (jthread)NULL);
  check_jvmti_error(jvmti, error,
                    "Cannot set event notification JVMTI_EVENT_GARBAGE_COLLECTION_FINISH");
	 
  error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, (jthread)NULL);
  check_jvmti_error(jvmti, error, "Cannot set event notification JVMTI_EVENT_VM_INIT"); 
	 
  error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, (jthread)NULL);
  check_jvmti_error(jvmti, error, "Cannot set event notification JVMTI_EVENT_VM_DEATH");
	 
  error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_OBJECT_ALLOC, (jthread)NULL);
  check_jvmti_error(jvmti, error, "Cannot set event notification JVMTI_EVENT_VM_OBJECT_ALLOC"); 

  //error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_NATIVE_METHOD_BIND, (jthread)NULL);
  //check_jvmti_error(jvmti, error, "Cannot set event notification JVMTI_EVENT_NATIVE_METHOD_BIND"); 
}

bool wantToInstrument (const char* name,
                       jclass,
                       const unsigned char* klass_data,
                       jint klass_data_len) {
  const char *classname;

  if (name == NULL) {/*Then we have to go fishing for the name in the class itself */	          
    classname = java_crw_demo_classname(klass_data, klass_data_len, NULL);

    if (classname == NULL) {
      fatal_error("ERROR: No classname inside classfile\n");
    }

  } else {
    classname = name;
  }

  if (string(classname) == "ElephantTracks") {
    return true; // trying special handling on the other end
  } else {
    return true;
  }
		
}

pair<string,string> parseOption (string option) {
  string opt = option;
  string val = "";
  
  size_t pos = option.find("=");
  if (pos != string::npos) {
    opt = option.substr(0, pos);
    val = option.substr(pos+1);
  }

  return pair<string,string>(opt, val);
}

map<string,string> parseOptionList (string options) {
  map<string,string> opt2Val;
  unsigned int beginning = 0;
  char sep = ':';

  // support :=c at start of options, to change the separator;
  // useful if an option contains a ':'
  if (options.length() > 2 && options[0] == ':' && options[1] == '=') {
    sep = options[2];
    beginning = 3;
  }

  size_t pos;
  while ((pos = options.find(sep, beginning)) != string::npos) {
    pair<string, string> pr = parseOption(options.substr(beginning, pos-beginning));
    opt2Val[pr.first] = pr.second;
    beginning = pos+1;
  }
  if (beginning < options.length()) {
    pair<string, string> pr = parseOption(options.substr(beginning));
    opt2Val[pr.first] = pr.second;
  }

  return opt2Val;
}

map<string,string> getOptions (char* optString) {
  map<string,string> optMap = parseOptionList(string(optString));

  if (optMap.find("optionsFile") != optMap.end()) {
    string optionsFile = optMap["optionsFile"];
    FILE *f = fopen(optionsFile.c_str(), "r");
    if (f == NULL) {
      cerr << "Could not open options file: " << optionsFile << endl;
    } else {
      char *buf = NULL;
      size_t size;
      int count;
      while ((count = getline(&buf, &size, f)) >= 0) {
        if (buf[count-1] == '\n') {
          buf[--count] = 0;
        }
        pair<string,string> pr = parseOption(string(buf));
        optMap[pr.first] = pr.second;
      }
      if (buf != NULL) {
        free(buf);
      }
      fclose(f);
    }
  }

  //Set some defaults if they were not set
  if (optMap.find("javaPath") == optMap.end()) {
    optMap["javaPath"] = "/usr/bin/java";
  }
  if (optMap.find("classPath") == optMap.end()) {
    optMap["classPath"] = "/h/sguyer/Projects/KeyObjects/asm-3.1.jar";
  }
  if (optMap.find("classReWriter") == optMap.end()) {
    optMap["classReWriter"] = "classReWriter.ClassReWriter";
  }
  if (optMap.find("outputFile") == optMap.end()) {
    optMap["outputFile"] = "elephantTracksOut";
  }
  if (optMap.find("traceFile") == optMap.end()) {
    optMap["traceFile"] = optMap["outputFile"] + ".trace";
  }
  if (optMap.find("namesFile") == optMap.end()) {
    optMap["namesFile"] = optMap["outputFile"] + ".names";
  }
  if (optMap.find("whichRecs") == optMap.end()) {
    optMap["whichRecs"] = "ADMEUNR";
  }
  if (optMap.find("bufferSize") == optMap.end()) {
    optMap["bufferSize"] =  "10000000";
  }
  if (optMap.find("verbose") == optMap.end()) {
    optMap["verbose"] = "F";
  }
  if (optMap.find("experimental") == optMap.end()) {
    optMap["experimental"] = "";
  } else {
    optMap["experimental"] = "-x" + optMap["experimental"];
  }
  if (optMap.find("showFields") == optMap.end()) {
    optMap["showFields"] = "F";
  }

  /*
  int i = 0;
  for (map<string,string>::iterator it = optMap.begin(); it != optMap.end(); it++) {
    cerr << "main arg " << dec << ++i << ": " << it->first << " = " << it->second << endl;
  }
  */

  return optMap;
}

// Local Variables:
// mode:C++
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
