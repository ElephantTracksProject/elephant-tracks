#include "ExternalClassInstrumenter.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <errno.h>
#include <agent_util.h>
#include <java_crw_demo.h>
using namespace std;

ExternalClassInstrumenter::ExternalClassInstrumenter (jvmtiEnv *jvmti, string javapath,
                                                      string classpath, string mainclass,
                                                      string outputFile,
                                                      bool verbose, string experimental,
                                                      map<string,string> optionMap,
                                                      ETCallBackHandler *etcb) {
  this->javapath     = javapath;
  this->classpath    = classpath;
  this->mainclass    = mainclass;
  this->experimental = experimental;
  this->optionMap    = optionMap;
  this->etcb         = etcb;

  int pipeErr;

  // -- Set up the ASM subprocess

  pipeErr = pipe(parentToChild);
  if (pipeErr) {
    cerr << "ExternalClassInstrumenter::ExternalClassInstrumenter: could not pipe parentToChild."
         << endl;
    exit(1);
  }

  pipeErr = pipe(childToParent);
  if (pipeErr) {
    cerr << "ExternalClassInstrumenter::ExternalClassInstrumenter: could not pipe childToParent."
         << endl;
    exit(1);
  }

  childPid = fork();

  if (childPid < 0) {
    cerr << "ExternalClassInstrumenter::ExternalClassInstrumenter: Fork error." << endl;
    exit(1);
  }

  if (childPid) {

    // -- Parent
    //    Close unused ends of the pipe -- return to agent
    close(parentToChild[0]);
    close(childToParent[1]);

    // -- Create a lock

    jvmti->CreateRawMonitor("instrumenterLock", &lock);

  } else {

    // -- Child: start the ASM subprocess
    close(0);

    int dupErr = dup(parentToChild[0]);
    if (dupErr == -1) {
      cerr << "ExternalClassInstrumenter::ExternalClassInstrumenter: Could not dup." << endl;
      exit(1);
    }

    close(parentToChild[1]);

    close(1);
    dupErr = dup(childToParent[1]);
    if (dupErr == -1) {
      cerr << "ExternalClassInsturmenter::ExternalClassInsturmenter: Could not dup childToPartent[1]." << endl;
    }

    close(childToParent[0]);
    
    int Xflags = 0;
    for (map<string,string>::iterator it = optionMap.begin(); it != optionMap.end(); it++) {
      if (it->first.find("-X") == 0) {
        ++Xflags;
      }
    }
    int nflags = Xflags + 10;
    const char **args = (const char**) malloc((nflags + 1) * sizeof(char *));
    int pos = 0;
    args[pos++] = this->javapath.c_str();  // program name in arg 0
    args[pos++] = "-classpath";
    args[pos++] = this->classpath.c_str();
    args[pos++] = this->mainclass.c_str();
    args[pos++] = "-n";
    args[pos++] = outputFile.c_str();
    args[pos++] = (verbose ? "-v" : "");
    args[pos++] = this->experimental.c_str();

    for (map<string,string>::iterator it = optionMap.begin(); it != optionMap.end(); it++) {
      if (it->first.find("-X") == 0) {
        string arg = it->first;
        if (it->second.length() > 0) {
          arg += ("=" + it->second);
        }
        char *argStr = (char *)malloc(arg.length() + 1);
        strcpy(argStr, arg.c_str());
        args[pos++] = argStr;
      }
    }
    args[pos++] = NULL;
    if (pos > nflags) {
      cerr << "Overflowed arg array in ExternalClassInstrumenter!" << endl;
    } else if (optionMap.find("-XechoArgs") != optionMap.end() && 0) {
      for (int i = 0; i < pos-1; ++i) {
        cerr << "arg " << setw(2) << i << " = " << args[i] << endl;
      }
    }

    int execReturn = execv(this->javapath.c_str(), (char * const*)args);

    if (execReturn == -1) {
      //Do I even need to check the condition? If we got here something screwed up
      cerr << "Exec failed." << endl;
      cerr << "Command line was: " << javapath << " -classpath " << classpath
           << " " << mainclass << " -n " << outputFile.c_str() << " " << experimental
           << endl;
      exit(-1);
    }
  }
}

