#!/bin/bash -xv

# export ET_JAVA_PATH=/usr/lib/jvm/java-1.7.0-oracle.x86_64
# export ET_JAVA_PATH=/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.39.x86_64
export ET_JAVA_PATH=/data/rveroy/src/ibm-java-x86_64-60

export ASMJAR=/data/rveroy/src/JAR/asm-all-3.3.jar
export ETDIR=/data/rveroy/pulsrc/ETRUN/lib
export ETJAR=${ETDIR}/elephantTracksRewriter.jar
export SIMPLEJAR=$ETDIR/../simplelist-0.1-all.jar
# Sequential add and then sequential delete
export SEQ_SEQFILE=simplelist-seq-seqdel.trace
export SEQ_SEQ_NAMEFILE=simplelist-seq-seqdel.names
# Sequential add and then delete all at end
export SEQ_ENDDELFILE=simplelist-seq-enddel.trace
export SEQ_ENDDEL_NAMEFILE=simplelist-seq-enddel.names
# Random add and then sequential delete
export RAND_SEQDELFILE=simplelist-rand-seqdel.trace
export RAND_SEQDEL_NAMEFILE=simplelist-rand-seqdel.names
# Random add and then delete all at end
export RAND_ENDDELFILE=simplelist-rand-enddel.trace
export RAND_ENDDEL_NAMEFILE=simplelist-rand-enddel.names

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
exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${SEQ_SEQFILE}@namesFile=${SEQ_SEQ_NAMEFILE}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${SIMPLEJAR} 5 1 seq seqdel node"

echo $exec_string
eval $exec_string

if [ -f ${SEQ_SEQFILE}".bz2" ]
then
    rm -vf ${SEQ_SEQFILE}".bz2"
fi
echo "bzipping..."
bzip2 -1 ${SEQ_SEQFILE} &

# echo "====[ 2: SEQ - ATEND ]=================================================="
# Case 2: Sequential create/ At end delete
exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${SEQ_ENDDELFILE}@namesFile=${SEQ_ENDDEL_NAMEFILE}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${SIMPLEJAR} 5 1 seq atend node"

echo $exec_string
eval $exec_string

if [ -f ${SEQ_ENDDELFILE}".bz2" ]
then
    rm -vf ${SEQ_ENDDELFILE}".bz2"
fi
echo "bzipping..."
bzip2 -1 ${SEQ_ENDDELFILE} &


echo "====[ 3: RANDOM - SEQ DEL ]============================================="
# Case 3: Random create/ Sequential (one by one) delete
exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${RAND_SEQDELFILE}@namesFile=${RAND_SEQDEL_NAMEFILE}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${SIMPLEJAR} 5 1 random seqdel node"

echo $exec_string
eval $exec_string

if [ -f ${RAND_SEQDELFILE}".bz2" ]
then
    rm -vf ${RAND_SEQDELFILE}".bz2"
fi
echo "bzipping..."
bzip2 -1 ${RAND_SEQDELFILE} &

echo "====[ 4: RANDOM - ATEND ]==============================================="
# Case 4: Random create/ At end delete
exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${RAND_ENDDELFILE}@namesFile=${RAND_ENDDEL_NAMEFILE}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${SIMPLEJAR} 5 1 random atend node"

echo $exec_string
eval $exec_string

if [ -f ${RAND_ENDDELFILE}".bz2" ]
then
    rm -vf ${RAND_ENDDELFILE}".bz2"
fi
echo "bzipping..."
bzip2 -1 ${RAND_ENDDELFILE}
echo "Done."
