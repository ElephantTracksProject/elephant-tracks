# TODO: We need to get rid of recursive make because
# recursive make makes (haha) the build harder to understand
# for no gain.

# USER SETTINGS
# Change to suit your local settings

INSTALL_DIR = /data/rveroy/ETRUN/lib

ASMJAR = /data/rveroy/src/JAR/asm-all-3.3.jar

# So far compiles with the following versions of Java
JAVA_PATH = /usr/lib/jvm/java-1.6.0-openjdk-1.6.0.39.x86_64
#
# JAVA_PATH for Tufts black-hole and white-hole
# JAVA_PATH = /usr/lib/jvm/java-1.7.0-oracle.x86_64
#
# JAVA_PATH for Tufts red-giant
# JAVA_PATH = /usr/lib/jvm/java-1.7.0-openjdk-1.7.0.111-2.6.7.2.el7_2.x86_64

# Uncomment and customize if you need a non-standard location for TBB
TBB_PATH = /data/rveroy/src/tbb44_20160526oss
TBB_LIBPATH = /data/rveroy/src/tbb44_20160526oss/lib/intel64/gcc4.4

JAVAC = $(JAVA_PATH)/bin/javac

# TODO: Shouldn't this be JAVA_OPTS? And where is this used? - RLV
JAVAOPTS = -cp .:$(ASMJAR)

# Use this if using system installation of TBB
# INCLUDES = -I$(JAVA_PATH)/include -I$(JAVA_PATH)/include/linux -I./java_crw_demo -I./agent_util -I./GraphAlgorithms -I./RootEnumerator

# Use this if you need to specify a custom location for TBB
INCLUDES = -I$(TBB_PATH)/include -I$(JAVA_PATH)/include -I$(JAVA_PATH)/include/linux -I./java_crw_demo -I./agent_util \
#     -I./GraphAlgorithms -I./RootEnumerator

# CXXFLAGS = $(INCLUDES) -O2 -fPIC   -L./GraphAlgorithms -L./RootEnumerator -I./GraphAlgorithms -I./gzstream -W -Wall -Werror -std=c++11
CXXFLAGS = $(INCLUDES) -O2 -fPIC   -L./GraphAlgorithms -L./RootEnumerator -I./GraphAlgorithms -I./gzstream -W -Wall -Werror -std=c++11
CFLAGS = $(INCLUDES) -O2 -fPIC

javaclasses = ElephantTracks.class 'ElephantTracks$$1.class'

# END USER SETTINGS
################################################################################################
