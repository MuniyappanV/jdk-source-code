#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)node.hpp	1.186 04/05/04 13:26:15 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// Portions of code courtesy of Clifford Click

// Optimization - Graph Style


class AddNode;
class AddPNode;
class AliasInfo;
class Block;
class Block_Array;
class BoolNode;
class BoxLockNode;
class CMoveNode;
class CallNode;
class CatchNode;
class CatchProjNode;
class CmpNode;
class CodeBuffer;
class ConNode;
class CountedLoopNode;
class FastLockNode;
class IfNode;
class JVMState;
class JumpNode;
class JumpProjNode;
class LoadNode;
class LoopNode;
class MachNode;
class MachSpillCopyNode;
class Matcher;
class MemBarNode;
class MemNode;
class MergeMemNode;
class MulNode;
class MultiNode;
class Node;
class Node_Array;
class Node_List;
class Node_Stack;
class NullCheckNode;
class OopMap;
class PCTableNode;
class PhaseCCP;
class PhaseGVN;
class PhaseIterGVN;
class PhaseRegAlloc;
class PhaseTransform;
class PhaseValues;
class PhiNode;
class Pipeline;
class ProjNode;
class RegMask;
class RegionNode;
class RootNode;
class SafePointNode;
class StartC2INode;
class StartI2CNode;
class StartNode;
class State;
class StoreNode;
class SubNode;
class Type;
class TypeNode;
class VectorSet;
typedef void (*NFunc)(Node&,void*);
extern "C" {
  typedef int (*C_sort_func_t)(const void *, const void *); 
}

// The type of all node counts and indexes.
// It must hold at least 16 bits, but must also be fast to load and store.
// This type, if less than 32 bits, could limit the number of possible nodes.
// (To make this type platform-specific, move to globalDefinitions_xxx.hpp.)
typedef unsigned int node_idx_t;


#ifndef OPTO_DU_ITERATOR_ASSERT
#ifdef ASSERT
#define OPTO_DU_ITERATOR_ASSERT 1
#else
#define OPTO_DU_ITERATOR_ASSERT 0
#endif
#endif //OPTO_DU_ITERATOR_ASSERT

#if OPTO_DU_ITERATOR_ASSERT
class DUIterator;
class DUIterator_Fast;
#else
typedef uint   DUIterator;
typedef Node** DUIterator_Fast;
typedef Node** DUIterator_Last;
#endif

// Node Sentinel
#define NodeSentinel (Node*)-1

// Unknown count frequency
#define COUNT_UNKNOWN (-1.0f)

//------------------------------Node-------------------------------------------
// Nodes define actions in the program.  They create values, which have types.
// They are both vertices in a directed graph and program primitives.  Nodes
// are labeled; the label is the "opcode", the primitive function in the lambda
// calculus sense that gives meaning to the Node.  Node inputs are ordered (so
// that "a-b" is different from "b-a").  The inputs to a Node are the inputs to
// the Node's function.  These inputs also define a Type equation for the Node.
// Solving these Type equations amounts to doing dataflow analysis.
// Control and data are uniformly represented in the graph.  Finally, Nodes
// have a unique dense integer index which is used to index into side arrays
// whenever I have phase-specific information.

class Node {
  // Lots of restrictions on cloning Nodes
  Node(const Node&);            // not defined; linker error to use these
  Node &operator=(const Node &rhs);

public:
  friend class Compile;
  #if OPTO_DU_ITERATOR_ASSERT
  friend class DUIterator_Common;
  friend class DUIterator;
  friend class DUIterator_Fast;
  friend class DUIterator_Last;
  #endif

  // Because Nodes come and go, I define an Arena of Node structures to pull
  // from.  This should allow fast access to node creation & deletion.  This
  // field is a local cache of a value defined in some "program fragment" for
  // which these Nodes are just a part of.
  
  // New Operator that takes a Compile pointer, this will eventually
  // be the "new" New operator. 
  inline void* operator new( size_t x, Compile* C) {
    return C->node_arena()->Amalloc_4(x);
  }

  // New Operator that takes a Compile pointer, this will eventually
  // be the "new" New operator. 
  inline void* operator new( size_t x, int y, Compile* C) { 
    void* thsi = (char*)C->node_arena()->Amalloc_4(x+y*sizeof(void*))+y*sizeof(void*);
#ifdef ASSERT
    ((void**)thsi)[0-y] = thsi; // magic cookie for assertion check
#endif
    return thsi; 
  }  

  // The code is duplicated here because there is a speed
  // penalty if we call the "new" new from here.  These
  // operators will be deleted when the node work is done
  inline void* operator new( size_t x ) {
    return Compile::current()->node_arena()->Amalloc_4(x);
  }  

  // Version of new that also allocates Node array
  inline void* operator new( size_t x, int y ) {
    void* thsi = (char*)Compile::current()->node_arena()->Amalloc_4(x+y*sizeof(void*))+y*sizeof(void*);
#ifdef ASSERT
    ((void**)thsi)[0-y] = thsi; // magic cookie for assertion check
#endif
    return thsi;
  }  

  // Delete is a NOP
  void operator delete( void *ptr ) {}
  // Fancy destructor; eagerly attempt to reclaim Node numberings and storage
  void destruct();

  // Create a new Node.  Required is the number is of inputs required for
  // semantic correctness.
  Node( uint required );

