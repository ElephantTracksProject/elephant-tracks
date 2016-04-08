#ifndef JNI_H_
#define JNI_H_
#include <string>

using namespace std;

typedef int jobject;
typedef int jclass;
typedef long jlong;

class JNIEnv {
 public:
  jobject AllocObject(jclass klass) { return 1; };
  jclass FindClass(string klassName) { return 1; };
};

#endif // JNI_H_
