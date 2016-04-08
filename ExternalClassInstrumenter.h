#ifndef EXTERNALCLASSINSTRUMENTER_H_
#define EXTERNALCLASSINSTRUMENTER_H_
#include "ClassInstrumenter.h"
#include "ETCallBackHandler.h"
#include <jni.h>
#include <jvmti.h>

using namespace std;
#include <string>
#include <map>

class ExternalClassInstrumenter : public ClassInstrumenter {
public:
  ExternalClassInstrumenter (jvmtiEnv *jvmti, string javapath, string classpath, string mainclass,
                             string outputFile, bool verbose, string experimental,
                             map<string,string> optionMap, ETCallBackHandler *etcb);
  virtual ~ExternalClassInstrumenter ();

  virtual void instrumentClass (jvmtiEnv *jvmti,
                                JNIEnv* env,
                                jclass class_being_redefined,
                                jobject loader,
                                const char* name,
                                jobject protection_domain,
                                jint class_data_len,
                                const unsigned char* class_data,
                                jint* new_class_data_len,
                                unsigned char** new_class_data);
  virtual void sendInfo (jvmtiEnv *jvmti, JNIEnv *jni, SendKind kind, const char *line);
private:
  string javapath;
  string classpath;
  string mainclass;
  string experimental;
  map<string,string> optionMap;

  void writeUntilDone (SendKind kind, int fd, const unsigned  char* buffer, int length);
  unsigned char *readUntilDone (jvmtiEnv *jvmti, int fd, RecvKind &kind, int &length);

  unsigned char readByte (int fd);
  void writeByte (unsigned char b, int fd);
	
  unsigned int readShort (int fd);

  unsigned int readSize (int fd);
  void writeSize (unsigned int size, int fd);
	
  void readFully (int fd, unsigned char * buffer, int length);
  void writeFully (int fd, const unsigned char * buffer, int length);

  jrawMonitorID lock;
  int childPid;
  int parentToChild[2];
  int childToParent[2];

  ETCallBackHandler *etcb;
};

#endif /*EXTERNALCLASSINSTRUMENTER_H_*/

// Local Variables:
// mode:C++
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