  // Create a new Node with given input edges.
  // This version requires use of the "edge-count" new.  
  // E.g.  new (C,3) FooNode( C, NULL, left, right );
  Node( Node *n0 );
  Node( Node *n0, Node *n1 );
  Node( Node *n0, Node *n1, Node *n2 );
  Node( Node *n0, Node *n1, Node *n2, Node *n3 );
  Node( Node *n0, Node *n1, Node *n2, Node *n3, Node *n4 );
  Node( Node *n0, Node *n1, Node *n2, Node *n3,
            Node *n4, Node *n5, Node *n6 );

  // Clone an inherited Node given only the base Node type.
  virtual Node *clone( ) const;

//----------------- input edge handling
protected:
  friend class PhaseCFG;        // Access to address of _in array elements
  Node **_in;                   // Array of use-def references to Nodes
  Node **_out;                  // Array of def-use references to Nodes

  // Input edges are split into two catagories.  Required edges are required
  // for semantic correctness; order is important and NULLs are allowed.
  // Precedence edges are used to help determine execution order and are
  // added, e.g., for scheduling purposes.  They are unordered and not
  // duplicated; they have no embedded NULLs.  Edges from 0 to _cnt-1
  // are required, from _cnt to _max-1 are precedence edges.
  node_idx_t _cnt;              // Total number of required Node inputs.

  node_idx_t _max;              // Actual length of input array.

  // Output edges are an unordered list of def-use edges which exactly
  // correspond to required input edges which point from other nodes
  // to this one.  Thus the count of the output edges is the number of
  // users of this node.
  node_idx_t _outcnt;           // Total number of Node outputs.

  node_idx_t _outmax;           // Actual length of output array.

  // Grow the actual input array to the next larger power-of-2 bigger than len.
  void grow( uint len );
  // Grow the output array to the next larger power-of-2 bigger than len.
  void out_grow( uint len );

 public:
  // Each Node is assigned a unique small/dense number.  This number is used
  // to index into auxiliary arrays of data and bitvectors.
  // It is declared const to defend against inadvertant assignment,
  // since it is used by clients as a naked field.
  const node_idx_t _idx;

  // Get the (read-only) number of input edges
  uint req() const { return _cnt; }
  uint len() const { return _max; }
  // Get the (read-only) number of output edges
  uint outcnt() const { return _outcnt; }

#if OPTO_DU_ITERATOR_ASSERT
  // Iterate over the out-edges of this node.  Deletions are illegal.
  inline DUIterator outs() const;
  // Use this when the out array might have changed to suppress asserts.
  inline DUIterator& refresh_out_pos(DUIterator& i) const;
  // Does the node have an out at this position?  (Used for iteration.)
  inline bool has_out(DUIterator& i) const;
  inline Node*    out(DUIterator& i) const;
  // Iterate over the out-edges of this node.  All changes are illegal.
  inline DUIterator_Fast fast_outs(DUIterator_Fast& max) const;
  inline Node*    fast_out(DUIterator_Fast& i) const;
  // Iterate over the out-edges of this node, deleting one at a time.
  inline DUIterator_Last last_outs(DUIterator_Last& min) const;
  inline Node*    last_out(DUIterator_Last& i) const;
  // The inline bodies of all these methods are after the iterator definitions.
#else
  // Iterate over the out-edges of this node.  Deletions are illegal.
  // This iteration uses integral indexes, to decouple from array reallocations.
  DUIterator outs() const  { return 0; }
  // Use this when the out array might have changed to suppress asserts.
  DUIterator refresh_out_pos(DUIterator i) const { return i; }

  // Reference to the i'th output Node.  Error if out of bounds.
  Node*    out(DUIterator i) const { assert(i < _outcnt, "oob"); return _out[i]; }
  // Does the node have an out at this position?  (Used for iteration.)
  bool has_out(DUIterator i) const { return i < _outcnt; }

  // Iterate over the out-edges of this node.  All changes are illegal.
  // This iteration uses a pointer internal to the out array.
  DUIterator_Fast fast_outs(DUIterator_Fast& max) const {
    Node** out = _out;
    // Assign a limit pointer to the reference argument:
    max = out + (ptrdiff_t)_outcnt;
    // Return the base pointer:
    return out;
  }
  Node*    fast_out(DUIterator_Fast i) const  { return *i; }
  // Iterate over the out-edges of this node, deleting one at a time.
  // This iteration uses a pointer internal to the out array.
  DUIterator_Last last_outs(DUIterator_Last& min) const {
    Node** out = _out;
    // Assign a limit pointer to the reference argument:
    min = out;
    // Return the pointer to the start of the iteration:
    return out + (ptrdiff_t)_outcnt - 1;
  }
  Node*    last_out(DUIterator_Last i) const  { return *i; }
#endif

