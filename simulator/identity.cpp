#include "parser.h"
#include "callback_handler.h"

/*Author: Nathan Ricci (nricci01@cs.tufts.edu)
 *This program is meant to be an example of how one might parse an
 *Elephant Tracks trace. 
 * It takes a trace on standard in, parses it, 
 * and produces an identical trace on standard out.
 */
int main() {
  CallbackHandler* cb = new IdentityCallbackHandler();
  Parser parser(&cin, &cin, cb);

  parser.read_trace();
}
