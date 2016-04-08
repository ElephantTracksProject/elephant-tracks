#include "ExceptionalExitRecord.h"
#include "main.h"

ExceptionalExitRecord::ExceptionalExitRecord (jlong methodID, jlong receiver,
				    jlong exception, jlong thread) {
  this->methodID = methodID;
  this->receiver = receiver;
  this->exception = exception;
  this->thread = thread;
}

ostream &ExceptionalExitRecord::streamTo (ostream &out) {
  if (wantRecord['X']){
    out << "X " << hex << this->methodID << " " << this->receiver << " ";
    out << exception << " ";
    out << this->thread << endl;
  }
  return out;
}

char ExceptionalExitRecord::getKind () {
  return 'X';
}

ExceptionalExitRecord::~ExceptionalExitRecord () { }