  // Reference to the i'th input Node.  Error if out of bounds.
  Node* in(uint i) const { assert(i < _max,"oob"); return _in[i]; }
  // Reference to the i'th output Node.  Error if out of bounds.
  // Use this accessor sparingly.  We are going trying to use iterators instead.
  Node* raw_out(uint i) const { assert(i < _outcnt,"oob"); return _out[i]; }
  // Return the unique out edge.
  Node* unique_out() const { assert(_outcnt==1,"not unique"); return _out[0]; }
  // Delete out edge at position 'i' by moving last out edge to position 'i'
  void  raw_del_out(uint i) { 
    assert(i < _outcnt,"oob"); 
    assert(_outcnt > 0,"oob"); 
    _out[i] = _out[--_outcnt];
    debug_only(_out[_outcnt] = NULL;)
  }
  // Set a required input edge, also updates corresponding output edge
  void add_req( Node *n ); // Append a NEW required input
  void add_req_batch( Node* n, uint m ); // Append m NEW required inputs (all n).
  void del_req( uint idx ); // Delete required edge & compact
  void ins_req( uint i, Node *n ); // Insert a NEW required input
  void set_req( uint i, Node *n ) {
    assert( i < _cnt, "oob");
    assert( !VerifyHashTableKeys || _hash_lock == 0,
            "remove node from hash table before modifying it");
    Node** p = &_in[i];    // cache this._in, across the del_out call
    if (*p != NULL)  (*p)->del_out((Node *)this);
    (*p) = n;
    if (n != NULL)      n->add_out((Node *)this);
  }
  // NULL out all inputs to eliminate incoming Def-Use edges.
  // Return the number of edges between 'n' and 'this'
  int  disconnect_inputs(Node *n);

  // Quickly, return true if and only if I am Compile::current()->top().
  bool is_top() const {
    assert((this == (Node*) Compile::current()->top()) == (_out == NULL), "");
    return (_out == NULL);
  }
  // Reaffirm invariants for is_top.  (Only from Compile::set_cached_top_node.)
  void setup_is_top();

private:
  // Add an output edge to the end of the list
  void add_out( Node *n ) {
    if (is_top())  return;
    if( _outcnt == _outmax ) out_grow(_outcnt);
    _out[_outcnt++] = n;
  }
  // Delete an output edge
  void del_out( Node *n ) {
    if (is_top())  return;
    Node** outp = &_out[_outcnt];
    // Find and remove n
    do {
      assert(outp > _out, "Missing Def-Use edge");
    } while (*--outp != n);
    *outp = _out[--_outcnt];
    // Smash the old edge so it can't be used accidentally.
    debug_only(_out[_outcnt] = (Node *)(uintptr_t)0xdeadbeef);
    // Record that a change happened here.
    #if OPTO_DU_ITERATOR_ASSERT
    debug_only(_last_del = n; ++_del_tick);
    #endif
  }

public:
  // Globally replace this node by a given new node, updating all uses.
  void replace_by(Node* new_node);
  void set_req_X( uint i, Node *n, PhaseIterGVN *igvn );
  // Find the one non-null required input.  RegionNode only
  Node *nonnull_req() const;
  // Add or remove precedence edges
  void add_prec( Node *n );
  void rm_prec( uint i );
  void set_prec( uint i, Node *n ) {
    assert( i >= _cnt, "not a precedence edge"); 
    if (_in[i] != NULL) _in[i]->del_out((Node *)this);
    _in[i] = n;
    if (n != NULL) n->add_out((Node *)this);
  }
  // Set this node's index, used by cisc_version to replace current node
  void set_idx(uint new_idx) {
    const node_idx_t* ref = &_idx;
    *(node_idx_t*)ref = new_idx;
  }
  // Swap input edge order.  (Edge indexes i1 and i2 are usually 1 and 2.)
  void swap_edges(uint i1, uint i2) {
    debug_only(uint check_hash = (VerifyHashTableKeys && _hash_lock) ? hash() : NO_HASH);
    // Def-Use info is unchanged
    Node* n1 = in(i1);
    Node* n2 = in(i2);
    _in[i1] = n2;
    _in[i2] = n1;
    // If this node is in the hash table, make sure it doesn't need a rehash.
    assert(check_hash == NO_HASH || check_hash == hash(), "edge swap must preserve hash code");
  }

  // Iterators over input Nodes for a Node X are written as:
  // for( i = 0; i < X.req(); i++ ) ... X[i] ...
  // NOTE: Required edges can contain embedded NULL pointers.

//----------------- Other Node Properties

  // Return a dense integer opcode number
  virtual int Opcode() const;
  
  // Virtual inherited Node size
  virtual uint size_of() const;

