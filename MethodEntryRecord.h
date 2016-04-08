#ifndef METHODENTRYRECORD_H_
#define METHODENTRYRECORD_H_
#include "Record.h"

class MethodEntryRecord : public Record
{
public:
	MethodEntryRecord(jlong methodID, jlong receiver, jlong thread);
	virtual ~MethodEntryRecord();
	virtual ostream &streamTo(ostream &out);
	virtual char getKind ();
private:
	jlong methodID;
	jlong receiver;
	jlong thread;
};

#endif /*METHODENTRYRECORD_H_*/
