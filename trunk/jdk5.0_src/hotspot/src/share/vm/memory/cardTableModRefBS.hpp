#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)cardTableModRefBS.hpp	1.42 03/12/23 16:40:55 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// This kind of "BarrierSet" allows a "CollectedHeap" to detect and
// enumerate ref fields that have been modified (since the last
// enumeration.)

// As it currently stands, this barrier is *imprecise*: when a ref field in
// an object "o" is modified, the card table entry for the card containing
// the head of "o" is dirtied, not necessarily the card containing the
// modified field itself.  For object arrays, however, the barrier *is*
// precise; only the card containing the modified element is dirtied.
// Any MemRegionClosures used to scan dirty cards should take these
// considerations into account.

class Generation;
class OopsInGenClosure;
class DirtyCardToOopClosure;

class CardTableModRefBS: public ModRefBarrierSet {
  // Some classes get to look at some private stuff.
#ifdef CC_INTERP
  friend class cInterpreter;
#endif
  friend class VMStructs;
  friend class CardTableRS;
  friend class CheckForUnmarkedOops; // Needs access to raw card bytes.
#ifndef PRODUCT
  // For debugging.
  friend class GuaranteeNotModClosure;
#endif
 protected:

  enum CardValues {
    clean_card                  = -1,
    dirty_card                  =  0,
    precleaned_card             =  1,
    last_card                   =  4,
    CT_MR_BS_last_reserved      = 10
  };

  // dirty and precleaned are equivalent wrt younger_refs_iter.
  static bool card_is_dirty_wrt_gen_iter(jbyte cv) {
    return cv == dirty_card || cv == precleaned_card;
  }

  // Returns "true" iff the value "cv" will cause the card containing it
  // to be scanned in the current traversal.  May be overridden by
  // subtypes.
  virtual bool card_will_be_scanned(jbyte cv) {
    return CardTableModRefBS::card_is_dirty_wrt_gen_iter(cv);
  }

  // Returns "true" iff the value "cv" may have represented a dirty card at 
  // some point.
  virtual bool card_may_have_been_dirty(jbyte cv) {
    return card_is_dirty_wrt_gen_iter(cv);
  }

  // The region that the card table can cover.
  MemRegion    _whole_heap;

  jbyte*       _byte_map;      // the card marking array
  size_t       _byte_map_size;  // In bytes.

  size_t       _last_valid_index;  // index of last valid element in card table
  size_t       _guard_index;       // index of very last element in card table;
                                   //  it is set to a guard value "last_card" and 
                                   // should never be modified

  int _cur_covered_regions;
  // The covered regions should be in address order.
  MemRegion* _covered;
  // The committed regions correspond one-to-one to the covered regions.
  // They represent the card-table memory that has been committed to service
  // the corresponding covered region.  It may be that committed region for
  // one covered region corresponds to a larger region because of page-size
  // roundings.  Thus, a committed region for one covered region may
  // actually extend onto the card-table space for the next covered region.
  MemRegion* _committed;

  // The last card is a guard card, and we commit the page for it so
  // we can use the card for verification purposes. We make sure we never
  // uncommit the MemRegion for that page.
  MemRegion _guard_region;

  // Finds and return the index of the region, if any, to which the given
  // region would be contiguous.  If none exists, assign a new region and
  // returns its index.  Requires that no more than the maximum number of
  // covered regions defined in the constructor are ever in use.
  int find_covering_region_by_base(HeapWord* base);

  // Same as above, but finds the region containing the given address
  // instead of starting at a given base address.
  int find_covering_region_containing(HeapWord* addr);

  // Resize one of the regions covered by the remembered set.
  void resize_covered_region(MemRegion new_region);

  // Returns the leftmost end of a committed region corresponding to a
  // covered region before covered region "ind", or else "NULL" if "ind" is 
  // the first covered region.
  HeapWord* largest_prev_committed_end(int ind) const;

  // Returns the part of the region mr that doesn't intersect with 
  // any committed region other than self.  Used to prevent uncommitting 
  // regions that are also committed by other regions.
  MemRegion committed_unique_to_self(int self, MemRegion mr) const;

  // Mapping from address to card marking array entry
  jbyte* byte_for(const void* p) const { 
    assert(_whole_heap.contains(p),
           "out of bounds access to card marking array");
    jbyte* result = &byte_map_base[uintptr_t(p) >> card_shift];
    assert(result >= _byte_map && result < _byte_map + _byte_map_size,
           "out of bounds accessor for card marking array");
    return result;
  }

  // The card table byte one after the card marking array
  // entry for argument address. Typically used for higher bounds
  // for loops iterating through the card table.
  jbyte* byte_after(const void* p) const {
    return byte_for(p) + 1;
  }

