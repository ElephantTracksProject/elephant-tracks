#ifndef EXCEPTIONRECORD_H_
#define EXCEPTIONRECORD_H_
#include "Record.h"

class ExceptionRecord : public Record {
public:
  ExceptionRecord (jlong methodID,
		   jlong receiver,
		   jlong exception,
		   jlong thread,
		   ExceptKind kind);
  virtual ~ExceptionRecord ();
  virtual ostream &streamTo (ostream &out);
  virtual char getKind ();
private:
  jlong methodID;
  jlong receiver;
  jlong exception;
  jlong thread;
  ExceptKind kind;
};

#endif /*EXCEPTIONRECORD_H_*/
