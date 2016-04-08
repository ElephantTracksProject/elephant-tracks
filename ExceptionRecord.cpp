#include "ExceptionRecord.h"
#include "main.h"

ExceptionRecord::ExceptionRecord (jlong methodID,
				  jlong receiver,
				  jlong exception,
				  jlong thread,
				  ExceptKind kind) {
  this->methodID  = methodID;
  this->receiver  = receiver;
  this->exception = exception;
  this->thread    = thread;
  this->kind      = kind;
}

ostream &ExceptionRecord::streamTo (ostream &out) {
  char letter = (char)kind;
  if (wantRecord[(unsigned int)letter]){
    out << letter << " " << hex << this->methodID << " " << this->receiver << " "
	<< exception << " " << this->thread << endl;
  }
  return out;
}

char ExceptionRecord::getKind () {
  return (char)kind;
}

ExceptionRecord::~ExceptionRecord () { }
