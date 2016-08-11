#!/bin/bash -xv

# export ET_JAVA_PATH=/usr/lib/jvm/java-1.7.0-oracle.x86_64
# export ET_JAVA_PATH=/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.39.x86_64
export ET_JAVA_PATH=/data/rveroy/src/ibm-java-x86_64-60

export ASMJAR=/data/rveroy/src/JAR/asm-all-3.3.jar
export ETDIR=/data/rveroy/pulsrc/ETRUN/lib
export ETJAR=${ETDIR}/elephantTracksRewriter.jar
export SIMPLEJAR=$ETDIR/../simplelist-0.1-all.jar
# Sequential add and then sequential delete
export TRACEFILE1=simplelist-seq-seqdel.trace
export NAMEFILE1=simplelist-seq-seqdel.names
# Random add and then sequential delete
export TRACEFILE2=simplelist-rand-seqdel.trace
export NAMEFILE2=simplelist-rand-seqdel.names
# Sequential add and then delete all at end
export TRACEFILE3=simplelist-seq-enddel.trace
export NAMEFILE3=simplelist-seq-enddel.names
# Random add and then delete all at end
export TRACEFILE4=simplelist-rand-enddel.trace
export NAMEFILE4=simplelist-rand-enddel.names

export LD_LIBRARY_PATH=$ETDIR:$LD_LIBRARY_PATH

ET_CLASS_PATH=${ETDIR}:${ETJAR}:${ASMJAR} 

export JAVA_PATH=$ET_JAVA_PATH

scratchdir="scratch-${OFILE}"

#exec_string="LD_LIBRARY_PATH=${LD_LIBRARY_PATH} ${JAVA_PATH}/bin/java -classpath .:${ASMJAR}: -Xbootclasspath/a:${ETDIR} -agentlib:ElephantTracks=:=@traceFile=>(gzip >test.trace.alloc.gz)@namesFile=test.names@-XechoArgs@optionsFile=etconfig@-Xdefault=OW $*"

# exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}  ${JAVA_PATH}/bin/java  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=javaPath=${JAVA_PATH}:traceFile=test.trace:namesFile=test.names:classReWriter=${ETDIR}/elephantTracksRewriter.jar  $*"

# exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}  ${JAVA_PATH}/bin/java  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=javaPath=${JAVA_PATH}:traceFile=test.trace:namesFile=test.names:classReWriter=${ETDIR}/elephantTracksRewriter.jar  $*"
 
# exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETDIR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=test.trace.gz@namesFile=test.names@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW $*"

echo "====[ 1: SEQ - SEQ DEL ]================================================"
# Case 1: Sequential create/ Sequential (one by one) delete
exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${TRACEFILE1}@namesFile=${NAMEFILE1}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${SIMPLEJAR} 1000 1 seq seqdel node"

echo $exec_string
eval $exec_string

if [ -f ${TRACEFILE1}".bz2" ]
then
    rm -vf ${TRACEFILE1}".bz2"
fi
echo "bzipping..."
bzip2 -1 ${TRACEFILE1} &

echo "====[ 2: RANDOM - SEQ DEL ]============================================="
# Case 2: Random create/ Sequential (one by one) delete
exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${TRACEFILE2}@namesFile=${NAMEFILE2}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${SIMPLEJAR} 1000 1 random seqdel node"

echo $exec_string
eval $exec_string

if [ -f ${TRACEFILE2}".bz2" ]
then
    rm -vf ${TRACEFILE2}".bz2"
fi
echo "bzipping..."
bzip2 -1 ${TRACEFILE2} &

echo "====[ 3: SEQ - ATEND ]=================================================="
# Case 3: Sequential create/ At end delete
exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${TRACEFILE3}@namesFile=${NAMEFILE3}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${SIMPLEJAR} 1000 1 seq atend node"

echo $exec_string
eval $exec_string

if [ -f ${TRACEFILE3}".bz2" ]
then
    rm -vf ${TRACEFILE3}".bz2"
fi
echo "bzipping..."
bzip2 -1 ${TRACEFILE3} &

echo "====[ 4: RANDOM - ATEND ]==============================================="
# Case 3: Sequential create/ At end delete
exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${TRACEFILE4}@namesFile=${NAMEFILE4}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${SIMPLEJAR} 1000 1 random atend node"

echo $exec_string
eval $exec_string

if [ -f ${TRACEFILE4}".bz2" ]
then
    rm -vf ${TRACEFILE4}".bz2"
fi
echo "bzipping..."
bzip2 -1 ${TRACEFILE4} &
