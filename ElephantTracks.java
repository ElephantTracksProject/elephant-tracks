import java.lang.reflect.Field;

public class ElephantTracks {
	
  // these must match the values in main.cpp
  private static final int PHASE_ONLOAD = 0;
  private static final int PHASE_START  = 1;
  private static final int PHASE_LIVE   = 2;
  private static final int PHASE_DEAD   = 3;

  /**
   * @return whether we are supposed to count calls or not
   */
  private static native boolean getCountCalls ();

  private static final boolean countCalls = getCountCalls();

  /**
   * indicates the phase of the JVM Tool Interface as known to the agent
   */
  private static int engaged = PHASE_ONLOAD;

  private static Object updateTest;  // see testing code in ETCallbackHandler.cpp

  /**
   * Native method to indicate that a field in a scalar object has been updated;
   * could also be used for arrays by passing the negative of the index
   * @param o an Object reference indicating the object being updated; here
   *        null means a static variable is being updated
   * @param newTarget an Object reference indicating the new referent of the slot
   * @param fieldId an int giving the unique number used the identify the field
   */
  private static native void _pointerUpdated (Object o, Object newTarget, int fieldId);

  private static long numPointerUpdated3 = 0;

  /**
   * Method to indicate that a field in a scalar object has been updated
   * @param o an Object reference indicating the object being updated
   * @param newTarget an Object reference indicating the new referent of the field
   * @param fieldId an int giving the unique number used the identify the field
   */
  public static void pointerUpdated (Object o, Object newTarget, int fieldId) {
    if (countCalls) ++numPointerUpdated3;
    if (engaged == PHASE_LIVE) {
      _pointerUpdated(o, newTarget, fieldId);
    }
   }

  private static long numPointerUpdated2 = 0;

  /**
   * Method to indicate that a static field has been updated
   * @param newTarget an Object reference indicating the new referent of the field
   * @param fieldId an int giving the unique number used the identify the field
   */
  public static void pointerUpdated (Object newTarget, int fieldId) {
    if (countCalls) ++numPointerUpdated2;
    if (engaged == PHASE_LIVE) {
      _pointerUpdated(null, newTarget, fieldId);
    }
  }

  /**
   * Native method to indicate that a field in a scalar object has been updated;
   * could also be used for arrays by passing the negative of the index
   * @param o an Object reference indicating the object being updated; here
   *        null means a static variable is being updated
   * @param newTarget an Object reference indicating the new referent of the slot
   * @param offset a long giving the offset of the field (as in Unsafe.putObject, etc.)
   */
  private static native void _pointerUpdated (Object o, Object newTarget, long offset);

  private static long numPointerUpdated3off = 0;

  /**
   * Method to indicate that a field in a scalar object has been updated
   * @param o an Object reference indicating the object being updated
   * @param newTarget an Object reference indicating the new referent of the field
   * @param offset a long giving the offset of the field (as in Unsafe.putObject, etc.)
   */
  public static void pointerUpdated (Object o, Object newTarget, long offset) {
    if (countCalls) ++numPointerUpdated3off;
    if (engaged == PHASE_LIVE) {
      _pointerUpdated(o, newTarget, offset);
    }
   }

  /**
   * Method to indicated that the referent in a Reference object has been updated,
   * as will be seen by the garbage collector, etc.  Called from instrumented version
   * of Reference.initReferenceImpl.
   * @param o the Reference object being updated
   * @param referent the new referent Object
   */
  private static native void _referentUpdated (Object o, Object referent);

  private static long numReferentUpdated = 0;

  /**
   * Method to indicated that the referent in a Reference object has been updated,
   * as will be seen by the garbage collector, etc.  Called from instrumented version
   * of Reference.initReferenceImpl.
   * @param o the Reference object being updated
   * @param referent the new referent Object
   */
  public static void referentUpdated (Object o, Object referent) {
    if (countCalls) ++numReferentUpdated;
    if (engaged == PHASE_LIVE) {
      _referentUpdated(o, referent);
    }
  }

  /**
   * indicates update of a slot in an array of objects
   * @param array the array Object being updated
   * @param i the int index being updated
   * @param storee the Object being stored into the slot
   */
  private static native void _arrayStore (Object array, int i, Object storee);

  private static long numArrayStore = 0;

  /**
   * indicates update of a slot in an array of objects
   * NOTE: order is different than for the native method, to make the
   * bytecode sequence shorter in the instrumented code
   * @param i the int index being updated
   * @param storee the Object being stored into the slot
   * @param array the array Object being updated
   */
  public static void arrayStore (int i, Object storee, Object array) {
    if (countCalls) ++numReferentUpdated;
    if (engaged == PHASE_LIVE) {
      _arrayStore(array, i, storee);
    }
  }

