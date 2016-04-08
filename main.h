#ifndef MEARTHMAIN_H_
#define MEARTHMAIN_H_
#include <jni.h>
#include <jvmti.h>

#include "timer.h"

#define DO_TIMING false
#define IF_TIMING(x) if (DO_TIMING) {x;}

using namespace std;
#include <string>

class ETCallBackHandler;

// our phase values (not the same as the JVMTI_PHASE constants!)
typedef enum phase { PHASE_ONLOAD, PHASE_START, PHASE_LIVE, PHASE_DEAD } Phase;

// kinds of allocation records
typedef enum allocKind {
  AllocArray       = 'A',
  AllocInitialHeap = 'I',
  AllocNew         = 'N',
  AllocPreexisting = 'P',
  AllocVM          = 'V' } AllocKind;

// kinds of exception records
typedef enum excKind {
  ExceptHandle = 'H',
  ExceptThrow  = 'T' } ExceptKind;

extern bool engageCallbacks;
extern bool countCalls;

// index by record character (A, D, M, E, U, N, etc.)
// to see if user wants those records output
extern unsigned char wantRecord[256];

extern bool showFields;

extern bool includeDeathChunks;

extern int ReferenceClassId;     // our assigned id for java/lang/refer/Reference
extern int referentFieldNumber;  // our assigned number for its referent field (NOT a JNI jfieldID!)

void verbose_println (string s);

void sendInfo (jvmtiEnv *jvmti, JNIEnv *jni, string s);

extern bool isWeakRefClass (jvmtiEnv *jvmti_env, JNIEnv *jni_env, jclass klass);

extern void enter_critical_section (jvmtiEnv *theJVMTI);
extern void exit_critical_section  (jvmtiEnv *theJVMTI);

extern ETCallBackHandler* cbHandler;

extern JavaVM *theVM;

extern jvmtiEnv *cachedJvmti;

extern JNIEnv *getJNI ();

#endif

// Local Variables:
// mode:C++
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
