package classReWriter;

import org.objectweb.asm.*;
import org.objectweb.asm.commons.*;
import java.util.HashMap;
import java.util.Map;

public class ETInlinerAdapter extends ClassAdapter {
	
  public final Map<String,Integer> methodMaxLocals = new HashMap<String,Integer>();

  public ETInlinerAdapter (ClassVisitor cv) {
    super(cv);
  }
	
  @Override
  public void visit (int version, int access, String name, String signature,
                     String superName, String[] interfaces ){
    cv.visit(ClassReWriter.outputVersion, access, name, signature, superName, interfaces);
  }

  @Override 
  public MethodVisitor visitMethod (int access, String name, String desc,
                                    String signature, String[] exceptions) {
    MethodVisitor writer = super.visitMethod(access, name, desc, signature, exceptions);
    final String key = name + desc;
    return new JSRInlinerAdapter(writer, access, name, desc, signature, exceptions) {
        public void visitMaxs (int maxStack, int maxLocals) {
          methodMaxLocals.put(key, maxLocals);
          super.visitMaxs(maxStack, maxLocals);
        }
      };
  }

}

// Local Variables:
// mode:Java
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