  /**
   * Indicates update to field of an uninitialized object.
   * @param newTarget the Object being stored into the field
   * @param objSlot an int giving the local variable slot with a ref to the uninitialized object
   * @param fieldId an int giving the unique id of the field being updated
   */
  private static native void _uninitPutfield (Object newTarget, int objSlot, int fieldId);

  private static long numUninitPutfield = 0;

  /**
   * Indicates update to field of an uninitialized object.
   * @param newTarget the Object being stored into the field
   * @param objSlot an int giving the local variable slot with a ref to the uninitialized object
   * @param fieldId an int giving the unique id of the field being updated
   */
  public static void uninitPutfield (Object newTarget, int objSlot, int fieldId) {
    if (countCalls) ++numUninitPutfield;
    if (engaged == PHASE_LIVE) {
      _uninitPutfield(newTarget, objSlot, fieldId);
    }
  }

  /**
   * Indicates that the given thread has allocated the given object;
   * called from Object.<init>, i.e., as soon as it is legal, or
   * immediate after NEWARRAY/ANEWARRAY for arrays (will change in future)
   * @param o the newly allocated Object
   */
  private static native void _newobj (Object o);

  private static long numNewobj = 0;

  /**
   * Indicates a newly allocated object;
   * called from Object.<init>, i.e., as soon as it is legal to pass the pbject itself;
   * called elswhere when an object is not allocated by a bytecode
   * @param o the newly allocated Object
   */
  public static void newobj (Object o) {
    if (countCalls) ++numNewobj;
    if (engaged == PHASE_LIVE) {
      _newobj(o);
    }
  }
	
  /**
   * indicates that the given thread has allocated the given object;
   * called at the NEW bytecode, so cannot pass the object;
   * rather, we identify its slot and let the agent access it;
   * we provide the class name since the class is not filled in yet
   * @param className a String giving the class of the object
   * @param slot an int giving the slot within the caller's stack that holds the new object
   * @param site an int giving the unique number of the allocation site
   */
  private static native void _newobj (String classname, int slot, int site);

  private static long numNewobj2 = 0;

  /**
   * indicates that the given thread has allocated the given object;
   * called at the NEW bytecode, so cannot pass the object;
   * rather, we identify its slot and let the agent access it;
   * we provide the class name since the class is not filled in yet
   * @param className a String giving the class of the object
   * @param slot an int giving the slot within the caller's stack that holds the new object
   * @param site an int giving the unique number of the allocation site
   */
  public static void newobj (String className, int slot, int site) {
    if (countCalls) ++numNewobj2;
    if (engaged == PHASE_LIVE) {
      _newobj(className, slot, site);
    }
  }

  /**
   * Indicates that the given thread has allocated the given array
   * called immediately after NEWARRAY/ANEWARRAY
   * @param o the newly allocated array Object
   * @param site an int giving the unique number of the allocation site
   */
  private static native void _newarray (Object o, int site);

  private static long numNewarray = 0;

  /**
   * indicates a newly allocated array
   * @param o the newly allocated array Object
   * @param site an int giving the unique number of the allocation site
   */
  public static void newarray (Object o, int site) {
    if (countCalls) ++numNewarray;
    if (engaged == PHASE_LIVE) {
      _newarray(o, site);
    }
  }
	
  private static long numMultiNewArray = 0;

  /**
   * called just after allocating a multidimensional array
   * @param array the array Object just allocated
   * @param dims an int giving the number of dimensions of the array,
   *        which is necessarily > 0
   * @param site an int giving the unique number of the allocation site
   */
  public static void multiNewArray (Object[] array, int dims, int site) {
    if (countCalls) ++numMultiNewArray;
    if (engaged == PHASE_LIVE) {

      newarray(array, site);
      if (dims == 1) {
        // This can only happen if some one allocated
        // a single dimensional array with MULTIANEWARRAY,
        // which is legal, but odd.
        // In this case, we are all done.
        return;
      }

      for (int i = 0; i < array.length; i++) {
        if (dims > 2) {
          multiNewArray((Object[])array[i], dims-1, site);	
          _arrayStore(array, i, array[i]);
        } else {
          // If it is just 2D, go through time stamping all the sub arrays.
          // This avoids having to do funny things with casts and instanceof
          newarray(array[i], site);
          _arrayStore(array, i, array[i]);
        }
      }
    }

  }

