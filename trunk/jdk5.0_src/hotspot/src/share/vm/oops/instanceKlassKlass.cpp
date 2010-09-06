#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)instanceKlassKlass.cpp	1.118 03/10/02 13:48:37 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

# include "incls/_precompiled.incl"
# include "incls/_instanceKlassKlass.cpp.incl"

klassOop instanceKlassKlass::create_klass(TRAPS) {
  instanceKlassKlass o;
  KlassHandle h_this_klass(THREAD, Universe::klassKlassObj());  
  KlassHandle k = base_create_klass(h_this_klass, header_size(), o.vtbl_value(), CHECK_0);
  // Make sure size calculation is right
  assert(k()->size() == align_object_size(header_size()), "wrong size for object");
  java_lang_Class::create_mirror(k, CHECK_0); // Allocate mirror
  return k();
}

int instanceKlassKlass::oop_size(oop obj) const {
  assert(obj->is_klass(), "must be klass");
  return instanceKlass::cast(klassOop(obj))->object_size();
}

bool instanceKlassKlass::oop_is_parsable(oop obj) const {
  assert(obj->is_klass(), "must be klass");
  return instanceKlass::cast(klassOop(obj))->object_is_parsable();
}

void instanceKlassKlass::oop_follow_contents(oop obj) {
  assert(obj->is_klass(),"must be a klass");
  assert(klassOop(obj)->klass_part()->oop_is_instance(), "must be instance klass");

  instanceKlass* ik = instanceKlass::cast(klassOop(obj));
  ik->follow_static_fields();
  {
    HandleMark hm;
    ik->vtable()->oop_follow_contents();
    ik->itable()->oop_follow_contents();
  }

  MarkSweep::mark_and_push(ik->adr_array_klasses());
  MarkSweep::mark_and_push(ik->adr_methods());
  MarkSweep::mark_and_push(ik->adr_method_ordering());
  MarkSweep::mark_and_push(ik->adr_local_interfaces());
  MarkSweep::mark_and_push(ik->adr_transitive_interfaces());
  MarkSweep::mark_and_push(ik->adr_fields());
  MarkSweep::mark_and_push(ik->adr_constants());
  MarkSweep::mark_and_push(ik->adr_class_loader());
  MarkSweep::mark_and_push(ik->adr_source_file_name());
  MarkSweep::mark_and_push(ik->adr_source_debug_extension());
  MarkSweep::mark_and_push(ik->adr_inner_classes());
  MarkSweep::mark_and_push(ik->adr_protection_domain());
  MarkSweep::mark_and_push(ik->adr_signers());
  MarkSweep::mark_and_push(ik->adr_previous_version());
  MarkSweep::mark_and_push(ik->adr_generic_signature());
  MarkSweep::mark_and_push(ik->adr_class_annotations());
  MarkSweep::mark_and_push(ik->adr_fields_annotations());
  MarkSweep::mark_and_push(ik->adr_methods_annotations());
  MarkSweep::mark_and_push(ik->adr_methods_parameter_annotations());
  MarkSweep::mark_and_push(ik->adr_methods_default_annotations());

  // We do not follow adr_implementor() here. It is followed later
  // in instanceKlass::follow_weak_klass_links()

  klassKlass::oop_follow_contents(obj);

  if (ik->oop_map_cache() != NULL) {
    ik->oop_map_cache()->oop_iterate(&MarkSweep::mark_and_push_closure);
  }

  if (ik->jni_ids() != NULL) {
    ik->jni_ids()->oops_do(&MarkSweep::mark_and_push_closure);
  }

  if (ik->jni_id_map() != NULL) {
    ik->jni_id_map()->oops_do(&MarkSweep::mark_and_push_closure);
  }
}


