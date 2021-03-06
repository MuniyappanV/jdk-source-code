#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)histogram.cpp	1.14 03/12/23 16:44:48 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

# include "incls/_precompiled.incl"
# include "incls/_histogram.cpp.incl"

#ifdef ASSERT

////////////////// HistogramElement ////////////////////////

HistogramElement::HistogramElement() {
  _count = 0;
}

int HistogramElement::count() {
  return _count;
}

const char* HistogramElement::name() {
  return _name;
}

void HistogramElement::increment_count() {
  // We can't use the accessor :-(.
  Atomic::inc(&_count);
}

int HistogramElement::compare(HistogramElement* e1,HistogramElement* e2) {
  if(e1->count() > e2->count()) {
    return -1;
  } else if(e1->count() < e2->count()) {
    return 1;
  }
  return 0;
}

void HistogramElement::print_on(outputStream* st) const {
  st->print("%s",((HistogramElement*)this)->name());
  st->fill_to(40);
  st->print_cr("%d",((HistogramElement*)this)->count());
}

////////////////// Histogram ////////////////////////

int Histogram::sort_helper(HistogramElement** e1, HistogramElement** e2) {
  return (*e1)->compare(*e1,*e2);
}

Histogram::Histogram(const char* title,int estimatedCount) {
  _title = title;
  _elements = new (ResourceObj::C_HEAP) GrowableArray<HistogramElement*>(estimatedCount,true);
}
  
void Histogram::add_element(HistogramElement* element) {
  // Note, we need to add locking !
  elements()->append(element);
}

void Histogram::print_header(outputStream* st) {
  st->print_cr("%s",title());
  st->print_cr("--------------------------------------------------");
}

void Histogram::print_elements(outputStream* st) {
  elements()->sort(Histogram::sort_helper);
  jint total = 0;
  for(int i=0; i < elements()->length(); i++) {
    elements()->at(i)->print();
    total += elements()->at(i)->count();
  }
  st->print("Total");
  st->fill_to(40);
  st->print_cr("%d", total);
  st->cr();
}

void Histogram::print_on(outputStream* st) const {
  ((Histogram*)this)->print_header(st);
  ((Histogram*)this)->print_elements(st);
}

#endif