  /**
   * used to mark entry to a method
   * @param methodId an int giving our unique number for the method
   * @param receiver an Object giving the receiver for an instance method;
   *        use null for a static method
   * @param isStatic a boolean indicating whether the method is static
   */
  private static native void _methodEntry (int methodId, Object receiver, boolean isStatic);

  private static long numMethodEntryStatic = 0;

  /**
   * used to mark entry to a static method
   * @param methodId an int giving our unique number for the method
   */
  public static void methodEntryStatic (int methodId) {
    if (countCalls) ++numMethodEntryStatic;
    if (engaged == PHASE_LIVE) {
      _methodEntry(methodId, null, true);
    }
  }
	
  private static long numMethodEntry = 0;

  /**
   * used to mark entry to an instance method
   * @param methodId an int giving our unique number for the method
   * @param receiver an Object giving the receiver for an instance method;
   *        use null for an uninitialized object
   */
  public static void methodEntry (int methodId, Object receiver) {
    if (countCalls) ++numMethodEntry;
    if (engaged == PHASE_LIVE) {
      _methodEntry(methodId, receiver, false);
    }
  }
	
  /**
   * used to mark exit from a method
   * @param exception exception object if an exception is being thrown, otherwise null
   * @param methodId an int giving our unique number for the method
   * @param receiver an Object giving the receiver for an instance method;
   *        use null for a static method
   * @param isStatic a boolean indicating whether the method is static
   */
  private static native void _methodExit (Object exception, int methodId,
                                          Object receiver, boolean isStatic);

  private static long numMethodExitStatic = 0;

  /**
   * used to mark exit from a static method
   * @param methodId an int giving our unique number for the method
   */
  public static void methodExitStatic (int methodId) {
    if (countCalls) ++numMethodExitStatic;
    if (engaged == PHASE_LIVE) {
      _methodExit(null, methodId, null, true);
    }		
  }
	
  private static long numMethodExit = 0;

  /**
   * used to mark exit from an instance method
   * @param methodId an int giving our unique number for the method
   * @param receiver an Object giving the receiver for the method;
   * can be null in a constructor
   */
  public static void methodExit (int methodId, Object receiver) {
    if (countCalls) ++numMethodExit;
    if (engaged == PHASE_LIVE) {
      _methodExit(null, methodId, receiver, false);
    }
  }
	
  private static long numMethodExceptionStatic = 0;

  /**
   * used to mark exceptional exit from a static method
   * @param exception an Object giving the exception being thrown
   * @param methodId an int giving our unique number for the method
   */
  public static void methodExceptionStatic (Object exception, int methodId) {
    if (countCalls) ++numMethodExceptionStatic;
    if (engaged == PHASE_LIVE) {
      _methodExit(exception, methodId, null, true);
    }
  }
	
  private static long numMethodException = 0;

  /**
   * used to mark exceptional exit from an instance method
   * @param exception an Object giving the exception being thrown
   * @param methodId an int giving our unique number for the method
   * @param receiver an Object giving the receiver for the method;
   *        this will be null for a constructor
   */
  public static void methodException (Object exception, int methodId, Object receiver) {
    if (countCalls) ++numMethodException;
    if (engaged == PHASE_LIVE) {
      _methodExit(exception, methodId, receiver, false);
    }
  }
	
  private static native void _exceptionThrow (Object exception, int methodId,
                                              Object receiver, boolean isStatic);

  private static long numExceptionThrow = 0;

  /**
   * used to indicate we are about to throw an exception;
   * this is different from methodException, which indicates we are
   * passing an exception through to our caller
   * @param exception an Object giving the exception being thrown
   * @param methodId an int giving our id for the method throwing the exception
   * @param receiver an Object giving the receiver of this method;
   * may be null in a constructor
   */
  public static void exceptionThrow (Object exception, int methodId, Object receiver) {
    if (countCalls) ++numExceptionThrow;
    if (engaged == PHASE_LIVE) {
      _exceptionThrow(exception, methodId, receiver, false);
    }
  }

  private static long numExceptionThrowStatic = 0;

  /**
   * used to indicate we are about to throw an exception from a static method;
   * this is different from methodExceptionStatic, which indicates we are
   * passing an exception through to our caller
   * @param exception an Object giving the exception being thrown
   * @param methodId an int giving our id for the method throwing the exception
   */
  public static void exceptionThrowStatic (Object exception, int methodId) {
    if (countCalls) ++numExceptionThrowStatic;
    if (engaged == PHASE_LIVE) {
      _exceptionThrow(exception, methodId, null, true);
    }
  }

  private static native void _exceptionHandle (Object exception, int methodId,
                                               Object receiver, boolean isStatic);

