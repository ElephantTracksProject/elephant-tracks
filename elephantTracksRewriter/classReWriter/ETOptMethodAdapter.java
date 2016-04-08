package classReWriter;

import org.objectweb.asm.*;
import java.io.PrintStream;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.Vector;

public class ETOptMethodAdapter extends MethodAdapter implements Opcodes {
	
  // private static final boolean wrapAll = true;
  // private static int wrapOnly = 184; // 184
  // private static int seenSoFar = 0;
  // private static final String nowrapClass = "java/lang/ClassLoaderxxx";
  // private static final String nowrapMethod = "loadClass";
  // private static final String nowrapDesc = "(Ljava/lang/String;Z)Ljava/lang/Class;";

  private final ETAdapter classAdapter;

  private static final boolean TRACE_NEW_OBJECTS = false;

  /**
   * indicate whether we have seen an instruction yet
   */
  private boolean beforeFirstInsn = true;

  /**
   * indicates if we are in a method safe to instrument;
   * avoids inserting code in a constructor before the call
   * to the super's constructor, so as not to break JVM rules;
   * equivalently, indicates that onMethodEnter has been called
   */
  private boolean insideMethod = false;

  /**
   * indicates whether to timestamp arguments and this on entry
   */
  private static final boolean STACK_SCAN_ON_ENTRY = true; 
	
  /**
   * how many word/ref counter increments to do for a wide (long/double) item
   */
  private static final int COUNT_FOR_WIDES = 1;

  /**
   * name of the current method
   */
  protected final String methodName;

  /**
   * type descriptor of the current method
   */
  protected final String methodDesc;

  /**
   * name of the current class
   */
  protected final String className;

  /**
   * access (public, protected, etc.) of the current method
   */
  protected final int access;

  /**
   * the particular experimental features enabled
   */
  protected final Set<String> expFlags;

  /**
   * unique id that we have assigned to the current method
   */
  protected final int methodId;
	
  /**
   * maintains value numbers for the local variables
   */
  protected IntStack locals = new IntStack();

  /**
   * maintains value numbers for the working stack
   */
  protected IntStack stack = new IntStack();

  /**
   * next value number to use
   */
  protected int vnCounter = 0;

  /**
   * values timestamped since last reset
   */
  protected Set<Integer> stamped = new HashSet<Integer>();

  /**
   * a map for holding numbers we assigned to uninitialized (new) objects;
   * the keys are Label objects and the values are our internal numbers
   * that uniquely identify a NEW bytecode
   */
  private Map<Object, Integer> uninits = new HashMap<Object, Integer>();

  /**
   * a counter for assigning numbers to uninitialized objects, based
   * on the label at which they were allocated
   */
  private int uninitCounter = -2;

  /**
   * true iff we are processing an instance constructor (init method)
   */
  private boolean inConstructor;

  /**
   * turn off to avoid instrumenting a method
   */
  private boolean instrument = true;

  /**
   * maps the index of an uninit object (as seen in the stack model)
   * to the unique id of the site that generated it;
   * -1 always maps to -1, meaning "uninitialized this"
   */
  protected final Map<Integer, Integer> newSiteId = new HashMap<Integer, Integer>();

  /**
   * maps a Label at a NEW to the NEW's site number;
   * not every NEW has a Label right on it, but those NEWs
   * are not referred to by Frames so we will never look them up
   */
  protected final Map<Object, Integer> siteForLabel = new HashMap<Object, Integer>();

  /**
   * last new site id assigned
   */
  private static int lastNewSiteId = 0;

  /**
   * label for start of the method, if we are adding a wrapped exception handler
   */
  private Label startLabel;

  /**
   * label for end of the (original) method, if we are adding a wrapped exception handler
   */
  private Label endLabel;

  /**
   * most recent label seen
   */
  private Label lastLabel;

  /**
   * places where handlers start, to help us insert code there
   */
  private Set<Label> handlers = new HashSet<Label>();

  /**
   * for writing out information about each method, alloc site, etc.
   */
  private PrintStream nameStream;

  /**
   * next local variable that we can allocate
   */
  private int nextLocal;

  /**
   * local variable that we use for storing result of NEW so that the agent can access
   * it using GetLocalObject; initialized by the first NEW
   */
  private int localForNewObject = -1;

  private int instCount;
  private int heapReads;
  private int heapWrites;
  private int heapRefReads;
  private int heapRefWrites;

  private void resetCounts () {
    instCount     = 0;
    heapReads     = 0;
    heapWrites    = 0;
    heapRefReads  = 0;
    heapRefWrites = 0;
  }

  private void incrCounts (int insts, int hrs, int hws, int hrrs, int hrws) {
    if (haveXFlag("C")) {
      instCount     += insts;
      heapReads     += hrs  ;
      heapWrites    += hws  ;
      heapRefReads  += hrrs ;
      heapRefWrites += hrws ;
    }
  }

  private void incrCountsAndFlush (int insts, int hrs, int hws, int hrrs, int hrws) {
    incrCounts(insts, hrs, hws, hrrs, hrws);
    outputCounts();
    resetCounts();
  }

  private void flushCounts () {
    if ((instCount + heapReads + heapWrites + heapRefReads + heapRefWrites) > 0) {
      outputCounts();
      resetCounts();
    }
  }

  private final static int HEAP_BITS_SMALL = 6;
  private final static int INST_BITS_SMALL = 8;

  private final static int HEAP_BOUND_SMALL = 1 << HEAP_BITS_SMALL;
  private final static int INST_BOUND_SMALL = 1 << INST_BITS_SMALL;
  private final static int HEAP_BOUND_LARGE = 1 << (HEAP_BITS_SMALL << 1);
  private final static int INST_BOUND_LARGE = 1 << (INST_BITS_SMALL << 1);

  private static int bounded (int amount, int bound) {
    return (amount < bound) ? amount : bound-1;
  }

