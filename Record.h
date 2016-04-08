#ifndef RECORD_H_
#define RECORD_H_
#include <ostream>
#include <jni.h>
#include "main.h"
using namespace std;

class Record
{
public:
  Record ();
  virtual ~Record ();
  virtual jlong getTime () const;
  virtual void setTime (jlong time);
  virtual char getKind () = 0;
  bool operator> (const Record &otherRecord) const;
  virtual ostream &streamTo(ostream &stream) = 0;
private:
  long time; 
};

#endif /*RECORD_H_*/