  private static long numExceptionHandle = 0;

  /**
   * used to indicate we are about to handle an exception
   * @param exception an Object giving the exception being thrown
   * @param methodId an int giving our id for the method handling the exception
   * @param receiver an Object giving the receiver of this method;
   * may be null in a constructor
   */
  public static void exceptionHandle (Object exception, int methodId, Object receiver) {
    if (countCalls) ++numExceptionHandle;
    if (engaged == PHASE_LIVE) {
      _exceptionHandle(exception, methodId, receiver, false);
    }
  }

  private static long numExceptionHandleStatic = 0;

  /**
   * used to indicate we are about to handle an exception in a static method
   * @param exception an Object giving the exception being thrown
   * @param methodId an int giving our id for the method throwing the exception
   */
  public static void exceptionHandleStatic (Object exception, int methodId) {
    if (countCalls) ++numExceptionHandleStatic;
    if (engaged == PHASE_LIVE) {
      _exceptionHandle(exception, methodId, null, true);
    }
  }

  /**
   * for stamping uninitialized this at start of a constructor
   */
  private static native void _stampThis (String className);

  private static long numStampThis = 0;

  /**
   * used to stamp an uninitialized this in a constructor
   */
  public static void stampThis (String className) {
    if (countCalls) ++numStampThis;
    if (engaged == PHASE_LIVE) {
      _stampThis(className);
    }
  }

  /**
   * used to timestamp up to 5 objects
   * @param num an int indicating how many objects to timestamp;
   *        must lie in the range 1 .. 5; arguments past the given count are ignored
   * @param obj0 an Object timestamped if num >= 1 (i.e., always)
   * @param obj1 an Object timestamped if num > 1
   * @param obj2 an Object timestamped if num > 2
   * @param obj3 an Object timestamped if num > 3
   * @param obj4 an Object timestamped if num > 4
   */
  public static native void _varEvent (int num, Object obj0, Object obj1,
                                       Object obj2, Object obj3, Object obj4);

  private static long numAAStoreEvent = 0;

  /**
   * called to store an element in an array of Objects
   * @param array an Object giving the array
   * @param idx an int giving the index of the slot being updated
   * @param val an Object giving the new value to store in the slot
   */
  public static void AAStoreEvent (Object[] array, int idx, Object val)
      throws NullPointerException, ArrayIndexOutOfBoundsException, ArrayStoreException {
    if (countCalls) ++numAAStoreEvent;
    if (engaged == PHASE_LIVE) {
      _arrayStore(array, idx, val);
    }
    array[idx] = val;
  }

  private static long numCAStoreEvent = 0;

  /**
   * called to store an element in an array of characters
   * @param array an Object giving the array
   * @param idx an int giving the index of the slot being updated
   * @param val a char giving the new value to store in the slot
   */
  public static void CAStoreEvent (char[] array, int idx, char val)
    throws NullPointerException, ArrayIndexOutOfBoundsException {
    if (countCalls) ++numCAStoreEvent;
    if (engaged == PHASE_LIVE) {
      _varEvent(1, array, null, null, null, null);
    }
    array[idx] = val;
  }

  private static long numSAStoreEvent = 0;

  /**
   * called to store an element in an array of shorts
   * @param array an Object giving the array
   * @param idx an int giving the index of the slot being updated
   * @param val an int giving the new value to store in the slot
   */
  public static void SAStoreEvent (short[] array, int idx, int val)
    throws NullPointerException, ArrayIndexOutOfBoundsException {
    if (countCalls) ++numSAStoreEvent;
    if (engaged == PHASE_LIVE) {
      _varEvent(1, array, null, null, null, null);
    }
    array[idx] = (short)val;
  }

  private static long numIAStoreEvent = 0;

  /**
   * called to store an element in an array of ints
   * @param array an Object giving the array
   * @param idx an int giving the index of the slot being updated
   * @param val an int giving the new value to store in the slot
   */
  public static void IAStoreEvent (int[] array, int idx, int val)
    throws NullPointerException, ArrayIndexOutOfBoundsException {
    if (countCalls) ++numIAStoreEvent;
    if (engaged == PHASE_LIVE) {
      _varEvent(1, array, null, null, null, null);
    }
    array[idx] = val;
  }

  private static long numLAStoreEvent = 0;