int instanceKlassKlass::oop_oop_iterate(oop obj, OopClosure* blk) {
  assert(obj->is_klass(),"must be a klass");
  assert(klassOop(obj)->klass_part()->oop_is_instance(), "must be instance klass");
  instanceKlass* ik = instanceKlass::cast(klassOop(obj));
  // Get size before changing pointers.
  // Don't call size() or oop_size() since that is a virtual call.
  int size = ik->object_size();

  ik->iterate_static_fields(blk);
  ik->vtable()->oop_oop_iterate(blk);
  ik->itable()->oop_oop_iterate(blk);

  blk->do_oop(ik->adr_array_klasses());
  blk->do_oop(ik->adr_methods());
  blk->do_oop(ik->adr_method_ordering());
  blk->do_oop(ik->adr_local_interfaces());
  blk->do_oop(ik->adr_transitive_interfaces());
  blk->do_oop(ik->adr_fields());
  blk->do_oop(ik->adr_constants());
  blk->do_oop(ik->adr_class_loader());
  blk->do_oop(ik->adr_protection_domain());
  blk->do_oop(ik->adr_signers());
  blk->do_oop(ik->adr_source_file_name());
  blk->do_oop(ik->adr_source_debug_extension());
  blk->do_oop(ik->adr_inner_classes());
  blk->do_oop(ik->adr_implementor());
  blk->do_oop(ik->adr_previous_version());
  blk->do_oop(ik->adr_generic_signature());
  blk->do_oop(ik->adr_class_annotations());
  blk->do_oop(ik->adr_fields_annotations());
  blk->do_oop(ik->adr_methods_annotations());
  blk->do_oop(ik->adr_methods_parameter_annotations());
  blk->do_oop(ik->adr_methods_default_annotations());

  klassKlass::oop_oop_iterate(obj, blk);

  if(ik->oop_map_cache() != NULL) ik->oop_map_cache()->oop_iterate(blk);
  return size;
}

int instanceKlassKlass::oop_oop_iterate_m(oop obj, OopClosure* blk,
					   MemRegion mr) {
  assert(obj->is_klass(),"must be a klass");
  assert(klassOop(obj)->klass_part()->oop_is_instance(), "must be instance klass");
  instanceKlass* ik = instanceKlass::cast(klassOop(obj));
  // Get size before changing pointers.
  // Don't call size() or oop_size() since that is a virtual call.
  int size = ik->object_size();

  ik->iterate_static_fields(blk, mr);
  ik->vtable()->oop_oop_iterate_m(blk, mr);
  ik->itable()->oop_oop_iterate_m(blk, mr);

  oop* adr;
  adr = ik->adr_array_klasses();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_methods();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_method_ordering();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_local_interfaces();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_transitive_interfaces();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_fields();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_constants();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_class_loader();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_protection_domain();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_signers();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_source_file_name();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_source_debug_extension();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_inner_classes();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_implementor();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_previous_version();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_generic_signature();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_class_annotations();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_fields_annotations();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_methods_annotations();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_methods_parameter_annotations();
  if (mr.contains(adr)) blk->do_oop(adr);
  adr = ik->adr_methods_default_annotations();
  if (mr.contains(adr)) blk->do_oop(adr);

  klassKlass::oop_oop_iterate_m(obj, blk, mr);

  if(ik->oop_map_cache() != NULL) ik->oop_map_cache()->oop_iterate(blk, mr);
  return size;
}


