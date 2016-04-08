include ../../Makefile.inc

include $(wildcard ../../Makefile.local)

all: ClassReWriter.class ETAdapter.class ETClassWriter.class \
     ETOptMethodAdapter.class \
     ETInlinerAdapter.class ETNativeWrapper.class ETETMethodAdapter.class \
     ETClassLoader.class

ClassReWriter.class ETAdapter.class ETClassWriter.class \
     ETOptMethodAdapter.class  \
     ETInlinerAdapter.class ETNativeWrapper.class ETETMethodAdapter.class \
     ETClassLoader.class: \
     ClassReWriter.java ETAdapter.java ETClassWriter.java \
     ETOptMethodAdapter.java  \
     ETInlinerAdapter.java ETNativeWrapper.java ETETMethodAdapter.java \
     ETClassLoader.java
	javac $(JAVAOPTS) *.java 

clean:
	rm -f *.class
