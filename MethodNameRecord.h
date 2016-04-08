#ifndef METHODNAMERECORD_H_
#define METHODNAMERECORD_H_
#include <string>
#include "Record.h"

using namespace std;


class MethodNameRecord  :  public Record
{
public:
	MethodNameRecord(unsigned long methodCode, string className, string methodName, string signature);
	virtual ostream &streamTo(ostream &out);
	virtual ~MethodNameRecord();
	virtual char getKind ();
private:
	long methodCode;
	string className;
	string methodName;
	string signature;
};

#endif /*METHODNAMERECORD_H_*/