  // Other interesting Node properties
  virtual       AddPNode      *is_AddP      ()       { return 0; }
  virtual       BoolNode      *is_Bool      ()       { return 0; }
  virtual       CMoveNode     *is_CMove     ()       { return 0; }
  virtual       CallNode      *is_Call      ()       { return 0; }
  virtual       CountedLoopNode*is_CountedLoop()     { return 0; }
  virtual       IfNode        *is_If        ()       { return 0; }
  virtual       LoadNode      *is_Load      ()       { return 0; }
  virtual       LoopNode      *is_Loop      ()       { return 0; }
  virtual       MachNode      *is_Mach      ()       { return 0; }
  virtual       MemNode       *is_Mem       ()       { return 0; }
  virtual       MergeMemNode  *is_MergeMem  ()       { return 0; }
  virtual       MultiNode     *is_Multi     ()       { return 0; }
  virtual       PhiNode       *is_Phi       ()       { return 0; }
  virtual       ProjNode      *is_Proj      ()       { return 0; }
  virtual       RootNode      *is_Root      ()       { return 0; }
  virtual       SafePointNode *is_SafePoint ()       { return 0; }
  virtual       MachSpillCopyNode *is_SpillCopy ()   { return 0; }
  virtual       StartNode     *is_Start     ()       { return 0; }
  virtual       SubNode       *is_Sub       ()       { return 0; }
  virtual       TypeNode      *is_Type      ()       { return 0; }
  virtual const AddNode       *is_Add       () const { return 0; }
  virtual const BoxLockNode   *is_BoxLock   () const { return 0; }
  virtual const CatchNode     *is_Catch     () const { return 0; }
  virtual const CatchProjNode *is_CatchProj () const { return 0; }
  virtual const CmpNode       *is_Cmp       () const { return 0; }
  virtual const FastLockNode  *is_FastLock  () const { return 0; }
  virtual const JumpProjNode  *is_JumpProj  () const { return 0; }
  virtual const JumpNode      *is_Jump      () const { return 0; }
  virtual const MemBarNode    *is_MemBar    () const { return 0; }
  virtual const MulNode       *is_Mul       () const { return 0; }
  virtual const NullCheckNode *is_NullCheck () const { return 0; }
  virtual const PCTableNode   *is_PCTable   () const { return 0; }
  virtual const RegionNode    *is_Region    () const { return 0; }
  virtual const StoreNode     *is_Store     () const { return 0; }

  virtual uint  is_Con   () const { return 0; }
  virtual uint  is_Copy  () const { return 0; } // return copied edge index
  virtual uint  is_Goto  () const { return 0; }
  virtual bool  is_CFG   () const { return false; }
  // If this node is control-dependent on a test, can it be
  // rerouted to a dominating equivalent test?  This is usually
  // true of non-CFG nodes, but can be false for operations which
  // depend for their correct sequencing on more than one test.
  // (In that case, hoisting to a dominating test may silently
  // skip some other important test.)
  virtual bool depends_only_on_test() const { assert(!is_CFG(), ""); return true; };

  // defined for MachNodes that match 'If' | 'Goto' | 'CountedLoopEnd'
  virtual uint  is_Branch() const { return 0; }

  // When building basic blocks, I need to have a notion of block beginning
  // Nodes, next block selector Nodes (block enders), and next block
  // projections.  These calls need to work on their machine equivalents.  The
  // Ideal beginning Nodes are RootNode, RegionNode and StartNode.
  virtual int is_block_start() const;

  // The Ideal control projection Nodes are IfTrue/IfFalse, Root,
  // Goto and Return.  This call also returns the block ending Node.
  virtual const Node *is_block_proj() const;

//----------------- Optimization

  // Get the worst-case Type output for this Node.
  virtual const class Type *bottom_type() const;

  // If we find a better type for a node, try to record it permanently.
  // Return true if this node actually changed.
  // Be sure to do the hash_delete game in the "rehash" variant.
  virtual void raise_bottom_type(const Type* new_type) { }

  // Get the address type with which this node uses and/or defs memory,
  // or NULL if none.  The address type is conservatively wide.
  // Returns non-null for calls, membars, loads, stores, etc.
  // Returns TypePtr::BOTTOM if the node touches memory "broadly".
  virtual const class TypePtr *adr_type() const { return NULL; }

  // Return an existing node which computes the same function as this node.
  // The optimistic combined algorithm requires this to return a Node which 
  // is a small number of steps away (e.g., one of my inputs).
  virtual Node *Identity( PhaseTransform *phase );

  // Return the set of values this Node can take on at runtime.
  virtual const Type *Value( PhaseTransform *phase ) const;

  // Return a node which is more "ideal" than the current node.  
  // The invariants on this call are subtle.  If in doubt, read the
  // treatise in node.cpp above the default implemention AND TEST WITH
  // +VerifyIterativeGVN!
  virtual Node *Ideal(PhaseGVN *phase, bool can_reshape);
protected:
  bool remove_dead_region(PhaseGVN *phase, bool can_reshape);
public:

  // Idealize graph, using DU info.  Done after constant propagation
  virtual Node *Ideal_DU_postCCP( PhaseCCP *ccp );

  // See if there is valid pipeline info
  static  const Pipeline *pipeline_class();
  virtual const Pipeline *pipeline() const;
  bool valid_pipeline_info() const { return (pipeline() != NULL); }

  // Compute the latency from the def to this instruction of the ith input node
  uint latency(uint i);

  // Compute the (backwards) latency from the uses of this instructions
  int latency_from_uses(Block_Array &bbs, GrowableArray<uint> &node_latency) const;

  // Compute the (backwards) latency from a single use
  int latency_from_use (Block_Array &bbs, GrowableArray<uint> &node_latency, const Node *def, Node *use) const;

  // Compute the (backwards) latency from the uses of this instructions
  void partial_latency_of_defs(Block_Array &bbs, GrowableArray<uint> &node_latency);

  // Compute the (backwards) local (intra-bb) latency from the uses of this instructions
  void partial_latency_of_local_defs(Block_Array &bbs, GrowableArray<uint> &node_latency);

  // Hash & compare functions, for pessimistic value numbering

  // If the hash function returns the special sentinel value NO_HASH,
  // the node is guaranteed never to compare equal to any other node.
  // If we accidently generate a hash with value NO_HASH the node
  // won't go into the table and we'll lose a little optimization.
  enum { NO_HASH = 0 };
  virtual uint hash() const;
  virtual uint cmp( const Node &n ) const;

//----------------- Code Generation