  private void outputCounts () {
    if ((!instrument) || !(haveXFlag("C"))) {
      return;
    }
    // System.err.printf("Counts: %d %d %d %d %d%n",
    //                   instCount, heapReads, heapWrites, heapRefReads, heapRefWrites);
    while ((heapReads     >= HEAP_BOUND_SMALL) ||
           (heapWrites    >= HEAP_BOUND_SMALL) ||
           (heapRefReads  >= HEAP_BOUND_SMALL) ||
           (heapRefWrites >= HEAP_BOUND_SMALL) ||
           (instCount     >= INST_BOUND_SMALL)) {
      int r  = bounded(heapReads    , HEAP_BOUND_LARGE);
      int w  = bounded(heapWrites   , HEAP_BOUND_LARGE);
      int rr = bounded(heapRefReads , HEAP_BOUND_LARGE);
      int rw = bounded(heapRefWrites, HEAP_BOUND_LARGE);
      int i  = bounded(instCount    , INST_BOUND_LARGE);
      long value = ((((long)r ) << ((INST_BITS_SMALL + 3 * HEAP_BITS_SMALL) << 1)) |
                    (((long)w ) << ((INST_BITS_SMALL + 2 * HEAP_BITS_SMALL) << 1)) |
                    (((long)rr) << ((INST_BITS_SMALL + 1 * HEAP_BITS_SMALL) << 1)) |
                    (((long)rw) << ((INST_BITS_SMALL + 0 * HEAP_BITS_SMALL) << 1)) |
                    ((long)i));
      mv.visitLdcInsn(new Long(value));
      mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "longCounts", "(J)V");
      heapReads     -= r;
      heapWrites    -= w;
      heapRefReads  -= rr;
      heapRefWrites -= rw;
      instCount     -= i;
    }
    if ((heapReads + heapWrites + heapRefReads + heapRefWrites + instCount) > 0) {
      int r  = bounded(heapReads    , HEAP_BOUND_SMALL);
      int w  = bounded(heapWrites   , HEAP_BOUND_SMALL);
      int rr = bounded(heapRefReads , HEAP_BOUND_SMALL);
      int rw = bounded(heapRefWrites, HEAP_BOUND_SMALL);
      int i  = bounded(instCount    , INST_BOUND_SMALL);
      int value = ((r  << (INST_BITS_SMALL + 3 * HEAP_BITS_SMALL)) |
                   (w  << (INST_BITS_SMALL + 2 * HEAP_BITS_SMALL)) |
                   (rr << (INST_BITS_SMALL + 1 * HEAP_BITS_SMALL)) |
                   (rw << (INST_BITS_SMALL + 0 * HEAP_BITS_SMALL)) |
                   i);
      mv.visitLdcInsn(new Integer(value));
      mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "counts", "(I)V");
    }
  }

  /**
   * checks whether a particular experimental flag is set;
   * we use this help method incase we want a richer data structure
   * later, such as a Map instead of a Set
   * @param flag a String giving the flag to check
   * @result a boolean, true iff the flag is set
   */
  protected boolean haveXFlag (String flag) {
    return expFlags.contains(flag);
  }

  /**
   * constructor to build an adapter for a particular class;
   * initializes fields, assigns a new unique id, and outputs the uid info
   */
  public ETOptMethodAdapter (int access, String className,
                             String name, String desc, MethodVisitor mv,
                             Set<String> expFlags, PrintStream nameStream,
                             int methodMaxLocals, ETAdapter classAdapter) {
    super(mv);
    // super(mv, access, name, desc);
    this.classAdapter = classAdapter;
    this.className    = className;
    this.methodName   = name;
    this.methodDesc   = desc;
    this.access       = access;
    this.expFlags     = expFlags;
    this.methodId     = ClassReWriter.getNextMethodId();
    String instance = ((access & ACC_STATIC) == 0 ? "I" : "S");
    if ((access & ACC_NATIVE) != 0) {
      instance += "N";
    }

    int classId = classAdapter.getClassId(className);

    this.nameStream = nameStream;

    nameStream.printf("N 0x%x 0x%x %s %s %s %s%n",
                      methodId, classId, className, methodName, methodDesc, instance);

    inConstructor = name.equals("<init>");
    this.nextLocal = methodMaxLocals;
  }

  /**
   * Looks through the descriptor to determine which parameter local var slots
   * contain object references; useful for timestamping them, for example
   * @param ignoreThis a boolean that when true means to skip "this"
   */
  protected Vector<Integer> findParamSlots (boolean ignoreThis) {
    Vector<Integer> slots = new Vector<Integer>();
    int slotIndex = 0;  // next local variable slot

    if (((access & ACC_STATIC) == 0)) {
      if (!ignoreThis) {
        // deal with 'this' for non-static methods
        slots.add(slotIndex); // timestamp here to show live after method entry
      }
      slotIndex++;
    }

    if (methodDesc.charAt(0) != '(') {
      // throw some exception?
      return slots;
    }
    // start i at 1 to skip the left-paren '('
    scan: for (int i = 1; ; i++) { // note: runs to the right paren
      switch (methodDesc.charAt(i)) {
      case ')':
        break scan;

      case 'D': case 'J':
        slotIndex += 2;
        continue;

      case 'L':
        slots.add(slotIndex++);
        while (methodDesc.charAt(i) != ';') { i++; }
        continue;

      case '[':
        slots.add(slotIndex++);
        // skip over all dimensions
        while (methodDesc.charAt(i) == '[') { i++; }
        // if an array of bject type, skip over the object type
        if (methodDesc.charAt(i) == 'L') {
          while (methodDesc.charAt(i) != ';') { i++; }
        }
        continue;

      default:
        slotIndex += 1;
        continue;
      }
    }
    return slots;
  }

  /**
   * Based on the given descriptor, does the given method return an object/array?
   */
  public boolean returnsObject (String desc){
    int closeIdx = desc.indexOf(')');
    char retKind = desc.charAt(closeIdx + 1);
    return (retKind == 'L' || retKind == '[');
  }

  /**
   * get the unique id of the allocating site for a value number
   * that decribes an uninitialized object; returns -1 for uninitialized this
   * @param vn an int giving the value number, which must be for an uninit object
   * @return an int giving the unique allocating site, or -1 for "uninitialized this"
   */
  protected int getNewSiteId (int vn) {
    Integer vni = vn;
    if (!newSiteId.containsKey(vni)) {
      // return -100;
      throw new IllegalArgumentException("arg was " + vn);
    }
    return newSiteId.get(vni);
  }

  /**
   * Instruments Object.<init> to note the new object;
   * also controls whether to instrument a class at all,
   * and thus avoids instrumenting Object.finalize, which
   * would lead to problems.
   */
  @Override
  public void visitCode () {

    boolean inObject = className.equals("java/lang/Object");

    if (inObject && methodName.equals("finalize")) {
      // don't instrument finalize() -- causes infinite recursion!
      instrument = false;
    }

    if (inObject && methodName.equals("<init>")) {
      // Stack is ...,o
      super.visitVarInsn(ALOAD, 0);
      // Stack is ...,o,o
      super.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "newobj", "(Ljava/lang/Object;)V");
      // Stack is ...,o
    }

    if (instrument) {
      // Code for putting a handler around the method
      // Defer emitting the try block until just before the first instruction;
      //   this insures that other handlers starting at 0 come first, and our
      //   new one match only *after* them (important for correctness since the
      //   JVM run-time takes the first applicable handler)
      // Note that this means we must override some visit methods just for
      //   checking for the first instruction; however, we need not do so
      //   for instructions that examine the operand stack since the stack
      //   is empty at the first instruction.  See checkFirstInsn and its callers.
      startLabel = new Label();
      endLabel   = new Label();
    }

    super.visitCode();

    onMethodEnter(inConstructor);

    // initialize locals
    if ((access & ACC_STATIC) == 0) {
      // deal with 'this' for non-static methods
      if (inConstructor) {
        locals.push(-1);
        newSiteId.put(-1, -1);
      } else {
        locals.push(++vnCounter);
        nowStamped(vnCounter);
      }
    }

    // start i at 1 to skip the left-paren '('
    scan: for (int i = 1; ; i++) { // note: runs to the right paren
      switch (methodDesc.charAt(i)) {
      case ')':
        break scan;

      case 'D': case 'J':
        locals.pushZero(2);
        continue;

      case 'L':
        locals.push(++vnCounter);
        stampedIf(vnCounter, !inConstructor);
        while (methodDesc.charAt(i) != ';') { i++; }
        continue;

      case '[':
        locals.push(++vnCounter);
        stampedIf(vnCounter, !inConstructor);
        // skip over all dimensions
        while (methodDesc.charAt(i) == '[') { i++; }
        // if an array of object type, skip over the object type
        if (methodDesc.charAt(i) == 'L') {
          while (methodDesc.charAt(i) != ';') { i++; }
        }
        continue;

      default:
        locals.push(0);
        continue;
      }
    }

    resetCounts();  // probably not really necessary, but shows we thought about it
  }


  /**
   * processes a Frame to get ready for the next (extended) basic block
   */
  @Override
  public void visitFrame (int type, int nLocal, Object[] locals, int nStack, Object[] stack) {
    if (type != F_NEW) { // uncompressed frame
      throw new
        IllegalStateException("ClassReader.accept() should be called with EXPAND_FRAMES flag");
    }
    vnCounter = 0;
    uninitCounter = -1;
    uninits.clear();
    visitFrameInfo(this.locals, nLocal, locals);
    visitFrameInfo(this.stack , nStack, stack );
    super.visitFrame(type, nLocal, locals, nStack, stack);
    tick();  // counts as a tick since we don't know anything
    flushCounts();
  }

  /**
   * helper subroutine to initialize an IntStack with value numbers
   * according the types given (either locals or working stack)
   * @param s the IntStack to set up; this routine clears it first
   * @param n an int giving the number of items to set up (as for Frames)
   * @param infos an Object array with the information; must have size at least n
   */
  private void visitFrameInfo (IntStack s, final int n, Object[] infos) {
    s.clear();
    for (int i = 0; i < n; ++i) {
      Object info = infos[i];
      if (info == UNINITIALIZED_THIS) {
        // uninitialized "this"
        s.push(-1);
      } else if (info instanceof String) {
        // an Object type
        s.push(++vnCounter);
      } else if (info instanceof Label) {
        // an uninitialized object (other than "this")
        if (uninits.containsKey(info)) {
          int counterValue = uninits.get(info);
          s.push(counterValue);
          if (TRACE_NEW_OBJECTS) {
            System.err.printf("Frame: method = %x, local # = %d, site = %d, Label = %s (old)%n",
                              methodId, counterValue, (int)siteForLabel.get(info), info);
          }
        } else {
          s.push(--uninitCounter);
          uninits.put(info, uninitCounter);
          Integer siteInt = siteForLabel.get(info);
          newSiteId.put(uninitCounter, siteInt);
          if (TRACE_NEW_OBJECTS) {
            System.err.printf("Frame: method = %x, local # = %d, site = %d, Label = %s (new)%n",
                              methodId, uninitCounter, siteInt, info);
          }
        }
      } else {
        // a primitive, NULL, or TOP
        s.push (0);
        if (info == LONG || info == DOUBLE) {
          // a double length primitive
          s.push (0);
        }
      }
    }
  }

  @Override
  public void visitInsn (int opcode) {
    checkFirstInsn();

    // first check for onMethodExit work
    switch (opcode) {
    case IRETURN:
    case LRETURN:
    case FRETURN:
    case DRETURN:
    case ARETURN:
    case RETURN:
      onMethodExit(opcode, true);
      break;

    case ATHROW:
      onMethodExit(opcode, false);
      break;
    }

    doCounts(opcode);

    outer: {
      switch (opcode) {
        // Note: cases that "break outer" have done the work of the original byte code

      case AALOAD:
      case BALOAD:
        if (shouldStamp(1)) {
          // use for BALOAD since combines byte and boolean, for AALOAD because of return type
          mv.visitInsn(DUP2);
          // Stack is a,idx,a,idx
          mv.visitInsn(POP);
          // Stack is a,idx,a
          stampOne();
        }
        break;

      case CALOAD:
        if (shouldStamp(1)) {
          mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "CALoadEvent", "([CI)C");
          break outer;
        }
        break;

      case FALOAD:
        if (shouldStamp(1)) {
          mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "FALoadEvent", "([FI)F");
          break outer;
        }
        break;

      case IALOAD:
        if (shouldStamp(1)) {
          mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "IALoadEvent", "([II)I");
          break outer;
        }
        break;

      case SALOAD:
        if (shouldStamp(1)) {
          mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "SALoadEvent", "([SI)S");
          break outer;
        }
        break;

      case LALOAD:
        if (shouldStamp(1)) {
          mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "LALoadEvent", "([JI)J");
          break outer;
        }
        break;

      case DALOAD:
        if (shouldStamp(1)) {
          mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "DALoadEvent", "([DI)D");
          break outer;
        }
        break;

      case AASTORE:
        // always instrument this: creates a heap edge; never an uninit
        if (instrument) {
          mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "AAStoreEvent",
                             "([Ljava/lang/Object;ILjava/lang/Object;)V");
        }
        break outer;

      case CASTORE:
        if (shouldStamp(2)) {
          mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "CAStoreEvent", "([CIC)V");
          break outer;
        }
        break;

      case FASTORE:
        if (shouldStamp(2)) {
          mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "FAStoreEvent", "([FIF)V");
          break outer;
        }
        break;

      case IASTORE:
        if (shouldStamp(2)) {
          mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "IAStoreEvent", "([III)V");
          break outer;
        }
        break;

      case SASTORE:
        if (shouldStamp(2)) {
          mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "SAStoreEvent", "([SII)V");
          break outer;
        }
        break;

      case BASTORE:
        if (shouldStamp(2)) {
          mv.visitInsn(DUP2_X1);  // idx, val, arrayref, idx, val
          mv.visitInsn(POP2);     // idx, val, arrayref
          mv.visitInsn(DUP_X2);   // arrayref, idx, val, arrayref
          stampOne();
        }
        break;

      case DASTORE:
        if (shouldStamp(3)) {
          mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "DAStoreEvent", "([DID)V");
          break outer;
        }
        break;

      case LASTORE:
        if (shouldStamp(3)) {
          mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "LAStoreEvent", "([JIJ)V");
          break outer;
        }
        break;

      case ARRAYLENGTH:
      case ATHROW:
      case MONITORENTER:
      case MONITOREXIT:
        if (shouldStamp(0)) {
          stampDup();
        }
        break;

      }
      super.visitInsn(opcode);	
    } // end of outer

    // optimizer processing
    switch (opcode) {
    case NOP:
    case INEG:
    case LNEG:
    case FNEG:
    case DNEG:
    case I2F:
    case L2D:
    case F2I:
    case D2L:
    case I2B:
    case I2C:
    case I2S:
      // no net stack change
    case IRETURN:
    case LRETURN:
    case FRETURN:
    case DRETURN:
    case ARETURN:
    case RETURN:
    case ATHROW:
      // these will terminate a block, so don't bother
      break;

    case ACONST_NULL:
    case ICONST_M1:
    case ICONST_0:
    case ICONST_1:
    case ICONST_2:
    case ICONST_3:
    case ICONST_4:
    case ICONST_5:
    case FCONST_0:
    case FCONST_1:
    case FCONST_2:
    case I2L:
    case I2D:
    case F2L:
    case F2D:
      // push one primtive
      stack.push(0);
      break;

    case LCONST_0:
    case LCONST_1:
    case DCONST_0:
    case DCONST_1:
      // push two primitive slots
      stack.pushZero(2);
      break;

    case IALOAD:
    case FALOAD:
    case BALOAD:
    case CALOAD:
    case SALOAD:
      // pop array, idx, push prim
      stack.pop();
      nowStamped(stack.pop());
      stack.push(0);
      break;

    case LALOAD:
    case DALOAD:
      // pop array, idx, push two prim slots
      stack.pop();
      nowStamped(stack.pop());
      stack.pushZero(2);
      break;

    case AALOAD:
      // pop array, idx; push new object
      stack.pop();
      nowStamped(stack.pop());
      nowStamped(stack.push(++vnCounter));
      break;

    case IASTORE:
    case FASTORE:
    case BASTORE:
    case CASTORE:
    case SASTORE:
      // pop array, idx, value
      stack.pop(2);
      nowStamped(stack.pop());
      break;

    case AASTORE:
      // pop array, idx, value
      nowStamped(stack.pop());
      stack.pop();
      nowStamped(stack.pop());
      break;

    case LCMP:
    case DCMPL:
    case DCMPG:
      // pop 4 prims, push one prim
      stack.pop(3);
      break;

    case LASTORE:
    case DASTORE:
      // pop array, idx, two prims
      stack.pop(3);
      nowStamped(stack.pop());
      break;

    case MONITORENTER:
    case MONITOREXIT:
      // pop one slot
      nowStamped(stack.pop());
      break;
      
    case POP:
      // pop one slot
    case IADD:
    case FADD:
    case ISUB:
    case FSUB:
    case IMUL:
    case FMUL:
    case IDIV:
    case FDIV:
    case IREM:
    case FREM:
    case ISHL:
    case ISHR:
    case IUSHR:
    case IAND:
    case IOR:
    case IXOR:
    case L2I:
    case L2F:
    case D2I:
    case D2F:
    case FCMPL:
    case FCMPG:
      // pop two prims, push one
    case LSHL:
    case LSHR:
    case LUSHR:
      // pop 3 prims, push 2
      stack.pop();
      break;

    case POP2:
      // pop two slots
    case LADD:
    case DADD:
    case LSUB:
    case DSUB:
    case LMUL:
    case DMUL:
    case LDIV:
    case DDIV:
    case LREM:
    case DREM:
    case LAND:
    case LOR:
    case LXOR:
      // pop 4 prims, push 2
      stack.pop(2);
      break;

    case DUP:
      stack.push(stack.top());
      break;

    case DUP_X1: {
      int v1 = stack.pop();
      int v2 = stack.pop();
      stack.push(v1);
      stack.push(v2);
      stack.push(v1);
      break;
    }
        
    case DUP_X2: {
      int v1 = stack.pop();
      int v2 = stack.pop();
      int v3 = stack.pop();
      stack.push(v1);
      stack.push(v3);
      stack.push(v2);
      stack.push(v1);
      break;
    }

    case DUP2: {
      int v1 = stack.pop();
      int v2 = stack.pop();
      stack.push(v2);
      stack.push(v1);
      stack.push(v2);
      stack.push(v1);
      break;
    }

    case DUP2_X1: {
      int v1 = stack.pop();
      int v2 = stack.pop();
      int v3 = stack.pop();
      stack.push(v2);
      stack.push(v1);
      stack.push(v3);
      stack.push(v2);
      stack.push(v1);
      break;
    }

    case DUP2_X2: {
      int v1 = stack.pop();
      int v2 = stack.pop();
      int v3 = stack.pop();
      int v4 = stack.pop();
      stack.push(v2);
      stack.push(v1);
      stack.push(v4);
      stack.push(v3);
      stack.push(v2);
      stack.push(v1);
      break;
    }

    case SWAP: {
      int v1 = stack.pop();
      int v2 = stack.pop();
      stack.push(v1);
      stack.push(v2);
      break;
    }

    case ARRAYLENGTH:
      nowStamped(stack.pop());
      stack.push(0);
      break;

    }

  }
  

  /**
   * handles field insns; timestamps accesses to reference fields
   */
  @Override
  public void visitFieldInsn (int opcode, String owner, String fieldName, String fieldDesc) {
    checkFirstInsn();

    int w = width(fieldDesc);
    boolean isRef = isObjectReference(fieldDesc);

    switch (opcode) {

    case PUTFIELD:
      if (instrument) {
        if (isRef) {
          // storing a pointer
          if (stack.top(1) < 0) {
            // storing into an uninit obj (which is legal)
            if (localForNewObject < 0) {
              localForNewObject = nextLocal++;
            }
            // Stack is ...,o,v
            mv.visitInsn(DUP2);
            // Stack is ...,o,v,o,v
            mv.visitInsn(SWAP);
            // Stack is ...,o,v,v,o
            mv.visitVarInsn(ASTORE, localForNewObject);
            locals.set(localForNewObject, stack.top(1));
            // Stack is ...,o,v,v
            int newId = getNewSiteId(stack.top(1));  // may be used below
            mv.visitLdcInsn(localForNewObject);  // ...,o,v,v,s (s is slot for o)
            // Stack is ...,o,v,v,slot
            int fieldId = classAdapter.getFieldId(owner, fieldName, fieldDesc, false);
            mv.visitLdcInsn(fieldId);
            // Stack is ...,o,v,v,slot,fid
            if (TRACE_NEW_OBJECTS) {
              System.err.printf("Uninit PUTFIELD: method = %x, local # = %d, site = %d, field = %d%n",
                                methodId, stack.top(1), newId, fieldId);
            }
            mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "uninitPutfield",
                               "(Ljava/lang/Object;II)V");
          } else {
            // storing a pointer into a regular (initialized) object
            // Reference case: call pointerUpdated
            // Stack is ...,o,v
            mv.visitInsn(DUP2);
            // Stack is ...,o,v,o,v
            int fieldId = classAdapter.getFieldId(owner, fieldName, fieldDesc, false);
            mv.visitLdcInsn(fieldId);
            // Stack is ...,o,v,o,v,fid
            mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "pointerUpdated",
                               "(Ljava/lang/Object;Ljava/lang/Object;I)V");
            // Stack is ...,o,v
          }
        } else if (w == 2) {
          // a long or double field
          if (shouldStamp(2)) {
            // wide field case: call varEvent1
            // Stack is ...,o,wide
            mv.visitInsn(DUP2_X1);
            // Stack is ...,wide,o,wide
            mv.visitInsn(POP2);
            // Stack is ...,wide,o
            mv.visitInsn(DUP_X2);
            // Stack is ...,o,wide,o
            stampOne();
            // Stack is ...,o,wide
          }
        } else {
          // a length-one primitive field
          if (shouldStamp(1)) {
            // narrow field case: call varEvent1
            // Stack is ...,o,v
            mv.visitInsn(DUP2);
            // Stack is ...,o,v,o,v
            mv.visitInsn(POP);
            // Stack is ...,o,v,o
            stampOne();
            // Stack is ...,o,v
          }
        }
      }
      break;
		  
    case PUTSTATIC:
      if (instrument) {
        if (isRef) {
          // Stack is ...,v;
          mv.visitInsn(DUP);
          // Stack is ...,v,v;
          mv.visitLdcInsn(classAdapter.getFieldId(owner, fieldName, fieldDesc, true));
          // Stack is ...,v,v,fieldId
          mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "pointerUpdated",
                             "(Ljava/lang/Object;I)V");
          // Stack is ...,v;
        }
      }
      break;
          
    case GETFIELD:
      if (shouldStamp(0)) {
        stampDup();
      }
      break;

    }

    super.visitFieldInsn(opcode, owner, fieldName, fieldDesc);

    { 
      int refs  = (isRef ? 1 : 0);
      int count = (w == 1 ? 1 : COUNT_FOR_WIDES) & (-refs);
      switch (opcode) {
      case GETFIELD: 
      case GETSTATIC:
        incrCounts(1, count-refs, 0, refs, 0); break;
      case PUTFIELD:
      case PUTSTATIC:
        incrCounts(1, 0, count-refs, 0, refs); break;
      }
    }

    switch (opcode) {

    case GETFIELD:
      nowStamped(stack.pop());
      // fall through
    case GETSTATIC:
      if (isRef) {
        stack.push(++vnCounter);
      } else {
        stack.pushZero(w);
      }
      break;

    case PUTFIELD:
      nowStamped(stack.top());  // works because nowStamped filters its argument
      stack.pop(w);
      nowStamped(stack.pop());
      break;

    case PUTSTATIC:
      nowStamped(stack.top());  // works because nowStamped filters its argument
      stack.pop(w);
      break;

    }

  }
		
  @Override
  public void visitJumpInsn (int opcode, Label l) {
    checkFirstInsn();

    switch (opcode) {
    case IF_ACMPEQ:
    case IF_ACMPNE:
      if (shouldStamp(1) || shouldStamp(0)) {
        // cannot have uninits here, so this test is ok
        mv.visitInsn(DUP2);
        mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "varEvent2",
                           "(Ljava/lang/Object;Ljava/lang/Object;)V");
      }
      break;

    case IFNULL:
    case IFNONNULL:
      if (shouldStamp(0)) {
        stampDup();
      }
      break;
      
    }

    doCounts(opcode);
    super.visitJumpInsn(opcode,l);

    switch (opcode) {
    case IFEQ:
    case IFNE:
    case IFLT:
    case IFGE:
    case IFGT:
    case IFLE:
      stack.pop();
      break;

    case IF_ICMPEQ:
    case IF_ICMPNE:
    case IF_ICMPLT:
    case IF_ICMPGE:
    case IF_ICMPGT:
    case IF_ICMPLE:
      stack.pop(2);
      break;

    case IF_ACMPEQ:
    case IF_ACMPNE:
      nowStamped(stack.pop());
      nowStamped(stack.pop());
      break;

    case IFNULL:
    case IFNONNULL:
      nowStamped(stack.pop());
      break;

    case GOTO:
    case JSR:
      // ignore: ends a chunk of interest
      break;
    }
  }

  @Override
  public void visitIntInsn (int opcode, int operand) {
    checkFirstInsn();

    doCounts(opcode);
    super.visitIntInsn(opcode, operand);

    if (opcode == NEWARRAY && instrument) {
      // Stack is ...,a
      mv.visitInsn(DUP);
      // Stack is ...,a,a
      mv.visitLdcInsn(++lastNewSiteId);
      // Stack is ...,a,a,site
      mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "newarray",
                         "(Ljava/lang/Object;I)V");
      // Stack is ...,a
      int classId = classAdapter.getClassId(className);
      String desc = "?";
      switch (operand) {
      case  4: desc = "Z"; break;
      case  5: desc = "C"; break;
      case  6: desc = "F"; break;
      case  7: desc = "D"; break;
      case  8: desc = "B"; break;
      case  9: desc = "S"; break;
      case 10: desc = "I"; break;
      case 11: desc = "J"; break;
      }
      nameStream.printf("S 0x%x 0x%x 0x%x %s 1%n", methodId, classId, lastNewSiteId, desc);
    }

    switch (opcode) {

    case BIPUSH:
    case SIPUSH:
      stack.push(0);
      break;

    case NEWARRAY:
      stack.pop();
      nowStamped(stack.push(++vnCounter));
      break;

    }

  }

  @Override
  public void visitLdcInsn (Object obj) {
    checkFirstInsn();

    doCounts(LDC);  // assumes values are synthesized inline, or in any case, not in the heap
    super.visitLdcInsn(obj);
    if (obj instanceof String || obj instanceof Type) {
      stack.push(++vnCounter);
    } else if (obj instanceof Long || obj instanceof Double) {
      stack.pushZero(2);
    } else {
      stack.push(0);
    }
  }

  @Override
  public void visitMethodInsn (int opcode, String owner, String name, String desc) {
    checkFirstInsn();

    // System.err.println("MethodInsn opcode=" + opcode + " callee=" + owner +
    //                    "." + name + desc);

    // pop arguments off stack
    int idx = 1; // skip the '('
    outer:
    while (true) {
      switch (desc.charAt(idx++)) {
      case 'B':
      case 'C':
      case 'F':
      case 'I':
      case 'S':
      case 'Z':
        stack.pop();
        continue;

      case 'D':
      case 'J':
        stack.pop(2);
        continue;

      case 'L':
        stack.pop();
        while (desc.charAt(idx++) != ';');
        continue;

      case '[':
        stack.pop();
        while (desc.charAt(idx) == '[')
          ++idx;
        if (desc.charAt(idx++) == 'L') {
          while (desc.charAt(idx++) != ';');
        }
        continue;

      case ')':
        break outer;
      }
    }

    doCounts(opcode);
    super.visitMethodInsn(opcode, owner, name, desc);

    if (opcode == INVOKESPECIAL && stack.top() < 0 && name.equals("<init>")) {
      // found an initializer call!
      foundInit(stack.top());
    }

    if (opcode != INVOKESTATIC) {
      stack.pop();
    }

    tick();

    switch (desc.charAt(idx)) {
    case 'B':
    case 'C':
    case 'F':
    case 'I':
    case 'S':
    case 'Z':
      stack.push(0);
      break;

    case 'D':
    case 'J':
      stack.pushZero(2);
      break;

    case 'L':
    case '[':
      nowStamped(stack.push(++vnCounter));
      break;

    }

    if (returnsObject(desc)) { // the method returns an object: timestamp it now
      stampDup();
    }	
  }

  @Override
  public void visitMultiANewArrayInsn (String desc, int dims) {
    checkFirstInsn();

    doCounts(MULTIANEWARRAY);
    super.visitMultiANewArrayInsn(desc, dims);

    if (instrument) {
      // Stack is ...,a
      mv.visitInsn(DUP);
      // Stack is ...,a,a
      // This requires that dims actually be a short.
      // I hope no one wants an array with more than 32,767 dimensions
      mv.visitIntInsn(SIPUSH, dims);
      // Stack is ...,a,a,dims
      mv.visitLdcInsn(++lastNewSiteId);
      // Stack is ...,a,a,dims,site
      mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "multiNewArray",
                         "([Ljava/lang/Object;II)V");
      // Stack is ...,a
      int classId = classAdapter.getClassId(className);
      nameStream.printf("S 0x%x 0x%x 0x%x %s %d%n", methodId, classId, lastNewSiteId, desc, dims);
    }
    stack.pop(dims);
    nowStamped(stack.push(++vnCounter));
  }

  @Override
  public void visitTypeInsn (int opcode, String type) {
    checkFirstInsn();

    outer: {
      switch (opcode) {

      case ANEWARRAY:
          // Stack is ...,n
        super.visitTypeInsn(opcode, type);
        if (instrument) {
          // Stack is ...,a
          mv.visitInsn(DUP);
          // Stack is ...,a,a
          mv.visitLdcInsn(++lastNewSiteId);
          // Stack is ...,a,a,site
          mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "newarray",
                             "(Ljava/lang/Object;I)V");
          // Stack is ...,a
          int classId = classAdapter.getClassId(className);
          String desc = "L" + type + ";";
          nameStream.printf("S 0x%x 0x%x 0x%x %s 1%n", methodId, classId, lastNewSiteId, desc);
        }
        break outer;
          
      case CHECKCAST:
      case INSTANCEOF:
        if (shouldStamp(0)) {
          stampDup();
        }
        break;
          
      }
      super.visitTypeInsn(opcode, type);
    }
    
    doCounts(opcode);

    switch (opcode) {

    case NEW:
      stack.push(--uninitCounter);
      newSiteId.put(uninitCounter, ++lastNewSiteId);
      if (localForNewObject < 0) {
        localForNewObject = nextLocal++;
      }
      // Stack is ...,o
      mv.visitInsn(DUP);
      // Stack is ...,o,o
      mv.visitVarInsn(ASTORE, localForNewObject);
      locals.set(localForNewObject, uninitCounter);
      // Stack is ...,o
      String desc = "L" + type + ";";
      mv.visitLdcInsn(desc);
      // Stack is ...,o,desc
      mv.visitLdcInsn(localForNewObject);
      // Stack is ...,o,desc,slot
      mv.visitLdcInsn(lastNewSiteId);
      // Stack is ...,o,desc,slot,site
      mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "newobj", "(Ljava/lang/String;II)V");
      // Stack is ...,o
      if (siteForLabel.get(lastLabel) == null)
        siteForLabel.put(lastLabel, lastNewSiteId);
      if (TRACE_NEW_OBJECTS) {
        System.err.printf("NEW: method = %x, local # = %d, site = %d, Label = %s%n",
                          methodId, uninitCounter, lastNewSiteId,
                          (lastLabel == null ? "null" : lastLabel.toString()));
      }
      int classId = classAdapter.getClassId(className);
      nameStream.printf("S 0x%x 0x%x 0x%x %s 0%n", methodId, classId, lastNewSiteId, desc);
      break;

    case ANEWARRAY:
      stack.pop();
      nowStamped(stack.push(++vnCounter));
      break;

    case CHECKCAST:
      nowStamped(stack.top());
      break;

    case INSTANCEOF:
      nowStamped(stack.pop());
      stack.push(0);
      break;
    }
  }

  @Override
  public void visitVarInsn (int opcode, int var) {
    checkFirstInsn();

    doCounts(opcode);
    super.visitVarInsn(opcode, var);

    switch (opcode) {
    case ILOAD:
    case FLOAD:
      stack.push(0);
      break;

    case ALOAD:
      stack.push(locals.get(var));
      break;

    case LLOAD:
    case DLOAD:
      stack.pushZero(2);
      break;

    case ISTORE:
    case FSTORE:
      stack.pop();
      locals.set(var, 0);
      break;

    case LSTORE:
    case DSTORE:
      stack.pop(2);
      locals.set(var  , 0);
      locals.set(var+1, 0);
      break;

    case ASTORE:
      locals.set(var, stack.pop());
      break;

    case RET:
      break;
    }
  }
		
  @Override
  public void visitIincInsn (int var, int incr) {
    checkFirstInsn();

    doCounts(IINC);
    super.visitIincInsn(var, incr);
  }

  /**
   * called when we can instrument entry to a method
   * For most methods this is immediately, but for constructors
   * we may wait until the super-constructor call.
   */
  protected void onMethodEnter (boolean inConstructor) {
    // System.err.println("onMethodEnter: " + className + "::" + methodName);

    boolean inObject = className.equals("java/lang/Object");

    boolean inThread = className.equals("java/lang/Thread");

    insideMethod = true;

    if (!instrument) {
      return;
    }

    if (inConstructor && !inObject && !inThread) {
      // We need to timestamp these so they don't disappear too soon!
      // Since this also detects JNI created objects, etc., we prefer to do this *before*
      //   we note entry into the routine
      mv.visitLdcInsn("L" + className + ";");
      mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "stampThis", "(Ljava/lang/String;)V");
    }

    if (STACK_SCAN_ON_ENTRY) {
      // -- Emit method ID
      mv.visitLdcInsn(methodId);
					
      if ((access & ACC_STATIC) != 0) {
	// System.err.println("ADD-ENTRY: " + className + "::" + methodName);
	mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "methodEntryStatic", 
			   "(I)V" );
      } else if (inConstructor) {
	mv.visitInsn(ACONST_NULL);
	mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "methodEntry", 
			   "(ILjava/lang/Object;)V");
      } else {
	// System.err.println("ADD-ENTRY: " + className + "::" + methodName);
	mv.visitVarInsn(ALOAD, 0);
	mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "methodEntry", 
			   "(ILjava/lang/Object;)V");
      }
    }

    Vector<Integer> paramSlots = findParamSlots(inConstructor);

    // load all the parameters to the operand stack
    
    for (int ps: paramSlots) {
      mv.visitVarInsn(ALOAD, ps);
    }
    
    // In case there are a lot of parameters,
    // stamp them using as few calls to varEvent 5 as possible.
    for (int i = 0; i < paramSlots.size()/5; i++) {
      mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "varEvent5",
                         "(Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;" +
                         "Ljava/lang/Object;Ljava/lang/Object;)V");
    }
    
    // Then get the remainder
    switch (paramSlots.size() % 5) {
    case 0:
      break;
    case 1:
      mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "varEvent1",
                         "(Ljava/lang/Object;)V");
      break;
    case 2:
      mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "varEvent2",
                         "(Ljava/lang/Object;Ljava/lang/Object;)V");
      break;
    case 3:
      mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "varEvent3",
                         "(Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V");
      break;
    case 4:
      mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "varEvent4",
                         "(Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;" +
                         "Ljava/lang/Object;)V");
      break;
    }

  }

  /**
   * called when we are about to return or throw
   * @param opcode an int giving the opcode of the instruction causing the exit
   */
  protected void onMethodExit (int opcode, boolean normal) {
    // System.err.println("onMethodExit: " + className + "::" + methodName);
    // if( (opcode >= IRETURN && opcode <= RETURN) || opcode == ATHROW ){
      
    if (!instrument) {
      return;
    }

    String etMethod;
    String etSig;

    if (normal) {
      etMethod = "methodExit";
      etSig = "(I";
    } else{
      etMethod = "exceptionThrow";
      etSig = "(Ljava/lang/Object;I";
      mv.visitInsn(DUP);  // push exception object
    }
    // -- Emit method ID
    mv.visitLdcInsn(methodId);
      
    if ((access & ACC_STATIC) != 0) {
      etMethod += "Static";
    } else {
      if (insideMethod) {
        mv.visitVarInsn(ALOAD, 0);
      } else {
        mv.visitInsn(ACONST_NULL);
      }
      etSig += "Ljava/lang/Object;";
    }
    etSig += ")V";
    mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", etMethod, etSig);
  }

  @Override
  public void visitLabel (Label l) {
    // ClassReader guarantees one Label per instruction
    // The only possible extras are ones we add, and they (presently)
    //   do not interfere with the logic of this method
    flushCounts();  // so values flowing from above don't get mixed in
    super.visitLabel(l);
    lastLabel = l;
    if (handlers.contains(l)) {
      // we are at the start of an exception handler
      mv.visitInsn(DUP);
      mv.visitLdcInsn(methodId);
      if ((access & ACC_STATIC) != 0) {
        mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "exceptionHandleStatic", "(Ljava/lang/Object;I)V");
      } else {
        if (insideMethod) {
          mv.visitVarInsn(ALOAD, 0);
        } else {
          mv.visitInsn(ACONST_NULL);
        }
        mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "exceptionHandle",
                           "(Ljava/lang/Object;ILjava/lang/Object;)V");
      }
    }
    /*
    System.err.printf("Label: method = %x, %s%n", methodId, lastLabel.toString());
    */
  }

  @Override
  public void visitTryCatchBlock (Label regionStart, Label regionEnd, Label handler, String excType) {
    handlers.add(handler);
    super.visitTryCatchBlock(regionStart, regionEnd, handler, excType);
  }

  @Override
  public void visitMaxs (int maxStack, int maxLocals) {
    // complete the wrapped handler (if any)
    if (endLabel != null) {
      mv.visitLabel(endLabel);

      mv.visitInsn(DUP);
      mv.visitLdcInsn(methodId);

      if ((access & ACC_STATIC) != 0) {
        mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "methodExceptionStatic", "(Ljava/lang/Object;I)V");
      } else {
        if (inConstructor) {
          mv.visitInsn(ACONST_NULL);
        } else {
          mv.visitVarInsn(ALOAD, 0);
        }
        mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "methodException",
                           "(Ljava/lang/Object;ILjava/lang/Object;)V");
      }
      mv.visitInsn(ATHROW);
    }
    super.visitMaxs(maxStack, nextLocal);
  }

  /**
   * checks if we are just before the first instruction, and if so,
   * does actions needed right before the instruction
   */
  protected void checkFirstInsn () {
    if (beforeFirstInsn) {
      if (startLabel != null) {
        // need to do this after any other handlers!
        mv.visitTryCatchBlock(startLabel, endLabel, endLabel, null);
        mv.visitLabel(startLabel);
      }
    }
    beforeFirstInsn = false;
  }

  /**
   * timestamps one object on the top of the stack, leaving it there
   */
  protected void stampDup () {
    mv.visitInsn(DUP);
    stampOne();
  }

  /**
   * timestamps the object on the top of the stack, consuming it
   */
  protected void stampOne() {
    mv.visitMethodInsn(INVOKESTATIC, "ElephantTracks", "varEvent1",
                       "(Ljava/lang/Object;)V");
  }

  /**
   * marks a value as stamped if a condition is true
   * @param val an int giving the value number to stamp conditionally
   * @param stamped a boolean indicating whether to mark the value as stamped
   */
  private void stampedIf (int val, boolean stamped) {
    if (stamped) {
      nowStamped(val);
    }
  }

  /**
   * marks a value as stamped, provided it is not an uninitialized object
   * @param val an int giving the value number to mark as stamped
   */
  private void nowStamped (int val) {
    if (val > 0) {
      this.stamped.add(val);
    }
  }

  /**
   * indicates whether we should stamp (instrument) the ith value down from the top
   * @param i an int indicating the desired number of slots below the top
   * @return a boolean that is true if we should instrument
   */
  private boolean shouldStamp (int i) {
    int vn = stack.top(i);
    return instrument && (vn > 0) && !stamped.contains(vn);
  }

  /**
   * handles optimizer tracking adjustments for operations that "tick" the stamp clock;
   * notably, it clears the set of recently stamped things so that they may be stamped
   * again with the new clock value
   */
  private void tick () {
    stamped.clear();
  }

  /**
   * found the initializing call for the given value number;
   * flip other copies to a regular value number
   * @param vn an int giving the (negative) value number being initialized
   */
  private void foundInit (int vn) {
    int newNum = ++vnCounter;
    locals.renumber(vn, newNum);
    stack .renumber(vn, newNum);
  }

  /*
   * Returns true if fieldDesc is the descriptor fora field that might point to an object.
   */
  private boolean isObjectReference (String fieldDesc) {
    char kind = fieldDesc.charAt(0);
    return (kind == 'L' || kind == '[');
  }

  /**
   * does this designate a wide (long or double) type?
   * @param desc a String giving the type descriptor
   * @return a boolean that is true iff the type is long or double
   */
  private boolean isWide (String desc) {
    char kind = desc.charAt(0);
    return (kind == 'J' || kind == 'D');
  }

  /**
   * the width of this type: 2 for long or double, 1 for others
   * @param desc a String giving the type descriptor
   * @return an int giving the type's width in slots (1 or 2)
   */
  private int width (String desc) {
    return (isWide(desc) ? 2 : 1);
  }

  private void doCounts (int opcode) {
    switch (opcode) {
    case NOP:
    case ACONST_NULL:
    case ICONST_M1:
    case ICONST_0:
    case ICONST_1:
    case ICONST_2:
    case ICONST_3:
    case ICONST_4:
    case ICONST_5:
    case LCONST_0:
    case LCONST_1:
    case FCONST_0:
    case FCONST_1:
    case FCONST_2:
    case DCONST_0:
    case DCONST_1:
    case BIPUSH:
    case SIPUSH:
    case LDC:
    case ILOAD:
    case LLOAD:
    case FLOAD:
    case DLOAD:
    case ALOAD:
    case ISTORE:
    case LSTORE:
    case FSTORE:
    case DSTORE:
    case ASTORE:
    case POP:
    case POP2:
    case DUP:
    case DUP_X1:
    case DUP_X2:
    case DUP2:
    case DUP2_X1:
    case DUP2_X2:
    case SWAP:
    case IADD:
    case LADD:
    case FADD:
    case DADD:
    case ISUB:
    case LSUB:
    case FSUB:
    case DSUB:
    case IMUL:
    case LMUL:
    case FMUL:
    case DMUL:
    case IDIV:
    case LDIV:
    case FDIV:
    case DDIV:
    case IREM:
    case LREM:
    case FREM:
    case DREM:
    case INEG:
    case LNEG:
    case FNEG:
    case DNEG:
    case ISHL:
    case LSHL:
    case ISHR:
    case LSHR:
    case IUSHR:
    case LUSHR:
    case IAND:
    case LAND:
    case IOR:
    case LOR:
    case IXOR:
    case LXOR:
    case IINC:
    case I2L:
    case I2F:
    case I2D:
    case L2I:
    case L2F:
    case L2D:
    case F2I:
    case F2L:
    case F2D:
    case D2I:
    case D2L:
    case D2F:
    case I2B:
    case I2C:
    case I2S:
    case LCMP:
    case FCMPL:
    case FCMPG:
    case DCMPL:
    case DCMPG:
      incrCounts(1, 0, 0, 0, 0); break;
    case IALOAD:
    case LALOAD:
    case FALOAD:
    case DALOAD:
    case BALOAD:
    case CALOAD:
    case SALOAD:
      incrCounts(1, 2, 0, 0, 0); break; // length and item
    case AALOAD:
      incrCounts(1, 1, 0, 1, 0); break; // length and item
    case IASTORE:
    case LASTORE:
    case FASTORE:
    case DASTORE:
    case BASTORE:
    case CASTORE:
    case SASTORE:
      incrCounts(1, 1, 1, 0, 0); break; // length and item
    case AASTORE:
      incrCounts(1, 1, 0, 0, 1); break; // length and item
    case IFEQ:
    case IFNE:
    case IFLT:
    case IFGE:
    case IFGT:
    case IFLE:
    case IF_ICMPEQ:
    case IF_ICMPNE:
    case IF_ICMPLT:
    case IF_ICMPGE:
    case IF_ICMPGT:
    case IF_ICMPLE:
    case IF_ACMPEQ:
    case IF_ACMPNE:
    case GOTO:
    case JSR:
    case RET:
    case TABLESWITCH:
    case LOOKUPSWITCH:
    case IRETURN:
    case LRETURN:
    case FRETURN:
    case DRETURN:
    case ARETURN:
    case RETURN:
    case INVOKESTATIC:
    case ATHROW:
    case IFNULL:
    case IFNONNULL:
      incrCountsAndFlush(1, 0, 0, 0, 0); break;
    // GETSTATIC:  // handle with instruction
    // PUTSTATIC:
    // GETFIELD:
    // PUTFIELD:
    case INVOKEVIRTUAL:
    case INVOKESPECIAL:
    case INVOKEINTERFACE:
    case INVOKEDYNAMIC:
      incrCountsAndFlush(1, 0, 0, 2, 0); break;
    case NEW:
      incrCounts(1, 0, 2, 0, 0); break; // header words
    case NEWARRAY:
    case ANEWARRAY:
    case MULTIANEWARRAY: // hard to estimate; we'll see the pointer stores ...
      incrCounts(1, 0, 3, 0, 0); break; // header words
    case ARRAYLENGTH:
      incrCounts(1, 1, 0, 0, 0); break; // length word
    case CHECKCAST:
    case INSTANCEOF:
      incrCounts(1, 1, 0, 0, 0); break; // type word
    case MONITORENTER:
    case MONITOREXIT:
      incrCounts(1, 1, 1, 0, 0); break; // read/write lock field
    }
  }

  /**
   * implements a subset of List for ints, without creating Integer objects
   */
  protected class IntStack {

    /**
     * number of items currently in the list
     */
    private int size;

    /**
     * holds the items
     */
    private int[] ints;

    /**
     * default initial capacity
     */
    private static final int DEFAULT_CAPACITY = 4;

    /**
     * construct an IntStack with default initial capacity
     */
    public IntStack () {
      this(DEFAULT_CAPACITY);
    }
    
    /**
     * construct an IntStack with given initial capacity
     * @param init an int giving the initial capacity
     */
    public IntStack (int init) {
      size = 0;
      ints = new int[(init >= 0) ? init : DEFAULT_CAPACITY];
    }
    
    /**
     * push an int
     * @param value an int to push
     * @return the int pushed, as a convenience
     */
    public int push (int value) {
      if (size >= ints.length) {
        int newCap = ints.length << 1;
        if (newCap > 1024) {
          newCap = ints.length + 1024;
        }
        ints = Arrays.copyOf(ints, newCap);
      }
      return (ints[size++] = value);
    }

    /**
     * push some zeroes on the sack
     * @param n an int indicating how many zeroes to push
     */
    public void pushZero (int n) {
      while (--n >= 0) {
        stack.push(0);
      }
    }

    /**
     * pop and return a value at the end
     * @return an int giving the value popped
     */
    public int pop () {
      if (size <= 0) return 0;
      return ints[--size];
    }

    /**
     * discard num items
     * @param num an int giving the number of items to discard (pop)
     */
    public void pop (int num) {
      size = ((size >= num) ? size-num : 0);
    }

    /**
     * clear the stack (make it have no items pushed)
     */
    public void clear () {
      size = 0;
    }

    /**
     * the current number of ints on the stack
     * @return an int giving the number of ints on the stack
     */
    public int size () {
      return size;
    }

    /**
     * the ith value pushed (first is numbered 0)
     * @param an int giving the index of the item desired
     * @return the int value at that index
     */
    public int get (int i) {
      if (i < 0 || i >= size)
        return 0;
      return ints[i];
    }

    /**
     * set the ith value pushed (first is numbered 0)
     * @param i an int giving the index of the item to set
     * @param v an int giving the value to store
     */
    public void set (int i, int v) {
      if (i >= 0 && i < size) {
        ints[i] = v;
      }
    }

    /**
     * the topmost value
     * @return the int on top of the stack
     */
    public int top () {
      return (size <= 0 ? 0 : ints[size-1]);
    }

    /**
     * the ith down from the top (top(0) gives topmost)
     * @param i an int indicating the number of slots down from the top desired
     * @return the int stored i slots down from the top (i == 0 gives topmost)
     */
    public int top (int i) {
      return (i < 0 || i >= size) ? 0 : ints[size-i-1];
    }

    /**
     * renumber from one value number to another
     * @param oldNum an int giving the number to be replaced wherever it occurs
     * @param newNum an int giving the new number to replace it with
     */
    public void renumber (int oldNum, int newNum) {
      for (int i = size; --i >= 0; ) {
        if (ints[i] == oldNum) {
          ints[i] = newNum;
        }
      }
    }

  }

}

// Local Variables:
// mode:Java
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
