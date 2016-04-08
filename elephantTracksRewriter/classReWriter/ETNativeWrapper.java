package classReWriter;

import org.objectweb.asm.*;
import java.util.*;

public class ETNativeWrapper extends ClassAdapter implements Opcodes {
	
  private static final String lastClassName = "";  // useful in debugging

  private static boolean lastClassSeen = false;

  public static final String nativePrefix = "$$ET$$";

  private static Map<String,Set<String>> wrapped = new HashMap<String,Set<String>>();

  private static void addWrapped (String className, String methodNameDesc) {
    if (!wrapped.containsKey(className)) {
      wrapped.put(className, new HashSet<String>());
    }
    Set<String> wrappedMethods = wrapped.get(className);
    wrappedMethods.add(methodNameDesc);
  }

  public static String wrappedName (String className, String methodNameDesc) {
    if (!wrapped.containsKey(className)) {
      return "";
    }
    Set<String> wrappedMethods = wrapped.get(className);
    if (!wrappedMethods.contains(methodNameDesc)) {
      return "";
    }
    String methodName = methodNameDesc.substring(0, methodNameDesc.indexOf('('));
    return nativePrefix + methodName;
  }

  private final Map<String,Integer> methodMaxLocals;

  public ETNativeWrapper (ClassVisitor cv, Map<String,Integer> methodMaxLocals) {
    super(cv);
    this.methodMaxLocals = methodMaxLocals;
  }
	
  protected String className;

  @Override
  public void visit (int version, int access, String name, String signature,
                     String supername, String[] interfaces) {
    this.className = name;
    if (name.equals(lastClassName)) {
      lastClassSeen = true;
    }
    super.visit(ClassReWriter.outputVersion, access, name, signature, supername, interfaces);
  }

  private boolean shouldWrap (String name, String desc) {
    // forced true cases first
    if (className.equals("java/lang/Thread") &&
        name.equals("currentThread") &&
        desc.equals("()Ljava/lang/Thread;")) {
      return true;
    }
    /* no longer needed
    if (className.equals("sun/misc/Unsafe") &&
        name.equals("getObject") &&
        desc.equals("(Ljava/lang/Object;J)Ljava/lang/Object;")) {
      // needed by putObject, etc.
      return true;
    }
    */

    // forced-false cases next
    if (className.equals("com/ibm/jvm/ClassLoader") &&
        name.equals("getClassContext")) {
      return false; // can't see how to wrap this without distorting the result
    }
    if (className.equals("java/lang/Class") &&
        name.equals("getStackClasses")) {
      return false; // can't see how to wrap this without distorting the result
    }
    if (className.equals("java/lang/Object")) {
      if (name.equals("getClass" )) return false;  // things break if this is wrapped
      if (name.equals("notify"   )) return false;  // things break if this is wrapped
      if (name.equals("notifyAll")) return false;  // things break if this is wrapped
      if (name.equals("wait"     )) return false;  // things break if this is wrapped
    }
    if (className.equals("java/lang/SecurityManager") &&
        name.equals("classDepth")) {
      return false; // can't see how to wrap this without distorting the result
    }
    if (className.equals("java/lang/SecurityManager") &&
        name.equals("currentClassLoader0")) {
      return false; // not certain, but wrapping this looks shaky
    }
    if (className.equals("java/lang/SecurityManager") &&
        name.equals("currentLoadedClass0")) {
      return false; // not certain, but wrapping this looks shaky
    }
    if (className.equals("java/lang/SecurityManager") &&
        name.equals("getClassContext")) {
      return false; // can't see how to wrap this without distorting the result
    }
    if (className.equals("java/lang/Thread") &&
        name.equals("getStackTraceImpl")) {
      return false; // can't see how to wrap this without distorting the result
    }
    if (className.equals("java/sql/DriverManager") &&
        name.equals("getCallerClassLoader")) {
      return false; // can't see how to wrap this without distorting the result
    }
    if (className.equals("java/util/ResourceBundle") &&
        name.equals("getClassContext")) {
      return false; // can't see how to wrap this without distorting the result
    }
    if (className.equals("sun/reflect/NativeConstructorAccessorImpl") &&
        name.equals("newInstance0")) {
      return false; // things break when this is wrapped
    }
    if (className.equals("sun/reflect/NativeMethodAccessorImpl") &&
        name.equals("invoke0x")) {
      return false; // things break when this is wrapped
    }

    if (lastClassSeen && !className.equals(lastClassName)) {
      return false;
    }

    // default
    return true;
  }

