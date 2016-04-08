#include "DeathRecord.h"
#include "main.h"

DeathRecord::DeathRecord (jlong objectID, jlong deathTime, jlong thread) {	
	setTime(deathTime);
	this->objectID = objectID;
	this->thread = thread;
}

ostream &DeathRecord::streamTo (ostream &out) {
	if (wantRecord['D']) {
	  out << "D " << hex << this->objectID << " " << this->thread <<  endl;
	}
	return out; 	
}

char DeathRecord::getKind () {
	return 'D';
}

DeathRecord::~DeathRecord () { }

