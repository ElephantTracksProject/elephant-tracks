#include "RootRecord.h"
#include "main.h"

RootRecord::RootRecord (jlong oid, jlong thread) {
  this->oid = oid;
  this->thread = thread;
}

RootRecord::~RootRecord () { }

ostream &RootRecord::streamTo (ostream &out) {
  if (wantRecord['R']) {
    out << "R " << hex << this->oid << " " << this->thread << endl;
  }
  return out;	
}

char RootRecord::getKind () {
  return 'R';
}