  private MethodVisitor wrapAndStartWrapperMethod (int access, String name, String desc,
                                                   String signature, String[] exceptions) {
    addWrapped(className, name + desc);
    MethodVisitor mv = super.visitMethod(access & ~(ACC_NATIVE|ACC_SYNCHRONIZED), name, desc, signature, exceptions);
    mv.visitCode();
    return mv;
  }

  // NOTE: We assume that the MethodWriter is invoked with ClassWriter.COMPUTE_MAXS
  //   or ClassWriter.COMPUTE_FRAMES set!

  private boolean speciallyWrapped (int access, String name, String desc, String signature, String[] exceptions) {
    String innerName = nativePrefix + name;
    if (className.equals("com/ibm/oti/vm/VM") &&
        name.equals("getStackClass") &&
        desc.equals("(I)Ljava/lang/Class;")) {
      MethodVisitor mv = wrapAndStartWrapperMethod(access, name, desc, signature, exceptions);
      pushArgs(mv, access, desc);
      // this adds one to the level of stack frame for com.ibm.oti.vm.VM.getStackClass,
      // to compensate for the level of call added by wrapping
      mv.visitInsn(ICONST_1);
      mv.visitInsn(IADD);
      mv.visitMethodInsn(INVOKESTATIC, className, innerName, desc);
      mv.visitInsn(ARETURN);
      doMethodEnd(mv, access, name, desc, 0);
      return true;
    }
    if (className.equals("com/ibm/oti/vm/VM") &&
        name.equals("getStackClassLoader") &&
        desc.equals("(I)Ljava/lang/ClassLoader;")) {
      MethodVisitor mv = wrapAndStartWrapperMethod(access, name, desc, signature, exceptions);
      pushArgs(mv, access, desc);
      // this adds one to the level of stack frame for com.ibm.oti.vm.VM.ClassLoader.getStackClassLoader,
      // to compensate for the level of call added by wrapping
      mv.visitInsn(ICONST_1);
      mv.visitInsn(IADD);
      mv.visitMethodInsn(INVOKESTATIC, className, innerName, desc);
      mv.visitInsn(ARETURN);
      doMethodEnd(mv, access, name, desc, 0);
      return true;
    }
    if (className.equals("java/lang/Class") &&
        name.equals("getStackClass") &&
        desc.equals("(I)Ljava/lang/Class;")) {
      MethodVisitor mv = wrapAndStartWrapperMethod(access, name, desc, signature, exceptions);
      pushArgs(mv, access, desc);
      // this adds one to the level of stack frame for java.lang.Class.getStackClass,
      // to compensate for the level of call added by wrapping
      mv.visitInsn(ICONST_1);
      mv.visitInsn(IADD);
      mv.visitMethodInsn(INVOKESTATIC, className, innerName, desc);
      mv.visitInsn(ARETURN);
      doMethodEnd(mv, access, name, desc, 0);
      return true;
    }
    if (className.equals("java/lang/ClassLoader") &&
        name.equals("getStackClassLoader") &&
        desc.equals("(I)Ljava/lang/ClassLoader;")) {
      MethodVisitor mv = wrapAndStartWrapperMethod(access, name, desc, signature, exceptions);
      pushArgs(mv, access, desc);
      // this adds one to the level of stack frame for java.lang.ClassLoader.getStackClassLoader,
      // to compensate for the level of call added by wrapping
      mv.visitInsn(ICONST_1);
      mv.visitInsn(IADD);
      mv.visitMethodInsn(INVOKESTATIC, className, innerName, desc);
      mv.visitInsn(ARETURN);
      doMethodEnd(mv, access, name, desc, 0);
      return true;
    }
    if (className.equals("java/lang/ref/Reference") &&
        name.equals("initReferenceImpl") &&
        desc.equals("(Ljava/lang/Object;)V")) {
      MethodVisitor mv = wrapAndStartWrapperMethod(access, name, desc, signature, exceptions);
      pushArgs(mv, access, desc);
      mv.visitInsn(DUP2);
      mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "referentUpdated",
                         "(Ljava/lang/Object;Ljava/lang/Object;)V");
      mv.visitMethodInsn(INVOKEVIRTUAL, className, innerName, desc);
      mv.visitInsn(RETURN);
      doMethodEnd(mv, access, name, desc, 0);
      return true;
    }
    if (className.equals("java/security/AccessController") &&
        name.equals("getProtectionDomains") &&
        desc.equals("(I)[Ljava/lang/Object;")) {
      MethodVisitor mv = wrapAndStartWrapperMethod(access, name, desc, signature, exceptions);
      pushArgs(mv, access, desc);
      // this adds one to the level of stack frame for java.lang.ClassLoader.getStackClassLoader,
      // to compensate for the level of call added by wrapping
      mv.visitInsn(ICONST_1);
      mv.visitInsn(IADD);
      mv.visitMethodInsn(INVOKESTATIC, className, innerName, desc);
      mv.visitInsn(ARETURN);
      doMethodEnd(mv, access, name, desc, 0);
      return true;
    }
    if (className.equals("sun/misc/Unsafe")) {
      if (name.equals("allocateInstance") &&
          desc.equals("(Ljava/lang/Class;)Ljava/lang/Object;")) {
        // Object allocateInstance(Class cls[1])
        MethodVisitor mv = wrapAndStartWrapperMethod(access, name, desc, signature, exceptions);
        pushArgs(mv, access, desc);
        mv.visitMethodInsn(INVOKEVIRTUAL, className, innerName, desc);
        mv.visitInsn(DUP);
        mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "newobj", "(Ljava/lang/Object;)V");
        mv.visitInsn(ARETURN);
        doMethodEnd(mv, access, name, desc, 0);
        return true;
      }
      if (name.equals("compareAndSwapObject") &&
          desc.equals("(Ljava/lang/Object;JLjava/lang/Object;Ljava/lang/Object;)Z")) {
        // boolean compareAndSwapObject(Object o[1], long offset[2], Object expected[4], Object x[5])
        MethodVisitor mv = wrapAndStartWrapperMethod(access, name, desc, signature, exceptions);
        pushArgs(mv, access, desc);
        mv.visitMethodInsn(INVOKEVIRTUAL, className, innerName, desc);
        Label returnLabel = new Label();
        mv.visitInsn(DUP);
        mv.visitJumpInsn(IFEQ, returnLabel);
        mv.visitVarInsn(ALOAD, 1); // o
        mv.visitVarInsn(ALOAD, 5); // x
        mv.visitVarInsn(LLOAD, 2); // offset
        mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "pointerUpdated",
                           "(Ljava/lang/Object;Ljava/lang/Object;J)V");
        mv.visitLabel(returnLabel);
        mv.visitInsn(IRETURN);
        doMethodEnd(mv, access, name, desc, 0);
        return true;
      }
      if ((name.equals("arrayBaseOffset") || name.equals("arrayIndexScale")) &&
          desc.equals("(Ljava/lang/Class;)I")) {
        // int Unsafe.arrayXXX(Class)
        // Unsafe:0, Class:1
        MethodVisitor mv = wrapAndStartWrapperMethod(access, name, desc, signature, exceptions);
        pushArgs(mv, access, desc);
        mv.visitMethodInsn(INVOKEVIRTUAL, className, innerName, desc);
        mv.visitInsn(DUP);
        mv.visitVarInsn(ALOAD, 1);
        String etMethodName = "record" + Character.toUpperCase(name.charAt(0)) + name.substring(1);
        mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", etMethodName,
                           "(ILjava/lang/Class;)V");
        mv.visitInsn(IRETURN);
        doMethodEnd(mv, access, name, desc, 0);
        return true;
      }
      if ((name.equals("objectFieldOffset") || name.equals("staticFieldOffset")) &&
          desc.equals("(Ljava/lang/reflect/Field;)J")) {
        // long Unsafe.xxxFieldOffset(Field)
        // Unsafe:0, Field:1
        MethodVisitor mv = wrapAndStartWrapperMethod(access, name, desc, signature, exceptions);
        pushArgs(mv, access, desc);
        mv.visitMethodInsn(INVOKEVIRTUAL, className, innerName, desc);
        mv.visitInsn(DUP2);
        mv.visitVarInsn(ALOAD, 1);
        String etMethodName = "record" + Character.toUpperCase(name.charAt(0)) + name.substring(1);
        mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", etMethodName,
                           "(JLjava/lang/reflect/Field;)V");
        mv.visitInsn(LRETURN);
        doMethodEnd(mv, access, name, desc, 0);
        return true;
      }
      if (name.equals("staticFieldBase") &&
          desc.equals("(Ljava/lang/reflect/Field;)Ljava/lang/Object;")) {
        // Object Unsafe.staticFieldBase(Field)
        // Unsafe:0, Field:1
        MethodVisitor mv = wrapAndStartWrapperMethod(access, name, desc, signature, exceptions);
        pushArgs(mv, access, desc);
        mv.visitMethodInsn(INVOKEVIRTUAL, className, innerName, desc);
        mv.visitInsn(DUP);
        mv.visitVarInsn(ALOAD, 1);
        mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "recordStaticFieldBase",
                           "(Ljava/lang/Object;Ljava/lang/reflect/Field;)V");
        mv.visitInsn(ARETURN);
        doMethodEnd(mv, access, name, desc, 0);
        return true;
      }
      if ((name.equals("putObject") ||
           name.equals("putObjectVolatile") ||
           name.equals("putOrderedObject")) &&
          desc.equals("(Ljava/lang/Object;JLjava/lang/Object;)V")) {
        // void putObject        (Object o[1], long offset[2], Object x[4])
        // void putObjectVolatile(Object o[1], long offset[2], Object x[4])
        // void putOrderedObject (Object o[1], long offset[2], Object x[4])
        MethodVisitor mv = wrapAndStartWrapperMethod(access, name, desc, signature, exceptions);

        mv.visitVarInsn(ALOAD, 1);  // o
        mv.visitVarInsn(ALOAD, 4);  // x
        mv.visitVarInsn(LLOAD, 2);  // offset

        mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "pointerUpdated",
                           "(Ljava/lang/Object;Ljava/lang/Object;J)V");
        pushArgs(mv, access, desc);
        mv.visitMethodInsn(INVOKEVIRTUAL, className, innerName, desc);
        mv.visitInsn(RETURN);
        doMethodEnd(mv, access, name, desc, 0);
        return true;
      }
      return false;
    }
    if (className.equals("java/lang/System") &&
        name.equals("arraycopy") &&
        desc.equals("(Ljava/lang/Object;ILjava/lang/Object;II)V")) {
      MethodVisitor mv = wrapAndStartWrapperMethod(access, name, desc, signature, exceptions);
      pushArgs(mv, access, desc);
      mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "doSystemArraycopy", desc);
      mv.visitInsn(RETURN);
      doMethodEnd(mv, access, name, desc, 0);
      return true;
    }
    if (className.equals("sun/reflect/Reflection") &&
        name.equals("getCallerClass") &&
        desc.equals("(I)Ljava/lang/Class;")) {
      MethodVisitor mv = wrapAndStartWrapperMethod(access, name, desc, signature, exceptions);
      pushArgs(mv, access, desc);
      // this adds one to the level of stack frame for sun.reflect.Reflection.getCallerClass,
      // to compensate for the level of call added by wrapping
      mv.visitInsn(ICONST_1);
      mv.visitInsn(IADD);
      mv.visitMethodInsn(INVOKESTATIC, className, innerName, desc);
      mv.visitInsn(ARETURN);
      doMethodEnd(mv, access, name, desc, 0);
      return true;
    }
    return false;
  }

  @Override 
  public MethodVisitor visitMethod (int access, String name, String desc,
                                    String signature, String[] exceptions) {
    String plainName = name;
    String innerName = nativePrefix + name;
    if (speciallyWrapped(access, plainName, desc, signature, exceptions)) {
      name = innerName;
    } else if (((access & ACC_NATIVE) != 0) && shouldWrap(plainName, desc)) {
      name = innerName;
      MethodVisitor mv = wrapAndStartWrapperMethod(access, plainName, desc, signature, exceptions);
      pushArgs(mv, access, desc);
      int opcode = ((access & ACC_STATIC) == 0 ? INVOKEVIRTUAL : INVOKESTATIC);
      if (opcode == INVOKEVIRTUAL &&
          (access & ACC_PRIVATE) != 0) {
        opcode = INVOKESPECIAL;
      }
      mv.visitMethodInsn(opcode, className, innerName, desc);
      int pos = desc.indexOf(')');
      char first = desc.charAt(pos+1);
      switch (first) {
      case 'B':
      case 'C':
      case 'I':
      case 'S':
      case 'Z': mv.visitInsn(IRETURN); break;
      case 'D': mv.visitInsn(DRETURN); break;
      case 'F': mv.visitInsn(FRETURN); break;
      case 'J': mv.visitInsn(LRETURN); break;
      case 'L':
      case '[': mv.visitInsn(ARETURN); break;
      case 'V': mv.visitInsn(RETURN ); break;
      }
      doMethodEnd(mv, access, plainName, desc, 0);
    }
    return super.visitMethod(access, name, desc, signature, exceptions);
  }

  /**
   * Scans through the descriptor and pushes each argument of the methos
   * @param mv the MethodVisitor to use for writing instructions
   * @param access an int giving the access flags of the method
   * @param desc a String giving the descriptor of the method
   */
  protected void pushArgs (MethodVisitor mv, int access, String methodDesc) {
    int slotIndex = 0;
    if ((access & ACC_STATIC) == 0) {
      mv.visitVarInsn(ALOAD, 0);
      slotIndex += 1;
    }
    // start i at 1 to skip the left-paren '('
    scan: for (int i = 1; ; i++) { // note: runs to the right paren
      switch (methodDesc.charAt(i)) {
      case ')':
        break scan;

      case 'B':
      case 'C':
      case 'I':
      case 'S':
      case 'Z': mv.visitVarInsn(ILOAD, slotIndex); slotIndex += 1; continue;
      case 'F': mv.visitVarInsn(FLOAD, slotIndex); slotIndex += 1; continue;
      case 'D': mv.visitVarInsn(DLOAD, slotIndex); slotIndex += 2; continue;
      case 'J': mv.visitVarInsn(LLOAD, slotIndex); slotIndex += 2; continue;
      case 'L': mv.visitVarInsn(ALOAD, slotIndex); slotIndex += 1;
        while (methodDesc.charAt(i) != ';') { i++; }
        continue;
      case '[': mv.visitVarInsn(ALOAD, slotIndex); slotIndex += 1;
        // skip over all dimensions
        while (methodDesc.charAt(i) == '[') { i++; }
        // if an array of object type, skip over the object type
        if (methodDesc.charAt(i) == 'L') {
          while (methodDesc.charAt(i) != ';') { i++; }
        }
        continue;
      }
    }
  }

  protected void doMethodEnd (MethodVisitor mv, int access, String name, String methodDesc, int extras) {
    int numLocals = argsSize(access, methodDesc) + extras;
    mv.visitMaxs(0, numLocals);
    methodMaxLocals.put(name+methodDesc, numLocals);
  }

  /**
   * Scans through the descriptor and determines the size of the initial locals
   * @param access an int giving the access flags of the method
   * @param desc a String giving the descriptor of the method
   */
  protected int argsSize (int access, String methodDesc) {
    int slotIndex = 0;
    if ((access & ACC_STATIC) == 0) {
      slotIndex += 1;
    }
    // start i at 1 to skip the left-paren '('
    scan: for (int i = 1; ; i++) { // note: runs to the right paren
      switch (methodDesc.charAt(i)) {
      case ')':
        break scan;

      case 'B':
      case 'C':
      case 'F':
      case 'I':
      case 'S':
      case 'Z': slotIndex += 1; continue;
      case 'D':
      case 'J': slotIndex += 2; continue;
      case 'L': slotIndex += 1;
        while (methodDesc.charAt(i) != ';') { i++; }
        continue;
      case '[': slotIndex += 1;
        // skip over all dimensions
        while (methodDesc.charAt(i) == '[') { i++; }
        // if an array of object type, skip over the object type
        if (methodDesc.charAt(i) == 'L') {
          while (methodDesc.charAt(i) != ';') { i++; }
        }
        continue;
      }
    }
    return slotIndex;
  }

}

// Local Variables:
// mode:Java
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