int instanceKlassKlass::oop_adjust_pointers(oop obj) {
  assert(obj->is_klass(),"must be a klass");
  assert(klassOop(obj)->klass_part()->oop_is_instance(), "must be instance klass");

  instanceKlass* ik = instanceKlass::cast(klassOop(obj));
  ik->adjust_static_fields();
  ik->vtable()->oop_adjust_pointers();
  ik->itable()->oop_adjust_pointers();

  MarkSweep::adjust_pointer(ik->adr_array_klasses());
  MarkSweep::adjust_pointer(ik->adr_methods());
  MarkSweep::adjust_pointer(ik->adr_method_ordering());
  MarkSweep::adjust_pointer(ik->adr_local_interfaces());
  MarkSweep::adjust_pointer(ik->adr_transitive_interfaces());
  MarkSweep::adjust_pointer(ik->adr_fields());
  MarkSweep::adjust_pointer(ik->adr_constants());
  MarkSweep::adjust_pointer(ik->adr_class_loader());
  MarkSweep::adjust_pointer(ik->adr_protection_domain());
  MarkSweep::adjust_pointer(ik->adr_signers());
  MarkSweep::adjust_pointer(ik->adr_source_file_name());
  MarkSweep::adjust_pointer(ik->adr_source_debug_extension());
  MarkSweep::adjust_pointer(ik->adr_inner_classes());
  MarkSweep::adjust_pointer(ik->adr_implementor());
  MarkSweep::adjust_pointer(ik->adr_previous_version());
  MarkSweep::adjust_pointer(ik->adr_generic_signature());
  MarkSweep::adjust_pointer(ik->adr_class_annotations());
  MarkSweep::adjust_pointer(ik->adr_fields_annotations());
  MarkSweep::adjust_pointer(ik->adr_methods_annotations());
  MarkSweep::adjust_pointer(ik->adr_methods_parameter_annotations());
  MarkSweep::adjust_pointer(ik->adr_methods_default_annotations());

  if (ik->oop_map_cache() != NULL) {
    ik->oop_map_cache()->oop_iterate(&MarkSweep::adjust_root_pointer_closure);
  }

  if (ik->jni_ids() != NULL) {
    ik->jni_ids()->oops_do(&MarkSweep::adjust_root_pointer_closure);
  }

  if (ik->jni_id_map() != NULL) {
    ik->jni_id_map()->oops_do(&MarkSweep::adjust_root_pointer_closure);
  }

  return klassKlass::oop_adjust_pointers(obj);
}

void instanceKlassKlass::oop_copy_contents(PSPromotionManager* pm, oop obj) {
  instanceKlass* ik = instanceKlass::cast(klassOop(obj));
  ik->copy_static_fields(pm);

  oop* loader_addr = ik->adr_class_loader();
  if (PSScavenge::should_scavenge(*loader_addr)) {
    pm->claim_or_forward(loader_addr);
  }

  oop* pd_addr = ik->adr_protection_domain();
  if (PSScavenge::should_scavenge(*pd_addr)) {
    pm->claim_or_forward(pd_addr);
  }

  oop* sg_addr = ik->adr_signers();
  if (PSScavenge::should_scavenge(*sg_addr)) {
    pm->claim_or_forward(sg_addr);
  }

  klassKlass::oop_copy_contents(pm, obj);
}

klassOop instanceKlassKlass::allocate_instance_klass(int vtable_len, int itable_len, int static_field_size, 
                                                     int nonstatic_oop_map_size, ReferenceType rt, TRAPS) {

  int size = instanceKlass::object_size(align_object_offset(vtable_len) + align_object_offset(itable_len) + static_field_size + nonstatic_oop_map_size);

  // Allocation
  KlassHandle h_this_klass(THREAD, as_klassOop());  
  KlassHandle k;
  if (rt == REF_NONE) {
    // regular klass
    instanceKlass o;
    k = base_create_klass(h_this_klass, size, o.vtbl_value(), CHECK_0);
  } else {
    // reference klass
    instanceRefKlass o;
    k = base_create_klass(h_this_klass, size, o.vtbl_value(), CHECK_0);
  }
  instanceKlass* ik = (instanceKlass*) k()->klass_part();
  assert(!k()->is_parsable(), "not expecting parsability yet.");

  // The sizes of these these three variables are used for determining the
  // size of the instanceKlassOop. It is critical that these are set to the right
  // sizes before the first GC, i.e., when we allocate the mirror.
  ik->set_vtable_length(vtable_len);  
  ik->set_itable_length(itable_len);  
  ik->set_static_field_size(static_field_size);
  ik->set_nonstatic_oop_map_size(nonstatic_oop_map_size);
  assert(k()->size() == size, "wrong size for object");

  ik->set_array_klasses(NULL);
  ik->set_methods(NULL);
  ik->set_method_ordering(NULL);
  ik->set_local_interfaces(NULL);
  ik->set_transitive_interfaces(NULL);
  ik->init_implementor();
  ik->set_fields(NULL);
  ik->set_constants(NULL);
  ik->set_class_loader(NULL);
  ik->set_protection_domain(NULL);
  ik->set_signers(NULL);
  ik->set_source_file_name(NULL);
  ik->set_source_debug_extension(NULL);
  ik->set_inner_classes(NULL);  
  ik->set_static_oop_field_size(0);
  ik->set_nonstatic_field_size(0);  
  ik->set_is_marked_dependent(false);
  ik->set_init_state(instanceKlass::allocated);
  ik->set_init_thread(NULL);
  ik->set_reference_type(rt);
  ik->set_oop_map_cache(NULL);
  ik->set_jni_ids(NULL);
  ik->set_jni_id_map(NULL);
  ik->set_osr_nmethods_head(NULL);
  ik->set_breakpoints(NULL);
  ik->init_previous_version();
  ik->set_generic_signature(NULL);
  ik->set_class_annotations(NULL);
  ik->set_fields_annotations(NULL);
  ik->set_methods_annotations(NULL);
  ik->set_methods_parameter_annotations(NULL);
  ik->set_methods_default_annotations(NULL);
  ik->set_enclosing_method_indices(0, 0);
  ik->set_rewritten_by_redefine(false);

  // initialize the non-header words to zero
  intptr_t* p = (intptr_t*)k();
  for (int index = instanceKlass::header_size(); index < size; index++) {
    p[index] = NULL;
  }

  // To get verify to work - must be set to partial loaded before first GC point.
  NOT_PRODUCT(k()->set_partially_loaded());     

  assert(k()->is_parsable(), "should be parsable here.");

  // GC can happen here
  java_lang_Class::create_mirror(k, CHECK_0); // Allocate mirror
  return k();
}



