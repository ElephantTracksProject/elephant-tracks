package classReWriter;

import org.objectweb.asm.*;
import java.io.*;
import java.lang.System;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import classReWriter.ClassReWriter.SendKind;
import static classReWriter.ClassReWriter.SendKind.*;

public class ETAdapter extends ClassAdapter implements Opcodes {
	
  /**
   * the ASM name for this class, such as "java/lang/String"
   */
  private String className;

  /**
   * the unique id assigned to this class
   */
  private int classId;

    /**
   * flag settings to use if -x but no string given;
   * meanings are:
   * @ a place holder - no flags, but experimental indicated
   * C - emit counts calls (number of bytecodes executed, number of ref/non-ref heap read/writes)
   */
  private static final String defaultXFlags = "";

  /**
   * flag settings to use if -x is not given
   */
  private static final String defaultNoXFlags = defaultXFlags;

  /**
   * parsed form of the experimental flags passed
   */
  private Set<String> expFlags;

  /**
   * stream for outputting names information
   */
  private PrintStream nameStream;

  /**
   * stream for outputting field information to agent
   */
  private DataOutputStream dataOut;

  /**
   * the last unique class number assigned
   */
  private static int lastClassId = 0;

  /**
   * table giving the unique class number for each class
   */
  static final Map<String,Integer> classIds = new HashMap<String,Integer>();

  /**
   * should we process only certain methods?
   */
  private boolean selective;

  /**
   * looks up the unique id of a class, assigning a new one if necessary
   * @param className a String giving the name of the class whose id is desired
   * @param expectedOld a boolean that is true iff we expect this to be already assigned
   * @param nameStream a PrintStream to receive a description, as necessary
   * @param isInterface a boolean indicating that this is an interface, not a class
   * @return an int giving that class's unique id, which is assigned now if the
   * class has not been seen before
   */
  public static int getClassId (String className, boolean expectedOld,
                                PrintStream nameStream, boolean isInterface) {
    if (className == null) {
      // superclass of Object
      return 0;
    }
    Integer id = classIds.get(className);
    if (id == null) {
      id = ++lastClassId;
      classIds.put(className, id);
      if (expectedOld) {
        // record the information we have
        nameStream.printf("%c 0x%x %s%n", (isInterface ? 'I' : 'C'), id, className);
      }
    }
    return id;
  }

  /**
   * look up the id of a class;
   * an instance method so we get get nameStream for outputting information
   */
  public int getClassId (String className) {
    return getClassId(className, true, nameStream, false);
  }

  /**
   * maps an internal class name, such as "java/lang/String"
   * to a map giving the unique number assigned to each of its fields
   */
  static final Map<String, Map<String, Integer>> fieldIdTables =
    new HashMap<String, Map<String, Integer>> ();

  /**
   * last field id assigned
   */
  private static int lastFieldId = 0;

  /**
   * assign a unique id to this field, identified by its owning class
   * and its Java field name
   * @param owningClass a String giving the owning class, as in "java/lang/String"
   * @param fieldName a String giving the Java name of the field, as in "foo"
   * @param desc a String giving the type descriptor of the field
   * @param isStatic a boolean that is true iff this is a static field
   * @param nameStream a PrintStream to receive information about the new field
   * @result an int giving the unique number we have assigned to this field
   */
  static int newFieldId (String owningClass, String fieldName, String desc, boolean isStatic,
                         PrintStream nameStream, DataOutputStream dataOut) {
    Map<String, Integer> fieldIds = fieldIdTables.get(owningClass);
    if (fieldIds == null) {
      fieldIds = new HashMap<String, Integer>();
      fieldIdTables.put(owningClass, fieldIds);
    }
    Integer id = fieldIds.get(fieldName);
    if (id == null) {
      id = ++lastFieldId;
      fieldIds.put(fieldName, id);
      String instance = (isStatic ? "S" : "I");
      int classId = getClassId(owningClass, true, nameStream, false);
      String fieldRec = String.format("F %s 0x%x %s 0x%x %s %s%n",
                                      instance, id, fieldName, classId, owningClass, desc);
      nameStream.printf("%s", fieldRec);
      try {
        dataOut.writeByte(FieldInfo.ordinal());  // field info record type is 3
        dataOut.writeShort(0);       // effectively extends writeUTF's length to a 4 byte int
        dataOut.writeUTF(fieldRec);
      } catch (IOException exc) {
        System.err.printf("Problem writing to pipe to agent: " + exc);
        System.exit(1);
      }
    } else {
      // System.err.printf("Duplicate definition of field %s, class %s%n", fieldName, owningClass);
    }
    return id;
  }

  /**
   * look up field id, assigning a new one if necessary;
   * is an instance method to have access to nameStream
   * @param owningClass a String giving the class to search for the field
   * @param fieldName a String giving the name of the field
   * @param desc a String giving the field's type descriptor
   * @param isStatic a boolean indicating whether this is a static field
   * @return an int giving the unique id assigned to this field
   */
  int getFieldId (String owningClass, String fieldName, String desc, boolean isStatic) {
    Map<String, Integer> fieldIds = fieldIdTables.get(owningClass);
    Integer id;
    if (fieldIds == null || (id = fieldIds.get(fieldName)) == null) {
      return newFieldId(owningClass, fieldName, desc, isStatic, nameStream, dataOut);
    }
    return id;
  }

