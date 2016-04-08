#include "parser_ismm2013.h"
#include "../callback_handler.h"
#include <stdlib.h>
#include <sstream>
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

unsigned int Parser::count_spaces(string s) {
  unsigned int spaces = 0;
  for (int i = 0; i < s.size(); i++) {
    if (s[i] == ' ') {
      spaces++;
    }
  }

  return spaces;
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
    // (*in) >> kind;
    getline(*in, line);
    stringstream line_stream(line);
    line_stream >> kind;

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
        unsigned int spaces = count_spaces(line);
	if (spaces == 5) {
       	  line_stream >> hex >> object_id;
          line_stream >> hex >> size;
          line_stream >> type;
	  line_stream >> hex >> site;
          line_stream >> hex >> thread_id;
      	  cbHandler->alloc(kind, object_id, size, type, site, 0, thread_id); 
        }
        else if (spaces == 6) {
       	  line_stream >> hex >> object_id;
          line_stream >> hex >> size;
          line_stream >> type;
	  line_stream >> hex >> site;
          line_stream >> hex >>length;
          line_stream >> hex >> thread_id;
      	  cbHandler->alloc(kind, object_id, size, type, site, length, thread_id); 
	}
	else {
	  cerr << "Unexpected number of fields in allocation record:  " << spaces << "line: " << line << endl;
	  exit(1);
        }
        
      }
      break;
    case 'D':
      {
        line_stream >> hex >> object_id;

	cbHandler->death(object_id);
      } 
      break;
    case 'U':
      {
        line_stream >> hex >> old_target;
        line_stream >> hex >> object_id;
        line_stream >> hex >> new_target;
	line_stream >> hex >> field_id;
	line_stream >> hex >> thread_id;
        cbHandler->pointerUpdate(old_target, object_id, new_target, field_id, thread_id);
      }
      break;
    case 'M':
      {
        line_stream >> hex >> method_id;
        line_stream >> hex >> object_id;
        line_stream >> hex >> thread_id;
       
	cbHandler->methodEntry(method_id, object_id, thread_id);
 
      }
      break;
    case 'E':
      { 
        unsigned int spaces = count_spaces(line);
	if (spaces == 3) {
          line_stream >> hex >> method_id;
          line_stream >> hex >> receiver_id;
          line_stream >> hex >> thread_id;
	  cbHandler->methodExit(method_id, receiver_id, thread_id);
        }
        else if (spaces == 4) {
	  line_stream >> hex >> method_id;
          line_stream >> hex >> receiver_id;
	  line_stream >> exception_id;
          line_stream >> hex >> thread_id;
 	  cbHandler->exceptionalExit(method_id, receiver_id, exception_id,thread_id);
	}
	else {
	  cerr << "Wrong number of feilds in method exit record: " << line;
	  exit(1);
	}

      }
      break;
    case 'T':
      {
	line_stream >> hex >> method_id;
	line_stream >> hex >> receiver_id;
        line_stream >> exception_id;
        line_stream >> thread_id;
	cbHandler->exceptionThrow(method_id, receiver_id, exception_id, thread_id);
      }
      break;
    case 'H':
      {
	line_stream >> hex >> method_id;
	line_stream >> hex >> receiver_id;
	line_stream >> hex >> exception_id;
        line_stream >> hex >> thread_id;
	
	cbHandler->exceptionHandle(method_id, receiver_id, exception_id, thread_id);
      }
      break;
    case 'C':
      break;
    case 'X':
      {
        line_stream >> hex >> method_id;
        line_stream >> hex >> object_id;
        line_stream >> hex >> exception_id;
        line_stream >> hex >> thread_id;

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
