#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)virtualspace.hpp	1.32 03/12/23 16:44:28 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// ReservedSpace is a data structure for reserving a contiguous address range.

class ReservedSpace VALUE_OBJ_CLASS_SPEC {
  friend class VMStructs;
 private:
  char*  _base;
  size_t _size;

  // ReservedSpace
  ReservedSpace(char* base, size_t size);
  void initialize(size_t size, size_t forced_base_alignment,
                  bool ism,  char* requested_address = NULL);

 public:
  // Constructor
  ReservedSpace(size_t size);
  ReservedSpace(size_t size, size_t forced_base_alignment,
                bool ism, char* requested_address = NULL);

  // Accessors
  char*  base() { return _base; }
  size_t size() { return _size; }

  bool is_reserved() { return _base != NULL; }
  void release();

  // Splitting
  ReservedSpace first_part(size_t partition_size,
                           bool split = false, bool realloc = true);
  ReservedSpace last_part (size_t partition_size);

  // Alignment
  static size_t page_align_size_up(size_t size);
  static size_t page_align_size_down(size_t size);
  static size_t allocation_align_size_up(size_t size);
  static size_t allocation_align_size_down(size_t size);
};


// VirtualSpace is data structure for committing a previously reserved address range in smaller chunks.

class VirtualSpace VALUE_OBJ_CLASS_SPEC {
  friend class VMStructs;
 private:
  // Reserved area
  char* _low_boundary;
  char* _high_boundary;

  // Committed area
  char* _low;
  char* _high;

  // MPSS Support
  // Each virtualspace region has a lower, middle, and upper region.
  // Each region has an end boundary and a high pointer which is the 
  // high water mark for the last allocated byte.
  // The lower and upper unaligned to LargePageSizeInBytes uses default page.
  // size.  The middle region uses large page size.
  char* _lower_high;  
  char* _middle_high;
  char* _upper_high;

  char* _lower_high_boundary;
  char* _middle_high_boundary;
  char* _upper_high_boundary;

  size_t _lower_alignment;
  size_t _middle_alignment;
  size_t _upper_alignment;

  // MPSS Accessors
  char* lower_high() const { return _lower_high; }
  char* middle_high() const { return _middle_high; }
  char* upper_high() const { return _upper_high; }

  char* lower_high_boundary() const { return _lower_high_boundary; }
  char* middle_high_boundary() const { return _middle_high_boundary; }
  char* upper_high_boundary() const { return _upper_high_boundary; }
  
  size_t lower_alignment() const { return _lower_alignment; }
  size_t middle_alignment() const { return _middle_alignment; }
  size_t upper_alignment() const { return _upper_alignment; }

 public:
  // Committed area
  char* low()  const { return _low; }
  char* high() const { return _high; }

  // Reserved area
  char* low_boundary()  const { return _low_boundary; }
  char* high_boundary() const { return _high_boundary; }

 public:
  // Initialization
  VirtualSpace();
  bool initialize(ReservedSpace rs, size_t committed_byte_size);
  
  // Destruction
  ~VirtualSpace();

  // Testers (all sizes are byte sizes)
  size_t committed_size()   const;
  size_t reserved_size()    const;
  size_t uncommitted_size() const;
  bool   contains(const void* p)  const;

  // Operations
  bool expand_by(size_t bytes);    // returns true on success, false otherwise
  void shrink_by(size_t bytes);
  void release();

  void check_for_contiguity() PRODUCT_RETURN;

  // Debugging
  void print() PRODUCT_RETURN;
};
