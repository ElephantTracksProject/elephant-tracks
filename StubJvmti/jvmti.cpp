#include <map>
#include <iostream>
#include <vector>
#include "jvmti.h"
#include "tbb/queuing_mutex.h"
#include "tbb/compat/thread"

using namespace std;

jvmtiEnv::jvmtiEnv(): jrawMonitorIDCounter(0) {
  jni = new JNIEnv();
}


jvmtiEnv::~jvmtiEnv() {
  for(map<int,Monitor*>::iterator it = monitors.begin();
                                  it != monitors.end();
                                  it ++) {
    delete (*it).second;
  }

  delete jni;
}


void jvmtiEnv::assert_own_monitor(jrawMonitorID id) {
  assert(monitors[id]->owned && monitors[id]->current_owner == this_thread::get_id());
}

jvmtiError jvmtiEnv::CreateRawMonitor(string name, jrawMonitorID* id) {
  *id = jrawMonitorIDCounter++;
  monitors[*id] = new Monitor();

  cerr << "jvmtiEnv()::CreateRawMonitor: " << name << " " << *id << endl;
  return JVMTI_ERROR_NONE;
}


jvmtiError jvmtiEnv::RawMonitorEnter(jrawMonitorID monitor_id) {
  assert_monitor_exists(monitor_id);

  cerr << "jvmtiEnv::RawMonitorEnter: monitor_id" 
        << monitor_id << " thread: " << this_thread::get_id() << endl;
  Monitor* monitor = monitors[monitor_id];
  monitor->acquire();
  cerr << "jvmtiEnv::RawMonitorEnter: Aquired monitor" 
       << monitor_id << " thread: "  << this_thread::get_id() << endl;

  /*
  if (acquired == false) {
    thread::id thread_id = this_thread::get_id();
    monitors[monitor_id].waiting_threads.push_back(thread_id);
    signaled[thread_id] = false;
    this_thread::yield(); 
  }*/

  return JVMTI_ERROR_NONE;
}


jvmtiError jvmtiEnv::RawMonitorExit(jrawMonitorID id) {
  assert_monitor_exists(id);

  cerr << "jvmtiEnv::RawMonitorExit: " << id << endl;
  monitors[id]->release();
  return JVMTI_ERROR_NONE;
}


jvmtiError jvmtiEnv::RawMonitorWait(jrawMonitorID monitor_id, int time) {
  cerr << "jvmtiEnv::RawMonitorWait: " << monitor_id << endl;
  Monitor* monitor = monitors[monitor_id];
  tbb::atomic<bool> notification;

  thread::id thread_id = this_thread::get_id();
  //Tell the monitor to notify us by setting this bool.
  monitor->notifications[thread_id] = &notification;
  monitor->waiting_threads.push_back(thread_id);
  
  notification = false;
  monitor->release();

  // Spin wait
  cerr << "jvmtiEnv::RawMonitorWait: Waiting on: " << &notification << endl;
  while (!notification) {
    this_thread::yield();
  }

  monitor->acquire();
  monitor->notifications.erase(thread_id);
  
  return JVMTI_ERROR_NONE;
}


jvmtiError jvmtiEnv::RawMonitorNotifyAll(jrawMonitorID monitor_id) {
  assert_monitor_exists(monitor_id);
  Monitor* monitor = monitors[monitor_id];
  assert_own_monitor(monitor_id);


  cerr << "jvmtiEnv::RawMonitorNotifyAll: " << monitor_id << endl;
  for(vector<thread::id>::iterator thread_it = monitor->waiting_threads.begin();
                                   thread_it != monitor->waiting_threads.end();
                                   thread_it++) {
    cerr << "jvmtiEnv::RawMonitorNotifyAll: Notifying thread " << *thread_it << endl;
    *(monitor->notifications[*thread_it]) = true;
  }
                                  
  return JVMTI_ERROR_NONE;
}

typedef void (*RunAgentFunction)(jvmtiEnv*, JNIEnv*, void*);

void agentThreadWrapper(pair<jvmtiEnv*, JNIEnv*> jvmti_jni,
                  pair<RunAgentFunction, void*> func_data) {

    cerr << "Agent thread wrapper called." << endl;
    jvmtiEnv* jvmti = jvmti_jni.first;
    JNIEnv* jni = jvmti_jni.second;
  
    RunAgentFunction f = func_data.first;
    void* data = func_data.second;

    f(jvmti, jni, data);
}

jvmtiError jvmtiEnv::RunAgentThread(jobject thread,
                                    void (func)(jvmtiEnv* env, JNIEnv* jni, void* data),
                                    void* data,
                                    int priority) {

    cerr << "RunAgentThread" << endl;
    // We need to pack things up to make these API's compatible
    pair<jvmtiEnv*, JNIEnv*> jvmti_jni;

    jvmti_jni.first = this;
    jvmti_jni.second = jni;

    pair<RunAgentFunction,void*> func_data;
    func_data.first = func;
    func_data.second = data;

 
    std::thread t1(agentThreadWrapper, jvmti_jni, func_data);
}
