#ifndef COUNTSRECORD_H_
#define COUNTSRECORD_H_
#include <ostream>
#include "Record.h"
using namespace std;


class CountsRecord : public Record
{
public:
  CountsRecord (jlong countsTime, int heapReads, int heapWrites,
		int heapRefReads, int heapRefWrites, int instCount);
	virtual ~CountsRecord ();
	virtual ostream &streamTo (ostream &out);
	virtual char getKind ();
	virtual void combineFrom (CountsRecord *other);
private:
	int heapReads;
	int heapWrites;
	int heapRefReads;
	int heapRefWrites;
	int instCount;
};

#endif /*COUNTSRECORD_H_*/