  /**
   * called to store an element in an array of longs
   * @param array an Object giving the array
   * @param idx an int giving the index of the slot being updated
   * @param val a long giving the new value to store in the slot
   */
  public static void LAStoreEvent (long[] array, int idx, long val)
    throws NullPointerException, ArrayIndexOutOfBoundsException {
    if (countCalls) ++numLAStoreEvent;
    if (engaged == PHASE_LIVE) {
      _varEvent(1, array, null, null, null, null);
    }
    array[idx] = val;
  }

  private static long numFAStoreEvent = 0;

  /**
   * called to store an element in an array of floats
   * @param array an Object giving the array
   * @param idx an int giving the index of the slot being updated
   * @param val a float giving the new value to store in the slot
   */
  public static void FAStoreEvent (float[] array, int idx, float val)
    throws NullPointerException, ArrayIndexOutOfBoundsException {
    if (countCalls) ++numFAStoreEvent;
    if (engaged == PHASE_LIVE) {
      _varEvent(1, array, null, null, null, null);
    }
    array[idx] = val;
  }

  private static long numDAStoreEvent = 0;

  /**
   * called to store an element in an array of doubles
   * @param array an Object giving the array
   * @param idx an int giving the index of the slot being updated
   * @param val a double giving the new value to store in the slot
   */
  public static void DAStoreEvent (double[] array, int idx, double val)
    throws NullPointerException, ArrayIndexOutOfBoundsException {
    if (countCalls) ++numDAStoreEvent;
    if (engaged == PHASE_LIVE) {
      _varEvent(1, array, null, null, null, null);
    }
    array[idx] = val;
  }

  private static long numCALoadEvent = 0;

  /**
   * called to load an element from an array of characters
   * @param array an Object giving the array
   * @param idx an int giving the index of the slot to load
   * @result the character in the slot
   */
  public static char CALoadEvent (char[] array, int idx)
    throws NullPointerException, ArrayIndexOutOfBoundsException {
    if (countCalls) ++numCALoadEvent;
    if (engaged == PHASE_LIVE) {
      _varEvent(1, array, null, null, null, null);
    }
    return array[idx];
  }

  private static long numSALoadEvent = 0;

  /**
   * called to load an element from an array of shorts
   * @param array an Object giving the array
   * @param idx an int giving the index of the slot to load
   * @result the short in the slot
   */
  public static short SALoadEvent (short[] array, int idx)
    throws NullPointerException, ArrayIndexOutOfBoundsException {
    if (countCalls) ++numSALoadEvent;
    if (engaged == PHASE_LIVE) {
      _varEvent(1, array, null, null, null, null);
    }
    return array[idx];
  }

  private static long numIALoadEvent = 0;

  /**
   * called to load an element from an array of ints
   * @param array an Object giving the array
   * @param idx an int giving the index of the slot to load
   * @result the int in the slot
   */
  public static int IALoadEvent (int[] array, int idx)
    throws NullPointerException, ArrayIndexOutOfBoundsException {
    if (countCalls) ++numIALoadEvent;
    if (engaged == PHASE_LIVE) {
      _varEvent(1, array, null, null, null, null);
    }
    return array[idx];
  }

  private static long numLALoadEvent = 0;

  /**
   * called to load an element from an array of longs
   * @param array an Object giving the array
   * @param idx an int giving the index of the slot to load
   * @result the long in the slot
   */
  public static long LALoadEvent (long[] array, int idx)
    throws NullPointerException, ArrayIndexOutOfBoundsException {
    if (countCalls) ++numLALoadEvent;
    if (engaged == PHASE_LIVE) {
      _varEvent(1, array, null, null, null, null);
    }
    return array[idx];
  }

  private static long numFALoadEvent = 0;

  /**
   * called to load an element from an array of floats
   * @param array an Object giving the array
   * @param idx an int giving the index of the slot to load
   * @result the float in the slot
   */
  public static float FALoadEvent (float[] array, int idx)
    throws NullPointerException, ArrayIndexOutOfBoundsException {
    if (countCalls) ++numFALoadEvent;
    if (engaged == PHASE_LIVE) {
      _varEvent(1, array, null, null, null, null);
    }
    return array[idx];
  }

  private static long numDALoadEvent = 0;

  /**
   * called to load an element from an array of doubles
   * @param array an Object giving the array
   * @param idx an int giving the index of the slot to load
   * @result the double in the slot
   */
  public static double DALoadEvent (double[] array, int idx)
    throws NullPointerException, ArrayIndexOutOfBoundsException {
    if (countCalls) ++numDALoadEvent;
    if (engaged == PHASE_LIVE) {
      _varEvent(1, array, null, null, null, null);
    }
    return array[idx];
  }

  private static long numVarEvent1 = 0;

