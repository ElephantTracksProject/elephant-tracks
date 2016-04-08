#include "MethodExitRecord.h"
#include "main.h"

MethodExitRecord::MethodExitRecord (jlong methodID, jlong receiver,
				    jlong thread) {
  this->methodID = methodID;
  this->receiver = receiver;
  this->thread = thread;
}

ostream &MethodExitRecord::streamTo (ostream &out) {
  if (wantRecord['E']){
    out << "E " << hex << this->methodID << " " << this->receiver << " ";
    out << this->thread << endl;
  }
  return out;
}

char MethodExitRecord::getKind () {
  return 'E';
}

MethodExitRecord::~MethodExitRecord () { }
