#ifndef TRACEOUTPUTER_H_
#define TRACEOUTPUTER_H_
#include <jni.h>
#include <jvmti.h>
#include <string>

using namespace std;

class TraceOutputter
{
	
public:
	TraceOutputter ();
	virtual ~TraceOutputter ();
	
	virtual void doDeath (jlong tag, jlong deathTime);
	virtual void doAllocation (jlong time, jlong objectTag, jlong size, int length,
				   string type, jlong thread, char letter, jint site);
	virtual void doNewObject (jlong time, jlong objectTag, jlong classID, jlong thread);
	virtual void doPointerUpdate (jlong time,
				      jlong oldTarget, jlong origin, jlong newTarget, int fieldId, jlong thread);
	virtual void doSyntheticUpdate (jlong time,
					jlong oldTarget, jlong origin, jlong newTarget, int fieldId);
	virtual void doMethodEntry (jlong time, int methodId,
				    jlong receiver, jlong thread);
	virtual void doMethodExit (jlong time, int methodId,
				   jlong receiver, jlong exception, jlong thread);
	virtual void doException (jlong time, int methodId,
				  jlong receiver, jlong exception, jlong thread,
				  char letter);
	virtual void doVarEvent (jlong time, jlong obj, jlong thread);
	virtual void doCounts (jlong countsTime, int heapReads, int heapWrites,
			       int heapRefReads, int heapRefWrites, int instCount);
	virtual void doGCStart ();
        virtual void doGCEnd ();
        virtual void flush ();
	virtual void doRoot (jlong time, jlong obj);
};

#endif /*TRACEOUTPUTER_H_*/
