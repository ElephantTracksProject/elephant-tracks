#include "ETThreadLocalData.h"
#include "ETCallBackHandler.h"
#include <stdlib.h>
#include <iostream>
#define ATOMICALLY(x) enter_critical_section(theJVMTI); x exit_critical_section(theJVMTI);
//#define DEBUG_NEWOBJECT 1

using namespace std;

ETThreadLocalData::ETThreadLocalData (int threadTag, jvmtiEnv *theJVMTI) {
  this->threadTag   = threadTag;
  this->callDepth   = 0;
  this->allocInfos  = NULL;
#ifdef DEBUG_CALL_DEPTH
  this->callEntries = NULL;
#endif /* DEBUG_CALL_DEPTH */
  this->theJVMTI = theJVMTI;
}

int ETThreadLocalData::getCallDepth () {
  return callDepth;
}

void ETThreadLocalData::incrementCallDepth (int incr) {
  callDepth += incr;
#ifdef DEBUG_NEWOBJECT_DEPTH
  ATOMICALLY(
  cerr << dec << "New depth = " << callDepth << "; thread = " << hex << threadTag << endl;
  )
#endif
}

void ETThreadLocalData::noteEntry (int depth, int methodId) {
#ifdef DEBUG_CALL_DEPTH
  struct callEntry *newEntry = (struct callEntry *)malloc(sizeof(struct callEntry));
  newEntry->next     = this->callEntries;
  newEntry->depth    = depth;
  newEntry->methodId = methodId;
  this->callEntries = newEntry;
#endif /* DEBUG_CALL_DEPTH */
}

void ETThreadLocalData::noteExit (int depth, int methodId) {
  struct allocInfo *ai;

  while (((ai = this->allocInfos) != NULL) && (ai->callDepth >= depth)) {
    this->allocInfos = ai->next;
    struct updateInfo *ui;
    while ((ui = ai->updates) != NULL) {
      ai->updates = ui->next;
      free(ui);
    }
    free(ai);
  }
#ifdef DEBUG_NEWOBJECT_DEPTH
  ATOMICALLY(
  cerr << dec << "Exit; depth = " << callDepth << "; thread = " << hex << threadTag << endl;
  )
#endif

#ifdef DEBUG_CALL_DEPTH
  struct callEntry *entry = this->callEntries;
  if (entry == NULL) {
    ATOMICALLY(
    cerr << dec << "Call depth mismatch; no entry; expected: " << depth << " " << hex << methodId
	 << "; thread = " << threadTag << endl;
    )
    return;
  }
  this->callEntries = entry->next;
  if (entry->depth != depth) {
    ATOMICALLY(
    cerr << dec << "Call depth mismatch; is: " << entry->depth << "; expected: " << depth
	 << "; thread = " << hex << threadTag << endl;
    )
  } else if (entry->methodId != methodId) {
    ATOMICALLY(
    cerr << hex << "Method id mismatch; is: " << entry->methodId << "; expected: " << methodId
	 << "; thread = " << threadTag << endl;
    )
  }
  free(entry);
#endif /* DEBUG_CALL_DEPTH */
}

void ETThreadLocalData::newObject (int allocSite, jlong newTag, unsigned long timestamp) {
  struct allocInfo *ai = (struct allocInfo *)malloc(sizeof(struct allocInfo));
  ai->next = this->allocInfos;
  ai->callDepth = this->callDepth;
  ai->allocSite = allocSite;
  ai->objectTag = newTag;
  ai->allocTime = timestamp;
  ai->updates   = 0;
  this->allocInfos = ai;
#ifdef DEBUG_NEWOBJECT
  ATOMICALLY(
  cerr << dec << "New object; site = " << allocSite << "; tag = " << hex << newTag
       << "; depth = " << dec << callDepth << "; thread = " << hex << threadTag << endl;
  )
#endif
}

static struct allocInfo *getAllocInfoForSite (struct allocInfo *list, int allocSite) {
  while ((list != NULL) && (list->allocSite != allocSite)) {
    list = list->next;
  }
  return list;
}

static struct updateInfo *getUpdateInfoForField (struct updateInfo *list, int fieldId) {
  while ((list != NULL) && (list->fieldId != fieldId)) {
    list = list->next;
  }
  return list;
}