  // Mapping from card marking array entry to address of first word
  HeapWord* addr_for(const jbyte* p) const { 
    assert(p >= _byte_map && p < _byte_map + _byte_map_size,
	   "out of bounds access to card marking array");
    size_t delta = pointer_delta(p, byte_map_base, sizeof(jbyte));
    HeapWord* result = (HeapWord*) (delta << card_shift);
    assert(_whole_heap.contains(result),
           "out of bounds accessor from card marking array");
    return result;
  }

  // Iterate over the portion of the card-table which covers the given
  // region mr in the given space and apply cl to any dirty sub-regions
  // of mr. cl and dcto_cl must either be the same closure or cl must
  // wrap dcto_cl. Both are required - neither may be NULL. Also, dcto_cl
  // may be modified. Note that this function will operate in a parallel
  // mode if worker threads are available.
  void non_clean_card_iterate(Space* sp, MemRegion mr,
			      DirtyCardToOopClosure* dcto_cl,
			      MemRegionClosure* cl,
			      bool clear);

  // Utility function used to implement the other versions below.
  void non_clean_card_iterate_work(MemRegion mr, MemRegionClosure* cl,
				   bool clear);

  void par_non_clean_card_iterate_work(Space* sp, MemRegion mr,
                                       DirtyCardToOopClosure* dcto_cl,
                                       MemRegionClosure* cl,
                                       bool clear,
                                       int n_threads);

  // Dirty the bytes corresponding to "mr" (not all of which must be
  // covered.)
  void dirty_MemRegion(MemRegion mr);

  // Clear (to clean_card) the bytes entirely contained within "mr" (not
  // all of which must be covered.)
  void clear_MemRegion(MemRegion mr);

  // *** Support for parallel card scanning.

  enum SomeConstantsForParallelism {
    StridesPerThread    = 2,
    CardsPerStrideChunk = 256
  };

  // This is an array, one element per covered region of the card table.
  // Each entry is itself an array, with one element per chunk in the
  // covered region.  Each entry of these arrays is the lowest non-clean
  // card of the corresponding chunk containing part of an object from the
  // previous chunk, or else NULL.
  typedef jbyte*  CardPtr;
  typedef CardPtr* CardArr;
  CardArr* _lowest_non_clean;
  size_t*  _lowest_non_clean_chunk_size;
  uintptr_t* _lowest_non_clean_base_chunk_index;
  int* _last_LNC_resizing_collection;

  // Initializes "lowest_non_clean" to point to the array for the region
  // covering "sp", and "lowest_non_clean_base_chunk_index" to the chunk
  // index of the corresponding to the first element of that array.
  // Ensures that these arrays are of sufficient size, allocating if necessary.
  // May be called by several threads concurrently.  
  void get_LNC_array_for_space(Space* sp,
			       jbyte**& lowest_non_clean, 
			       uintptr_t& lowest_non_clean_base_chunk_index,
			       size_t& lowest_non_clean_chunk_size);

  // Returns the number of chunks necessary to cover "mr".
  inline size_t chunks_to_cover(MemRegion mr);

  // Returns the index of the chunk in a stride which
  // covers the given address.
  inline uintptr_t addr_to_chunk_index(const void* addr);

  // Apply cl, which must either itself apply dcto_cl or be dcto_cl,
  // to the cards in the stride (of n_strides) within the given space.
  void process_stride(Space* sp,
		      MemRegion used,
		      jint stride, int n_strides,
		      DirtyCardToOopClosure* dcto_cl,
		      MemRegionClosure* cl,
		      bool clear,
		      jbyte** lowest_non_clean,
		      uintptr_t lowest_non_clean_base_chunk_index,
		      size_t lowest_non_clean_chunk_size);

  // Makes sure that chunk boundaries are handled appropriately, by
  // adjusting the min_done of dcto_cl, and by using a special card-table
  // value to indicate how min_done should be set.
  void process_chunk_boundaries(Space* sp,
				DirtyCardToOopClosure* dcto_cl,
				MemRegion chunk_mr,
				MemRegion used,
				jbyte** lowest_non_clean,
				uintptr_t lowest_non_clean_base_chunk_index,
				size_t    lowest_non_clean_chunk_size);

public:
  // Constants
  enum SomePublicConstants {
    card_shift                  = 9,
    card_size                   = 1 << card_shift,
    card_size_in_words          = card_size / sizeof(HeapWord)
  };

  // For RTTI simulation.
  BarrierSet::Name kind() { return BarrierSet::CardTableModRef; }
  bool is_a(BarrierSet::Name bsn) {
    return bsn == BarrierSet::CardTableModRef || bsn == BarrierSet::ModRef;
  }

  CardTableModRefBS(MemRegion whole_heap, int max_covered_regions); 

  // *** Barrier set functions.

  inline bool write_ref_needs_barrier(oop* field, oop new_val) {
    // Note that this assumes the perm gen is the highest generation
    // in the address space
    return new_val != NULL && !new_val->is_perm();
  }