  private Map<String,Integer> methodMaxLocals;

  public ETAdapter (ClassVisitor cv,
		    String xFlags,
                    PrintStream nameStream,
                    DataOutputStream dataOut,
                    boolean selective,
		     Map<String,Integer> methodMaxLocals) {
    super(cv);
    this.nameStream      = nameStream;
    this.dataOut         = dataOut;
    this.expFlags        = prepareXFlags(xFlags);
    this.selective       = selective;
    this.methodMaxLocals = methodMaxLocals;
  }
	
  /**
   * parse experimental flags string
   * @param xFlags a String with the experimental flags as passed
   * @result a Set<String> giving the parsed flags
   */
  private Set<String> prepareXFlags (String xFlags) {
    if (xFlags == null) {
      xFlags = defaultNoXFlags;
    } else if (xFlags.length() == 0) {
      xFlags = defaultXFlags;
    }

    Set<String> flags = new HashSet<String>();

    // For now this is quite simple: each character forms one flag string
    for (Character c : xFlags.toCharArray()) {
      flags.add(c.toString());
    }

    return flags;
  }

  /**
   * format class ids of superinterfaces for printing;
   * assigns ids as necessary
   * @param interfaces an arry of String giving the neams of the interfaces
   * @result formatted list of their ids
   */
  private String superinterfaceIds (String[] interfaces) {
    String result = "";
    for (String intf : interfaces) {
      result += String.format(" I:0x%x", getClassId(intf, true, nameStream, true));
    }
    return result;
  }

  @Override
  public void visit (int version, int access, String name, String signature,
                     String superName, String[] interfaces ){
    if (selective) {
      ClassReWriter.startNewClass();
    }

    cv.visit(ClassReWriter.outputVersion, access, name, signature, superName, interfaces);

    boolean isInterface = ((access & ACC_INTERFACE) != 0);

    className = name;
    classId = getClassId(className, false, nameStream, isInterface);

    if (isInterface) {
      // interface: no harm in assigning a number
      // helpful to have a line to bracket the method lines
      String ids = superinterfaceIds(interfaces);
      nameStream.printf("I 0x%x %s%s%n", classId, className, ids);
    } else {
      // class, not interface
      int superId = getClassId(superName);
      String ids = superinterfaceIds(interfaces);
      String classRec = String.format("C 0x%x %s 0x%x%s%n", classId, className, superId, ids);
      nameStream.printf("%s", classRec);
      try {
        dataOut.writeByte(ClassInfo.ordinal());
        dataOut.writeShort(0);       // effectively extend writeUTF's byte count to a 4 byte int
        dataOut.writeUTF(classRec);
      } catch (IOException exc) {
        System.err.printf("Problem writing to pipe to agent: " + exc);
        System.exit(1);
      }
    }
    nameStream.flush();
  }

  @Override
  public FieldVisitor visitField (final int access, final String name, final String desc,
                                  final String signature, final Object value) {

    newFieldId(className, name, desc, (access & ACC_STATIC) != 0, nameStream, dataOut);

    return super.visitField(access, name, desc, signature, value);
  }

  @Override 
  public MethodVisitor visitMethod (int access, String name, String desc,
                                    String signature, String[] exceptions) {
		
    if (className.equals("ElephantTracks")) {
      MethodVisitor writer = cv.visitMethod(access, name, desc, signature, exceptions);
      return new ETETMethodAdapter(access, className, name, desc, writer, nameStream);
    }

    if (skipMethod(access, name, desc, signature, exceptions)) {
      ClassReWriter.verbosePrintln("Skipping method name:" + className + "#" + name + " " + desc);
      return cv.visitMethod(access, name, desc, signature, exceptions);
    }

    boolean omit = (selective && ClassReWriter.isMethodSpecial(name + desc));
    if (omit) {
      System.err.println("Not instrumenting " + className + " " + name + desc);
      return cv.visitMethod(access, name, desc, signature, exceptions);
    }

    ClassReWriter.verbosePrintln("Instrumenting method name: " + name + " " + desc);
    MethodVisitor writer = cv.visitMethod(access, name, desc, signature, exceptions);
    int maxLocals = ((access & (ACC_NATIVE|ACC_ABSTRACT)) != 0) ? 0 : methodMaxLocals.get(name + desc);
    return new ETOptMethodAdapter (access, className, name, desc, writer,
                                     expFlags, nameStream, maxLocals, this);
  }

  @Override
  public void visitEnd () {
    // end-of-class (output for interfaces as well)
    nameStream.printf("E 0x%x %s%n", classId, className);
    cv.visitEnd();
  }

  private boolean skipMethod(int access, String name, String desc,
                             String signature, String[] exceptions){
    return false;
  }

}

// Local Variables:
// mode:Java
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
