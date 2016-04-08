#ifndef LIFETIME_CORRELATION_CALLBACK_HANDLER_H_
#define LIFETIME_CORRELATION_CALLBACK_HANDLER_H_
#include <string>
#include "callback_handler.h"
#include <map>
#include <vector>

using namespace std;

typedef unsigned long ObjectID;
typedef unsigned long MethodID;
typedef unsigned long SiteID;
typedef unsigned long FieldID;
typedef vector<unsigned long> Context;
typedef vector<unsigned long> Stack;

class LifetimeCorrelationCallbackHandler : public CallbackHandler {
 public:
  LifetimeCorrelationCallbackHandler();

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
			     ObjectID receiver_id,
			     ObjectID thread_id);

    virtual void methodExit(MethodID method_id,
			    ObjectID receiver_id,
			    ObjectID thread_id);


    virtual void exceptionThrow(MethodID method_id,
				ObjectID receiver_id,
                                ObjectID exception_id,
				ObjectID thread_id);

    virtual void exceptionHandle(MethodID method_id,
				 ObjectID receiver_id,
                                 ObjectID exception_id,
				 ObjectID object_id);

    void exceptionalExit(unsigned long method_id,
                         unsigned long receiver_id,
                         unsigned long exception_id,
                         unsigned long thread_id);

 private:
  map<ObjectID, Stack> stackMap_;
  ObjectID currentStack_;
  map<ObjectID,Context> id2allocContext_;
  map<ObjectID, pair< unsigned long, unsigned long> > id2allocTime_;
  unsigned long methodTime_;
  unsigned long byteTime_;


};
#endif // LIFETIME_CORRELATION_CALLBACK_HANDLER_H_

