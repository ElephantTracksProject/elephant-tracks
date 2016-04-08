#include "TraceOutputter.h"
#include <string>

using namespace std;

TraceOutputter::TraceOutputter () { }

TraceOutputter::~TraceOutputter () { }

void TraceOutputter::doDeath (jlong tag, jlong deathTime) { }

void TraceOutputter::doAllocation (jlong time, jlong objectTag, jlong size, int length,
				   string type, jlong thread, char letter, jint site) { }

void TraceOutputter::doNewObject (jlong time, jlong objectTag, jlong classID, jlong thread) { }

void TraceOutputter::doMethodEntry (jlong time, int methodId,
				    jlong receiver, jlong thread) { }

void TraceOutputter::doMethodExit (jlong time, int methodId,
				   jlong receiver, jlong exception, jlong thread) { }

void TraceOutputter::doException (jlong time, int methodId,
				  jlong receiver, jlong exception, jlong thread,
				  char letter) { }

void TraceOutputter::doPointerUpdate (jlong time, jlong oldTarget, jlong origin, jlong newTarget,
				      int fieldId, jlong thread) { }

void TraceOutputter::doSyntheticUpdate (jlong time, jlong oldTarget, jlong origin, jlong newTarget,
					int fieldId) { }

void TraceOutputter::doVarEvent (jlong time, jlong obj, jlong thread) { }

void TraceOutputter::doCounts (jlong countsTime, int heapReads, int heapWrites,
			       int heapRefReads, int heapRefWrites, int instCount) { }

void TraceOutputter::doGCStart () { }

void TraceOutputter::doGCEnd () { }

void TraceOutputter::flush () { }

void TraceOutputter::doRoot (jlong time, jlong obj) { }
