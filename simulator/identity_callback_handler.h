#ifndef IDENTITY_CALLBACK_HANDLER_H_
#define IDENTITY_CALLBACK_HANDLER_H_
#include <string>
#include "callback_handler.h"

using namespace std;

class IdentityCallbackHandler : public CallbackHandler {
 public:
  virtual void alloc(char kind,
		     unsigned long object_id,
		     unsigned long size,
		     string type,
		     unsigned long site,
                     unsigned long length,
		     unsigned long thread_id);

   virtual void death(unsigned long object_id, unsigned long thread_id);

  
   virtual void pointerUpdate(unsigned long old_target,
		       	      unsigned long origin,
                              unsigned long new_target,
		      	      unsigned long field_id,
		              unsigned long thread_id);

    virtual void methodEntry(unsigned long method_id,
			     unsigned long object_id,
			     unsigned long thread_id);

    virtual void methodExit(unsigned long method_id,
			    unsigned long receiver_id,
			    unsigned long thread_id);


    virtual void exceptionThrow(unsigned long method_id,
                                unsigned long receiver_id,
				unsigned long exception_id,
				unsigned long thread_id);

    virtual void exceptionHandle(unsigned long method_id,
				 unsigned long receiver_id,
				 unsigned long exception_id,
                                 unsigned long thread_id); 

    virtual void exceptionalExit(unsigned long method_id,
                                 unsigned long receiver_id,
                                 unsigned long exception_id,
                                 unsigned long thread_id);
};
#endif // IDENTITY_CALLBACK_HANDLER_H_

