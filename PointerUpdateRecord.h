#ifndef POINTERUPDATERECORD_H_
#define POINTERUPDATERECORD_H_
#include "Record.h"

class PointerUpdateRecord : public Record
{
public:
	PointerUpdateRecord(jlong oldTarget, jlong origin, jlong newTarget,
                            int fieldId, jlong thread);
	virtual ~PointerUpdateRecord();
	virtual ostream &streamTo(ostream &out);
	virtual char getKind ();
private:
	jlong oldTarget;
	jlong origin;
	jlong newTarget;
        int           fieldId;
	jlong thread;
};

#endif /*POINTERUPDATERECORD_H_*/
