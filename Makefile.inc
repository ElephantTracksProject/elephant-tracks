
# The lines below give variables that probably need customization to
# the target system.  The easiest way to do that is to redefine them
# in a Makefile.local file.
#

INSTALL_DIR = /data/rveroy/share/elephant-tracks

ASMJAR = /data/rveroy/src/JAR/asm-all-3.3.jar

# JAVA_PATH = /data/rveroy/src/jdk1.8.0_66
JAVA_PATH = /usr/lib/jvm/java-1.6.0

TBB_PATH = /data/rveroy/src/tbb44_20160526oss

#
# The lines below give variables that may be customized if needed/desired
#

JAVAOPTS = -cp .:$(ASMJAR)

INCLUDES = -I$(JAVA_PATH)/include -I$(JAVA_PATH)/include/linux -I./java_crw_demo -I./agent_util -I./GraphAlgorithms -I./RootEnumerator \
	-I$(TBB_PATH)/include

CXXFLAGS = $(INCLUDES) -std=c++11 -O2 -fPIC -L./GraphAlgorithms -L./RootEnumerator -I./GraphAlgorithms -I./gzstream -W -Wall -Werror
CFLAGS = $(INCLUDES) -O2 -fPIC 

javaclasses = ElephantTracks.class 'ElephantTracks$$1.class'
