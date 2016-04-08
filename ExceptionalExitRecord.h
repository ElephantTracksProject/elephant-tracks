#ifndef EXCEPTIONALEXITRECORD_H_
#define EXCEPTIONALEXITRECORD_H_
#include "Record.h"

class ExceptionalExitRecord : public Record
{
public:
	ExceptionalExitRecord(jlong methodID, jlong receiver,
			 jlong exception, jlong thread);
	virtual ~ExceptionalExitRecord();
	virtual ostream &streamTo(ostream &out);
	virtual char getKind ();
private:
	jlong methodID;
	jlong receiver;
	jlong exception;
	jlong thread;
};

#endif /*EXCEPTIONALEXITRECORD_H_*/
