/*
 * @(#)GDIHashtable.cpp	1.9 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#include "GDIHashtable.h"
#include "awt_GDIObject.h"
#include "awt_dlls.h"

GDIHashtable::BatchDestructionManager GDIHashtable::manager;

/*
 * The order of monitor entrance is BatchDestructionManager->List->Hashtable.
 * GDIHashtable::put() and GDIHashtable::release() are designed to be called
 * only when we are synchronized on the BatchDestructionManager lock.
 */

void* GDIHashtable::put(void* key, void* value) {
    manager.decrementCounter();
    return Hashtable::put(key, value);
}

void GDIHashtable::release(void* key) {
    if (!manager.isBatchingEnabled()) {
        void* value = remove(key);
        DASSERT(value != NULL);
        m_deleteProc(value);
    }
    manager.update();
}

void GDIHashtable::flush() {

    CriticalSection::Lock l(lock);

    for (int i = capacity; i-- > 0;) {
        HashtableEntry* prev = NULL;
        for (HashtableEntry* e = table[i] ; e != NULL ; ) {
            AwtGDIObject* pGDIObject = (AwtGDIObject*)e->value;
            if (pGDIObject->GetRefCount() <= 0) {
                if (prev != NULL) {
                    prev->next = e->next;
                } else {
                    table[i] = e->next;
                }
                count--;
                HashtableEntry* next = e->next;
                if (m_deleteProc) {
                    (*m_deleteProc)(e->value);
                }
                delete e;
                e = next;
            } else {
                prev = e;
                e = e->next;
            }
        }
    }
}

void GDIHashtable::List::flushAll() {

    CriticalSection::Lock l(m_listLock);

    for (ListEntry* e = m_pHead; e != NULL; e = e->next) {
        e->table->flush();
    }
}

void GDIHashtable::List::add(GDIHashtable* table) {

    CriticalSection::Lock l(m_listLock);

    ListEntry* e = new ListEntry;
    e->table = table;
    e->next = m_pHead;
    m_pHead = e;
}

void GDIHashtable::List::remove(GDIHashtable* table) {

    CriticalSection::Lock l(m_listLock);

    ListEntry* prev = NULL;
    for (ListEntry* e = m_pHead; e != NULL; prev = e, e = e->next) {
        if (e->table == table) {
            if (prev != NULL) {
                prev->next = e->next;
            } else {
                m_pHead = e->next;
            }
            delete e;
            return;
        }
    }
}

void GDIHashtable::List::clear() {

    CriticalSection::Lock l(m_listLock);

    ListEntry* e = m_pHead;
    m_pHead = NULL;
    while (e != NULL) {
        ListEntry* next = e->next;
        delete e;
        e = next;
    }
}

#undef GFSR_GDIRESOURCES
#define GFSR_GDIRESOURCES     0x0001

GDIHashtable::BatchDestructionManager::BatchDestructionManager(UINT nFirstThreshold,
                                                               UINT nSecondThreshold,
                                                               UINT nDestroyPeriod) :
  m_nFirstThreshold(nFirstThreshold),
  m_nSecondThreshold(nSecondThreshold),
  m_nDestroyPeriod(nDestroyPeriod),
  m_nCounter(0),
  m_bBatchingEnabled(TRUE) {
    load_rsrc32_procs();
}

void GDIHashtable::BatchDestructionManager::update() {

    if (get_free_system_resources != NULL) {

        CriticalSection::Lock l(m_managerLock);

        if (m_nCounter < 0) {
            UINT nFreeResources = (*get_free_system_resources)(GFSR_GDIRESOURCES);
            /*
             * If m_bBatchingEnabled is FALSE there is no need
             * to flush since we have been destroying all
             * GDI resources as soon as they were released.
             */
            if (m_bBatchingEnabled) {
                if (nFreeResources < m_nFirstThreshold) {
                    flushAll();
                    nFreeResources = (*get_free_system_resources)(GFSR_GDIRESOURCES);
                }
            } 
            if (nFreeResources < m_nSecondThreshold) {
                m_bBatchingEnabled = FALSE;
                m_nCounter = m_nDestroyPeriod;
            } else {
                m_bBatchingEnabled = TRUE;
                /*
                 * The frequency of checks must depend on the currect amount
                 * of free space in GDI heaps. Otherwise we can run into the
                 * Resource Meter warning dialog when GDI resources are low.
                 * This is a heuristic rule that provides this dependency.
                 * These numbers have been chosen because:
                 * Resource Meter posts a warning dialog when less than 10%
                 * of GDI resources are free.
                 * 5 pens/brushes take 1%. So 3 is the upper bound.
                 * When changing this rule you should check that performance
                 * isn't affected (with Caffeine Mark and JMark).
                 */
                m_nCounter = (nFreeResources - 10) * 3;
            }
        }
    }
}

