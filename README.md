## Building

1. Set the variables as approprriate in Makefile.inc
	* `ASMJAR` must point at the jar for ASM
	* Java path should point to a jre
	* `INSTALL_DIR` is where the resulting binaries will end up.

2. `make`
3. `make install` 
        
Now the binaries will be wherever you set `INSTALL_DIR`
 
The program trace will be in `OutputFile`. Doing the tracing will slow down program execution considerably; on the order of 500-1000 times slower.

## Running Elephant Tracks
1. Make sure that `INSTALL_DIR` is in the `LD_LIBRARY_PATH`
2. Or, be sure that `libElephantTracks.so` and `libHashGraph.so` are in a directory in the `LD_LIBRARY_PATH`
3. There are two ways in the original documentation on how to run:
    * Run java with the following paramters:

```
java -Xbootclasspath/a:<INSTALL_DIR> -agentlib:ElephantTracks=javaPath=<path to Java>`:outputFile=<OutputFile>:classReWriter=<INSTALL_DIR>/elephantTracksRewriter.jar
```

    * Or this way:

```
java -classpath <other-paths>:$ASMJAR -Xbootclasspath/a:$INSTALL_DIR \ -agentlib:ElephantTracks=<ElephantTracks Options>
```
    * Options are represented as name=value pairs, and may be given on the command like so:

```
-agentlib:ElephantTracks=:=name1=value1@name2=value2@...@nameN=valueN
```

    * Note that JVMs often impose an (undocumented) limit on the length of the
      command strings passed to a JVMTI agent such as Elephant Tracks, and will
      silently truncate if this is limit is exceeded. For this reason, it is
      recommend that infrequently changing options are stored in an option
      file, and specify that option file on the command line (see below).

    * This is a path to a file containing options (the same as may be passed on the command line), one option per line.
```
optionsFile=<option file>
```

    * This is the path ElephantTracks will use to start its own Java process
      (not the one running your program). It must include `INSTALL_DIR`,
      `INSTALL_DIR/elephantTracksRewriter.jar`, and the `asm-3.3 jar` file.
```
classPath=<path>
```
    * This is the path to the actual java binary, not merely the directory it
      is in.
```
javaPath=<path to java executable>
```

    * The file in which to output the names information (see below)
```
namesFile=<file name>
```

    * The file in which to output the trace. You may also redirect trace output
      to a shell command with this syntax:
```
traceFile=<file name>
# or
traceFile=>(shell command)
```

```
bufferSize=<number>
```
    * How many records to hold in Elephant Tracks’ internal buffer; larger
      values generally give better performance, but will use more memory
      (approximately 40 bytes per record).

