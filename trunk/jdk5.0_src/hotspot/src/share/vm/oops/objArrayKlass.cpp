#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)objArrayKlass.cpp	1.128 03/12/23 16:42:03 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

# include "incls/_precompiled.incl"
# include "incls/_objArrayKlass.cpp.incl"

int objArrayKlass::oop_size(oop obj) const {
  assert(obj->is_objArray(), "must be object array");
  return objArrayOop(obj)->object_size();
}

objArrayOop objArrayKlass::allocate(int length, TRAPS) {  
  if (length >= 0) {
    if (length <= arrayOopDesc::max_array_length(T_OBJECT)) {
      int size = objArrayOopDesc::object_size(length);
      KlassHandle h_k(THREAD, as_klassOop());
      objArrayOop a = (objArrayOop)CollectedHeap::array_allocate(h_k, size, length, CHECK_0);
      return a;
    } else {
      THROW_OOP_0(Universe::out_of_memory_error_array_size());
    }
  } else {
    THROW_0(vmSymbols::java_lang_NegativeArraySizeException());
  }
}

static int multi_alloc_counter = 0;

oop objArrayKlass::multi_allocate(int rank, jint* sizes, int next_dim_step, TRAPS) { 
  assert(next_dim_step == 1  ||  next_dim_step == -1,  "next_dim_step better be one of these");
  int length = *sizes;
  // Call to lower_dimension uses this pointer, so most be called before a possible GC
  KlassHandle h_lower_dimension(THREAD, lower_dimension());
  // If length < 0 allocate will throw an exception.
  objArrayOop array = allocate(length, CHECK_0);
  objArrayHandle h_array (THREAD, array);
  if (rank > 1) {
    if (length != 0) {
      for (int index = 0; index < length; index++) {  
        arrayKlass* ak = arrayKlass::cast(h_lower_dimension());
        oop sub_array = ak->multi_allocate(rank-1, &sizes[next_dim_step],
                                           next_dim_step, CHECK_0);
        h_array->obj_at_put(index, sub_array);
      }
    } else {
      // Since this array dimension has zero length, nothing will be
      // allocated, however the lower dimension values must be checked
      // for illegal values.
      for (int i = 0; i < rank - 1; ++i) {
        sizes += next_dim_step;
        if (*sizes < 0) {
          THROW_0(vmSymbols::java_lang_NegativeArraySizeException());
        }
      }
    }
  }
  return h_array();
}

void objArrayKlass::copy_array(arrayOop s, int src_pos, arrayOop d,
                               int dst_pos, int length, TRAPS) {
  assert(s->is_objArray(), "must be obj array");

  if (!d->is_objArray()) {
    THROW(vmSymbols::java_lang_ArrayStoreException());
  }

  // Check is all offsets and lengths are non negative
  if (src_pos < 0 || dst_pos < 0 || length < 0) {
    THROW(vmSymbols::java_lang_ArrayIndexOutOfBoundsException());
  }
  // Check if the ranges are valid
  if  ( (((unsigned int) length + (unsigned int) src_pos) > (unsigned int) s->length())
     || (((unsigned int) length + (unsigned int) dst_pos) > (unsigned int) d->length()) ) {
    THROW(vmSymbols::java_lang_ArrayIndexOutOfBoundsException());
  }

  // Special case. Boundary cases must be checked first
  // This allows the following call: copy_array(s, s.length(), d.length(), 0).
  // This is correct, since the position is supposed to be an 'in between point', i.e., s.length(),
  // points to the right of the last element.
  if (length==0) {
    return;
  }

  oop* const src = objArrayOop(s)->obj_at_addr(src_pos);
  oop* const dst = objArrayOop(d)->obj_at_addr(dst_pos);
  const size_t word_len = length * HeapWordsPerOop;

  // For performance reasons, we assume we are using a card marking write
  // barrier. The assert will fail if this is not the case.
  BarrierSet* bs = Universe::heap()->barrier_set();
  assert(bs->has_write_ref_array_opt(), "Barrier set must have ref array opt");

  if (s == d) {
    // since source and destination are equal we do not need conversion checks. 
    assert(length > 0, "sanity check");
    Copy::conjoint_oops_atomic(src, dst, length);
  } else {
    // We have to make sure all elements conform to the destination array
    klassOop bound = objArrayKlass::cast(d->klass())->element_klass();
    klassOop stype = objArrayKlass::cast(s->klass())->element_klass();
    if (stype == bound || Klass::cast(stype)->is_subtype_of(bound)) {
      // elements are guaranteed to be subtypes, so no check necessary
      Copy::conjoint_oops_atomic(src, dst, length);
    } else {
      // slow case: need individual subtype checks
      // note: don't use obj_at_put below because it includes a redundant store check
      oop* from = src;
      oop* end = from + length;
      for (oop* p = dst; from < end; from++, p++) {
        oop element = *from;
        if (element == NULL || Klass::cast(element->klass())->is_subtype_of(bound)) {
          *p = element;
        } else {
	  // We must do a barrier to cover the partial copy.
          const size_t done_word_len = pointer_delta(p, dst, oopSize) *
                                       HeapWordsPerOop;
          bs->write_ref_array(MemRegion((HeapWord*)dst, done_word_len));
          THROW(vmSymbols::java_lang_ArrayStoreException());
          return;
        }
      }
    }
  }
  bs->write_ref_array(MemRegion((HeapWord*)dst, word_len));
}