  /**
   * Timestamp one object
   * @param obj the Object to timestamp
   */
  public static void varEvent1 (Object obj) {
    if (countCalls) ++numVarEvent1;
    if (engaged == PHASE_LIVE) {
      _varEvent(1, obj, null, null, null, null);
    }
  }

  private static long numVarEvent2 = 0;

  /**
   * Timestamp two objects
   * @param obj0 an Object to timestamp
   * @param obj1 an Object to timestamp
   */
  public static void varEvent2 (Object obj0, Object obj1) {
    if (countCalls) ++numVarEvent2;
    if (engaged == PHASE_LIVE) {
      _varEvent(2, obj0, obj1, null, null, null);
    }	
  }

  private static long numVarEvent3 = 0;

  /**
   * Timestamp three objects
   * @param obj0 an Object to timestamp
   * @param obj1 an Object to timestamp
   * @param obj2 an Object to timestamp
   */
  public static void varEvent3 (Object obj0, Object obj1, Object obj2) {
    if (countCalls) ++numVarEvent3;
    if (engaged == PHASE_LIVE) {
      _varEvent(3, obj0, obj1, obj2, null, null);
    }
  }

  private static long numVarEvent4 = 0;

  /**
   * Timestamp four objects
   * @param obj0 an Object to timestamp
   * @param obj1 an Object to timestamp
   * @param obj2 an Object to timestamp
   * @param obj3 an Object to timestamp
   */
  public static void varEvent4 (Object obj0, Object obj1, Object obj2, Object obj3) {
    if (countCalls) ++numVarEvent4;
    if (engaged == PHASE_LIVE) {
      _varEvent(4, obj0, obj1, obj2, obj3, null);
    }
  }

  private static long numVarEvent5 = 0;

  /**
   * Timestamp four objects
   * @param obj0 an Object to timestamp
   * @param obj1 an Object to timestamp
   * @param obj2 an Object to timestamp
   * @param obj3 an Object to timestamp
   * @param obj4 an Object to timestamp
   */
  public static void varEvent5 (Object obj0, Object obj1, Object obj2, Object obj3, Object obj4) {
    if (countCalls) ++numVarEvent5;
    if (engaged == PHASE_LIVE) {
      _varEvent(5, obj0, obj1, obj2, obj3, obj4);
    }
  }

  /**
   * gives opportunity to record the base for a static field query
   * (the information returned from a call to Unsafe.staticFieldBase)
   * @param base the Object returned by the staticFieldBase call
   * @param f the Field that was queried
   */
  private static native void _recordStaticFieldBase (Object base, Field f);

  private static long numRecordStaticFieldBase = 0;

  /**
   * gives opportunity to record the base for a static field query
   * (the information returned from a call to Unsafe.staticFieldBase)
   * public so that Unsafe can call it
   * @param base the Object returned by the staticFieldBase call
   * @param f the Field that was queried
   */
  public static void recordStaticFieldBase (Object base, Field f) {
    if (countCalls) ++numRecordStaticFieldBase;
    if (engaged == PHASE_LIVE) {
      _recordStaticFieldBase(base, f);
    }
  }

  /**
   * gives opportunity to record the offset for a static field query
   * (the information returned from a call to Unsafe.staticFieldOffset);
   * @param offset the long returned by the staticFieldOffset call
   * @param f the Field that was queried
   */ 
  private static native void _recordStaticFieldOffset (long offset, Field f);

  private static long numRecordStaticFieldOffset = 0;

  /**
   * gives opportunity to record the offset for a static field query
   * (the information returned from a call to Unsafe.staticFieldOffset);
   * public so that Unsafe can call it
   * @param offset the long returned by the staticFieldOffset call
   * @param f the Field that was queried
   */
  public static void recordStaticFieldOffset (long offset, Field f) {
    if (countCalls) ++numRecordStaticFieldBase;
    if (engaged == PHASE_LIVE) {
      _recordStaticFieldOffset(offset, f);
    }
  }

  /**
   * gives opportunity to record the offset for a non-static field query
   * (the information returned from a call to Unsafe.objectFieldOffset);
   * @param offset the long returned by the objectFieldOffset call
   * @param f the Field that was queried
   */
  private static native void _recordObjectFieldOffset (long offset, Field f);

  private static long numRecordObjectFieldOffset = 0;

  /**
   * gives opportunity to record the offset for a non-static field query
   * (the information returned from a call to Unsafe.objectFieldOffset);
   * public so that Unsafe can call it
   * @param offset the long returned by the objectFieldOffset call
   * @param f the Field that was queried
   */
  public static void recordObjectFieldOffset (long offset, Field f) {
    if (countCalls) ++numRecordObjectFieldOffset;
    if (engaged == PHASE_LIVE) {
      _recordObjectFieldOffset(offset, f);
    }
  }

