
#include "cctree.h"

extern int thread_number;
extern map<int, int> threadIdNumbering;

MethodMap Method::allMethods;

int CCNode::count = 0;

CCNode * CCNode::demand_child(int id, int thread_id, int time)
{
  for (CCNodeVector::iterator p = children.begin();
    p != children.end();
    p++)
  {
    CCNode * child = (*p);
    if (child->method_id == id) {
      child->setLastCall(time);
      return child;
    }
  }
  
  // -- Not Found
  CCNode * new_child = new CCNode(id, thread_id, time, this);
  children.push_back(new_child);
  return new_child;
}

void CCNode::computeTotals()
{
  // -- Start with this node's values

  incTotalAllocBytes(getAllocBytes());
  incTotalAllocObjects(getAllocObjects());
  incTotalDeadBytes(getDeadBytes());
  incTotalDeadObjects(getDeadObjects());

  // -- Compute totals for any children, then add up
  
  const CCNodeVector & children = getChildren();
  for (CCNodeVector::const_iterator p = children.begin();
       p != children.end();
       p++)
    {
      CCNode * child = (*p);
      child->computeTotals();

      incTotalAllocBytes(child->getTotalAllocBytes());
      incTotalAllocObjects(child->getTotalAllocObjects());
      incTotalDeadBytes(child->getTotalDeadBytes());
      incTotalDeadObjects(child->getTotalDeadObjects());
    }
}

string CCNode::getMethodFullName()
{
  Method * meth = Method::getMethod(getMethodId());
  string result;
  if (meth)
    result = meth->getClassName() + "::" + meth->getMethodName();
  else
    result = "ENTRY";

  return result;
}

void CCNode::printTree(int depth)
{
  if (getTotalAllocBytes() == 0 and
      getTotalAllocObjects() == 0)
    return;

  for (int i = 0; i < depth; i++)
    cout << ".";

  if (depth > 0)
    cout << " ";

  cout << getMethodFullName() << " ";

  cout << getTotalAllocBytes() << " "
       << getAllocRank() << " "
       << getTotalAllocObjects() << "  "
       << getCalls() << endl;

  const CCNodeVector & children = getChildren();
  for (CCNodeVector::const_iterator p = children.begin();
       p != children.end();
       p++)
    {
      CCNode * child = (*p);
      child->printTree(depth+1);
    }
}

void CCNode::collectNodes(CCNodeVector & all)
{
  all.push_back(this);
  const CCNodeVector & children = getChildren();
  for (CCNodeVector::const_iterator p = children.begin();
       p != children.end();
       p++)
    {
      CCNode * child = (*p);
      child->collectNodes(all);
    }
}  

bool compareNodes(CCNode * one, CCNode * two)
{
  return one->getTotalAllocBytes() < two->getTotalAllocBytes();
}

void CCNode::rankNodes()
{
  CCNodeVector all;
  collectNodes(all);
  sort(all.begin(), all.end(), compareNodes);

  int rank = all.size();
  for (CCNodeVector::const_iterator p = all.begin();
       p != all.end();
       p++)
    {
      CCNode * child = (*p);
      child->setAllocRank(rank);
      rank--;
    }
}

void CCNode::printStack()
{
  CCNode * cur = this;
  while (cur) {
    cout << "  " << cur->getMethodFullName() << "(0x" << hex << cur->getMethodId() << dec << ")" << endl;
    cur = cur->getParent();
  }
}

void CCNode::emitPath(ofstream & out)
{
  CCNode * parent = getParent();
  if (parent != 0)
    parent->emitPath(out);

  out << "\t" << getMethodFullName();
}

void CCNode::emitTreeMapTM3Rec(ofstream & out)
{
  int abytes = getTotalAllocBytes();
  int dbytes = getTotalDeadBytes();
  
  if (abytes > 0 or dbytes > 0) {
    const CCNodeVector & children = getChildren();
    
    // -- Compute total size of children
    int child_total_alloc = 0;
    int child_total_dead = 0;
    int num_children = 0;
    for (CCNodeVector::const_iterator p = children.begin();
      p != children.end();
      p++)
    {
      CCNode * child = (*p);
      int child_alloc = child->getTotalAllocBytes();
      int child_dead =  child->getTotalDeadBytes();
      child_total_alloc += child_alloc;
      child_total_dead  += child_dead;
      if (child_alloc > 0 or child_dead > 0)
        num_children++;
    }
    
    // -- Output entry for this node
    out << (abytes+dbytes) << "\t";
    out << getCalls() << "\t";
    out << "0" << "\t";
    out << getThreadId() << "\t";
    out << getFirstCall() << "\t";
    emitPath(out);
    out << endl;
    
    // -- output synthetic node representing the this node's contribution (if necessary)
    if (num_children > 0) {
      int alloc_diff = abytes - child_total_alloc;
      int dead_diff = dbytes - child_total_dead;
      if (alloc_diff > 0) {
        out << alloc_diff << "\t";
        out << getCalls() << "\t";
        out << "0" << "\t";
        out << getThreadId() << "\t";
        out << getFirstCall() << "\t";
        emitPath(out);
        out << "\talloc";
        out << endl;
      }
      if (dead_diff > 0) {
        out << dead_diff << "\t";
        out << getCalls() << "\t";
        out << "0" << "\t";
        out << getThreadId() << "\t";
        out << getFirstCall() << "\t";
        emitPath(out);
        out << "\tdead";
        out << endl;
      }
    }
    
    // -- Output the children
    for (CCNodeVector::const_iterator p = children.begin();
      p != children.end();
      p++)
    {
      CCNode * child = (*p);
      child->emitTreeMapTM3Rec(out);
    }
  }
}

