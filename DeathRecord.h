#ifndef DEATHRECORD_H_
#define DEATHRECORD_H_
#include <ostream>
#include "Record.h"
using namespace std;


class DeathRecord : public Record
{
public:
	DeathRecord(jlong objectID, jlong deathTime, jlong thread);
	virtual ~DeathRecord();
	virtual ostream &streamTo(ostream &out);
	virtual char getKind ();
private:
	jlong objectID;
	jlong thread;
};

#endif /*DEATHRECORD_H_*/
