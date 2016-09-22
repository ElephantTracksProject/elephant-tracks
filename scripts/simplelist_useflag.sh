#!/bin/bash -xv

# export ET_JAVA_PATH=/usr/lib/jvm/java-1.7.0-oracle.x86_64
# export ET_JAVA_PATH=/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.39.x86_64
export ET_JAVA_PATH=/data/rveroy/src/ibm-java-x86_64-60

export ASMJAR=/data/rveroy/src/JAR/asm-all-3.3.jar
export ETDIR=/data/rveroy/pulsrc/ETRUN/lib
export ETJAR=${ETDIR}/elephantTracksRewriter.jar
export SIMPLEJAR=$ETDIR/../simplelist-0.1-all.jar

# Sequential add and then use the node
export SEQ_USE_TRACEFILE=simplelist-seq-use.trace
export SEQ_USE_NAMEFILE=simplelist-seq-use.names

# Sequential add and then don't use the node
export SEQ_DONTUSE_TRACEFILE=simplelist-seq-dontuse.trace
export SEQ_DONTUSE_NAMEFILE=simplelist-seq-dontuse.names

# TODO: # Random add and then sequential delete
# TODO: export RAND_SEQDELFILE=simplelist-rand-seqdel.trace
# TODO: export RAND_SEQDEL_NAMEFILE=simplelist-rand-seqdel.names
# TODO: # Random add and then delete all at end
# TODO: export RAND_ENDDELFILE=simplelist-rand-enddel.trace
# TODO: export RAND_ENDDEL_NAMEFILE=simplelist-rand-enddel.names

export LD_LIBRARY_PATH=$ETDIR:$LD_LIBRARY_PATH

ET_CLASS_PATH=${ETDIR}:${ETJAR}:${ASMJAR} 

export JAVA_PATH=$ET_JAVA_PATH


echo "====[ 1: SEQ - USE ]===================================================="
# Case 1: Sequential create/ then use the removed object
exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${SEQ_USE_TRACEFILE}@namesFile=${SEQ_USE_NAMEFILE}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${SIMPLEJAR} 1 1 seq use node"

echo $exec_string
eval $exec_string

if [ -f ${SEQ_USE_TRACEFILE}".bz2" ]
then
    rm -vf ${SEQ_USE_TRACEFILE}".bz2"
fi
echo "bzipping..."
bzip2 -1 ${SEQ_USE_TRACEFILE} &

# echo "====[ 2: SEQ - DONT USE ]==============================================="
# Case 2: Sequential create/ then ignore the removed object
exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${SEQ_DONTUSE_TRACEFILE}@namesFile=${SEQ_DONTUSE_NAMEFILE}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${SIMPLEJAR} 1 1 seq dontuse node"

echo $exec_string
eval $exec_string

if [ -f ${SEQ_DONTUSE_TRACEFILE}".bz2" ]
then
    rm -vf ${SEQ_DONTUSE_TRACEFILE}".bz2"
fi
echo "bzipping..."
bzip2 -1 ${SEQ_DONTUSE_TRACEFILE} &


# TODO: echo "====[ 3: RANDOM - SEQ DEL ]============================================="
# TODO: # Case 3: Random create/ Sequential (one by one) delete
# TODO: exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${RAND_SEQDELFILE}@namesFile=${RAND_SEQDEL_NAMEFILE}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${SIMPLEJAR} 5 1 random seqdel node"
# TODO: 
# TODO: echo $exec_string
# TODO: eval $exec_string
# TODO: 
# TODO: if [ -f ${RAND_SEQDELFILE}".bz2" ]
# TODO: then
# TODO:     rm -vf ${RAND_SEQDELFILE}".bz2"
# TODO: fi
# TODO: echo "bzipping..."
# TODO: bzip2 -1 ${RAND_SEQDELFILE} &
# TODO: 
# TODO: echo "====[ 4: RANDOM - ATEND ]==============================================="
# TODO: # Case 4: Random create/ At end delete
# TODO: exec_string="LD_LIBRARY_PATH=${ETDIR}:${LD_LIBRARY_PATH} CLASSPATH=.:${ASMJAR}:${ETDIR}:${ETDIR}/*  ${JAVA_PATH}/bin/java  -DETJAR=${ETCLASSJAR}  -classpath .:${ASMJAR}:${ETDIR}  -Xbootclasspath/a:${ETDIR}  -agentlib:ElephantTracks=:=@traceFile=${RAND_ENDDELFILE}@namesFile=${RAND_ENDDEL_NAMEFILE}@-XechoArgs@optionsFile=./etconfig@-Xdefault=OW -jar ${SIMPLEJAR} 5 1 random atend node"
# TODO: 
# TODO: echo $exec_string
# TODO: eval $exec_string
# TODO: 
# TODO: if [ -f ${RAND_ENDDELFILE}".bz2" ]
# TODO: then
# TODO:     rm -vf ${RAND_ENDDELFILE}".bz2"
# TODO: fi
# TODO: echo "bzipping..."
# TODO: bzip2 -1 ${RAND_ENDDELFILE}
# TODO: echo "Done."