#ifndef PRODUCT

// Printing

static outputStream* printing_stream = NULL;

static void print_nonstatic_field(fieldDescriptor* fd, oop obj) {
  assert(printing_stream != NULL, "Invalid printing stream");
  printing_stream->print("   - ");
  fd->print_on(printing_stream);
  printing_stream->cr();
}

static void print_static_field(fieldDescriptor* fd, oop obj) {
  assert(printing_stream != NULL, "Invalid printing stream");
  printing_stream->print("   - ");
  fd->print_on_for(printing_stream, obj);
  printing_stream->cr();
}


static const char* state_names[] = {
  "unparseable_by_gc", "allocated", "loaded", "linked", "being_initialized", "fully_initialized", "initialization_error"
};


void instanceKlassKlass::oop_print_on(oop obj, outputStream* st) {
  assert(obj->is_klass(), "must be klass");
  instanceKlass* ik = instanceKlass::cast(klassOop(obj));
  klassKlass::oop_print_on(obj, st);

  st->print(" - instance size:     %d", ik->size_helper());                        st->cr();
  st->print(" - klass size:        %d", ik->object_size());                        st->cr();
  st->print(" - access:            "); ik->access_flags().print_on(st);            st->cr();
  st->print(" - state:             "); st->print_cr(state_names[ik->_init_state]);
  st->print(" - name:              "); ik->name()->print_value_on(st);             st->cr();
  st->print(" - super:             "); ik->super()->print_value_on(st);            st->cr();
  st->print(" - sub:               "); 
  Klass* sub = ik->subklass(); 
  int n;
  for (n = 0; sub != NULL; n++, sub = sub->next_sibling()) {
    if (n < MaxSubklassPrintSize) {       
      sub->as_klassOop()->print_value_on(st); 
      st->print("   "); 
    }
  }
  if (n >= MaxSubklassPrintSize) st->print("(%d more klasses...)", n - MaxSubklassPrintSize);
  st->cr();

  if (ik->is_interface()) { 
    st->print_cr(" - nof implementors:  %d", ik->nof_implementors());
    if (ik->nof_implementors() == 1) {      
      st->print_cr(" - implementor:       "); ik->implementor()->print_value_on(st);st->cr();
    }
  }

  st->print(" - arrays:            "); ik->array_klasses()->print_value_on(st);     st->cr();
  st->print(" - methods:           "); ik->methods()->print_value_on(st);           st->cr();
  if (Verbose) {
    objArrayOop methods = ik->methods();
    for(int i = 0; i < methods->length(); i++) {
      tty->print("%d : ", i); methods->obj_at(i)->print_value(); tty->cr();
    }
  }
  st->print(" - method ordering:   "); ik->method_ordering()->print_value_on(st);       st->cr();
  st->print(" - local interfaces:  "); ik->local_interfaces()->print_value_on(st);      st->cr();
  st->print(" - trans. interfaces: "); ik->transitive_interfaces()->print_value_on(st); st->cr();
  st->print(" - constants:         "); ik->constants()->print_value_on(st);         st->cr();
  st->print(" - class loader:      "); ik->class_loader()->print_value_on(st);      st->cr();
  st->print(" - protection domain: "); ik->protection_domain()->print_value_on(st); st->cr();
  st->print(" - signers:           "); ik->signers()->print_value_on(st);           st->cr();
  if (ik->source_file_name() != NULL) {
    st->print(" - source file:       "); 
    ik->source_file_name()->print_value_on(st);
    st->cr();
  }
  if (ik->source_debug_extension() != NULL) {
    st->print(" - source debug extension:       "); 
    ik->source_debug_extension()->print_value_on(st);
    st->cr();
  }
  if (ik->has_previous_version()) {      
    st->print_cr(" - previous version:       "); 
    ik->previous_version()->print_value_on(st);
    st->cr();
  }
  if (ik->generic_signature() != NULL) {
    st->print(" - generic signature:            "); 
    ik->generic_signature()->print_value_on(st);
  }
  st->print(" - inner classes:     "); ik->inner_classes()->print_value_on(st);     st->cr();
  st->print(" - java mirror:       "); ik->java_mirror()->print_value_on(st);       st->cr();
  st->print(" - vtable length      %d  (start addr: " INTPTR_FORMAT ")", ik->vtable_length(), ik->start_of_vtable());  st->cr();  
  st->print(" - itable length      %d (start addr: " INTPTR_FORMAT ")", ik->itable_length(), ik->start_of_itable()); st->cr();
  st->print_cr(" - static fields:");
  printing_stream = st;
  ik->do_local_static_fields(print_static_field, obj);
  st->print_cr(" - non-static fields:");
  ik->do_nonstatic_fields(print_nonstatic_field, NULL);
  printing_stream = NULL;

  st->print(" - static oop maps:     ");
  if (ik->static_oop_field_size() > 0) {
    int first_offset = ik->offset_of_static_fields();
    st->print("%d-%d", first_offset, first_offset + ik->static_oop_field_size() - 1);
  }
  st->cr();

  st->print(" - non-static oop maps: ");
  OopMapBlock* map     = ik->start_of_nonstatic_oop_maps();
  OopMapBlock* end_map = map + ik->nonstatic_oop_map_size();
  while (map < end_map) {
    st->print("%d-%d ", map->offset(), map->offset() + map->length() - 1);
    map++;
  }
  st->cr();
}


