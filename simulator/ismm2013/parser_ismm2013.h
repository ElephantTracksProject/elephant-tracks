#ifndef PARSER_H_
#define PARSER_H_
#include <iostream>
#include <fstream>
#include <string>
#include "../callback_handler.h"
#include "../identity_callback_handler.h"

using namespace std;

class Parser{
 public:
	Parser(istream* traceFile, istream* nameFile, CallbackHandler* cbHandler);
	void read_trace();	

 private:
	void read_trace_file(istream* traceFile);
	void read_name_file(istream* nameFile);
	unsigned int count_spaces(string s); 
	CallbackHandler *cbHandler;
	istream* traceFile;
	istream* namesFile;
};




#endif 
