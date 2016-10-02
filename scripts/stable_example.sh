#!/bin/bash -xv

# export ET_JAVA_PATH=/usr/lib/jvm/java-1.7.0-oracle.x86_64
# export ET_JAVA_PATH=/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.39.x86_64
export ET_JAVA_PATH=/data/rveroy/src/ibm-java-x86_64-60

export ASMJAR=/data/rveroy/src/JAR/asm-all-3.3.jar
export ETDIR=/data/rveroy/pulsrc/ETRUN/lib
export ETJAR=${ETDIR}/elephantTracksRewriter.jar
export STATICJAR=$ETDIR/../stable_example-0.1-all.jar

# Sequential add and then use the node
export STABLE_EXAMPLE_TRACEFILE=stable_example.trace
export STABLE_EXAMPLE_NAMEFILE=stable_example.names

export LD_LIBRARY_PATH=$ETDIR:$LD_LIBRARY_PATH

ET_CLASS_PATH=${ETDIR}:${ETJAR}:${ASMJAR} 

export JAVA_PATH=$ET_JAVA_PATH


echo "====[ 1: STATICVAR - USE ]===================================================="
# Case 1: Use the global static variable
exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${STABLE_EXAMPLE_TRACEFILE}@namesFile=${STABLE_EXAMPLE_NAMEFILE}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${STATICJAR} 1"

echo $exec_string
eval $exec_string

if [ -f ${STABLE_EXAMPLE_TRACEFILE}".bz2" ]
then
    rm -vf ${STABLE_EXAMPLE_TRACEFILE}".bz2"
fi
echo "bzipping..."
bzip2 -1 ${STABLE_EXAMPLE_TRACEFILE} &

