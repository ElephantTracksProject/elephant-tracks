#include "CountsRecord.h"
#include "main.h"

CountsRecord::CountsRecord (jlong countsTime,
			    int heapReads, int heapWrites,
			    int heapRefReads, int heapRefWrites, int instCount) {
  setTime(countsTime);
  this->heapReads     = heapReads;
  this->heapWrites    = heapWrites;
  this->heapRefReads  = heapRefReads;
  this->heapRefWrites = heapRefWrites;
  this->instCount     = instCount;
}

ostream &CountsRecord::streamTo (ostream &out) {
  if (wantRecord['C']) {
    out << "C" << dec
	<< " " << instCount
	<< " " << heapReads
	<< " " << heapWrites
	<< " " << heapRefReads
	<< " " << heapRefWrites
	<< endl;
  }
  return out; 	
}

void CountsRecord::combineFrom (CountsRecord *other) {
  this->heapReads     += other->heapReads;
  this->heapWrites    += other->heapWrites;
  this->heapRefReads  += other->heapRefReads;
  this->heapRefWrites += other->heapRefWrites;
  this->instCount     += other->instCount;
}

char CountsRecord::getKind () {
  return 'C';
}

CountsRecord::~CountsRecord () { }