klassOop objArrayKlass::array_klass_impl(bool or_null, int n, TRAPS) {
  objArrayKlassHandle h_this(THREAD, as_klassOop());
  return array_klass_impl(h_this, or_null, n, CHECK_0);
}


klassOop objArrayKlass::array_klass_impl(objArrayKlassHandle this_oop, bool or_null, int n, TRAPS) {  
  
  assert(this_oop->dimension() <= n, "check order of chain");
  int dimension = this_oop->dimension();
  if (dimension == n) 
    return this_oop();

  objArrayKlassHandle ak (THREAD, this_oop->higher_dimension());
  if (ak.is_null()) {    
    if (or_null)  return NULL;

    ResourceMark rm;
    JavaThread *jt = (JavaThread *)THREAD;
    {
      MutexLocker mc(Compile_lock, THREAD);   // for vtables
      // Ensure atomic creation of higher dimensions
      MutexLocker mu(MultiArray_lock, THREAD);

      // Check if another thread beat us
      ak = objArrayKlassHandle(THREAD, this_oop->higher_dimension());
      if( ak.is_null() ) {

        // We grab locks above and the allocate_objArray_klass() code
        // path needs to post OBJECT_ALLOC events for the newly
        // allocated objects. We can't post events while holding locks
        // so we store the information on the side until after we
        // release the locks.
        if (Universe::jvmpi_alloc_event_enabled()) {
          jt->set_deferred_obj_alloc_events(
            new GrowableArray<DeferredObjAllocEvent *>(dimension + 1, true));
        }
    
        // Create multi-dim klass object and link them together
        klassOop new_klass = 
          objArrayKlassKlass::cast(Universe::objArrayKlassKlassObj())->
          allocate_objArray_klass(dimension + 1, this_oop, CHECK_0);
        ak = objArrayKlassHandle(THREAD, new_klass);
        this_oop->set_higher_dimension(ak());    
        ak->set_lower_dimension(this_oop());
        assert(ak->oop_is_objArray(), "incorrect initialization of objArrayKlass");
      }
    }

    GrowableArray<DeferredObjAllocEvent *>* deferred_list =
      jt->deferred_obj_alloc_events();
    if (deferred_list != NULL) {
      if (deferred_list->length() > 0) {
        Universe::jvmpi_post_deferred_obj_alloc_events(deferred_list);
      }
      jt->set_deferred_obj_alloc_events(NULL);
    }
  }

  if (or_null) {
    return ak->array_klass_or_null(n);
  }
  return ak->array_klass(n, CHECK_0);
}

klassOop objArrayKlass::array_klass_impl(bool or_null, TRAPS) {
  return array_klass_impl(or_null, dimension() +  1, CHECK_0);
}

bool objArrayKlass::can_be_primary_super_slow() const {
  if (!bottom_klass()->klass_part()->can_be_primary_super())
    // array of interfaces
    return false;
  else
    return Klass::can_be_primary_super_slow();
}

objArrayOop objArrayKlass::compute_secondary_supers(int num_extra_slots, TRAPS) {
  // interfaces = { cloneable_klass, serializable_klass, elemSuper[], ... };
  objArrayOop es = Klass::cast(element_klass())->secondary_supers();
  objArrayHandle elem_supers (THREAD, es);
  int num_elem_supers = elem_supers.is_null() ? 0 : elem_supers->length();
  int num_secondaries = num_extra_slots + 2 + num_elem_supers;
  if (num_secondaries == 2) {
    // Must share this for correct bootstrapping!
    return Universe::the_array_interfaces_array();
  } else {
    objArrayOop sec_oop = oopFactory::new_system_objArray(num_secondaries, CHECK_0);
    objArrayHandle secondaries(THREAD, sec_oop);
    secondaries->obj_at_put(num_extra_slots+0, SystemDictionary::cloneable_klass());
    secondaries->obj_at_put(num_extra_slots+1, SystemDictionary::serializable_klass());
    for (int i = 0; i < num_elem_supers; i++) {
      klassOop elem_super = (klassOop) elem_supers->obj_at(i);
      klassOop array_super = elem_super->klass_part()->array_klass_or_null();
      assert(array_super != NULL, "must already have been created");
      secondaries->obj_at_put(num_extra_slots+2+i, array_super);
    }
    return secondaries();
  }
}

