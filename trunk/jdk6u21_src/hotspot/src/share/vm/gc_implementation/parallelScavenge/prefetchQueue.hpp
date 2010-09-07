/*
 * Copyright (c) 2002, 2008, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */

//
// PrefetchQueue is a FIFO queue of variable length (currently 8).
//
// We need to examine the performance penalty of variable lengths.
// We may also want to split this into cpu dependent bits.
//

const int PREFETCH_QUEUE_SIZE  = 8;

class PrefetchQueue : public CHeapObj {
 private:
  void* _prefetch_queue[PREFETCH_QUEUE_SIZE];
  uint  _prefetch_index;

 public:
  int length() { return PREFETCH_QUEUE_SIZE; }

  inline void clear() {
    for(int i=0; i<PREFETCH_QUEUE_SIZE; i++) {
      _prefetch_queue[i] = NULL;
    }
    _prefetch_index = 0;
  }

  template <class T> inline void* push_and_pop(T* p) {
    oop o = oopDesc::load_decode_heap_oop_not_null(p);
    Prefetch::write(o->mark_addr(), 0);
    // This prefetch is intended to make sure the size field of array
    // oops is in cache. It assumes the the object layout is
    // mark -> klass -> size, and that mark and klass are heapword
    // sized. If this should change, this prefetch will need updating!
    Prefetch::write(o->mark_addr() + (HeapWordSize*2), 0);
    _prefetch_queue[_prefetch_index++] = p;
    _prefetch_index &= (PREFETCH_QUEUE_SIZE-1);
    return _prefetch_queue[_prefetch_index];
  }

  // Stores a NULL pointer in the pop'd location.
  inline void* pop() {
    _prefetch_queue[_prefetch_index++] = NULL;
    _prefetch_index &= (PREFETCH_QUEUE_SIZE-1);
    return _prefetch_queue[_prefetch_index];
  }
};
