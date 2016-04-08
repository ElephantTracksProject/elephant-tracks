#include "Record.h"

Record::Record () { }

Record::~Record () { }

bool Record::operator> (const Record &otherRecord) const {
  return getTime() > otherRecord.getTime();
}

jlong Record::getTime () const {
  return time;	
}

void Record::setTime (jlong time) {
  this->time = time;
}
