package testPrograms;



public class InstrumentedDoublyLinkedList<T> {

	private class Node<T>{
		public T data;
		public Node<T> next;
		public Node<T> prev;

		public Node(T data){
			this.data = data;
		}
	}

	private Node<T> frontEnd;
	private Node<T> backEnd;

	public InstrumentedDoublyLinkedList(){
		frontEnd = new Node<T>(null);
		backEnd = new Node<T>(null);

		frontEnd.next = backEnd;
		backEnd.prev = frontEnd;
	}

	public void append(T item){
		Node<T> node = new Node<T>(item);
	
		Node<T> prev = backEnd.prev;
		Node<T> next = backEnd;
		
		pointerMovedOff(prev.next);
		prev.next = node;
		
		pointerMovedOff(node.prev);
		node.prev = prev;

		next=backEnd;
		
		pointerMovedOff(backEnd.prev);
		backEnd.prev = node;
	}

	public void print(){
		Node<T> curr = frontEnd.next;
		
		while(curr.next != null){
			System.out.print(curr.data);
			System.out.print(" ");
			curr = curr.next;
		}	
			
	}
	
	public static void main(String[] argv){
		DoublyLinkedList<Integer> l;
		
		l = new DoublyLinkedList<Integer>();

		l.append(1);
		l.append(2);

		System.out.println();	
		
		
	
	}

	private static void pointerMovedOff(Object o){
		
	}

}
