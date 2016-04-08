#include "parser.h"
#include "callback_handler.h"
#include <stdlib.h>
int debug = 0;



Parser::Parser(istream* traceFile, istream* nameFile, CallbackHandler* cb) :
	cbHandler(cb),
	traceFile(traceFile),
	namesFile(nameFile)
	
{
}

void Parser::read_trace() {
  read_trace_file(traceFile);
}

void Parser::read_trace_file(istream* in)
{
  char kind;
  unsigned long object_id, thread_id, method_id, old_target;
  unsigned long  new_target, site, field_id, receiver_id, exception_id, length;
  unsigned int size;
  string type;
  string line;
  unsigned long record_count = 0;
   

  while (in->good()) {
    (*in) >> kind;
    if (! in->good()) {
      break;
    }

    switch (kind) {
    case 'A':
    case 'I':
    case 'N':
    case 'P':
    case 'V':
      {
        (*in) >> hex >> object_id;
        (*in) >> hex >> size;
        (*in) >> type;
	(*in) >> hex >>  site;
        (*in) >> hex >>length;
        (*in) >> hex >> thread_id;
        
      	cbHandler->alloc(kind, object_id, size, type, site, length, thread_id); 
      }
      break;
    case 'D':
      {
        (*in) >> hex >> object_id;
	(*in) >> hex >> thread_id;

	cbHandler->death(object_id, thread_id);
      } 
      break;
    case 'U':
      {
        (*in) >> hex >> old_target;
        (*in) >> hex >> object_id;
        (*in) >> hex >> new_target;
	(*in) >> hex >> field_id;
	(*in) >> hex >> thread_id;

        cbHandler->pointerUpdate(old_target, object_id, new_target, field_id, thread_id);
      }
      break;
    case 'M':
      {
        (*in) >> hex >> method_id;
        (*in) >> hex >> object_id;
        (*in) >> hex >> thread_id;
       
	cbHandler->methodEntry(method_id, object_id, thread_id);
 
      }
      break;
    case 'E':
      {
        (*in) >> hex >> method_id;
        (*in) >> hex >> object_id;
        (*in) >> hex >> thread_id;

	cbHandler->methodExit(method_id, object_id, thread_id);
      }
      break;
    case 'T':
      {
	(*in) >> hex >> method_id;
	(*in) >> hex >> receiver_id;
        (*in) >> exception_id;
        (*in) >> thread_id;
	cbHandler->exceptionThrow(method_id, receiver_id, exception_id, thread_id);
      }
      break;
    case 'H':
      {
	(*in) >> hex >> method_id;
	(*in) >> hex >> receiver_id;
	(*in) >> hex >> exception_id;
        (*in) >> hex >> thread_id;
	
	cbHandler->exceptionHandle(method_id, receiver_id, exception_id, thread_id);
      }
      break;
    case 'C':
      break;
    case 'X':
      {
        (*in) >> hex >> method_id;
        (*in) >> hex >> object_id;
        (*in) >> hex >> exception_id;
        (*in) >> hex >> thread_id;

        cbHandler->exceptionalExit(method_id, object_id, exception_id, thread_id);
      }
      break;
    default:
      cerr << kind << " UNKNOWN RECORD TYPE" << endl;
      exit(1);
      break;
    }

    // getline(*in, line);

    record_count++;
    if (record_count % 1000000 == 0) {
      cerr << "At " << record_count << endl;
    }
  }
}

void Parser::read_name_file(istream* name_file)
{
  /*
  while ( ! name_file.eof()) {
    char kind;
    int method_id;
    string class_name;
    string method_name;
    string signature;
    
    name_file >> kind;

    if (kind == 'N') {
      name_file >> hex >> method_id;
      name_file >> class_name;
      name_file >> method_name;
      name_file >> signature;
     
      cbHandler->name(method_id, class_name, method_name, signature);

   }
  }*/
}
