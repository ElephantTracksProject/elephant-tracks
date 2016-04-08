#ifndef NEWOBJECTRECORD_H_
#define NEWOBJECTRECORD_H_
#include <string>
#include "Record.h"

using namespace std;

class NewObjectRecord :  public Record
{
public:
  NewObjectRecord (jlong objectID, jlong classID, jlong thread);
	virtual ostream &streamTo(ostream &out);
	virtual ~NewObjectRecord();
	virtual char getKind ();
private:
	jlong objectID;
	jlong classID;
	jlong thread;
};

#endif /*NEWOBJECTRECORD_H_*/