ExternalClassInstrumenter::~ExternalClassInstrumenter () {

  int closeErr;

  int status;
  closeErr = close(parentToChild[1]);
  if (closeErr) {
    cerr << "ExternalClassInstrumenter::~ExternalClassInstrumenter: "
         << "Could not close parent to child pipe." << endl;
    exit(1);
  }

  closeErr =close(childToParent[0]);
  if (closeErr) {
    cerr << "ExternalClassInstrumenter::~ExternalClassInstrumenter: "
         << "Could not close childToParent pipe." << endl;
    exit(1);
  }
	
  wait(&status);
}

// TODO(nricci01) We don't even use a lot of these paramters,
// and this API is actually under our control. Should we just
// get rid of them?
void ExternalClassInstrumenter::instrumentClass (jvmtiEnv *jvmti,
                                                 JNIEnv*,
                                                 jclass,
                                                 jobject,
                                                 const char*,
                                                 jobject,
                                                 jint class_data_len,
                                                 const unsigned char* class_data,
                                                 jint* new_class_data_len,
                                                 unsigned char** new_class_data) {
  jvmtiError error = jvmti->RawMonitorEnter(lock);
  check_jvmti_error(jvmti, error, "ExternalClassInstrumenter::lockOn: Cannot enter with raw monitor");

  writeUntilDone(OrigClassFile, parentToChild[1], class_data, (int)class_data_len);

  bool noClassFile = true;
  do {
    RecvKind kind;
    int new_length = 0;
    unsigned char *new_bytes = readUntilDone(jvmti, childToParent[0], kind, new_length);

    switch (kind) {
    case InstClassFile: {
      // class file coming now
      if (new_length == 0) {
        cerr << "Could not read from pipe." << endl;
        exit(1);
      }

      (*new_class_data) = new_bytes;
      (*new_class_data_len) = new_length;
      // Note: the VM will Deallocate the buffer

      noClassFile = false;
      break;  // if the instrumented wants to tell us something, it needs
      // to do so *before* sending the class file
    }
    case ClassInfo: {
      // class information, so that field info can be recorded
      char *className;
      int classId;
      int superId;
      int result = sscanf((const char*)new_bytes, "C 0x%x %as 0x%x",
                          &classId, &className, &superId);
      if (result != 3) {
        cerr << "Could not parse class information." << endl;
        exit(2);
      }
      // cerr << "Parsed class info: 0x" << hex << classId << " " << className << " 0x" << superId << endl;
      etcb->RecordClassInfo(classId, className, superId);
      free(className);
      jvmti->Deallocate(new_bytes);
      break;
    }
    case FieldInfo: {
      // field description: a line of text preceded by a two byte count
      char *instanceOrStatic;
      char *fieldName;
      char *className;
      char *desc;
      int fieldId;
      int classId;
      int result = sscanf((const char *)new_bytes, "F %as 0x%x %as 0x%x %as %as",
                          &instanceOrStatic, &fieldId, &fieldName, &classId, &className, &desc);
      if (result != 6) {
        cerr << "Could not parse field information." << endl;
        exit(2);
      }
      char kind = desc[0];
      if (kind == 'L' || kind == '[') {
        // we're interested only in pointer fields
        char staticKind = instanceOrStatic[0];
        // cerr << "Recorded field info: 0x" << hex << classId << " " << fieldName
        //      << " 0x" << fieldId << " " << staticKind << " (" << desc << ")" << endl;
        etcb->RecordFieldInfo(classId, fieldName, fieldId, staticKind == 'S');
      }
      free(instanceOrStatic);
      free(fieldName);
      free(className);
      free(desc);
      jvmti->Deallocate(new_bytes);
      break;
    }
    default: {
      // do nothing
    }
    }
  } while (noClassFile);

  error = jvmti->RawMonitorExit(lock);
  check_jvmti_error(jvmti, error, "ExternalClassInstrumenter::UnlockOn:Cannot exit with raw monitor");
}

