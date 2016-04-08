
# The lines below give variables that probably need customization to
# the target system.  The easiest way to do that is to redefine them
# in a Makefile.local file.
#

INSTALL_DIR = /home/nricci01/elephantTracks

ASMJAR = /home/nricci01/asm-3.1.jar

JAVA_PATH = /usr/lib/jvm/java-6-openjdk

#
# The lines below give variables that may be customized if needed/desired
#

JAVAOPTS = -cp .:$(ASMJAR)

INCLUDES = -I$(JAVA_PATH)/include -I$(JAVA_PATH)/include/linux -I./java_crw_demo -I./agent_util -I./GraphAlgorithms -I./RootEnumerator

CXXFLAGS = $(INCLUDES) -O2 -fPIC   -L./GraphAlgorithms -L./RootEnumerator -I./GraphAlgorithms -I./gzstream -W -Wall -Werror
CFLAGS = $(INCLUDES) -O2 -fPIC 

javaclasses = ElephantTracks.class 'ElephantTracks$$1.class'
