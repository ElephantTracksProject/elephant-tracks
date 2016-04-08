package classReWriter;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.PrintStream;
import org.objectweb.asm.*;
import org.objectweb.asm.commons.*;
import org.objectweb.asm.util.*;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import static classReWriter.ETAdapter.*;

public class ClassReWriter implements Opcodes /* for -l output */ {

  public static final int MIN_VERSION = V1_5;  // minimum version to create
  public static final int MAX_VERSION = V1_5;  // maximum version to create

  private static String className;     // name of this class
  private static int originalVersion;  // JVM version of the original class bytes
  public  static int outputVersion;

  private static Map<String,String> argMap;
  private static int methodIdCounter = 0;
  private static PrintStream nameStream = null; //For outputting the method ID->Name mapping.
  private static boolean verbose = false;
	
  private static Set<String> includedClasses = new HashSet<String>();
  private static Set<String> excludedClasses = new HashSet<String>();
  private static Set<String> includedMethods = new HashSet<String>();
  private static Set<String> excludedMethods = new HashSet<String>();

  private static Set<String> dumpedClasses = new HashSet<String>();
  private static Set<String> nodumpClasses = new HashSet<String>();

  private static boolean firstDumpSeen = false;
  private static boolean lastDumpSeen  = false;

  private static boolean firstClassSeen = false;
  private static boolean lastClassSeen  = false;

  private static boolean firstMethodSeen;
  private static boolean lastMethodSeen;

  private static boolean omittedMethods = false;

  // private static boolean outputTransformed = true;

  private static ClassLoader loader;

  enum RecvKind {
    Zero,  // to skip value 0
    OrigClassFile,
    SynthClassInfo,
    SynthFieldInfo,
  }

  enum SendKind {
    Zero,  // to skip value 0
    InstClassFile,
    ClassInfo,
    FieldInfo,
    Ack,
  }

  public static int getNextMethodId() {
    methodIdCounter++;
    return methodIdCounter;
  }

  public static void verbosePrintln (String s) {
    if (verbose) {
      System.err.println(s);
    }
  }