void ExternalClassInstrumenter::sendInfo (jvmtiEnv *jvmti,
                                          JNIEnv *,
                                          SendKind kind,
                                          const char *line) {
  jvmtiError error = jvmti->RawMonitorEnter(lock);
  check_jvmti_error(jvmti, error, "ExternalClassInstrumenter::lockOn: Cannot enter with raw monitor");

  writeUntilDone(kind, parentToChild[1], (const unsigned char *)line, (int)strlen(line));

  RecvKind rkind;
  int count;
  unsigned char *buf = readUntilDone(jvmti, childToParent[0], rkind, count);
  if (rkind != Ack || count != 0) {
    // check for acknowledgment 'record' (no contents)
    cerr << "sendInfo: unexpected record kind " << dec << (int)kind << endl;
  }
  jvmti->Deallocate(buf);
  error = jvmti->RawMonitorExit(lock);
  check_jvmti_error(jvmti, error, "ExternalClassInstrumenter::UnlockOn:Cannot exit with raw monitor");
}

void ExternalClassInstrumenter::writeUntilDone (SendKind sk, int fd, const unsigned char* buffer, int length) {
  writeByte((unsigned char)sk, fd);
  writeSize(length, fd);
  writeFully(fd, buffer, length);
}

unsigned char *ExternalClassInstrumenter::readUntilDone (jvmtiEnv *jvmti, int fd, RecvKind &kind, int &length) {
  int bytes_to_read = 0;
  jvmtiError error;

  // -- Read kind and number of bytes
  kind = (RecvKind)readByte(fd);
  bytes_to_read = readSize(fd);

  // -- Allocate buffer
  unsigned char* buffer;
  //Cannot use malloc,
  //We need to use jvmti->Allocate, because the VM may try to free this memory.
  error = jvmti->Allocate(bytes_to_read, &buffer);
  check_jvmti_error(jvmti, error,
    "ExternalClassInstrumenter:: readUntilDone: could not allocate memory for instrumented class.");

  readFully(fd, buffer, bytes_to_read);
  length = bytes_to_read;
  return buffer;
}

unsigned char ExternalClassInstrumenter::readByte (int fd) {
  unsigned char b[1];
  readFully(fd, b, 1);
  return b[0];
}

void ExternalClassInstrumenter::writeByte (unsigned char b, int fd) {
  unsigned char buf[1];
  buf[0] = b;
  writeFully(fd, buf, 1);
}

unsigned int ExternalClassInstrumenter::readShort (int fd) {
  // NOTE: must be compatible with DataOutputStream
  unsigned char shortBytes[2];
  readFully(fd, shortBytes, 2);
  unsigned int value = ((int)shortBytes[1]) + (((int)shortBytes[0]) << 8);
  return value;
}

unsigned int ExternalClassInstrumenter::readSize (int fd) {
  // NOTE: must be compatible with DataOutputStream
  unsigned char sizeBytes[4];
  readFully(fd, sizeBytes, 4);

  unsigned int bytes_to_read = 
       ((int)sizeBytes[3]) 
    + (((int)sizeBytes[2]) << 8)
    + (((int)sizeBytes[1]) << 16)
    + (((int)sizeBytes[0]) << 24);

  return bytes_to_read;
}

void ExternalClassInstrumenter::writeSize (unsigned int size, int fd) {
  //    (NOTE: must be compatible with DataInputStream)
  unsigned char sizeBytes[4];
  sizeBytes[0] = (size & 0xFF000000) >> 24;
  sizeBytes[1] = (size & 0x00FF0000) >> 16;
  sizeBytes[2] = (size & 0x0000FF00) >> 8;
  sizeBytes[3] =  size & 0x000000FF;
  writeFully(fd, sizeBytes, 4);
}

void ExternalClassInstrumenter::readFully (int fd, unsigned char * buffer, int length) {
  // -- Read until done
  int bytes_left = length;
  int bytes_read = 0;
  int total_read = 0;
  while (bytes_left > 0) {
    bytes_read = read(fd, (buffer + total_read), bytes_left);
    bytes_left = bytes_left - bytes_read;
    total_read = total_read + bytes_read;
  }
}

void ExternalClassInstrumenter::writeFully (int fd, const unsigned char * buffer, int length) {
  // -- Write until done
  int bytes_left = length;
  int bytes_written = 0;
  int total_written = 0;
  while (bytes_left > 0) {
    bytes_written = write(fd, (buffer + total_written), bytes_left);
    bytes_left    = bytes_left    - bytes_written;
    total_written = total_written + bytes_written;
  }
}

// Local Variables:
// mode:C++
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