  /**
   * gives opportunity to record the offset for an array base query
   * (the information returned from a call to Unsafe.arrayBaseOffset);
   * @param offset the int returned by the arrayBaseOffset call
   * @param cls the Class that was queried
   */
  private static native void _recordArrayBaseOffset (int offset, Class cls);

  private static long numRecordArrayBaseOffset = 0;

  /**
   * gives opportunity to record the offset for an array base query
   * (the information returned from a call to Unsafe.arrayBaseOffset);
   * @param offset the int returned by the arrayBaseOffset call
   * @param cls the Class that was queried
   */
  public static void recordArrayBaseOffset (int offset, Class cls) {
    if (countCalls) ++numRecordArrayBaseOffset;
    if (engaged == PHASE_LIVE) {
      _recordArrayBaseOffset(offset, cls);
    }
  }

  /**
   * gives opportunity to record the scale for an array index scale query
   * (the information returned from a call to Unsafe.arrayIndexScale);
   * @param scale the int returned by the arrayIndexScale call
   * @param cls the Class that was queried
   */
  private static native void _recordArrayIndexScale (int scale, Class cls);

  private static long numRecordArrayIndexScale = 0;

  /**
   * gives opportunity to record the scale for an array index scale query
   * (the information returned from a call to Unsafe.arrayIndexScale);
   * @param scale the int returned by the arrayIndexScale call
   * @param cls the Class that was queried
   */
  public static void recordArrayIndexScale (int scale, Class cls) {
    if (countCalls) ++numRecordArrayIndexScale;
    if (engaged == PHASE_LIVE) {
      _recordArrayIndexScale(scale, cls);
    }
  }

  private static long numDoSystemArraycopy = 0;

  /**
   * calls our special native version of System.Arraycopy, which updates the shadow heap;
   * arguments are as for System.arraycopy
   */
  private static native boolean _doSystemArraycopy (Object src, int srcIdx,
                                                    Object dst, int dstIdx, int cnt);

  public static void doSystemArraycopy (Object src, int srcIdx, Object dst, int dstIdx, int cnt) {
    if (countCalls) ++numDoSystemArraycopy;
    if (engaged == PHASE_LIVE &&
        _doSystemArraycopy (src, srcIdx, dst, dstIdx, cnt)) {
      return;
    }
    System.arraycopy(src, srcIdx, dst, dstIdx, cnt);
  }

  /**
   * pass updated counts of number of (bytecode) instructions so far,
   * number of heap reads, etc., to the agent
   */
  private static native void _counts (int heapReads   , int heapWrites,
                                      int heapRefReads, int HeapRefWrites,
                                      int instCount);

  private static long numCounts = 0;

  /**
   * take a packed set of counts, unpack, and pass them to the agent
   * @param packed an int giving the five packed counts, laid out as
   * rrrrrrwwwwwwRRRRRRWWWWWWIIIIIIII (high to low)
   */
  public static void counts (int packed) {
    if (countCalls) ++numCounts;
    if (engaged == PHASE_LIVE) {
      int instCount     = (packed & ((1 << 8) - 1)); packed >>>= 8;
      int heapRefWrites = (packed & ((1 << 6) - 1)); packed >>>= 6;
      int heapRefReads  = (packed & ((1 << 6) - 1)); packed >>>= 6;
      int heapWrites    = (packed & ((1 << 6) - 1)); packed >>>= 6;
      int heapReads     = (packed & ((1 << 6) - 1));
      _counts(heapReads, heapWrites, heapRefReads, heapRefWrites, instCount);
    }
  }

  private static long numLongCounts = 0;

  /**
   * As for counts(int) except the fields are twice as wide
   * @param packed a long giving the five packed counts, laid out as
   * rrrrrrrrrrrrwwwwwwwwwwwwRRRRRRRRRRRRWWWWWWWWWWWWIIIIIIIIIIIIIIII (high to low)
   */
  public static void longCounts (long packed) {
    if (countCalls) ++numLongCounts;
    if (engaged == PHASE_LIVE) {
      int instCount     = ((int)packed) & ((1 << 16) - 1); packed >>>= 16;
      int heapRefWrites = ((int)packed) & ((1 << 12) - 1); packed >>>= 12;
      int heapRefReads  = ((int)packed) & ((1 << 12) - 1); packed >>>= 12;
      int heapWrites    = ((int)packed) & ((1 << 12) - 1); packed >>>= 12;
      int heapReads     = ((int)packed) & ((1 << 12) - 1);
      _counts(heapReads, heapWrites, heapRefReads, heapRefWrites, instCount);
    }
  }