  public static void main (String[] argv) throws IOException {

    loader = Thread.currentThread().getContextClassLoader();

    argMap = parseCommandLineArgs(argv);

    verbose = argMap.containsKey("-v");

    if (argMap.containsKey("-XerrorOut")) {
      String outName = argMap.get("-XerrorOut");
      FileOutputStream errOut = new FileOutputStream(outName);
      PrintStream ps = new PrintStream(errOut);
      System.setErr(ps);
    }

    if (argMap.containsKey("-XechoArgs")) {
      System.err.print("args:");
      for (String arg : argv) {
        System.err.print(' ');
        System.err.print(arg);
      }
      System.err.println();
    }

    if (argMap.containsKey("-Xprejars")) {
      String prejars = argMap.get("-Xprejars");
      if (argMap.containsKey("-Xextrajars")) {
        prejars = prejars + ":" + argMap.get("-Xextrajars");
      }
      String[] jarnames = prejars.split(":");
      loader = ETClassLoader.create(jarnames);
      if (verbose) {
        System.err.println("Installed prejars:");
        for (String jar : jarnames) {
          System.err.printf("    %s%n", jar);
        }
      }
    }

    if (argMap.containsKey("-l")) {
      // a special invocation just to check paths through the class loader
      String className = argMap.get("-l");
      if (className.endsWith(".class")) {
        className = className.substring(0, className.length()-".class".length());
      }
      className = className.replace(".", "/") + ".class";
      InputStream is = loader.getResourceAsStream(className);
      if (is == null) {
        System.err.printf("Could not get input stream for %s%n", className);
      } else {
        try {
          ClassReader cr = new ClassReader(is);
          System.err.printf("Class name: %s%n", cr.getClassName());
          System.err.printf("Superclass: %s%n", cr.getSuperName());
          for (String intf : cr.getInterfaces()) {
            System.err.printf("Implements: %s%n", intf);
          }
          System.err.print("Access flags:");
          int acc = cr.getAccess();

          if ((acc & ACC_PRIVATE) != 0) {
            System.err.print(" private");
          }
          if ((acc & ACC_PROTECTED) != 0) {
            System.err.print(" protected");
          }
          if ((acc & ACC_PUBLIC) != 0) {
            System.err.print(" public");
          }
          if ((acc & (ACC_PRIVATE|ACC_PROTECTED|ACC_PUBLIC)) == 0) {
            System.err.print(" (package)");
          }

          if ((acc & ACC_ABSTRACT) != 0) {
            System.err.print(" abstract");
          }

          if ((acc & ACC_FINAL) != 0) {
            System.err.print(" final");
          }

          if ((acc & ACC_ENUM) != 0) {
            System.err.print(" enum");
          }
          if ((acc & ACC_INTERFACE) != 0) {
            System.err.print(" interface");
          } else {
            System.err.print(" class");
          }
          System.err.println();
        } catch (IOException e) {
          System.err.println("Problem reading class " + className + ": " + e);
        }
      }
      return;
    }

    if (argMap.containsKey("-n")) {
      File nameFile = new File(argMap.get("-n"));
      nameStream = new PrintStream(new FileOutputStream(nameFile) );
    } else {
      nameStream = new PrintStream(System.out);
    }

    if (!argMap.containsKey("-Xdefault")) {
      argMap.put("-Xdefault", "OW");
    }

    if (!argMap.containsKey("-Xspecial")) {
      argMap.put("-Xspecial", argMap.get("-Xdefault"));
    }

    if (!argMap.containsKey("-XclassFirst")) {
      argMap.put("-XclassFirst", "");
    } else if (argMap.get("-XclassFirst").equals("") || argMap.containsKey("XclassLast")) {
      firstClassSeen = true;
    }

    if (!argMap.containsKey("-XclassLast")) {
      argMap.put("-XclassLast", "");
    }

    if (!argMap.containsKey("-XdumpFirst")) {
      argMap.put("-XdumpFirst", "");
    } else if (argMap.get("-XdumpFirst").equals("") || argMap.containsKey("XdumpLast")) {
      firstDumpSeen = true;
    }

    if (!argMap.containsKey("-XdumpLast")) {
      argMap.put("-XdumpLast", "");
    }

    for (Map.Entry<String,String> e : argMap.entrySet()) {
      String key = e.getKey();
      if      (key.startsWith("-Xclass+" )) { includedClasses.add(key.substring("-Xclass+" .length())); }
      else if (key.startsWith("-Xclass-" )) { excludedClasses.add(key.substring("-Xclass-" .length())); }
      else if (key.startsWith("-Xmethod+")) { includedMethods.add(key.substring("-Xmethod+".length())); }
      else if (key.startsWith("-Xmethod-")) { excludedMethods.add(key.substring("-Xmethod-".length())); }
      else if (key.startsWith("-Xdump+"  )) { dumpedClasses  .add(key.substring("-Xdump+"  .length())); }
      else if (key.startsWith("-Xdump-"  )) { nodumpClasses  .add(key.substring("-Xdump-"  .length())); }
    }

    if (!argMap.containsKey("-Xdump")) {
      argMap.put("-Xdump", "");
    }

    while (instrumentOneClass()) {}
  }
	
  private static Map<String,String> parseCommandLineArgs (String[] args) {
    Map<String,String> argMap = new HashMap<String,String>();
		
    for (int i = 0; i < args.length; i++) {
      String arg = args[i];
      // first, fixed flags that take a separate argument
      if      (arg.equals("-c"  )) { argMap.put("-c"  , args[++i]); }
      else if (arg.equals("-n"  )) { argMap.put("-n"  , args[++i]); }
      else if (arg.equals("-l"  )) { argMap.put("-l"  , args[++i]); }
      else if (arg.equals(""    )) { /* do nothing */               }
      // second, fixed flags that take no argument
      else if (arg.equals("-v"  )) { argMap.put("-v"  , null);      }
      else if (arg.equals("-opt")) { argMap.put("-opt", null);      }
      // third, prefix flags with embedded argument
      else if (arg.startsWith("-x")) { argMap.put("-x", arg.substring(2)); }
      else if (arg.startsWith("-X")) {
        String key = arg;
        String val = "";
        int pos = arg.indexOf('=');
        if (pos >= 0) {
          key = arg.substring(0, pos);
          val = arg.substring(pos+1);
        }
        argMap.put(key, val);
      } else {
        // last: unrecognized flag/argument
        System.err.println("ClassReWriter: unrecognized argument ignored: " + arg);
      }		
    }
    return argMap;
  }
	
