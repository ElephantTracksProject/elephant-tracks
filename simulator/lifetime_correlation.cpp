#include "parser.h"
#include "callback_handler.h"
#include "lifetime_correlation_callback_handler.h"

// TODO(nricci01): The name "lifetime correlation" is not meaningful.
//                 Think of something better.

/* Author: Nathan P. Ricci (nricci01@cs.tufts.edu)
 * This program takes an Elephant Tracks version 2 trace on standard in,
 * As out put, it produces a list of objects with the following format:
 *  
 * <object_id>  <allocation_context> <death_context> <death time in bytes> <death time in method time>

 * Where allocation_context has the following format;
 *
 *  method_id1,method_id2,site_id
 *
 * With method_id2 being the top of the stack when object_id was allocated.
 *
 * And death_context has the following format:
 *
 * method_id1, method_id2
 *
 * Where method_id2 is the method on the to of the stack when object_id died.
 *
 * Both death_context and allocation_context may be shorter than two methods
 * if fewer than two methods were on the stack at the time of the 
 * relevent event.
 *
 * They may also be empty, in which case the context is represented as ","
 *
 * 
 *
 */

int main() {
  CallbackHandler* cb = new LifetimeCorrelationCallbackHandler();
  Parser parser(&cin, &cin, cb);

  parser.read_trace();
}