  // Ideal register class for Matching.  Zero means unmatched instruction
  // (these are cloned instead of converted to machine nodes).
  virtual uint ideal_reg() const;

  static const uint NotAMachineReg;   // must be > max. machine register

  // Do we Match on this edge index or not?  Generally false for Control
  // and true for everything else.  Weird for calls & returns.
  virtual uint match_edge(uint idx) const;

  // Register class output is returned in
  virtual const RegMask &out_RegMask() const;
  // Register class input is expected in
  virtual const RegMask &in_RegMask(uint) const;
  // Should we clone rather than spill this instruction?
  virtual bool rematerialize() const { return false; }

  // Return JVM State Object if this Node carries debug info, or NULL otherwise
  virtual JVMState* jvms() const;
  virtual void set_oop_map(OopMap *map);
  virtual OopMap *oop_map() const;

  // Print as assembly
  virtual void format( PhaseRegAlloc * ) const;
  // Emit bytes starting at parameter 'ptr'
  // Bump 'ptr' by the number of output bytes 
  virtual void emit(CodeBuffer &cbuf, PhaseRegAlloc *ra_) const;
  // Size of instruction in bytes
  virtual uint size(PhaseRegAlloc *ra_) const;

  // Convenience function to extract an integer constant from a ConstNode.
  // Returns True if it is an integer constant ConstNode and puts the constant
  // into 'con', otherwise it returns False and 'con' is untouched.
  virtual int get_int( jint *con ) const;
  // Return the constant, knowing it is an integer constant already
  jint get_int( ) const;
  intptr_t get_ptr( ) const;
  jlong get_long( ) const;
  jdouble getd( ) const;
  jfloat getf( ) const;

  // Nodes which are pinned into basic blocks
  virtual int   pinned() const { return 0; }
  // Nodes which use memory without consuming it, hence need antidependences
  // More specifically, check_for_anti_dependence returns true iff the node
  // (a) does a load, and (b) does not perform a store (except perhaps to a
  // stack slot or some other unaliased location).
  virtual bool  check_for_anti_dependence() const { return false; }
  // Look into this node to see what memory it accesses.
  virtual Node *load() const { return NULL; }
  virtual int   may_alias( Node * ) { return 1; }

  // Return which operand this instruction may cisc-spill
  virtual int   cisc_operand() const { return AdlcVMDeps::Not_cisc_spillable; }
  virtual bool  is_cisc_alternate() const { return false; }

//----------------- Graph walking
public:
  // Walk and apply member functions recursively.
  // Supplied (this) pointer is root.
  void walk(NFunc pre, NFunc post, void *env);
  static void nop(Node &, void*); // Dummy empty function
  static void packregion( Node &n, void* );
private:
  void walk_(NFunc pre, NFunc post, void *env, VectorSet &visited);

//----------------- Printing, etc
public:
#ifndef PRODUCT
  Node* find(int idx) const;         // Search the graph for the given idx.
  Node* find_ctrl(int idx) const;    // Search control ancestors for the given idx.
  void dump() const;                 // Print this node,
  void dump(int depth) const;        // Print this node, recursively to depth d
  void dump_ctrl(int depth) const;   // Print control nodes, to depth d
  virtual void dump_req() const;     // Print required-edge info
  virtual void dump_prec() const;    // Print precedence-edge info
  virtual void dump_out() const;     // Print the output edge info
  virtual void dump_spec() const {}; // Print per-node info
  void verify_edges(Unique_Node_List &visited); // Verify bi-directional edges
  void verify() const;               // Check Def-Use info for my subgraph
  static void verify_recur(const Node *n, int verify_depth, VectorSet &old_space, VectorSet &new_space);

  // This call defines a class-unique string used to identify class instances
  virtual const char *Name() const;

  void dump_format(PhaseRegAlloc *ra) const; // debug access to MachNode::format(...)
  // RegMask Print Functions
  void dump_in_regmask(int idx) { in_RegMask(idx).dump(); }
  void dump_out_regmask() { out_RegMask().dump(); }
  static int _in_dump_cnt;
  static bool in_dump() { return _in_dump_cnt > 0; }
  void fast_dump() const {
    tty->print("%4d: %-17s", _idx, Name());
    for (uint i = 0; i < len(); i++)
      if (in(i))
        tty->print(" %4d", in(i)->_idx);
      else
        tty->print(" NULL");
    tty->print("\n");
  }
#endif
#ifdef ASSERT
  virtual void verify_construction();
  virtual bool verify_jvms(const JVMState* jvms) const;
  int  _debug_idx;                     // Unique value assigned to every node.
  int   debug_idx() const              { return _debug_idx; }
  void  set_debug_idx( int debug_idx ) { _debug_idx = debug_idx; }

  int        _hash_lock;               // Barrier to modifications of nodes in the hash table
  void  enter_hash_lock() { ++_hash_lock; assert(_hash_lock < 99, "in too many hash tables?"); }
  void   exit_hash_lock() { --_hash_lock; assert(_hash_lock >= 0, "mispaired hash locks"); }

  #if OPTO_DU_ITERATOR_ASSERT
  const Node* _last_del;               // The last deleted node.
  uint        _del_tick;               // Bumped when a deletion happens..
  #endif
#endif
};

//-----------------------------------------------------------------------------
// Iterators over DU info, and associated Node functions.

