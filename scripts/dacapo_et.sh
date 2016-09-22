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
export BATIK=batik.trace
export BATIK_NAMES=batik.names
export SUNFLOW=sunflow.trace
export SUNFLOW_NAMES=sunflow.names
export XALAN=xalan.trace
export XALAN_NAMES=xalan.names

export LD_LIBRARY_PATH=$ETDIR:$LD_LIBRARY_PATH

ET_CLASS_PATH=${ETDIR}:${ETJAR}:${ASMJAR} 

export JAVA_PATH=$ET_JAVA_PATH

echo "====[ SUNFLOW ]==========================================================="
exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${SUNFLOW}@namesFile=${SUNFLOW_NAMES}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${DACAPOJAR} batik"

echo $exec_string
eval $exec_string

if [ -f ${SUNFLOW}".bz2" ]
then
    rm -vf ${SUNFLOW}".bz2"
fi
echo "bzipping..."
bzip2 -1 ${SUNFLOW} &

echo "====[ AVRORA ]=========================================================="
exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${AVRORA}@namesFile=${AVRORA_NAMES}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${DACAPOJAR} avrora"

echo $exec_string
eval $exec_string

if [ -f ${AVRORA}".bz2" ]
then
    rm -vf ${AVRORA}".bz2"
fi
echo "bzipping..."
bzip2 -1 ${AVRORA} &

echo "====[ BATIK ]==========================================================="
exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${BATIK}@namesFile=${BATIK_NAMES}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${DACAPOJAR} batik"

echo $exec_string
eval $exec_string

if [ -f ${BATIK}".bz2" ]
then
    rm -vf ${BATIK}".bz2"
fi
echo "bzipping..."
bzip2 -1 ${BATIK} &