  private static boolean instrumentOneClass () {
    byte [] originalClassBytes;

    try {
      while (true) {
        // -- read a record: kind byte, int length, then 'length' bytes
        // However: in the -c case, just a class file, no kind, etc.
        RecvKind kind;
        int size_in;
        DataInputStream dataIn;

        if (!argMap.containsKey("-c")) {
          dataIn = new DataInputStream(System.in);

          // read the record kind: must match ExternalClassInstrumenter::SendKind
          byte b = dataIn.readByte();
          RecvKind[] values = RecvKind.values();
          if (b < 1 || b >= values.length) {
            verbosePrintln("ClassReWriter: unknown record kind from agent");
            return false;
          }
          kind = values[b];
        
          // read in the size, a four-byte int;
          // must be compatible with ExternalClassInstrumenter::writeUntilDone
          try {
            size_in = dataIn.readInt();
          } catch (IOException e) {
            verbosePrintln("ClassReWriter: input stream closed");
            return false;
          }

        } else {
          // We will read in the class specified by the -c parameter
          File c  = new File(argMap.get("-c"));
          kind = RecvKind.OrigClassFile;
          size_in = (int)c.length();
          dataIn = new DataInputStream(new FileInputStream(c));
        }

        // -- Read in the class bytes
        originalClassBytes = new byte[size_in];
        dataIn.readFully(originalClassBytes);
        switch (kind) {
        case OrigClassFile:  // the "usual" case -- just keep going
          break;
        case SynthClassInfo:
        case SynthFieldInfo: {
          String record = new String(originalClassBytes);
          nameStream.printf("%s%n", record);
          String[] parts = record.split("[ \\n]");
          switch (kind) {
          case SynthClassInfo: {
            // C 0x<id> name 0x<super id>
            if (parts.length == 4) {
              String id = parts[1];
              if (id.indexOf("0x") == 0) {
                id = id.substring(2);
              }
              Integer classId = Integer.valueOf(id, 16);
              classIds.put(parts[2], classId);
            } else {
              System.err.printf("Bad synthetic class record: %s%n", record);
            }
            break;
          }
          case SynthFieldInfo: {
            // F I/S 0x<id> name 0x<classId> class desc
            if (parts.length == 7) {
              String instanceStatic = parts[1];
              String fid = parts[2];
              if (fid.indexOf("0x") == 0) {
                fid = fid.substring(2);
              }
              Integer fieldId = Integer.valueOf(fid, 16);
              String fname = parts[3];
              String cid = parts[4];
              if (cid.indexOf("0x") == 0) {
                cid = cid.substring(2);
              }
              Integer classId = Integer.valueOf(cid, 16);
              String cname = parts[5];
              String desc = parts[6];
              Map<String, Integer> fieldNameToId = fieldIdTables.get(cname);
              if (fieldNameToId == null) {
                fieldNameToId = new HashMap<String, Integer>();
                fieldIdTables.put(cname, fieldNameToId);
              }
              fieldNameToId.put(fname, fieldId);
            } else {
              System.err.printf("Bad synthetic field records: %s%n", record);
            }
            break;
          }
          }
          DataOutputStream dataOut = new DataOutputStream(System.out);
          dataOut.writeByte(SendKind.Ack.ordinal());
          dataOut.writeInt(0);
          dataOut.flush();
          continue;
        }
        default:
          verbosePrintln("ClassReWriter: unknown record kind from agent");
          return false;
        }
        break;  // leave the while loop, whose purpose is to allow other records to come in
      }
    } catch (java.io.IOException e) {
      //System.err.println(e.getStackTrace().toString());
      verbosePrintln("ClassReWriter: could not read in class bytes: " + e.getMessage());
      return false;
    }

    // System.err.println("(4j) ClassReWriter: read in " + originalClassBytes.length + " bytes");

    /*
      int count = 0;
      System.err.println("Java Read Data:");
      for (byte b:originalClassBytes ){
      System.err.println("  " + (int)count + " : " + (int)b);
      count++;
      }
    */

    // -- Transform the class

    boolean optimizing   = argMap.containsKey("-opt");
    boolean experimental = argMap.containsKey("-x");
    String expFlags = (experimental ? argMap.get("-x") : null);

    byte[] preprocessedClassBytes = originalClassBytes;

    boolean specialClass = (firstClassSeen & !lastClassSeen);
    boolean dumpClass    = (firstDumpSeen  & !lastDumpSeen );

    String dumpsDesired = argMap.get("-Xdump");

    className = null;  // insure that code below sets this
    ClassReader ocr = new ClassReader(originalClassBytes);
    ClassVisitor getInfoVisitor = new EmptyVisitor() {
        @Override
        public void visit (int version, int access, String name,
                           String signature, String superName, String[] interfaces) {
          className = name;
          originalVersion = version;
        }
      };
    ocr.accept(getInfoVisitor, 0);
    verbosePrintln("Instrumenting: " + className);
    outputVersion = Math.min(MAX_VERSION, Math.max(MIN_VERSION, originalVersion));

    boolean computeFrames = ((originalVersion <= V1_5) && (outputVersion > V1_5));
    int baseWriterOptions = ClassWriter.COMPUTE_FRAMES;
    int instWriterOptions = (computeFrames ? ClassWriter.COMPUTE_FRAMES : ClassWriter.COMPUTE_MAXS);

    {
      // note this class relative to special treatment and dump processing
      if (className.equals(argMap.get("-XclassFirst"))) {
        firstClassSeen = true; specialClass = true;
      }
      if (className.equals(argMap.get("-XclassLast"))) {
        lastClassSeen = true;
      }
      if (excludedClasses.contains(className)) {
        specialClass = false;
      }
      if (includedClasses.contains(className)) {
        specialClass = true;
      }
      if (specialClass) {
        System.err.println("Special handling for class: " + className);
      }

      if (className.equals(argMap.get("-XdumpFirst"))) {
        firstDumpSeen = true; dumpClass = true;
      }
      if (className.equals(argMap.get("-XdumpLast"))) {
        lastDumpSeen = true;
      }
      if (nodumpClasses.contains(className)) {
        dumpClass = false;
      }
      if (dumpedClasses.contains(className)) {
        dumpClass = true;
      }
    }
    
    ETInlinerAdapter eia;
    {
      // pre-process to eliminate JSR instructions

      ClassReader cr = new ClassReader(originalClassBytes);
      ClassWriter cw = new ETClassWriter(baseWriterOptions, loader);
      eia = new ETInlinerAdapter(cw);
      cr.accept(eia, ClassReader.EXPAND_FRAMES);
      preprocessedClassBytes = cw.toByteArray();

      if (dumpClass) {
        if (dumpsDesired.contains("I")) {
          try {
            File f = formDumpFileName(argMap, "-XdumpOrig", className);
            if (f != null) {
              DataOutputStream dout = new DataOutputStream(new FileOutputStream(f));
              dout.write(originalClassBytes, 0, originalClassBytes.length);
              dout.close();
            }
          } catch (IOException exc) {
            System.err.printf("Problem writing dump of original %s%n", className);
          }
          
        }
        if (dumpsDesired.contains("P")) {
          try {
            File f = formDumpFileName(argMap, "-XdumpPre", className);
            if (f != null) {
              DataOutputStream dout = new DataOutputStream(new FileOutputStream(f));
              dout.write(preprocessedClassBytes, 0, preprocessedClassBytes.length);
              dout.close();
            }
          } catch (IOException exc) {
            System.err.printf("Problem writing dump of preprocessed %s%n", className);
          }
        }
      }
    }

    String processing = argMap.get(specialClass ? "-Xspecial" : "-Xdefault");

    byte [] wrappedClassBytes = preprocessedClassBytes;
    String wrappedKind = "preprocessed";

    if (!className.equals("ElephantTracks")) {
      ClassReader cr = new ClassReader(preprocessedClassBytes);
      ClassWriter cw = new ETClassWriter(baseWriterOptions, loader);
      ETNativeWrapper enw = new ETNativeWrapper(cw, eia.methodMaxLocals);
      cr.accept(enw, ClassReader.EXPAND_FRAMES);
      wrappedClassBytes = cw.toByteArray();
      wrappedKind = "wrapped";

      if (dumpClass) {
        if (dumpsDesired.contains("W")) {
          try {
            File f = formDumpFileName(argMap, "-XdumpWrap", className);
            if (f != null) {
              DataOutputStream dout = new DataOutputStream(new FileOutputStream(f));
              dout.write(wrappedClassBytes, 0, wrappedClassBytes.length);
              dout.close();
            }
          } catch (IOException exc) {
            System.err.printf("Problem writing dump of wrapped %s%n", className);
          }
        }
      }
    }

    byte [] instrumentedClassBytes = (processing.contains("W") ? wrappedClassBytes : preprocessedClassBytes);
    String instKind = (processing.contains("W") ? wrappedKind : "preprocessed");
    if (className.equals("ElephantTracks")) {
      instrumentedClassBytes = originalClassBytes;
      instKind = "original";
    }

    DataOutputStream dataOut = new DataOutputStream(System.out);

    {
      ClassReader cr = new ClassReader(instrumentedClassBytes);
      ClassWriter cw = new ETClassWriter(instWriterOptions, loader);
      ETAdapter adapter = new ETAdapter(cw, expFlags, nameStream, dataOut,
                                        specialClass, eia.methodMaxLocals);
      int crOptions = (optimizing ? (ClassReader.EXPAND_FRAMES | ClassReader.SKIP_DEBUG)
                                  : (ClassReader.EXPAND_FRAMES | ClassReader.SKIP_FRAMES));
      cr.accept(adapter, crOptions);
      instrumentedClassBytes = cw.toByteArray();
      instKind = "instrumented";

      if (dumpClass) {
        if (dumpsDesired.contains("O")) {
          try {
            File f = formDumpFileName(argMap, "-XdumpInst", className);
            if (f != null) {
              DataOutputStream dout = new DataOutputStream(new FileOutputStream(f));
              dout.write(instrumentedClassBytes, 0, instrumentedClassBytes.length);
              dout.close();
            }
          } catch (IOException exc) {
            System.err.printf("Problem writing dump of instrumented %s%n", className);
          }
        }
      }
    }

    if (className.equals("ElephantTracks")) {
      if (processing.contains("W")) {
        if (!processing.contains("O")) {
          // really need ElephantTracks to be processed, or we get infinite recursion
          wrappedClassBytes = instrumentedClassBytes;
        }
      } else {
        instrumentedClassBytes = originalClassBytes;
        instKind = "original";
      }
    }

    if (argMap.containsKey("-Xcheck")) {
      ClassWriter cw2 = new ETClassWriter(0, loader);
      CheckClassAdapter cca = new CheckClassAdapter(cw2);
      ClassReader cr2 = new ClassReader(instrumentedClassBytes);
      cr2.accept(cca, 0);
    }

    try {
      // -- Write the resulting class

      byte[] outputBytes = originalClassBytes;
      String outKind = "original";
      if      (processing.contains("O")) { outputBytes = instrumentedClassBytes; outKind = instKind;       }
      else if (processing.contains("W")) { outputBytes = wrappedClassBytes;      outKind = wrappedKind;    }
      else if (processing.contains("P")) { outputBytes = preprocessedClassBytes; outKind = "preprocessed"; }

      if (omittedMethods) {
        // if class was processed selectively, always use instrumented version
        outputBytes = instrumentedClassBytes;
        outKind = instKind;
        omittedMethods = false;  // for next class
      }

      if (!argMap.containsKey("-c")) {
        dataOut.writeByte(SendKind.InstClassFile.ordinal());
        dataOut.writeInt(outputBytes.length);
      }
      dataOut.write(outputBytes, 0, outputBytes.length);
      dataOut.flush();
      verbosePrintln("Wrote " + outKind + " bytes");
    } catch (java.io.IOException e) {
      System.err.println(e.getStackTrace().toString());
      System.err.println("ClassReWriter: could not write out class bytes: " + e.getMessage());
      return false;
    }

    // System.err.println("(9j) ClassReWriter: Done");
    if (argMap.containsKey("-c")) {
      return false;
    } else {
      return true;
    }
  }

