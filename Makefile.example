
# The lines below give variables that probably need customization to
# the target system.  The easiest way to do that is to redefine them
# in a Makefile.local file.
#

INSTALL_DIR = ###CHANGE ME###

ASMJAR = ###CHANGE ME### current version that we're using is asm-all-3.3.jar

JAVA_PATH = ### Use your java directory. Looks like this: /usr/lib/jvm/java-6-openjdk
# Has to be Java 1.6. I haven't tested 1.7+

#
# The lines below give variables that may be customized if needed/desired
#

JAVAOPTS = -cp .:$(ASMJAR)

INCLUDES = -I$(JAVA_PATH)/include -I$(JAVA_PATH)/include/linux -I./java_crw_demo -I./agent_util -I./GraphAlgorithms -I./RootEnumerator

CXXFLAGS = $(INCLUDES) -O2 -fPIC   -L./GraphAlgorithms -L./RootEnumerator -I./GraphAlgorithms -I./gzstream -W -Wall -Werror
CFLAGS = $(INCLUDES) -O2 -fPIC 

javaclasses = ElephantTracks.class 'ElephantTracks$$1.class'
