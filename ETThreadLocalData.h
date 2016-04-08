#ifndef ETTHREADLOCALDATA_H_
#define ETTHREADLOCALDATA_H_
#include "jni.h"
#include "main.h"
#include "ETCallBackHandler.h"

using namespace std;

struct updateInfo {
  struct updateInfo *next;
  jlong targetTag;
  int fieldId;
};

struct allocInfo {
  struct allocInfo *next;
  struct updateInfo *updates;
  int callDepth;
  int allocSite;
  jlong objectTag;
  unsigned long allocTime;
};

#ifdef DEBUG_CALL_DEPTH
struct callEntry {
  struct callEntry *next;
  int depth;
  int methodId;
};
#endif /* DEBUG_CALL_DEPTH */

class ETThreadLocalData {

public:

  ETThreadLocalData (int threadTag, jvmtiEnv *theJVMTI);

  virtual int getCallDepth ();

  virtual void incrementCallDepth (int incr);

  virtual ~ETThreadLocalData ();

  virtual void noteEntry (int depth, int methodId);

  virtual void noteExit  (int depth, int methodId);

  virtual void newObject (int allocSite, jlong newTag, unsigned long timestamp);

  virtual void uninitPutfield (int allocSite, int fieldId, jlong newValTag, ETCallBackHandler *cb);

  virtual void initOf (int allocSite, jboolean objectInit, ETCallBackHandler *cb);

  virtual jlong newObject (ETCallBackHandler *cb);

private:

  int threadTag;

  int callDepth;

  struct allocInfo *allocInfos;

#ifdef DEBUG_CALL_DEPTH
  struct callEntry *callEntries;
#endif /* DEBUG_CALL_DEPTH */

  jvmtiEnv *theJVMTI;
};

#endif /*ETTHREADLOCALDATA_H_*/

// Local Variables:
// mode:C++
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
