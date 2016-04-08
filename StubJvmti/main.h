#ifndef MEARTHMAIN_H_
#define MEARTHMAIN_H_
#include <jni.h>
#include <jvmti.h>
#include <string>
#include <iostream>
using namespace std;


class ETCallBackHandler;

// index by record character (A, D, M, E, U, N)
// to see if user wants those records output
extern unsigned char wantRecord[256];

extern bool showFields = true;

extern bool includeDeathChunks= false;

extern int referentFieldNumber;  // our assigned number (NOT a JNI jfieldID!)

void verbose_println(string s) {cerr << s; };

extern bool isWeakRefClass (jvmtiEnv *jvmti_env, JNIEnv *jni_env, jclass klass) { return false; };

extern void enter_critical_section (jvmtiEnv *theJVMTI) { };
extern void exit_critical_section  (jvmtiEnv *theJVMTI) { };

extern ETCallBackHandler* cbHandler;

extern JavaVM *theVM;

extern jvmtiEnv *cachedJvmti;

extern JNIEnv *getJNI();

extern jniNativeInterface* getUninstrumentedJNITable();

extern void setUninstrumentedJNITable(jniNativeInterface* jniFunctionTable);

static jniNativeInterface* uninstrumentedNativeInterface = NULL;

#endif

// Local Variables:
// mode:C++
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
