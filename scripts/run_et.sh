#!/bin/bash

# Author: Nathan P. Ricci
# This script is a simple wrapper to run a java program and trace it
# with elephant tracks. It is meant to be an example, and will need to be
# customized to one's own environment.
#
# usage:
# run_et.sh -o <base_name> <whatever commands you would normally pass to java>
#
# If -o is given, the trace will be stored in base_name.trace.gz,
# and the names in base_name.names.
# If -o is not given, the trace will be in 'elephant_tracks.<current date and time>.trace.gz
# and the names will be in 'elephant_tracks.<current data and time>.trace.gz
# Note that this script relies on gzip being installed.

echo $1

if [ "$1" = -o ]
then
  echo "Got here"
  base_file_name=$2
  shift 2
else
  base_file_name=elephant_tracks.`date | tr " " "_"`
fi

echo $*


export ET_JAVA_PATH=/home/vhalros/ibm/java-x86_64-60

export ASMJAR=/home/vhalros/workspace/asm-all-3.3.3.jar

export ETDIR=/home/vhalros/elephantTracks

export ETJAR=${ETDIR}/elephantTracksRewriter.jar

export LD_LIBRARY_PATH=$ETDIR:


ET_CLASS_PATH=${ETDIR}:${ETJAR}:${ASMJAR} 


export JAVA_PATH=$ET_JAVA_PATH


#echo "-Xdump=IPOW"                               >> "${OFILE}.args"
#echo "-XdumpOrig=${OFILE}-orig"                  >> "${OFILE}.args"
#echo "-XdumpPre=${OFILE}-pre"                    >> "${OFILE}.args"
#echo "-XdumpWrap=${OFILE}-wrap"                  >> "${OFILE}.args"
#echo "-XdumpInst=${OFILE}-inst"                  >> "${OFILE}.args"
#echo '-Xdump+org/python/core/BytecodeLoader$Loader'   >> "${OFILE}.args"
#echo '-Xdump+org/python/core/PyFunctionTable'         >> "${OFILE}.args"
#echo '-Xdump+posixpath$py'                            >> "${OFILE}.args"

exec_string="${JAVA_PATH}/bin/java -classpath . -Xbootclasspath/a:${ETDIR} -agentlib:ElephantTracks=:=@traceFile=>(gzip >${base_file_name}.trace.gz)@namesFile=${base_file_name}.names@-XechoArgs@optionsFile=${HOME}/.elephantTracks@-Xdefault=OW $*"

echo $exec_string

eval $exec_string
