#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)gSpaceCounters.hpp	1.8 04/02/06 20:47:18 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// A GSpaceCounter is a holder class for performance counters
// that track a space;

class GSpaceCounters: public CHeapObj {
  friend class VMStructs;

 private:
  PerfVariable*      _capacity;
  PerfVariable*      _used;

  // Constant PerfData types don't need to retain a reference.
  // However, it's a good idea to document them here.
  // PerfConstant*     _size;

  Generation*       _gen;
  char*             _name_space;

 public:

  GSpaceCounters(const char* name, int ordinal, size_t max_size, Generation* g,
                 GenerationCounters* gc, bool sampled=true);

  ~GSpaceCounters() {
    if (_name_space != NULL) FREE_C_HEAP_ARRAY(char, _name_space);
  }
  
  inline void update_capacity() {
    _capacity->set_value(_gen->capacity());
  }

  inline void update_used() {
    _used->set_value(_gen->used());
  }

  // special version of update_used() to allow the used value to be
  // passed as a parameter. This method can can be used in cases were
  // the  utilization is already known and/or when the _gen->used()
  // method is known to be expensive and we want to avoid unnecessary
  // calls to it.
  //
  inline void update_used(size_t used) {
    _used->set_value(used);
  }

  inline void inc_used(size_t size) {
    _used->inc(size);
  }

  debug_only(
    // for security reasons, we do not allow arbitrary reads from
    // the counters as they may live in shared memory. 
    jlong used() {
      return _used->get_value();
    }
    jlong capacity() {
      return _used->get_value();
    }
  )

  inline void update_all() {
    update_used();
    update_capacity();
  }

  const char* name_space() const        { return _name_space; }
};

class GenerationUsedHelper : public PerfLongSampleHelper {
  private:
    Generation* _gen;

  public:
    GenerationUsedHelper(Generation* g) : _gen(g) { }

    inline jlong take_sample() {
      return _gen->used();
    }
};

