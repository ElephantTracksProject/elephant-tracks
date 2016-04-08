#ifndef LIFETIME_CORRELATION_CALLBACK_HANDLER_H_
#define LIFETIME_CORRELATION_CALLBACK_HANDLER_H_
#include <string>
#include "callback_handler.h"
#include <map>
#include <vector>
#include <set>

using namespace std;

typedef unsigned long ObjectID;
typedef unsigned long MethodID;
typedef unsigned long SiteID;
typedef unsigned long FieldID;
typedef vector<unsigned long> Context;

// Each stack 'frame' is a set of objects allocated in that frame
typedef vector< set <ObjectID> > Stack;

class EscapeAnalysisCallbackHandler : public CallbackHandler {
 public:
  EscapeAnalysisCallbackHandler();

  virtual void alloc(char kind,
		     ObjectID object_id,
		     unsigned long size,
		     string type,
		     SiteID site,
		     unsigned long length,
		     ObjectID thread_id);

   virtual void death(ObjectID object_id, ObjectID thread_id);

  
   virtual void pointerUpdate(ObjectID old_target,
		              ObjectID origin,
                              ObjectID new_target,
		              FieldID field_id,
		              ObjectID thread_id);

    virtual void methodEntry(MethodID method_id,
			     ObjectID object_id,
			     ObjectID thread_id);

    virtual void methodExit(MethodID method_id,
			    ObjectID receiver_id,
			    ObjectID thread_id);


    virtual void exceptionThrow(MethodID method_id,
				ObjectID receiver_id,
				ObjectID object_id,
				ObjectID thread_id);

    virtual void exceptionHandle(MethodID method_id,
				 ObjectID receiver_id,
				 ObjectID object_id,
				 ObjectID thread_id);

    virtual void exceptionalExit(MethodID method_id,
                                 ObjectID receiver_id,
                                 ObjectID exception_id,
                                 ObjectID thread_id);
   

    unsigned long getObjectsAllocated() { return objectsAllocated_; };

    unsigned long getEscapees() { return escapees_; };
 private:
  map<ObjectID, Stack> stackMap_;
  set<ObjectID> deadSet_;
  unsigned long escapees_;
  unsigned long objectsAllocated_;
  // ObjectID currentStack_;
  // map<ObjectID,Context> id2allocContext_;
  // map<ObjectID, pair< unsigned long, unsigned long> > id2allocTime_;
  // unsigned long methodTime_;
  // unsigned long byteTime_;
};
#endif // LIFETIME_CORRELATION_CALLBACK_HANDLER_H_

