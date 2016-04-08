#ifndef PARSER_H_
#define PARSER_H_
#include <iostream>
#include <fstream>
#include <string>
#include "callback_handler.h"
#include "identity_callback_handler.h"

using namespace std;
/*Author: Nathan P. Ricci (nricci01@cs.tufts.edu)
 * This is a parser for the Elephant Tracks trace format, version 2
 */
class Parser{
 public:
	Parser(istream* traceFile, istream* nameFile, CallbackHandler* cbHandler);
	void read_trace();	

 private:
	void read_trace_file(istream* traceFile);
	void read_name_file(istream* nameFile);

	CallbackHandler *cbHandler;
	istream* traceFile;
	istream* namesFile;
};




#endif 