bool objArrayKlass::compute_is_subtype_of(klassOop k) {
  if (!k->klass_part()->oop_is_objArray())
    return arrayKlass::compute_is_subtype_of(k);

  objArrayKlass* oak = objArrayKlass::cast(k);
  return element_klass()->klass_part()->is_subtype_of(oak->element_klass());
}


void objArrayKlass::initialize(TRAPS) {
  Klass::cast(bottom_klass())->initialize(THREAD);  // dispatches to either instanceKlass or typeArrayKlass
}


void objArrayKlass::oop_follow_contents(oop obj) {
  assert (obj->is_array(), "obj must be array");
  arrayOop a = arrayOop(obj);
  a->follow_header();
  oop* base      = (oop*)a->base(T_OBJECT);
  oop* const end = base + a->length();
  while (base < end) {
    if (*base != NULL) 
      // we call mark_and_follow here to avoid excessive marking stack usage
      MarkSweep::mark_and_follow(base); 
    base++;
  }  
}

#define invoke_closure_on(base, closure, nv_suffix) {                                  \
  if (*(base) != NULL) {                                                               \
    (closure)->do_oop##nv_suffix(base);                                                \
  }                                                                                    \
}

#define ObjArrayKlass_OOP_OOP_ITERATE_DEFN(OopClosureType, nv_suffix)           \
                                                                                \
int objArrayKlass::oop_oop_iterate##nv_suffix(oop obj,                          \
                                              OopClosureType* closure) {        \
  SpecializationStats::record_iterate_call##nv_suffix(SpecializationStats::oa); \
  assert (obj->is_array(), "obj must be array");                                \
  objArrayOop a = objArrayOop(obj);                                             \
  /* Get size before changing pointers. */                                      \
  /* Don't call size() or oop_size() since that is a virtual call. */           \
  int size = a->object_size();                                                  \
  if (closure->do_header()) {                                                   \
    a->oop_iterate_header(closure);                                             \
  }                                                                             \
  oop* base               = a->base();                                          \
  oop* const end          = base + a->length();                                 \
  const intx field_offset = PrefetchFieldsAhead;                                \
  if (field_offset > 0) {                                                       \
    while (base < end) {                                                        \
      prefetch_beyond(base, end, field_offset, closure->prefetch_style());      \
      invoke_closure_on(base, closure, nv_suffix);                              \
      base++;                                                                   \
    }                                                                           \
  } else {                                                                      \
    while (base < end) {                                                        \
      invoke_closure_on(base, closure, nv_suffix);                              \
      base++;                                                                   \
    }                                                                           \
  }                                                                             \
  return size;                                                                  \
}

#define ObjArrayKlass_OOP_OOP_ITERATE_DEFN_m(OopClosureType, nv_suffix)         \
                                                                                \
int objArrayKlass::oop_oop_iterate##nv_suffix##_m(oop obj,                      \
                                                  OopClosureType* closure,      \
                                                  MemRegion mr) {               \
  SpecializationStats::record_iterate_call##nv_suffix(SpecializationStats::oa); \
  assert(obj->is_array(), "obj must be array");                                 \
  objArrayOop a  = objArrayOop(obj);                                            \
  /* Get size before changing pointers. */                                      \
  /* Don't call size() or oop_size() since that is a virtual call */            \
  int size = a->object_size();                                                  \
  if (closure->do_header()) {                                                   \
    a->oop_iterate_header(closure, mr);                                         \
  }                                                                             \
  oop* bottom = (oop*)mr.start();                                               \
  oop* top    = (oop*)mr.end();                                                 \
  oop* base = a->base();                                                        \
  oop* end    = base + a->length();                                             \
  if (base < bottom) {                                                          \
    base = bottom;                                                              \
  }                                                                             \
  if (end > top) {                                                              \
    end = top;                                                                  \
  }                                                                             \
  const intx field_offset = PrefetchFieldsAhead;                                \
  if (field_offset > 0) {                                                       \
    while (base < end) {                                                        \
      prefetch_beyond(base, end, field_offset, closure->prefetch_style());      \
      invoke_closure_on(base, closure, nv_suffix);                              \
      base++;                                                                   \
    }                                                                           \
  } else {                                                                      \
    while (base < end) {                                                        \
      invoke_closure_on(base, closure, nv_suffix);                              \
      base++;                                                                   \
    }                                                                           \
  }                                                                             \
  return size;                                                                  \
}

