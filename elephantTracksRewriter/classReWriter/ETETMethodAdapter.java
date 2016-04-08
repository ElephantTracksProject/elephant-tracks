package classReWriter;

import org.objectweb.asm.*;
import java.io.*;

public class ETETMethodAdapter extends MethodAdapter implements Opcodes {
	
  private final String methodName;

  public ETETMethodAdapter (int access, String className, String name, String desc,
                            MethodVisitor mv, PrintStream nameStream) {
    super(mv);

    String instance = ((access & ACC_STATIC) == 0 ? "I" : "S");

    String isNative = ((access & ACC_NATIVE) == 0 ? "" : "N");

    int methodId = ClassReWriter.getNextMethodId();
    int classId = ETAdapter.getClassId(className, true, nameStream, false);
    // last arg above is possibly bogus, but we should always have seen the class/interface
    // before we visit a method, so this should be ok

    nameStream.printf("N 0x%x 0x%x %s %s %s %s%n",
                      methodId, classId, className, name, desc, instance + isNative);

    methodName = name;
  }

  private static boolean okCall (String owner, String name, String desc) {
    if        (owner.equals("ElephantTracks")) {
      return true;
    } else if (owner.equals("java/lang/Object") && name.equals("<init>") && desc.equals("()V")) {
      return true;
    }
    return false;
  }

  private static boolean knownName (String owner, String name, String desc) {
    if (owner.equals("java/lang/Thread") && name.equals("currentThread") &&
        desc.equals("()Ljava/lang/Thread;")) {
      return true;
    }
    if (owner.equals("java/lang/System") && name.equals("arraycopy") &&
        desc.equals("(Ljava/lang/Object;ILjava/lang/Object;II)V")) {
      return true;
    }
    return false;
  }

  private static boolean okContaining (String containing) {
    if (containing.startsWith("liveHook")) {
      return true;
    }
    if (containing.startsWith("deathHook")) {
      return true;
    }
    return false;
  }

  private static String mappedName (String owner, String name, String desc, String containing) {
    if (okCall(owner, name, desc)) {
      return name;
    }
    if (knownName(owner, name, desc)) {
      return ETNativeWrapper.nativePrefix + name;
    }
    String wrappedName = ETNativeWrapper.wrappedName(owner, name+desc);
    if (!wrappedName.equals("")) {
      return wrappedName;
    }
    if (!okContaining(containing)) {
      System.err.println("WARNING: ElephantTracks has a non-wrapped non-local call to " +
                         owner + "." + name + desc);
    }
    return name;
  }

  @Override
  public void visitMethodInsn(int opcode, String owner, String name, String desc){
    name = mappedName(owner, name, desc, methodName);
    super.visitMethodInsn(opcode, owner, name, desc);
  }

}

// Local Variables:
// mode:Java
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