#if OPTO_DU_ITERATOR_ASSERT

// Common code for assertion checking on DU iterators.
class DUIterator_Common VALUE_OBJ_CLASS_SPEC {
#ifdef ASSERT
 protected:
  bool         _vdui;               // cached value of VerifyDUIterators
  const Node*  _node;               // the node containing the _out array
  uint         _outcnt;             // cached node->_outcnt
  uint         _del_tick;           // cached node->_del_tick
  Node*        _last;               // last value produced by the iterator

  void sample(const Node* node);    // used by c'tor to set up for verifies
  void verify(const Node* node, bool at_end_ok = false);
  void verify_resync();
  void reset(const DUIterator_Common& that);

// The VDUI_ONLY macro protects code conditionalized on VerifyDUIterators
  #define I_VDUI_ONLY(i,x) { if ((i)._vdui) { x; } }
#else
  #define I_VDUI_ONLY(i,x) { }
#endif //ASSERT
};

#define VDUI_ONLY(x)     I_VDUI_ONLY(*this, x)

// Default DU iterator.  Allows appends onto the out array.
// Allows deletion from the out array only at the current point.
// Usage:
//  for (DUIterator i = x->outs(); x->has_out(i); i++) {
//    Node* y = x->out(i);
//    ...
//  }
// Compiles in product mode to a unsigned integer index, which indexes
// onto a repeatedly reloaded base pointer of x->_out.  The loop predicate
// also reloads x->_outcnt.  If you delete, you must perform "--i" just
// before continuing the loop.  You must delete only the last-produced
// edge.  You must delete only a single copy of the last-produced edge,
// or else you must delete all copies at once (the first time the edge
// is produced by the iterator).
class DUIterator : public DUIterator_Common {
  friend class Node;

  // This is the index which provides the product-mode behavior.
  // Whatever the product-mode version of the system does to the
  // DUI index is done to this index.  All other fields in
  // this class are used only for assertion checking.
  uint         _idx;

  #ifdef ASSERT
  uint         _refresh_tick;    // Records the refresh activity.

  void sample(const Node* node); // Initialize _refresh_tick etc.
  void verify(const Node* node, bool at_end_ok = false);
  void verify_increment();       // Verify an increment operation.
  void verify_resync();          // Verify that we can back up over a deletion.
  void verify_finish();          // Verify that the loop terminated properly.
  void refresh();                // Resample verification info.
  void reset(const DUIterator& that);  // Resample after assignment.
  #endif

  DUIterator(const Node* node, int dummy_to_avoid_conversion)
    { _idx = 0;                         debug_only(sample(node)); }

 public:
  // initialize to garbage; clear _vdui to disable asserts
  DUIterator()
    { /*initialize to garbage*/         debug_only(_vdui = false); }

  void operator++(int dummy_to_specify_postfix_op)
    { _idx++;                           VDUI_ONLY(verify_increment()); }

  void operator--()
    { VDUI_ONLY(verify_resync());       --_idx; }

  ~DUIterator()
    { VDUI_ONLY(verify_finish()); }

  void operator=(const DUIterator& that)
    { _idx = that._idx;                 debug_only(reset(that)); }
};

DUIterator Node::outs() const
  { return DUIterator(this, 0); }
DUIterator& Node::refresh_out_pos(DUIterator& i) const
  { I_VDUI_ONLY(i, i.refresh());        return i; }
bool Node::has_out(DUIterator& i) const
  { I_VDUI_ONLY(i, i.verify(this,true));return i._idx < _outcnt; }
Node*    Node::out(DUIterator& i) const
  { I_VDUI_ONLY(i, i.verify(this));     return debug_only(i._last=) _out[i._idx]; }


// Faster DU iterator.  Disallows insertions into the out array.
// Allows deletion from the out array only at the current point.
// Usage:
//  for (DUIterator_Fast imax, i = x->fast_outs(imax); i < imax; i++) {
//    Node* y = x->fast_out(i);
//    ...
//  }
// Compiles in product mode to raw Node** pointer arithmetic, with
// no reloading of pointers from the original node x.  If you delete,
// you must perform "--i; --imax" just before continuing the loop.
// If you delete multiple copies of the same edge, you must decrement
// imax, but not i, multiple times:  "--i, imax -= num_edges".
class DUIterator_Fast : public DUIterator_Common {
  friend class Node;
  friend class DUIterator_Last;

  // This is the pointer which provides the product-mode behavior.
  // Whatever the product-mode version of the system does to the
  // DUI pointer is done to this pointer.  All other fields in
  // this class are used only for assertion checking.
  Node**       _outp;

  #ifdef ASSERT
  void verify(const Node* node, bool at_end_ok = false);
  void verify_limit();
  void verify_resync();
  void verify_relimit(uint n);
  void reset(const DUIterator_Fast& that);
  #endif

  // Note:  offset must be signed, since -1 is sometimes passed
  DUIterator_Fast(const Node* node, ptrdiff_t offset)
    { _outp = node->_out + offset;      debug_only(sample(node)); }

 public:
  // initialize to garbage; clear _vdui to disable asserts
  DUIterator_Fast()
    { /*initialize to garbage*/         debug_only(_vdui = false); }

  void operator++(int dummy_to_specify_postfix_op)
    { _outp++;                          VDUI_ONLY(verify(_node, true)); }

