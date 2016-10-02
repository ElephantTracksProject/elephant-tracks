#!/bin/bash -xv

# export ET_JAVA_PATH=/usr/lib/jvm/java-1.7.0-oracle.x86_64
# export ET_JAVA_PATH=/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.39.x86_64
export ET_JAVA_PATH=/data/rveroy/src/ibm-java-x86_64-60

export ASMJAR=/data/rveroy/src/JAR/asm-all-3.3.jar
export ETDIR=/data/rveroy/pulsrc/ETRUN/lib
export ETJAR=${ETDIR}/elephantTracksRewriter.jar
export DACAPOJAR=/data/rveroy/pulsrc/dacapo-9.12-bach.jar

# Sequential add and then sequential delete
export AVRORA=avrora.trace
export AVRORA_NAMES=avrora.names
export JYTHON=jython.trace
export JYTHON_NAMES=jython.names
export SUNFLOW=sunflow.trace
export SUNFLOW_NAMES=sunflow.names
export JYTHON=jython.trace
export JYTHON_NAMES=jython.names

export LD_LIBRARY_PATH=$ETDIR:$LD_LIBRARY_PATH

ET_CLASS_PATH=${ETDIR}:${ETJAR}:${ASMJAR} 

export JAVA_PATH=$ET_JAVA_PATH

echo "====[ JYTHON ]==========================================================="
# exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${ET_JAVA_PATH}/bin/java  -DETJAR=${ETJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${JYTHON}@namesFile=${JYTHON_NAMES}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${DACAPOJAR} jython"
exec_string="${ET_JAVA_PATH}/bin/java  -DETJAR=${ETJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${JYTHON}@namesFile=${JYTHON_NAMES}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${DACAPOJAR} jython"

echo $exec_string
eval $exec_string

if [ -f ${JYTHON}".bz2" ]
then
    rm -vf ${JYTHON}".bz2"
fi
echo "bzipping..."
bzip2 -1 ${JYTHON} &

