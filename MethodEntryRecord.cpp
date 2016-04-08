#include "MethodEntryRecord.h"
#include "main.h"

MethodEntryRecord::MethodEntryRecord (jlong methodID,
                                      jlong receiver,
                                      jlong thread) {
  this->methodID = methodID;
  this->receiver = receiver;
  this->thread = thread;
}

MethodEntryRecord::~MethodEntryRecord () { }

ostream &MethodEntryRecord::streamTo (ostream &out) {
  if (wantRecord['M']){
    out << "M " << hex << this->methodID << " " << this->receiver << " " 
        << this->thread << endl;
  }
  return out;	
}

char MethodEntryRecord::getKind () {
  return 'M';
}