void CCNode::emitTreeMapTM3(ofstream & out)
{
  out << "Bytes\tCalls\tLifetime\tThread\tFirst call" << endl;
  out << "INTEGER\tINTEGER\tINTEGER\tINTEGER\tINTEGER" << endl;
  
  emitTreeMapTM3Rec(out);
}

/* emitTreeML

   Emit the calling context tree in TreeML format, for use in prefuse
*/

void CCNode::emitTreeML(ofstream & out)
{
  out << "<tree>" << endl;
  out << " <declarations>" << endl;
  out << "  <attributeDecl name=\"name\" type=\"String\"/>" << endl;
  out << "  <attributeDecl name=\"class\" type=\"String\"/>" << endl;
  out << "  <attributeDecl name=\"bytes\" type=\"Long\"/>" << endl;
  out << "  <attributeDecl name=\"alloc\" type=\"Long\"/>" << endl;
  out << "  <attributeDecl name=\"dead\" type=\"Long\"/>" << endl;
  out << "  <attributeDecl name=\"objects\" type=\"Long\"/>" << endl;
  out << "  <attributeDecl name=\"objalloc\" type=\"Long\"/>" << endl;
  out << "  <attributeDecl name=\"objdead\" type=\"Long\"/>" << endl;
  out << "  <attributeDecl name=\"thread\" type=\"Long\"/>" << endl;
  out << " </declarations>" << endl;
  
  emitTreeMLRec(out, 1);
  
  out << "</tree>" << endl;
}

void CCNode::emitTreeMLRec(ofstream & out, int depth)
{
  int abytes = getTotalAllocBytes();
  int dbytes = getTotalDeadBytes();
  
  if (abytes + dbytes > 0) {
    
    const CCNodeVector & children = getChildren();
    bool is_leaf = (children.size() == 0);
    
    if (is_leaf) {
      // -- Leaf
      out << "<leaf>" << endl;
    } else {
      out << "<branch>" << endl;
    }
    
    Method * meth = Method::getMethod(getMethodId());
    string nm;
    string classnm;
    if (meth) {
      nm = meth->getMethodName();
      classnm = meth->getClassName();
    } else {
      nm = "ENTRY";
      classnm = "";
    }
    
    int i = nm.find("<init>");
    if (i != string::npos)
      nm.replace(i, i+6, "&lt;init&gt;");
    int j = nm.find("<clinit>");
    if (j != string::npos)
      nm.replace(j, j+8, "&lt;clinit&gt;");
    out << "<attribute name=\"name\" value=\"" << nm << "\"/>" << endl;
    out << "<attribute name=\"class\" value=\"" << classnm << "\"/>" << endl;
    out << "<attribute name=\"bytes\" value=\"" << (abytes+dbytes) << "\"/>" << endl;
    out << "<attribute name=\"alloc\" value=\"" << abytes << "\"/>" << endl;
    out << "<attribute name=\"dead\" value=\"" << dbytes << "\"/>" << endl;
    out << "<attribute name=\"objects\" value=\"" << (getAllocObjects() + getDeadObjects()) << "\"/>" << endl;
    out << "<attribute name=\"objalloc\" value=\"" << getAllocObjects() << "\"/>" << endl;
    out << "<attribute name=\"objdead\" value=\"" << getDeadObjects() << "\"/>" << endl;
    out << "<attribute name=\"thread\" value=\"" << threadIdNumbering[getThreadId()] << "\"/>" << endl;
    
    for (CCNodeVector::const_iterator p = children.begin();
      p != children.end();
      p++)
    {
      CCNode * child = (*p);
      child->emitTreeMLRec(out, depth+1);
    }
    
    if (is_leaf) {
      // -- Leaf
      out << "</leaf>" << endl;
    } else {
      out << "</branch>" << endl;
    }
  }
}