  public static void liveHookStats () {
    System.err.printf("ElephantTracks counts start%n");
    System.err.printf("numPointerUpdated3 = %d%n", numPointerUpdated3);
    System.err.printf("numPointerUpdated2 = %d%n", numPointerUpdated2);
    System.err.printf("numPointerUpdated3off = %d%n", numPointerUpdated3off);
    System.err.printf("numReferentUpdated = %d%n", numReferentUpdated);
    System.err.printf("numArrayStore = %d%n", numArrayStore);
    System.err.printf("numUninitPutfield = %d%n", numUninitPutfield);
    System.err.printf("numNewobj = %d%n", numNewobj);
    System.err.printf("numNewobj2 = %d%n", numNewobj2);
    System.err.printf("numNewarray = %d%n", numNewarray);
    System.err.printf("numMultiNewArray = %d%n", numMultiNewArray);
    System.err.printf("numMethodEntryStatic = %d%n", numMethodEntryStatic);
    System.err.printf("numMethodEntry = %d%n", numMethodEntry);
    System.err.printf("numMethodExitStatic = %d%n", numMethodExitStatic);
    System.err.printf("numMethodExit = %d%n", numMethodExit);
    System.err.printf("numMethodExceptionStatic = %d%n", numMethodExceptionStatic);
    System.err.printf("numMethodException = %d%n", numMethodException);
    System.err.printf("numExceptionThrow = %d%n", numExceptionThrow);
    System.err.printf("numExceptionThrowStatic = %d%n", numExceptionThrowStatic);
    System.err.printf("numExceptionHandle = %d%n", numExceptionHandle);
    System.err.printf("numExceptionHandleStatic = %d%n", numExceptionHandleStatic);
    System.err.printf("numStampThis = %d%n", numStampThis);
    System.err.printf("numAAStoreEvent = %d%n", numAAStoreEvent);
    System.err.printf("numCAStoreEvent = %d%n", numCAStoreEvent);
    System.err.printf("numSAStoreEvent = %d%n", numSAStoreEvent);
    System.err.printf("numIAStoreEvent = %d%n", numIAStoreEvent);
    System.err.printf("numLAStoreEvent = %d%n", numLAStoreEvent);
    System.err.printf("numFAStoreEvent = %d%n", numFAStoreEvent);
    System.err.printf("numDAStoreEvent = %d%n", numDAStoreEvent);
    System.err.printf("numCALoadEvent = %d%n", numCALoadEvent);
    System.err.printf("numSALoadEvent = %d%n", numSALoadEvent);
    System.err.printf("numIALoadEvent = %d%n", numIALoadEvent);
    System.err.printf("numLALoadEvent = %d%n", numLALoadEvent);
    System.err.printf("numFALoadEvent = %d%n", numFALoadEvent);
    System.err.printf("numDALoadEvent = %d%n", numDALoadEvent);
    System.err.printf("numVarEvent1 = %d%n", numVarEvent1);
    System.err.printf("numVarEvent2 = %d%n", numVarEvent2);
    System.err.printf("numVarEvent3 = %d%n", numVarEvent3);
    System.err.printf("numVarEvent4 = %d%n", numVarEvent4);
    System.err.printf("numVarEvent5 = %d%n", numVarEvent5);
    System.err.printf("numRecordStaticFieldBase = %d%n", numRecordStaticFieldBase);
    System.err.printf("numRecordStaticFieldOffset = %d%n", numRecordStaticFieldOffset);
    System.err.printf("numRecordObjectFieldOffset = %d%n", numRecordObjectFieldOffset);
    System.err.printf("numRecordArrayBaseOffset = %d%n", numRecordArrayBaseOffset);
    System.err.printf("numRecordArrayIndexScale = %d%n", numRecordArrayIndexScale);
    System.err.printf("numDoSystemArraycopy = %d%n", numDoSystemArraycopy);
    System.err.printf("numCounts = %d%n", numCounts);
    System.err.printf("numLongCounts = %d%n", numLongCounts);
    System.err.printf("ElephantTracks counts end%n");
  }

  public static void liveHook () {
    System.out.println("liveHook called");
    if (countCalls) {
      Thread stats = new Thread() {
          public void run () {
            ElephantTracks.liveHookStats();
          }
        };
      Runtime rt = Runtime.getRuntime();
      rt.addShutdownHook(stats);
    }
  }

}

// Local Variables:
// mode:Java
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