void instanceKlassKlass::oop_print_value_on(oop obj, outputStream* st) {
  assert(obj->is_klass(), "must be klass");
  instanceKlass* ik = instanceKlass::cast(klassOop(obj));
  ik->name()->print_value_on(st);
}


const char* instanceKlassKlass::internal_name() const {
  return "{instance class}";
}

// Verification


class VerifyFieldClosure: public OopClosure {
 public:
  void do_oop(oop* p) {
    guarantee(Universe::heap()->is_in(p), "should be in heap");
    guarantee((*p)->is_oop_or_null(), "should be in heap");
  }
};


void instanceKlassKlass::oop_verify_on(oop obj, outputStream* st) {
  klassKlass::oop_verify_on(obj, st);
  if (!obj->partially_loaded()) {
    Thread *thread = Thread::current();
    instanceKlass* ik = instanceKlass::cast(klassOop(obj));   

    // Avoid redundant verifies
    if (ik->_verify_count == Universe::verify_count()) return;
    ik->_verify_count = Universe::verify_count();

    // Verify that klass is present in SystemDictionary
    if (ik->is_loaded()) {
      symbolHandle h_name (thread, ik->name());
      Handle h_loader (thread, ik->class_loader());
      SystemDictionary::verify_obj_klass_present(obj, h_name, h_loader);
    }
    
    // Verify static fields
    VerifyFieldClosure blk;
    ik->iterate_static_fields(&blk);

    // Verify vtables
    if (ik->is_linked()) {
      ResourceMark rm(thread);  
      // $$$ This used to be done only for m/s collections.  Doing it
      // always seemed a valid generalization.  (DLD -- 6/00)
      ik->vtable()->verify(st);
    }
  
    // Verify oop map cache
    if (ik->oop_map_cache() != NULL) {
      ik->oop_map_cache()->verify();
    }

    // Verify first subklass
    if (ik->subklass_oop() != NULL) { 
      guarantee(ik->subklass_oop()->is_perm(),  "should be in permspace");
      guarantee(ik->subklass_oop()->is_klass(), "should be klass");
    }

    // Verify siblings
    klassOop super = ik->super();
    Klass* sib = ik->next_sibling();
    int sib_count = 0;
    while (sib != NULL) {
      if (sib == ik) {
        fatal1("subclass cycle of length %d", sib_count);
      }
      if (sib_count >= 100000) {
        fatal1("suspiciously long subclass list %d", sib_count);
      }
      guarantee(sib->as_klassOop()->is_klass(), "should be klass");
      guarantee(sib->as_klassOop()->is_perm(),  "should be in permspace");
      guarantee(sib->super() == super, "siblings should have same superklass");
      sib = sib->next_sibling();
    }

    // Verify implementor field
    if (ik->implementor() != NULL) {
      guarantee(ik->is_interface(), "only interfaces should have implementor set");
      guarantee(ik->nof_implementors() == 1, "should only have one implementor");
      klassOop im = ik->implementor();
      guarantee(im->is_perm(),  "should be in permspace");
      guarantee(im->is_klass(), "should be klass");
      guarantee(!Klass::cast(klassOop(im))->is_interface(), "implementors cannot be interfaces");
    }
    
    // Verify local interfaces
    objArrayOop local_interfaces = ik->local_interfaces();
    guarantee(local_interfaces->is_perm(),          "should be in permspace");
    guarantee(local_interfaces->is_objArray(),      "should be obj array");
    int j;
    for (j = 0; j < local_interfaces->length(); j++) {
      oop e = local_interfaces->obj_at(j);
      guarantee(e->is_klass() && Klass::cast(klassOop(e))->is_interface(), "invalid local interface");
    }

    // Verify transitive interfaces
    objArrayOop transitive_interfaces = ik->transitive_interfaces();
    guarantee(transitive_interfaces->is_perm(),          "should be in permspace");
    guarantee(transitive_interfaces->is_objArray(),      "should be obj array");
    for (j = 0; j < transitive_interfaces->length(); j++) {
      oop e = transitive_interfaces->obj_at(j);
      guarantee(e->is_klass() && Klass::cast(klassOop(e))->is_interface(), "invalid transitive interface");
    }

    // Verify methods
    objArrayOop methods = ik->methods();
    guarantee(methods->is_perm(),              "should be in permspace");
    guarantee(methods->is_objArray(),          "should be obj array");
    for (j = 0; j < methods->length(); j++) {
      guarantee(methods->obj_at(j)->is_method(), "non-method in methods array");
    }
    for (j = 0; j < methods->length() - 1; j++) {
      methodOop m1 = methodOop(methods->obj_at(j));
      methodOop m2 = methodOop(methods->obj_at(j + 1));
      guarantee(m1->name()->fast_compare(m2->name()) <= 0, "methods not sorted correctly");
    }
    
    // Verify method ordering
    typeArrayOop method_ordering = ik->method_ordering();
    guarantee(method_ordering->is_perm(),              "should be in permspace");
    guarantee(method_ordering->is_typeArray(),         "should be type array");
    int length = method_ordering->length();
    if (JvmtiExport::can_maintain_original_method_order()) {
      guarantee(length == methods->length(),           "invalid method ordering length");
      jlong sum = 0;
      for (j = 0; j < length; j++) {
        int original_index = method_ordering->int_at(j);
        guarantee(original_index >= 0 && original_index < length, "invalid method ordering index");
        sum += original_index;
      }
      // Verify sum of indices 0,1,...,length-1
      guarantee(sum == ((jlong)length*(length-1))/2,   "invalid method ordering sum");
    } else {
      guarantee(length == 0,                           "invalid method ordering length");
    }

    // Verify JNI static field identifiers
    if (ik->jni_ids() != NULL) {
      ik->jni_ids()->verify(ik->as_klassOop());
    }

    // Verify JNI method identifiers
    if (ik->jni_id_map() != NULL) {
      ik->jni_id_map()->verify();
    }

    // Verify other fields
    if (ik->array_klasses() != NULL) {
      guarantee(ik->array_klasses()->is_perm(),      "should be in permspace");
      guarantee(ik->array_klasses()->is_klass(),     "should be klass");
    }
    guarantee(ik->fields()->is_perm(),               "should be in permspace");
    guarantee(ik->fields()->is_typeArray(),          "should be type array");
    guarantee(ik->constants()->is_perm(),            "should be in permspace");
    guarantee(ik->constants()->is_constantPool(),    "should be constant pool");
    guarantee(ik->inner_classes()->is_perm(),        "should be in permspace");
    guarantee(ik->inner_classes()->is_typeArray(),   "should be type array");
    if (ik->source_file_name() != NULL) {
      guarantee(ik->source_file_name()->is_perm(),   "should be in permspace");
      guarantee(ik->source_file_name()->is_symbol(), "should be symbol");
    }
    if (ik->source_debug_extension() != NULL) {
      guarantee(ik->source_debug_extension()->is_perm(),   "should be in permspace");
      guarantee(ik->source_debug_extension()->is_symbol(), "should be symbol");
    }
    if (ik->protection_domain() != NULL) {
      guarantee(ik->protection_domain()->is_oop(),  "should be oop");
    }
    if (ik->signers() != NULL) {
      guarantee(ik->signers()->is_objArray(),       "should be obj array");
    }
    if (ik->generic_signature() != NULL) {
      guarantee(ik->generic_signature()->is_perm(),   "should be in permspace");
      guarantee(ik->generic_signature()->is_symbol(), "should be symbol");
    }
    if (ik->class_annotations() != NULL) {
      guarantee(ik->class_annotations()->is_typeArray(), "should be type array");
    }
    if (ik->fields_annotations() != NULL) {
      guarantee(ik->fields_annotations()->is_objArray(), "should be obj array");
    }
    if (ik->methods_annotations() != NULL) {
      guarantee(ik->methods_annotations()->is_objArray(), "should be obj array");
    }
    if (ik->methods_parameter_annotations() != NULL) {
      guarantee(ik->methods_parameter_annotations()->is_objArray(), "should be obj array");
    }
    if (ik->methods_default_annotations() != NULL) {
      guarantee(ik->methods_default_annotations()->is_objArray(), "should be obj array");
    }
  }
}


bool instanceKlassKlass::oop_partially_loaded(oop obj) const {
  assert(obj->is_klass(), "object must be klass");
  instanceKlass* ik = instanceKlass::cast(klassOop(obj));
  assert(ik-oop_is_instance(), "object must be instanceKlass");
  return ik->transitive_interfaces() == (objArrayOop) obj;   // Check whether transitive_interfaces points to self
}


// The transitive_interfaces is the last field set when loading an object.
void instanceKlassKlass::oop_set_partially_loaded(oop obj) {
  assert(obj->is_klass(), "object must be klass");
  instanceKlass* ik = instanceKlass::cast(klassOop(obj));
  assert(ik-oop_is_instance(), "object must be instanceKlass");
  assert(ik->transitive_interfaces() == NULL, "just checking");
  ik->set_transitive_interfaces((objArrayOop) obj);   // Temporarily set transitive_interfaces to point to self
}

#endif // PRODUCT