ALL_OOP_OOP_ITERATE_CLOSURES_1(ObjArrayKlass_OOP_OOP_ITERATE_DEFN)
ALL_OOP_OOP_ITERATE_CLOSURES_2(ObjArrayKlass_OOP_OOP_ITERATE_DEFN)
ALL_OOP_OOP_ITERATE_CLOSURES_3(ObjArrayKlass_OOP_OOP_ITERATE_DEFN)
ALL_OOP_OOP_ITERATE_CLOSURES_1(ObjArrayKlass_OOP_OOP_ITERATE_DEFN_m)
ALL_OOP_OOP_ITERATE_CLOSURES_2(ObjArrayKlass_OOP_OOP_ITERATE_DEFN_m)
ALL_OOP_OOP_ITERATE_CLOSURES_3(ObjArrayKlass_OOP_OOP_ITERATE_DEFN_m)

int objArrayKlass::oop_adjust_pointers(oop obj) {
  assert(obj->is_objArray(), "obj must be obj array");
  objArrayOop a = objArrayOop(obj);
  // Get size before changing pointers.
  // Don't call size() or oop_size() since that is a virtual call.
  int size = a->object_size();
  a->adjust_header();
  oop* base      = a->base();
  oop* const end = base + a->length();
  while (base < end) {
    MarkSweep::adjust_pointer(base);
    base++;
  }  
  return size;
}

void objArrayKlass::oop_copy_contents(PSPromotionManager* pm, oop obj) {
  assert (obj->is_objArray(), "obj must be obj array");
  // Compute oop range
  oop* base = objArrayOop(obj)->base();
  oop* end = base + objArrayOop(obj)->length();
  // Iterate over oops
  while (base < end) {
    if (PSScavenge::should_scavenge(*base)) {
      pm->claim_or_forward(base);
    }
    base++;
  }
  assert(align_object_size(end - (oop*)obj) == oop_size(obj), "checking size");
}

// JVM support

jint objArrayKlass::compute_modifier_flags(TRAPS) const {
  // The modifier for an objectArray is the same as its element
  if (element_klass() == NULL) {
    assert(Universe::is_bootstrapping(), "partial objArray only at startup");
    return JVM_ACC_ABSTRACT | JVM_ACC_FINAL | JVM_ACC_PUBLIC;
  }
  // Recurse down the element list
  jint element_flags = Klass::cast(element_klass())->compute_modifier_flags(CHECK_0);  

  return (element_flags & (JVM_ACC_PUBLIC | JVM_ACC_PRIVATE | JVM_ACC_PROTECTED))
                        | (JVM_ACC_ABSTRACT | JVM_ACC_FINAL);
}


#ifndef PRODUCT
// Printing

void objArrayKlass::oop_print_on(oop obj, outputStream* st) {
  arrayKlass::oop_print_on(obj, st);
  assert(obj->is_objArray(), "must be objArray");
  objArrayOop oa = objArrayOop(obj);
  int print_len = MIN2((intx) oa->length(), MaxElementPrintSize);
  for(int index = 0; index < print_len; index++) {
    st->print(" - %3d : ", index);
    oa->obj_at(index)->print_value_on(st);
    st->cr();
  }
  int remaining = oa->length() - print_len;
  if (remaining > 0) {
    tty->print_cr(" - <%d more elements, increase MaxElementPrintSize to print>", remaining);
  }
}


void objArrayKlass::oop_print_value_on(oop obj, outputStream* st) {
  assert(obj->is_objArray(), "must be objArray");
  element_klass()->print_value_on(st);
  st->print("a [%d] ", objArrayOop(obj)->length());
  as_klassOop()->klass()->print_value_on(st);
}

const char* objArrayKlass::internal_name() const {
  return external_name();
}

// Verification

void objArrayKlass::oop_verify_on(oop obj, outputStream* st) {
  arrayKlass::oop_verify_on(obj, st);
  guarantee(obj->is_objArray(), "must be objArray");
  objArrayOop oa = objArrayOop(obj);
  for(int index = 0; index < oa->length(); index++) {
    guarantee(oa->obj_at(index)->is_oop_or_null(), "should be oop");
  }
}

void objArrayKlass::oop_verify_old_oop(oop obj, oop* p, bool allow_dirty) {
  /* $$$ move into remembered set verification?
  RememberedSet::verify_old_oop(obj, p, allow_dirty, true);
  */
}
#endif // PRODUCT