  void operator--()
    { VDUI_ONLY(verify_resync());       --_outp; }

  void operator-=(uint n)   // applied to the limit only
    { _outp -= n;           VDUI_ONLY(verify_relimit(n));  }

  bool operator<(DUIterator_Fast& limit) {
    I_VDUI_ONLY(*this, this->verify(_node, true));
    I_VDUI_ONLY(limit, limit.verify_limit());
    return _outp < limit._outp;
  }

  void operator=(const DUIterator_Fast& that)
    { _outp = that._outp;               debug_only(reset(that)); }
};

DUIterator_Fast Node::fast_outs(DUIterator_Fast& imax) const {
  // Assign a limit pointer to the reference argument:
  imax = DUIterator_Fast(this, (ptrdiff_t)_outcnt);
  // Return the base pointer:
  return DUIterator_Fast(this, 0);
}
Node* Node::fast_out(DUIterator_Fast& i) const {
  I_VDUI_ONLY(i, i.verify(this));
  return debug_only(i._last=) *i._outp;
}


// Faster DU iterator.  Requires each successive edge to be removed.
// Does not allow insertion of any edges.
// Usage:
//  for (DUIterator_Last imin, i = x->last_outs(imin); i >= imin; i -= num_edges) {
//    Node* y = x->last_out(i);
//    ...
//  }
// Compiles in product mode to raw Node** pointer arithmetic, with
// no reloading of pointers from the original node x.
class DUIterator_Last : private DUIterator_Fast {
  friend class Node;

  #ifdef ASSERT
  void verify(const Node* node, bool at_end_ok = false);
  void verify_limit();
  void verify_step(uint num_edges);
  #endif

  // Note:  offset must be signed, since -1 is sometimes passed
  DUIterator_Last(const Node* node, ptrdiff_t offset)
    : DUIterator_Fast(node, offset) { }

  void operator++(int dummy_to_specify_postfix_op) {} // do not use
  void operator<(int)                              {} // do not use

 public:
  DUIterator_Last() { }
  // initialize to garbage

  void operator--()
    { _outp--;              VDUI_ONLY(verify_step(1));  }

  void operator-=(uint n)
    { _outp -= n;           VDUI_ONLY(verify_step(n));  }

  bool operator>=(DUIterator_Last& limit) {
    I_VDUI_ONLY(*this, this->verify(_node, true));
    I_VDUI_ONLY(limit, limit.verify_limit());
    return _outp >= limit._outp;
  }

  void operator=(const DUIterator_Last& that)
    { DUIterator_Fast::operator=(that); }
};

DUIterator_Last Node::last_outs(DUIterator_Last& imin) const {
  // Assign a limit pointer to the reference argument:
  imin = DUIterator_Last(this, 0);
  // Return the initial pointer:
  return DUIterator_Last(this, (ptrdiff_t)_outcnt - 1);
}
Node* Node::last_out(DUIterator_Last& i) const {
  I_VDUI_ONLY(i, i.verify(this));
  return debug_only(i._last=) *i._outp;
}

#endif //OPTO_DU_ITERATOR_ASSERT

#undef I_VDUI_ONLY
#undef VDUI_ONLY


//-----------------------------------------------------------------------------
// Map dense integer indices to Nodes.  Uses classic doubling-array trick.
// Abstractly provides an infinite array of Node*'s, initialized to NULL.
// Note that the constructor just zeros things, and since I use Arena 
// allocation I do not need a destructor to reclaim storage.
class Node_Array : public ResourceObj {
protected:
  Arena *_a;                    // Arena to allocate in
  uint   _max;
  Node **_nodes;
  void   grow( uint i );        // Grow array node to fit
public:
  Node_Array(Arena *a) : _a(a), _max(OptoNodeListSize) {
    _nodes = NEW_ARENA_ARRAY( a, Node *, OptoNodeListSize );
    for( int i = 0; i < OptoNodeListSize; i++ ) {
      _nodes[i] = NULL;
    }
  }

  Node_Array(Node_Array *na) : _a(na->_a), _max(na->_max), _nodes(na->_nodes) {}
  Node *operator[] ( uint i ) const // Lookup, or NULL for not mapped
  { return (i<_max) ? _nodes[i] : (Node*)NULL; }
  Node *at( uint i ) const { assert(i<_max,"oob"); return _nodes[i]; }
  Node **adr() { return _nodes; }
  // Extend the mapping: index i maps to Node *n.
  void map( uint i, Node *n ) { if( i>=_max ) grow(i); _nodes[i] = n; }
  void insert( uint i, Node *n );
  void remove( uint i );        // Remove, preserving order
  void sort( C_sort_func_t func);
  void reset( Arena *new_a );   // Zap mapping to empty; reclaim storage
  void clear();                 // Set all entries to NULL, keep storage
  uint Size() const { return _max; }
  void dump() const;
};

class Node_List : public Node_Array {
  uint _cnt;
public:
  Node_List() : Node_Array(Thread::current()->resource_area()), _cnt(0) {}
  Node_List(Arena *a) : Node_Array(a), _cnt(0) {}
  void insert( uint i, Node *n ) { Node_Array::insert(i,n); _cnt++; }
  void remove( uint i ) { Node_Array::remove(i); _cnt--; }
  void push( Node *b ) { map(_cnt++,b); }
  void yank( Node *n );         // Find and remove
  Node *pop() { return _nodes[--_cnt]; }
  Node *rpop() { Node *b = _nodes[0]; _nodes[0]=_nodes[--_cnt]; return b;}
  void clear() { _cnt = 0; Node_Array::clear(); } // retain storage
  uint size() const { return _cnt; }
  void dump() const;
};

