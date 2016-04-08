#include "NewObjectRecord.h"
#include "main.h"

NewObjectRecord::NewObjectRecord (jlong objectID, jlong classID, jlong thread) {
  this->objectID = objectID;
  this->classID  = classID;
  this->thread = thread;
}

ostream &NewObjectRecord::streamTo (ostream &out) {
  if (wantRecord['N']) {
    out << "N " << hex << this->objectID << " " << this->classID << " " << thread << endl;
  }
  return (out);
}

char NewObjectRecord::getKind () {
  return 'N';
}

NewObjectRecord::~NewObjectRecord () { }
