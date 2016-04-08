#include "ClassInstrumenter.h"

ClassInstrumenter::ClassInstrumenter () { }

ClassInstrumenter::~ClassInstrumenter () { }

void ClassInstrumenter::instrumentClass (jvmtiEnv*,
                              					 JNIEnv*,
                                         jclass,
                                         jobject,
                                         const char*,
                                         jobject,
                                         jint,
                                         const unsigned char* ,
                                         jint*,
                                         unsigned char**) { }
void ClassInstrumenter::sendInfo (jvmtiEnv*, JNIEnv*, SendKind, const char *) { }