  // Record a reference update. Note that these versions are precise!
  // The scanning code has to handle the fact that the write barrier may be 
  // either precise or imprecise. We make non-virtual inline variants of 
  // these functions here for performance.
protected:
  void write_ref_field_work(oop obj, size_t offset, oop newVal);
  void write_ref_field_work(oop* field, oop newVal);
public:

  bool has_write_ref_array_opt() { return true; }
  bool has_write_region_opt() { return true; }

  inline void inline_write_region(MemRegion mr) {
    dirty_MemRegion(mr);
  }
protected:
  void write_region_work(MemRegion mr) {
    inline_write_region(mr);
  }
public:

  inline void inline_write_ref_array(MemRegion mr) {
    dirty_MemRegion(mr);
  }
protected:
  void write_ref_array_work(MemRegion mr) {
    inline_write_ref_array(mr);
  }
public:

  bool is_aligned(HeapWord* addr) {
    return is_card_aligned(addr);
  }

  // *** Card-table-barrier-specific things.

  inline void inline_write_ref_field(oop* field, oop newVal) {
    jbyte* byte = byte_for(field);
    *byte = dirty_card;
  }

  // Card marking array base (adjusted for heap low boundary)
  // This would be the 0th element of _byte_map, if the heap started at 0x0.
  // But since the heap starts at some higher address, this points to somewhere
  // before the beginning of the actual _byte_map.
  jbyte* byte_map_base;

  // Return true if "p" is at the start of a card.
  bool is_card_aligned(HeapWord* p) {
    jbyte* pcard = byte_for(p);
    return (addr_for(pcard) == p);
  }

  // The kinds of precision a CardTableModRefBS may offer.
  enum PrecisionStyle {
    Precise,
    ObjHeadPreciseArray
  };

  // Tells what style of precision this card table offers.
  PrecisionStyle precision() {
    return ObjHeadPreciseArray; // Only one supported for now.
  }

  // ModRefBS functions.
  void invalidate(MemRegion mr);
  void clear(MemRegion mr);
  void mod_oop_in_space_iterate(Space* sp, OopClosure* cl,
				bool clear = false,
				bool before_save_marks = false);
 
  // *** Card-table-RemSet-specific things.

  // Invoke "cl.do_MemRegion" on a set of MemRegions that collectively
  // includes all the modified cards (expressing each card as a
  // MemRegion).  Thus, several modified cards may be lumped into one
  // region.  The regions are non-overlapping, and are visited in
  // *decreasing* address order.  (This order aids with imprecise card
  // marking, where a dirty card may cause scanning, and summarization
  // marking, of objects that extend onto subsequent cards.)
  // If "clear" is true, the card is (conceptually) marked unmodified before
  // applying the closure.
  void mod_card_iterate(MemRegionClosure* cl, bool clear = false) {
    non_clean_card_iterate_work(_whole_heap, cl, clear);
  }

  // Like the "mod_cards_iterate" above, except only invokes the closure
  // for cards within the MemRegion "mr" (which is required to be
  // card-aligned and sized.)
  void mod_card_iterate(MemRegion mr, MemRegionClosure* cl,
			bool clear = false) {
    non_clean_card_iterate_work(mr, cl, clear);
  }

  static uintx ct_max_alignment_constraint();

  // Apply closure cl to the dirty cards lying completely
  // within MemRegion mr, setting the cards to precleaned.
  void      dirty_card_iterate(MemRegion mr, MemRegionClosure* cl);

  // Return the MemRegion corresponding to the first maximal run
  // of dirty cards lying completely within MemRegion mr, after
  // marking those cards precleaned.
  MemRegion dirty_card_range_after_preclean(MemRegion mr);

  // Set all the dirty cards in the given region to precleaned state.
  void preclean_dirty_cards(MemRegion mr);

  // Mapping from address to card marking array index.
  int index_for(void* p) {
    assert(_whole_heap.contains(p),
	   "out of bounds access to card marking array");
    return byte_for(p) - _byte_map;
  }

  void verify()  PRODUCT_RETURN;

  void verify_clean_region(MemRegion mr) PRODUCT_RETURN;

  static size_t par_chunk_heapword_alignment() {
    return CardsPerStrideChunk * card_size_in_words;
  }
};

class CardTableRS;

// A specialization for the CardTableRS gen rem set.
class CardTableModRefBSForCTRS: public CardTableModRefBS {
  CardTableRS* _rs;
protected:
  bool card_will_be_scanned(jbyte cv);
  bool card_may_have_been_dirty(jbyte cv);
public:
  CardTableModRefBSForCTRS(MemRegion whole_heap,
			   int max_covered_regions) :
    CardTableModRefBS(whole_heap, max_covered_regions) {}
    
  void set_CTRS(CardTableRS* rs) { _rs = rs; }
};