  private static File formDumpFileName (Map<String,String> argMap, String prefixKey, String className) {
    String prefix = argMap.get(prefixKey);
    if (prefix == null) {
      prefix = prefixKey.substring("-Xdump".length()).toLowerCase();
    }
    if (!prefix.endsWith("/")) {
      prefix += "/";
    }
    String fname = prefix + className + ".class";
    char sep = File.separatorChar;
    fname = fname.replace('/', sep);
    int pos = fname.lastIndexOf(sep);
    String dirname = fname.substring(0, pos);
    File dir = new File(dirname);
    if (!dir.exists()) {
      if (!dir.mkdirs()) {
        System.err.print("Could not create dump directory: " + dir);
        return null;
      }
    }
    return new File(fname);
  }

  static void startNewClass () {
    lastMethodSeen = false;
    if (!argMap.containsKey("-XmethodFirst")) {
      firstMethodSeen = argMap.containsKey("-XmethodLast");
    } else {
      firstMethodSeen = argMap.get("-XmethodFirst").equals("");
    }
    omittedMethods = false;
  }

  static boolean isMethodSpecial (String methodName) {
    if (methodName.equals(argMap.get("-XmethodFirst"))) {
      firstMethodSeen = true;
    }
    boolean special = (firstMethodSeen & !lastMethodSeen);
    if (methodName.equals(argMap.get("-XmethodLast"))) {
      lastMethodSeen = true;
    }
    if (excludedMethods.contains(methodName)) {
      special = false;
    }
    if (includedMethods.contains(methodName)) {
      special = true;
    }
    if (special) {
      omittedMethods = true;
    }
    return special;
  }

}

// Local Variables:
// mode:Java
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
