#include <iostream>
#include <string>
#include "lifetime_correlation_callback_handler.h"



LifetimeCorrelationCallbackHandler::LifetimeCorrelationCallbackHandler() :
        methodTime_(0),
	byteTime_(0)
{
}
    

void LifetimeCorrelationCallbackHandler::alloc(char kind,
	   				       ObjectID object_id,
	   				       unsigned long size,
	   				       string type,
	   				       SiteID site,
                                               unsigned long length,
	   				       ObjectID thread_id) {

  Context allocation_context;
  Stack* current_stack = &(stackMap_[currentStack_]);

  int stack_height = current_stack->size();

  // Store two levels of context (if there are two),
  // and the site id.
  if (stack_height >= 2) {
    allocation_context.push_back((*current_stack)[stack_height-2]);
  }
  
  if (stack_height >= 1) {
    allocation_context.push_back((*current_stack)[stack_height-1]);  
  }

  allocation_context.push_back(site);

  id2allocContext_[object_id] = allocation_context;
  id2allocTime_[object_id] = pair<unsigned long, unsigned long>(byteTime_,methodTime_);

  byteTime_ += size;
}

static void outputContext(Context* c) {
  if (c->size() == 0) {
   cout << ",";
  }

  for (int i = 0; i < c->size(); i++ ) {
    const char* seperator = (i == c->size()-1) ? "":",";
    cout << (*c)[i]  << seperator;
  }
}

void LifetimeCorrelationCallbackHandler::death(unsigned long object_id, unsigned long thread_id) { 
  Context death_context;
  Stack* current_stack = &(stackMap_[thread_id]);

  int length = current_stack->size();

  if (length >= 2) {
    death_context.push_back((*current_stack)[length-2]);
  }
  
  if (length >= 1) {
   death_context.push_back((*current_stack)[length-1]);  
  }

  if (id2allocContext_.count(object_id) > 0) { 
    cout << hex << object_id << " ";
    Context* allocation_context = &(id2allocContext_[object_id]);

    outputContext(allocation_context);

    cout << " ";

    outputContext(&death_context);

    cout << " ";

    cout << id2allocTime_[object_id].first << " " << id2allocTime_[object_id].second;

    cout << " " << byteTime_ << " " << methodTime_;

    cout << endl; 

   


    id2allocContext_.erase(object_id);
    id2allocTime_.erase(object_id);	
  }

}

  
void LifetimeCorrelationCallbackHandler::pointerUpdate(unsigned long old_target,
	      	   unsigned long origin,
	           unsigned long new_target,
	           unsigned long field_id,
	           unsigned long thread_id) {
}

void LifetimeCorrelationCallbackHandler::methodEntry(unsigned long method_id,
	         unsigned long object_id,
	         unsigned long thread_id) {
  methodTime_++;
  currentStack_ = thread_id;
  stackMap_[thread_id].push_back(method_id);
}

void LifetimeCorrelationCallbackHandler::methodExit(unsigned long method_id,
        	unsigned long receiver_id,
		unsigned long thread_id) {
  methodTime_++;
  currentStack_ = thread_id;
  if (stackMap_.count(thread_id) > 0 && stackMap_[thread_id].size() > 0) {
    stackMap_[thread_id].pop_back();
  }
}


void LifetimeCorrelationCallbackHandler::exceptionThrow(MethodID method_id,
		    					ObjectID receiver_id,
							ObjectID exception_id,
		    					unsigned long thread_id) {
}

void LifetimeCorrelationCallbackHandler::exceptionHandle(MethodID method_id,
		     					 ObjectID receiver_id,
                     					 ObjectID exception_id,
		    					 ObjectID thread_id) {

}

void LifetimeCorrelationCallbackHandler::exceptionalExit(MethodID method_id,
                     ObjectID receiver_id,
                     ObjectID exception_id,
                     ObjectID thread_id) {
  methodExit(method_id, receiver_id, thread_id);
}
