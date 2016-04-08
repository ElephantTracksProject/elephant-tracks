#ifndef ROOTRECORD_H_
#define ROOTRECORD_H_
#include <ostream>
#include "Record.h"
using namespace std;


class RootRecord : public Record
{
public:
	RootRecord(jlong oid, jlong thread);
	virtual ~RootRecord();
	virtual ostream &streamTo(ostream &out);
	virtual char getKind ();
private:
	long oid;
	long thread;
};

#endif /*ROOTRECORD_H_*/