void ETThreadLocalData::uninitPutfield (int allocSite, int fieldId, jlong newValTag, ETCallBackHandler *cb) {
  allocInfo *ai = getAllocInfoForSite (this->allocInfos, allocSite);
  if (ai == NULL) {
    ATOMICALLY(
    cerr << "uninitPutField called for object that it cannot find; site is: " << dec << allocSite << hex
         << "; thread = " << threadTag << endl;
    )
    return;
  }
#ifdef DEBUG_NEWOBJECT
  ATOMICALLY(
  cerr << "UninitPutfield; site = " << dec << allocSite
       << hex << "; value = " << newValTag << "; field = " << fieldId << "; thread = " << threadTag << endl;
  )
#endif
  updateInfo *ui = getUpdateInfoForField (ai->updates, fieldId);
  jlong oldTag = 0;
  if (ui == NULL) {
    ui = (struct updateInfo *)malloc(sizeof(struct updateInfo));
    ui->next      = ai->updates;
    ui->fieldId   = fieldId;
    ai->updates = ui;
  } else {
    oldTag = ui->targetTag;
  }
  ui->targetTag = newValTag;
  cb->updateHeap(oldTag, ai->objectTag, newValTag);
  // TODO: PointerUpdateRecord
}

void ETThreadLocalData::initOf (int allocSite, jboolean objectInit, ETCallBackHandler *cb) {
  struct allocInfo *ai = getAllocInfoForSite(this->allocInfos, allocSite);

  if (ai == NULL) {
    ATOMICALLY(
    cerr << "ETThreadLocalData::initOf: allocSite " << dec << allocSite << " not found; depth = "
         << callDepth << "; thread id = " << hex << threadTag
         << "; next tag = " << cb->readNextTag() << endl;
    )
    return;
  }
#ifdef DEBUG_NEWOBJECT
  ATOMICALLY(
  cerr << "InitOf; site = " << dec << allocSite << "; tag = " << hex << ai->objectTag
       << "; depth = " << dec << callDepth << "; thread = " << hex << threadTag << endl;
  )
#endif

  struct allocInfo *newInfo = (struct allocInfo *)malloc(sizeof(struct allocInfo));
  newInfo->next = this->allocInfos;
  newInfo->callDepth = this->callDepth + 1;
  newInfo->allocSite = (objectInit ? -2 : -1);
  newInfo->objectTag = ai->objectTag;
  newInfo->allocTime = ai->allocTime;
  newInfo->updates   = ai->updates;  // transfer update list to this record :-)
  ai->updates = 0;
  this->allocInfos = newInfo;
}

jlong ETThreadLocalData::newObject (ETCallBackHandler *cb) {
  struct allocInfo *ai = this->allocInfos;
  if (ai == NULL) {
    ATOMICALLY(
    cerr << "ETThreadLocalData::newObject(jobject): no allocInfo available; thread = "
    << hex << threadTag << "; next tag = " << cb->readNextTag() << endl;
    )
    return 0;
  }
  if (ai->allocSite != -2) {
#ifdef DEBUG_NEWOBJECT
    ATOMICALLY(
    cerr << "NewObject() site != -2; depth = " << dec << callDepth
         << "; thread = " << hex << threadTag << endl;
    )
#endif
    return 0;
  }
#ifdef NEVER
  if (this->callDepth != ai->callDepth) {
    // NOTE: the newobj call happens *before* method entry is noted, so callDepth should be ==
    ATOMICALLY(
    cerr << "ETThreadLocalData::newObject(jobject): callDepth does not match; thread = "
         << hex << threadTag << endl;
    )
    return 0;
  }
#endif
  jlong tag = ai->objectTag;
#ifdef DEBUG_NEWOBJECT
  ATOMICALLY(
  cerr << "NewObject(); tag = " << hex << tag << "; depth = " << dec << callDepth
       << "; thread = " << hex << threadTag << endl;
  )
#endif
  struct allocInfo **prev = &(this->allocInfos);
  struct allocInfo *curr = this->allocInfos;
  while (curr != NULL) {
    struct allocInfo *next = curr->next;
    if (curr->objectTag == tag) {
      (*prev) = next;
      struct updateInfo *ui;
      while ((ui = curr->updates) != NULL) {
        curr->updates = ui->next;
        free(ui);
      }
      free(curr);
    } else {
      prev = &(curr->next);
    }
    curr = next;
  }
  return tag;
}

ETThreadLocalData::~ETThreadLocalData () { }

// Local Variables:
// mode:C++
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
