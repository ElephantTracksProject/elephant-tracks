#ifndef CLASSINSTRUMENTER_H_
#define CLASSINSTRUMENTER_H_
#include <jni.h>
#include <jvmti.h>

class ClassInstrumenter
{
 public:
  ClassInstrumenter();
  virtual ~ClassInstrumenter() = 0;
	
  enum SendKind { OrigClassFile = 1, SynthClassInfo, SynthFieldInfo };
  enum RecvKind { InstClassFile = 1, ClassInfo, FieldInfo, Ack };

  virtual void instrumentClass(jvmtiEnv *jvmti,
			       JNIEnv* env,
			       jclass class_being_redefined,
			       jobject loader,
			       const char* name,
			       jobject protection_domain,
			       jint class_data_len,
			       const unsigned char* class_data,
			       jint* new_class_data_len,
			       unsigned char** new_class_data) = 0;
  virtual void sendInfo (jvmtiEnv *jvmti, JNIEnv *jni, SendKind kind, const char *line)  = 0;
};

#endif /*CLASSINSTRUMENTER_H_*/
