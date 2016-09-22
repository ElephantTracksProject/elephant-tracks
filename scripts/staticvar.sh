#!/bin/bash -xv

# export ET_JAVA_PATH=/usr/lib/jvm/java-1.7.0-oracle.x86_64
# export ET_JAVA_PATH=/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.39.x86_64
export ET_JAVA_PATH=/data/rveroy/src/ibm-java-x86_64-60

export ASMJAR=/data/rveroy/src/JAR/asm-all-3.3.jar
export ETDIR=/data/rveroy/pulsrc/ETRUN/lib
export ETJAR=${ETDIR}/elephantTracksRewriter.jar
export STATICJAR=$ETDIR/../staticvar-0.1-all.jar

# Sequential add and then use the node
export STATIC_USE_TRACEFILE=staticvar-use.trace
export STATIC_USE_NAMEFILE=staticvar-use.names

# Sequential add and then don't use the node
export STATIC_DONTUSE_TRACEFILE=staticvar-dontuse.trace
export STATIC_DONTUSE_NAMEFILE=staticvar-dontuse.names

export LD_LIBRARY_PATH=$ETDIR:$LD_LIBRARY_PATH

ET_CLASS_PATH=${ETDIR}:${ETJAR}:${ASMJAR} 

export JAVA_PATH=$ET_JAVA_PATH


echo "====[ 1: STATICVAR - USE ]===================================================="
# Case 1: Use the global static variable
exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${STATIC_USE_TRACEFILE}@namesFile=${STATIC_USE_NAMEFILE}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${STATICJAR} 10 use"

echo $exec_string
eval $exec_string

if [ -f ${STATIC_USE_TRACEFILE}".bz2" ]
then
    rm -vf ${STATIC_USE_TRACEFILE}".bz2"
fi
echo "bzipping..."
bzip2 -1 ${STATIC_USE_TRACEFILE} &

echo "====[ 2: STATICVAR - DONT USE ]==============================================="
# Case 2: Ignore the global static variable 
exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${STATIC_DONTUSE_TRACEFILE}@namesFile=${STATIC_DONTUSE_NAMEFILE}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${STATICJAR} 1 dontuse"

echo $exec_string
eval $exec_string

if [ -f ${STATIC_DONTUSE_TRACEFILE}".bz2" ]
then
    rm -vf ${STATIC_DONTUSE_TRACEFILE}".bz2"
fi
echo "bzipping..."
bzip2 -1 ${STATIC_DONTUSE_TRACEFILE} &

