package nullReWriter;

import java.io.IOException;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.Vector;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.commons.*;

public class NullReWriter {

	
	public static void main(String[] argv) throws IOException{
		ClassReader cr = null;
		byte[] originalClassBytes;
		byte[] instrumentedClassBytes;
		HashMap<String,String> argMap;

		
		argMap = parseCommandLineArgs(argv);

		if(! argMap.containsKey("-c")){
			Vector<Integer> incomingClass = new Vector<Integer>();
		
			int b;
			while(  (b = System.in.read()) != -1){
				incomingClass.add(b);
			}
		
			originalClassBytes = new byte[incomingClass.size()];
			for(int i = 0; i < incomingClass.size(); i++){
				originalClassBytes[i] = (incomingClass.get(i)).byteValue();
			}
		
		
			//For debugging purposes, lets make a copy of the class from stdin...
		
			try{
				//cr = new ClassReader(System.in);
				//cr = new ClassReader("java.lang.ref.Finalizer");
				cr = new ClassReader(originalClassBytes);
			}
			catch(Exception e){
				System.err.println(e.toString() + "\n");
				System.err.println("Error reading from System.in");
				System.exit(1);
			}
		}
		else{
			cr = new ClassReader(argMap.get("-c"));
		}

		
		ClassWriter cw = new ClassWriter(0);
		
		
		
		
		//MerlinVisitor mv = new MerlinVisitor();
		try{
			cr.accept(new NullAdapter(cw),0);
		}
		catch(java.lang.ArrayIndexOutOfBoundsException e){
			System.err.println(e.getStackTrace().toString());
			System.err.println("Caught Array index out of boudn exception" + e.getMessage());
			System.exit(2);
		}
		
		//merlinAdapter.visitEnd();
		
		instrumentedClassBytes = cw.toByteArray();

		writeClass(System.out, instrumentedClassBytes);
	
		
	}
	
	private static HashMap<String,String> parseCommandLineArgs(String[] args){
		HashMap<String,String> argMap = new HashMap<String,String>();
		
		for(int i =0; i < args.length; i++){
			if(args[i].equals( "-c")){
				argMap.put("-c", args[++i]);
			}
			
			else if(args[i].equals("-s")){
				argMap.put("-s", null);
			}
			else{
				//TODO: Throw some kinda exception...
			}
			
		}
		
		return argMap;
	}
	
	private static void writeClass(OutputStream s, byte[] classBytes){
		try{
			for(byte b: classBytes){
				s.write(b);
			}

			s.flush();
		}
		catch(Exception e ){
			
		}
	}
	
	
	
	
}
