#include "MethodNameRecord.h"
#include "main.h"

MethodNameRecord::MethodNameRecord (unsigned long methodCode, string className, string methodName, string signature) {
	this->methodCode = methodCode;
	this->className = className;
	this->methodName = methodName;
	this->signature = signature;
}

ostream &MethodNameRecord::streamTo (ostream &out) {
	if (wantRecord['N']) {
		out << "N " << methodCode  << " " << className << " " << methodName << " " << signature << endl;
	}
	return out;
}

char MethodNameRecord::getKind () {
	return 'n';
}

MethodNameRecord::~MethodNameRecord () { }
