package nullReWriter;

import org.objectweb.asm.*;

public class NullAdapter extends ClassAdapter {
	
	public NullAdapter(ClassVisitor cv){
		super(cv);
	}

	@Override 
	public MethodVisitor visitMethod(int access, String name, String desc, String signature, String[] exceptions ){
		
		return null;
	}

}
