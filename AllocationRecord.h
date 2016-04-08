#ifndef ALLOCATIONRECORD_H_
#define ALLOCATIONRECORD_H_
#include <string>
#include "Record.h"

using namespace std;

class AllocationRecord :  public Record
{
public:
  AllocationRecord (jlong objectID,
		    jlong size,
		    int length,
		    string type,
		    jlong thread,
		    AllocKind kind,
		    jint site);
  virtual ostream &streamTo (ostream &out);
  virtual ~AllocationRecord ();
  virtual char getKind ();
private:
  jlong objectID;
  jlong size;
  jlong thread;
  string type;
  int length;
  AllocKind kind;
  jint site;
};

#endif /*ALLOCATIONRECORD_H_*/
