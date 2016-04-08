package nullReWriter;

import org.objectweb.asm.MethodAdapter;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.commons.AdviceAdapter;

public class NullMethodAdapter extends AdviceAdapter {
	
	
	
	public NullMethodAdapter(int access, String name, String desc, MethodVisitor mv){
		super(mv, access, name, desc);
	}
	
	
}
