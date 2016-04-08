#include "parser.h"
#include "callback_handler.h"
#include "escape_analysis_callback_handler.h"


/* Author: Nathan P. Ricci (nricci01@cs.tufts.edu)
 *
 * This program reads an Elephant Tracks version 2 trace in on standard in,
 * and reports an escape analysis on standard out.
 * The escape analysis simply counts how many objects escape their
 * allocating context.
 */

int main() {
  EscapeAnalysisCallbackHandler* cb = new EscapeAnalysisCallbackHandler();
  Parser parser(&cin, &cin, cb);

  parser.read_trace();


  unsigned long objectsAllocated = cb->getObjectsAllocated();
  unsigned long escapees = cb->getEscapees();

  cout << "Objects Allocated: " << objectsAllocated << endl;
  cout << "Objects Escaped: " << escapees << endl;
  cout << "Escape ratio: " << (double)escapees/objectsAllocated << endl;
}
