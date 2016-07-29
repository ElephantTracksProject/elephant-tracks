#!/bin/bash -xv

export ET_JAVA_PATH=/opt/ibm/java-x86_64-60

export ASMJAR=/home/sguyer/asm-3.3/lib/all/asm-all-3.3.jar

export ETDIR=/home/sguyer/elephant-tracks

export ETJAR=${ETDIR}/elephantTracksRewriter.jar

export LD_LIBRARY_PATH=$ETDIR:


ET_CLASS_PATH=${ETDIR}:${ETJAR}:${ASMJAR} 


export JAVA_PATH=$ET_JAVA_PATH

scratchdir="scratch-${OFILE}"

#echo "-Xdump=IPOW"                               >> "${OFILE}.args"
#echo "-XdumpOrig=${OFILE}-orig"                  >> "${OFILE}.args"
#echo "-XdumpPre=${OFILE}-pre"                    >> "${OFILE}.args"
#echo "-XdumpWrap=${OFILE}-wrap"                  >> "${OFILE}.args"
#echo "-XdumpInst=${OFILE}-inst"                  >> "${OFILE}.args"
#echo '-Xdump+org/python/core/BytecodeLoader$Loader'   >> "${OFILE}.args"
#echo '-Xdump+org/python/core/PyFunctionTable'         >> "${OFILE}.args"
#echo '-Xdump+posixpath$py'                            >> "${OFILE}.args"

exec_string="${JAVA_PATH}/bin/java -classpath . -Xbootclasspath/a:${ETDIR} -agentlib:ElephantTracks=:=@traceFile=>(gzip >${OFILE}.trace.alloc.gz)@namesFile=${OFILE}.names@-XechoArgs@optionsFile=${HOME}/.elephantTracks@-Xdefault=OW $*"

echo $exec_string

eval $exec_string