//------------------------------Unique_Node_List-------------------------------
class Unique_Node_List : public Node_List {
  VectorSet _in_worklist;
  uint _clock_index;            // Index in list where to pop from next
public:
  Unique_Node_List() : Node_List(), _in_worklist(Thread::current()->resource_area()), _clock_index(0) {}
  Unique_Node_List(Arena *a) : Node_List(a), _in_worklist(a), _clock_index(0) {}

  void remove( Node *n );
  bool member( Node *n ) { return _in_worklist.test(n->_idx); }
  VectorSet &member_set(){ return _in_worklist; }

  void push( Node *b ) { 
    if( !_in_worklist.test_set(b->_idx) ) 
      Node_List::push(b);
  }
  Node *pop() {
    if( _clock_index >= size() ) _clock_index = 0;
    Node *b = at(_clock_index);
    map( _clock_index++, Node_List::pop());
    _in_worklist >>= b->_idx;
    return b;
  }
  Node *remove( uint i ) {
    Node *b = Node_List::at(i);
    _in_worklist >>= b->_idx;
    map(i,Node_List::pop());
    return b;
  }
  void yank( Node *n ) { _in_worklist >>= n->_idx; Node_List::yank(n); }
  void  clear() {
    _in_worklist.Clear();        // Discards storage but grows automatically
    Node_List::clear();
    _clock_index = 0;
  }

  // Used after parsing to remove useless nodes before Iterative GVN
  void remove_useless_nodes(VectorSet &useful);

#ifndef PRODUCT
  void print_set() const { _in_worklist.print(); }
#endif
};

// Inline definition of Compile::record_for_igvn must be deferred to this point.
inline void Compile::record_for_igvn(Node* n) { _for_igvn->push(n); }

//------------------------------Node_Stack-------------------------------------
class Node_Stack {
protected:
  struct INode {
    Node *node; // Processed node
    uint  indx; // Index of next node's child
  };
  INode *_inode_top; // tos, stack grows up
  INode *_inode_max; // End of _inodes == _inodes + _max
  INode *_inodes;    // Array storage for the stack
  Arena *_a;         // Arena to allocate in
  void grow();
public:
  Node_Stack(int size) { 
    size_t max = (size > OptoNodeListSize) ? size : OptoNodeListSize;
    _a = Thread::current()->resource_area();
    _inodes = NEW_ARENA_ARRAY( _a, INode, max );
    _inode_max = _inodes + max;
    _inode_top = _inodes - 1; // stack is empty
  }
  
  Node_Stack(Arena *a, int size) : _a(a) { 
    size_t max = (size > OptoNodeListSize) ? size : OptoNodeListSize;
    _inodes = NEW_ARENA_ARRAY( _a, INode, max );
    _inode_max = _inodes + max;
    _inode_top = _inodes - 1; // stack is empty
  }

  void pop() {
    assert(_inode_top >= _inodes, "node stack underflow");
    --_inode_top;
  }
  void push(Node *n, uint i) {
    ++_inode_top;
    if (_inode_top >= _inode_max) grow();
    INode *top = _inode_top; // optimization
    top->node = n;
    top->indx = i;
  }
  Node *node() const {
    return _inode_top->node;
  }
  uint index() const {
    return _inode_top->indx;
  }
  void set_node(Node *n) {
    _inode_top->node = n;
  }
  void set_index(uint i) {
    _inode_top->indx = i;
  }
  uint Size() const { return (uint)(_inode_max - _inodes); } // Max size
  uint size() const { return (uint)(_inode_top - _inodes + 1); }
  bool is_nonempty() const { return (_inode_top >= _inodes); }
  bool is_empty() const { return (_inode_top < _inodes); }
  void clear() { _inode_top = _inodes - 1; } // retain storage
};


//------------------------------TypeNode---------------------------------------
// Node with a Type constant.
class TypeNode : public Node {
protected:
  virtual uint hash() const;    // Check the type
  virtual uint cmp( const Node &n ) const;
  virtual uint size_of() const; // Size is bigger
  const Type* const _type;
public:
  void set_type(const Type* t) {
    assert(t != NULL, "sanity");
    debug_only(uint check_hash = (VerifyHashTableKeys && _hash_lock) ? hash() : NO_HASH);
    *(const Type**)&_type = t;   // cast away const-ness
    // If this node is in the hash table, make sure it doesn't need a rehash.
    assert(check_hash == NO_HASH || check_hash == hash(), "type change must preserve hash code");
  }
  const Type* type() const { assert(_type != NULL, "sanity"); return _type; };
  TypeNode( const Type *t, uint required ) : Node(required), _type(t) {}
  virtual const Type *Value( PhaseTransform *phase ) const;
  virtual const Type *bottom_type() const;
  virtual       void  raise_bottom_type(const Type* new_type);
  virtual       uint  ideal_reg() const;
  virtual   TypeNode *is_Type() { return this; }
#ifndef PRODUCT
  virtual void dump_spec() const;
#endif
};

