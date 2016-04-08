package testPrograms;

public class Test{

	private int a = 0;

	public Test(){

	}

	public int fooMethod(){
		System.out.println("a: " + a);
		return a++;	
	}

	public static void main(String argv[]){
		Test t = new Test();
		for(int i = 0; i < 500; i++){
			someMethod();
			String a = "42";
			a.contains("4");
			t.fooMethod();
		}
		
		t.equals(t) ;
	
	}
	
	public static void someMethod(){
		//System.out.println("Some method called!");
	}

}
