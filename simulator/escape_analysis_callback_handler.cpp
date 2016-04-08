#include <iostream>
#include <string>
#include "escape_analysis_callback_handler.h"
#include <set>

using namespace std;

EscapeAnalysisCallbackHandler::EscapeAnalysisCallbackHandler():
  escapees_(0),
  objectsAllocated_(0) {
}

void EscapeAnalysisCallbackHandler::alloc(char kind,
	   				  ObjectID object_id,
	   				   unsigned long size,
	   				   string type,
	   				   unsigned long site,
           				   unsigned long length,
	   				   unsigned long thread_id) {
 if (stackMap_[thread_id].size() == 0) {
   stackMap_[thread_id].push_back(set<ObjectID>());
 }

 stackMap_[thread_id].back().insert(object_id);

 objectsAllocated_++;
}


void EscapeAnalysisCallbackHandler::death(unsigned long object_id, ObjectID thread_id) { 
  deadSet_.insert(object_id);
}

  
void EscapeAnalysisCallbackHandler::pointerUpdate(unsigned long old_target,
	      	   unsigned long origin,
	           unsigned long new_target,
	           unsigned long field_id,
	           unsigned long thread_id) {
}

void EscapeAnalysisCallbackHandler::methodEntry(unsigned long method_id,
	         unsigned long object_id,
	         unsigned long thread_id) {
  stackMap_[thread_id].push_back(set<ObjectID>());
}

void EscapeAnalysisCallbackHandler::methodExit(unsigned long method_id,
        	unsigned long receiver_id,
		unsigned long thread_id) {
  if (stackMap_[thread_id].size() > 0) {
    for (set<ObjectID>::iterator it = stackMap_[thread_id].back().begin();
      			         it != stackMap_[thread_id].back().end();
				 it++) {
      if (deadSet_.count(*it) == 0) {
	//this object was allocated here, but is still alive
        escapees_++;
       }
       else {
	//We won't ever see objectIDs twice, so we can
	deadSet_.erase(*it); 
       }	
     }
	   

     stackMap_[thread_id].pop_back(); 
  }
}


void EscapeAnalysisCallbackHandler::exceptionalExit(MethodID method_id,
                                 		    ObjectID receiver_id,
                                                    ObjectID exception_id,
                                                    ObjectID thread_id) {
  methodExit(method_id, receiver_id, thread_id);
}
   



void EscapeAnalysisCallbackHandler::exceptionThrow(MethodID method_id,
		    				   ObjectID object_id,
						   ObjectID exception_id,
		    				   ObjectID thread_id) {
}

void EscapeAnalysisCallbackHandler::exceptionHandle(unsigned long method_id,
		     				    ObjectID receiver_id,
		     				    ObjectID object_id,
		     				    ObjectID thread_id) {
}
