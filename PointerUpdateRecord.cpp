#include "PointerUpdateRecord.h"
#include "main.h"

PointerUpdateRecord::PointerUpdateRecord (jlong oldTarget, jlong origin, jlong newTarget,
					  int fieldId, jlong thread) {
	this->oldTarget = oldTarget;
	this->origin = origin;
	this->newTarget = newTarget;
	this->fieldId = fieldId;
	this->thread = thread;
}

ostream &PointerUpdateRecord::streamTo (ostream &out) {
	if (wantRecord['U']) {
	  out << "U " << hex << oldTarget << " " << origin << " " << newTarget;
		if (showFields) {
		        out << " " << fieldId;
		}
		out << " " << thread;
                out << endl;
	}

  return out;
}

char PointerUpdateRecord::getKind () {
	return 'U';
}

PointerUpdateRecord::~PointerUpdateRecord () { }
