#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/TestCase.h>
#include <cppunit/extensions/HelperMacros.h>
#include <pthread.h>
#include "TagTimeMap.h"

using namespace std;


static long num_to_insert = 1000000;

void* threadTestFunction(void* map) {
  TagTimeMap* tagTimeMap = (TagTimeMap*)map; 

  for (int i = 0; i < num_to_insert; i++) {
    tagTimeMap[i] = i;
  }

  return NULL;
}

class TagTimeMapTest : public CppUnit::TestCase {
  CPPUNIT_TEST_SUITE(TagTimeMapTest);
  CPPUNIT_TEST(testInsert);
  CPPUNIT_TEST(testIterator);
  CPPUNIT_TEST(testThreads);
  CPPUNIT_TEST(testErase);
  CPPUNIT_TEST_SUITE_END();


 public:
  void testInsert() {
    TagTimeMap tagTimeMap;

    tagTimeMap[1] = 1;
    tagTimeMap[2] = 2;
    tagTimeMap[3] = 3;

    CPPUNIT_ASSERT_EQUAL((jlong)1, tagTimeMap[1];
    CPPUNIT_ASSERT_EQUAL((jlong)2, tagTimeMap[2];
    CPPUNIT_ASSERT_EQUAL((jlong)3, tagTimeMap[3];
  }


  void testIterator() {
    typedef pair<jlong, jlong> TagTime;
    TagTimeMap tagTimeMap;

    tagTimeMap[1] = 1;
    tagTimeMap[2] = 2;
    tagTimeMap[3] = 3;
    
    int count = 0;
    set<pair<jlong,jlong> > found_pairs;
    for (TagTimeMap::Iterator it  = tagTimeMap.begin();
                              it != tagTimeMap.end();
                              it++) {
      count++;
      pair<jlong, jlong> tag_time = *it;
      found_pairs.insert(tag_time);
    }

    


    CPPUNIT_ASSERT_EQUAL(3, count);
    CPPUNIT_ASSERT_EQUAL(1, (int)found_pairs.count(TagTime(1, 1)));
    CPPUNIT_ASSERT_EQUAL(1, (int)found_pairs.count(TagTime(2, 2)));
    CPPUNIT_ASSERT_EQUAL(1, (int)found_pairs.count(TagTime(3, 3)));
 }


 void testErase() {
   TagTimeMap tagTimeMap;

   tagTimeMap[1] = 1;
   tagTimeMap[2] = 2;
   tagTimeMap[3] = 3;

   tagTimeMap.eraseTag(1);
    
   int count = 0;
   set<jlong> found_tags;
   for (TagTimeMap::Iterator it  = tagTimeMap.begin();
                             it != tagTimeMap.end();
                             it++) {
     count++;
     pair<jlong, jlong> tag_time = *it;
     found_tags.insert(tag_time.first);
   }

   CPPUNIT_ASSERT_EQUAL(0, (int)found_tags.count(1));
   CPPUNIT_ASSERT_EQUAL(1, (int)found_tags.count(2));
   CPPUNIT_ASSERT_EQUAL(1, (int)found_tags.count(3));
 }

 void testThreads() {
      int num_threads = 5;
      pthread_t threads[num_threads];
      TagTimeMap tagTimeMap;

      for (int i = 0; i < num_threads; i++) {
        pthread_create( &(threads[i]), NULL, threadTestFunction, (void*)&tagTimeMap);
      }

      void* status;
      for (int i = 0; i<num_threads; i++) {
        pthread_join( threads[i], &status);
      }


      long sum = 0;
      for (TagTimeMap::Iterator it = tagTimeMap.begin();
                               it != tagTimeMap.end();
                               it++ ) {
         pair<jlong, jlong> tag_time = *it;
         sum += tag_time.second;
         CPPUNIT_ASSERT_EQUAL(tag_time.second, tag_time.first);
      }

      CPPUNIT_ASSERT_EQUAL((long)((num_to_insert)*(num_to_insert-1))/2l, sum);
 }

};


CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TagTimeMapTest, "TagTimeMapTest");

CppUnit::Test* suite() {  
  CppUnit::TestFactoryRegistry &registry =
            CppUnit::TestFactoryRegistry::getRegistry();

   registry.registerFactory(
    &CppUnit::TestFactoryRegistry::getRegistry("TagTimeMapTest"));

  return registry.makeTest();
}


int main(int argc, char* argv[]) {
  CppUnit::TextUi::TestRunner runner;
  
  runner.addTest(suite());

  bool wasSucessfull = runner.run("") ;

  return wasSucessfull ? 0 : 1;
}
