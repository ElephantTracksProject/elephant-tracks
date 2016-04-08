#include <iostream>
#include <string>
#include "identity_callback_handler.h"



void IdentityCallbackHandler::alloc(char kind,
	   unsigned long object_id,
	   unsigned long size,
	   string type,
	   unsigned long site,
           unsigned long length,
	   unsigned long thread_id) {

  cout << hex << kind << " " << object_id << " "  << size << " " << type << 
       " " << site << " " << length << " " <<  thread_id << endl;
}

void IdentityCallbackHandler::death(unsigned long object_id, unsigned long thread_id) { 
  cout << hex << "D " << object_id << " " << thread_id << endl;
}

  
void IdentityCallbackHandler::pointerUpdate(unsigned long old_target,
	      	   unsigned long origin,
	           unsigned long new_target,
	           unsigned long field_id,
	           unsigned long thread_id) {
  cout << hex << "U " << old_target << " " << origin << " "  << new_target << " " << field_id << " " << thread_id << endl;
}

void IdentityCallbackHandler::methodEntry(unsigned long method_id,
	         unsigned long object_id,
	         unsigned long thread_id) {

  cout << hex << "M " << method_id << " " << object_id << " " << thread_id << endl;
}

void IdentityCallbackHandler::methodExit(unsigned long method_id,
        	unsigned long receiver_id,
		unsigned long thread_id) {

  cout << hex << "E " << method_id << " " << receiver_id << " " << thread_id << endl;
}


void IdentityCallbackHandler::exceptionThrow(unsigned long method_id,
                                             unsigned long receiver_id,
		    			     unsigned long exception_id,
		                             unsigned long thread_id) {
  cout << hex << "T " << method_id << " " << receiver_id << " " <<  exception_id << " " << thread_id << endl;
}

void IdentityCallbackHandler::exceptionHandle(unsigned long method_id,
		                              unsigned long receiver_id,
		                              unsigned long exception_id,
                                              unsigned long thread_id) {
  cout << hex << "H " << method_id << " " << receiver_id << " " << exception_id << " " << thread_id << endl; 
}

void IdentityCallbackHandler::exceptionalExit(unsigned long method_id,
                                             unsigned long receiver_id,
                                             unsigned long object_id,
                                             unsigned long thread_id) {
  cout << hex << "X " << method_id << " " << receiver_id << " " << object_id << " " << thread_id << endl;
}

