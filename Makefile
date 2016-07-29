#
# Note: this Makefile assumes you will use GNU make!
#
# Typical usage would be:
#   make     (or 'make all')
#   make install
#

# The system wide settings that affect all sub builds.
include Makefile.inc

# Only change things below if needed. These are build related and
# not local settings.

BASEDIR := $(shell pwd)

LIBRARY_PATH = TBB_LIBPATH:$(LIBRARY_PATH)

ETLIB = libElephantTracks$(ETLIB_VERSION).so

ETJAR = elephantTracksRewriter$(ETJAR_VERSION).jar

# define these before doing the includes, so they can br overridden

objects = main.o \
          AllocationRecord.o ClassInstrumenter.o\
          DeathRecord.o ExternalClassInstrumenter.o FlatTraceOutputter.o \
          ETCallBackHandler.o MethodEntryRecord.o MethodNameRecord.o \
          MethodExitRecord.o NewObjectRecord.o PointerUpdateRecord.o Record.o RootRecord.o \
          WeakRefCleaner.o CountsRecord.o ExceptionRecord.o ExceptionalExitRecord.o \
          SortAndOutputThread.o 

subobjects = agent_util/agent_util.o java_crw_demo/java_crw_demo.o \
             GraphAlgorithms/HashGraph.o

libraries = elephantTracksRewriter agent_util java_crw_demo gzstream GraphAlgorithms

sublibraries = -LGraphAlgorithms -Lgzstream -lgzstream -lz -ltbb

javaclasses = ElephantTracks.class 'ElephantTracks$$1.class'

#
# The primary/default target
#
all: $(objects) $(libraries) $(javaclasses) $(ETLIB)

main.o: main.cpp

AllocationRecord.o: AllocationRecord.cpp AllocationRecord.h

ClassInstrumenter.o: ClassInstrumenter.cpp ClassInstrumenter.h

CountsRecord.o: CountsRecord.cpp CountsRecord.h

DeathRecord.o: DeathRecord.cpp DeathRecord.h

ExceptionRecord.o: ExceptionRecord.cpp ExceptionRecord.h

ExceptionalExitRecord.o: ExceptionalExitRecord.cpp ExceptionalExitRecord.h

ExternalClassInstrumenter.o: ExternalClassInstrumenter.cpp ExternalClassInstrumenter.h

FlatTraceOutputter.o: FlatTraceOutputter.cpp FlatTraceOutputter.h

ETCallBackHandler.o: ETCallBackHandler.cpp ETCallBackHandler.h 

MethodEntryRecord.o: MethodEntryRecord.cpp MethodEntryRecord.h

MethodExitRecord.o: MethodExitRecord.cpp MethodExitRecord.h

MethodNameRecord.o: MethodNameRecord.cpp MethodNameRecord.h

NewObjectRecord.o: NewObjectRecord.cpp NewObjectRecord.h

PointerUpdateRecord.o: PointerUpdateRecord.cpp PointerUpdateRecord.h

Record.o: Record.cpp Record.h

RootRecord.o: RootRecord.cpp RootRecord.h

SortAndOutputThread.o: SortAndOutputThread.cpp SortAndOutputThread.h

WeakRefCleaner.o: WeakRefCleaner.cpp WeakRefCleaner.h

SortAndOutputThread_test: SortAndOutputThread_test.cpp SortAndOutputThread.o ./StubJvmti/jni.h ./StubJvmti/jvmti.h ./StubJvmti/jvmti.cpp
	g++ -std=c++11 -c -g ./StubJvmti/jvmti.cpp -o ./StubJvmti/jvmti.o -I./StubJvmti 
	g++ -std=c++11 -c -g ./StubJvmti/main.cc -o ./StubJvmti/main.o -I./StubJvmti 
	g++ -std=c++11 -c -g SortAndOutputThread.cpp -I./StubJvmti 
	g++ -std=c++11 -g -DTHREAD_TAG_UPDATES -I./StubJvmti -o SortAndOutputThread_test SortAndOutputThread_test.cpp AllocationRecord.o DeathRecord.o MethodEntryRecord.o MethodExitRecord.o PointerUpdateRecord.o SortAndOutputThread.o RootRecord.o Record.o ./StubJvmti/main.o ./StubJvmti/jvmti.o -ltbb

ElephantTracks.class 'ElephantTracks$$1.class': ElephantTracks.java
	@echo $(JAVAC)
	$(JAVAC) -g ElephantTracks.java

.PHONY: all $(libraries)

$(libraries) : 
	$(MAKE) BASEDIR=$(BASEDIR) --directory=$@ all

$(ETLIB): $(objects) $(javaclasses) $(libraries)
	g++ $(CXXFLAGS) -shared $(objects) $(subobjects) $(sublibraries) -o $(ETLIB)

clean:
	$(foreach lib,$(libraries),  $(MAKE) --directory=$(lib) clean;)
	rm -f $(objects) $(javaclasses) $(ETLIB)

install: $(objects) $(libraries) $(ETLIB)
	mkdir -p $(INSTALL_DIR)
	cp -p $(javaclasses) $(ETLIB) ./elephantTracksRewriter/$(ETJAR) ./gzstream/libgzstream.a $(INSTALL_DIR)
