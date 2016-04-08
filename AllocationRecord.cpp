#include "AllocationRecord.h"
#include "main.h"

AllocationRecord::AllocationRecord (jlong objectID,
				    jlong size,
				    int length,
				    string type,
				    jlong thread,
				    AllocKind kind,
				    jint site) {
  this->objectID = objectID;
  this->size     = size;
  this->length   = length;
  this->type     = type;
  this->thread   = thread;
  this->kind     = kind;
  this->site     = site;
}

ostream &AllocationRecord::streamTo (ostream &out) {
  char letter = (char)kind;
  if (wantRecord[(unsigned int)letter]) {
    out << hex << letter << " " << this->objectID << " "
	<< this->size << " " << this->type << " "
	<< this->site << " ";
 
    if (length >= 0) {
      out << length << " ";
    }
    else {
      out << hex << 0 << " ";
    }

    out << thread << endl;
  }
  return (out);
}

char AllocationRecord::getKind () {
  return 'A';
}

AllocationRecord::~AllocationRecord () { }
