/*
 * Copyright (c) 2000, 2001, Oracle and/or its affiliates. All rights reserved.
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

// A water mark points into a space and is used during GC to keep track of
// progress.

class Space;

class WaterMark VALUE_OBJ_CLASS_SPEC {
  friend class VMStructs;
 private:
  HeapWord* _point;
  Space*    _space;
 public:
  // Accessors
  Space* space() const        { return _space;  }
  void set_space(Space* s)    { _space = s;     }
  HeapWord* point() const     { return _point;  }
  void set_point(HeapWord* p) { _point = p;     }

  // Constructors
  WaterMark(Space* s, HeapWord* p) : _space(s), _point(p) {};
  WaterMark() : _space(NULL), _point(NULL) {};
};

inline bool operator==(const WaterMark& x, const WaterMark& y) {
  return (x.point() == y.point()) && (x.space() == y.space());
}

inline bool operator!=(const WaterMark& x, const WaterMark& y) {
  return !(x == y);
}
