#ifndef METHODEXITRECORD_H_
#define METHODEXITRECORD_H_
#include "Record.h"

class MethodExitRecord : public Record
{
public:
	MethodExitRecord(jlong methodID, jlong receiver, jlong thread);
	virtual ~MethodExitRecord();
	virtual ostream &streamTo(ostream &out);
	virtual char getKind ();
private:
	jlong methodID;
	jlong receiver;
	jlong thread;
};

#endif /*METHODEXITRECORD_H_*/
