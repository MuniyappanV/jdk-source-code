#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)binaryTreeDictionary.cpp	1.26 03/12/23 16:40:52 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

# include "incls/_precompiled.incl"
# include "incls/_binaryTreeDictionary.cpp.incl"

////////////////////////////////////////////////////////////////////////////////
// A binary tree based search structure for free blocks.
// This is currently used in the Concurrent Mark&Sweep implementation.
////////////////////////////////////////////////////////////////////////////////

TreeChunk* TreeChunk::as_TreeChunk(FreeChunk* fc) {
  // Do some assertion checking here.
  return (TreeChunk*) fc;
}

#ifndef PRODUCT
void TreeChunk::verifyTreeChunkList() const {
  TreeChunk* nextTC = (TreeChunk*)next();
  if (prev() != NULL) { // interior list node shouldn'r have tree fields
    assert(embedded_list()->parent() == NULL && embedded_list()->left() == NULL &&
           embedded_list()->right()  == NULL, "should be clear");
  }
  if (nextTC != NULL) {
    assert(as_TreeChunk(nextTC->prev()) == this, "broken chain");
    assert(nextTC->size() == size(), "wrong size");
    nextTC->verifyTreeChunkList();
  }
}
#endif  // not PRODUCT


TreeList* TreeList::as_TreeList(TreeChunk* tc) {
  // This first free chunk in the list will be the tree list.
  assert(tc->size() >= sizeof(TreeChunk), "Chunk is too small for a TreeChunk");
  TreeList* tl = tc->embedded_list();
  tc->set_list(tl);
#ifdef ASSERT
  tl->set_protecting_lock(NULL);
#endif
  tl->set_hint(0);
  tl->set_size(tc->size());
  tl->link_head(tc);
  tl->link_tail(tc);
  tl->set_count(1);
  tl->init_statistics();
  tl->setParent(NULL);
  tl->setLeft(NULL);
  tl->setRight(NULL);
  return tl;
}
TreeList* TreeList::as_TreeList(HeapWord* addr, size_t size) {
  TreeChunk* tc = (TreeChunk*) addr;
  assert(size >= sizeof(TreeChunk), "Chunk is too small for a TreeChunk");
  assert(tc->size() == 0 && tc->prev() == NULL && tc->next() == NULL, 
    "Space should be clear");
  tc->setSize(size);
  tc->linkPrev(NULL);
  tc->linkNext(NULL);
  TreeList* tl = TreeList::as_TreeList(tc);
  return tl;
}

TreeList* TreeList::removeChunkReplaceIfNeeded(TreeChunk* tc) {

  TreeList* retTL = this;
  FreeChunk* list = head();
  assert(!list || list != list->next(), "Chunk on list twice");
  assert(tc != NULL, "Chunk being removed is NULL");
  assert(parent() == NULL || this == parent()->left() || 
    this == parent()->right(), "list is inconsistent");
  assert(tc->isFree(), "Header is not marked correctly");
  assert(head() == NULL || head()->prev() == NULL, "list invariant");
  assert(tail() == NULL || tail()->next() == NULL, "list invariant");

  FreeChunk* prevFC = tc->prev();
  TreeChunk* nextTC = TreeChunk::as_TreeChunk(tc->next());
  assert(list != NULL, "should have at least the target chunk");

  // Is this the first item on the list?
  if (tc == list) {
    // The "getChunk..." functions for a TreeList will not return the
    // first chunk in the list unless it is the last chunk in the list
    // because the first chunk is also acting as the tree node.
    // When coalescing happens, however, the first chunk in the a tree
    // list can be the start of a free range.  Free ranges are removed
    // from the free lists so that they are not available to be 
    // allocated when the sweeper yields (giving up the free list lock)
    // to allow mutator activity.  If this chunk is the first in the
    // list and is not the last in the list, do the work to copy the
    // TreeList from the first chunk to the next chunk and update all
    // the TreeList pointers in the chunks in the list.
    if (nextTC == NULL) {
      assert(prevFC == NULL, "Not last chunk in the list")
      set_tail(NULL);
      set_head(NULL);
    } else {
      debug_only(
        if (PrintGC && Verbose) {
  	gclog_or_tty->print_cr("Removing first but not only chunk in TreeList");
        gclog_or_tty->print_cr("Node: " INTPTR_FORMAT " parent: " INTPTR_FORMAT
            " right: " INTPTR_FORMAT " left: " INTPTR_FORMAT, 
            tc, tc->list()->parent(), tc->list()->right(), tc->list()->left());
        gclog_or_tty->print_cr("Next before: " INTPTR_FORMAT " parent: " 
            INTPTR_FORMAT " right: " INTPTR_FORMAT " left: " INTPTR_FORMAT,
            nextTC, nextTC->list()->parent(), nextTC->list()->right(), 
  	  nextTC->list()->left());
        gclog_or_tty->print_cr("	head: " INTPTR_FORMAT " tail: " 
            INTPTR_FORMAT, nextTC->list()->head(), nextTC->list()->tail());
        }
      )
      // copy embedded list.
      nextTC->set_embedded_list(tc->embedded_list());
      retTL = nextTC->embedded_list();
      // Fix the pointer to the list in each chunk in the list.
      // This can be slow for a long list.  Consider having
      // an option that does not allow the first chunk on the
      // list to be coalesced.
      for (TreeChunk* curTC = nextTC; curTC != NULL; 
	  curTC = TreeChunk::as_TreeChunk(curTC->next())) {
        curTC->set_list(retTL);
      }
      // Fix the parent to point to the new TreeList.
      if (retTL->parent() != NULL) {
	if (this == retTL->parent()->left()) {
	  retTL->parent()->setLeft(retTL);
	} else {
	  assert(this == retTL->parent()->right(), "Parent is incorrect");
	  retTL->parent()->setRight(retTL);
	}
      }
      // Fix the children's parent pointers to point to the
      // new list.
      assert(right() == retTL->right(), "Should have been copied");
      if (retTL->right() != NULL) {
	retTL->right()->setParent(retTL);
      }
      assert(left() == retTL->left(), "Should have been copied");
      if (retTL->left() != NULL) {
	retTL->left()->setParent(retTL);
      }
      retTL->link_head(nextTC);
      debug_only(
        if (PrintGC && Verbose) {
          gclog_or_tty->print_cr("Next after: " INTPTR_FORMAT " parent: " 
            INTPTR_FORMAT " right: " INTPTR_FORMAT " left: " INTPTR_FORMAT,
            nextTC, nextTC->list()->parent(), nextTC->list()->right(), 
  	    nextTC->list()->left());
          gclog_or_tty->print_cr("	head: " INTPTR_FORMAT " tail: " 
            INTPTR_FORMAT, nextTC->list()->head(), nextTC->list()->tail());
        }
      )
      assert(nextTC->isFree(), "Should be a free chunk");
    }
  } else {
    if (nextTC == NULL) {
      // Removing chunk at tail of list
      link_tail(prevFC);
    }
    // Chunk is interior to the list
    prevFC->linkAfter(nextTC);
  }

  // Below this point the embeded TreeList being used for the
  // tree node may have changed. Don't use "this" 
  // TreeList*.
  // chunk should still be a free chunk (bit set in _prev)
  assert(!retTL->head() || retTL->size() == retTL->head()->size(), 
    "Wrong sized chunk in list");
  debug_only(
    tc->linkPrev(NULL);  
    tc->linkNext(NULL);
    tc->set_list(NULL);
    bool prev_found = false;
    bool next_found = false;
    for (FreeChunk* curFC = retTL->head(); 
	 curFC != NULL; curFC = curFC->next()) {
      assert(curFC != tc, "Chunk is still in list");
      if (curFC == prevFC) {
	prev_found = true;
      }
      if (curFC == nextTC) {
	next_found = true;
      }
    }
    assert(prevFC == NULL || prev_found, "Chunk was lost from list");
    assert(nextTC == NULL || next_found, "Chunk was lost from list");
    assert(retTL->parent() == NULL ||
	   retTL == retTL->parent()->left() || 
	   retTL == retTL->parent()->right(),
           "list is inconsistent");
  )
  retTL->decrement_count();

  assert(tc->isFree(), "Should still be a free chunk");
  assert(retTL->head() == NULL || retTL->head()->prev() == NULL, 
    "list invariant");
  assert(retTL->tail() == NULL || retTL->tail()->next() == NULL, 
    "list invariant");
  return retTL;
}
void TreeList::returnChunkAtTail(TreeChunk* chunk) {
  assert(chunk != NULL, "returning NULL chunk");
  assert(chunk->list() == this, "list should be set for chunk");
  assert(tail() != NULL, "The tree list is embedded in the first chunk");
  // which means that the list can never be empty.
  assert(!verifyChunkInFreeLists(chunk), "Double entry");
  assert(head() == NULL || head()->prev() == NULL, "list invariant");
  assert(tail() == NULL || tail()->next() == NULL, "list invariant");
  
  FreeChunk* fc = tail();
  fc->linkAfter(chunk);
  link_tail(chunk);

  assert(!tail() || size() == tail()->size(), "Wrong sized chunk in list");
  increment_count();
  debug_only(increment_returnedBytes_by(chunk->size()*sizeof(HeapWord));)
  assert(head() == NULL || head()->prev() == NULL, "list invariant");
  assert(tail() == NULL || tail()->next() == NULL, "list invariant");
}

// Add this chunk at the head of the list.  "At the head of the list"
// is defined to be after the chunk pointer to by head().  This is 
// because the TreeList is embedded in the first TreeChunk in the
// list.  See the definition of TreeChunk.
void TreeList::returnChunkAtHead(TreeChunk* chunk) {
  assert(chunk->list() == this, "list should be set for chunk");
  assert(head() != NULL, "The tree list is embedded in the first chunk");
  assert(chunk != NULL, "returning NULL chunk");
  assert(!verifyChunkInFreeLists(chunk), "Double entry");
  assert(head() == NULL || head()->prev() == NULL, "list invariant");
  assert(tail() == NULL || tail()->next() == NULL, "list invariant");

  FreeChunk* fc = head()->next();
  if (fc != NULL) {
    chunk->linkAfter(fc);
  } else {
    assert(tail() == NULL, "List is inconsistent");
    link_tail(chunk);
  }
  head()->linkAfter(chunk);
  assert(!head() || size() == head()->size(), "Wrong sized chunk in list");
  increment_count();
  debug_only(increment_returnedBytes_by(chunk->size()*sizeof(HeapWord));)
  assert(head() == NULL || head()->prev() == NULL, "list invariant");
  assert(tail() == NULL || tail()->next() == NULL, "list invariant");
}

TreeChunk* TreeList::head_as_TreeChunk() {
  assert(head() == NULL || TreeChunk::as_TreeChunk(head())->list() == this,
    "Wrong type of chunk?");
  return TreeChunk::as_TreeChunk(head());
}

TreeChunk* TreeList::first_available() {
  guarantee(head() != NULL, "The head of the list cannot be NULL");
  FreeChunk* fc = head()->next();
  TreeChunk* retTC;
  if (fc == NULL) {
    retTC = head_as_TreeChunk();
  } else {
    retTC = TreeChunk::as_TreeChunk(fc);
  }
  assert(retTC->list() == this, "Wrong type of chunk.");
  return retTC;
}

BinaryTreeDictionary::BinaryTreeDictionary(MemRegion mr, bool splay):
  _splay(splay)
{
  assert(mr.byte_size() > MIN_TREE_CHUNK_SIZE, "minimum chunk size");

  reset(mr);
  assert(root()->left() == NULL, "reset check failed");
  assert(root()->right() == NULL, "reset check failed");
  assert(root()->head()->next() == NULL, "reset check failed");
  assert(root()->head()->prev() == NULL, "reset check failed");
  assert(totalSize() == root()->size(), "reset check failed");
  assert(totalFreeBlocks() == 1, "reset check failed");
}

void BinaryTreeDictionary::reset(MemRegion mr) {
  assert(mr.byte_size() > MIN_TREE_CHUNK_SIZE, "minimum chunk size");
  set_root(TreeList::as_TreeList(mr.start(), mr.word_size()));
  set_totalSize(mr.word_size());
  set_totalFreeBlocks(1);
}

void BinaryTreeDictionary::reset(HeapWord* addr, size_t byte_size) {
  MemRegion mr(addr, heap_word_size(byte_size));
  reset(mr);
}

void BinaryTreeDictionary::reset() {
  set_root(NULL);
  set_totalSize(0);
  set_totalFreeBlocks(0);
}

// Get a free block of size at least size from tree, or NULL.
// If a splay step is requested, the removal algorithm (only) incorporates
// a splay step as follows:
// . the search proceeds down the tree looking for a possible
//   match. At the (closest) matching location, an appropriate splay step is applied
//   (zig, zig-zig or zig-zag). A chunk of the appropriate size is then returned
//   if available, and if it's the last chunk, the node is deleted. A deteleted
//   node is replaced in place by its tree successor.
TreeChunk*
BinaryTreeDictionary::getChunkFromTree(size_t size, Dither dither, bool splay)
{
  TreeList *curTL, *prevTL;
  TreeChunk* retTC = NULL;
  assert(size >= MIN_TREE_CHUNK_SIZE, "minimum chunk size");
  if (FLSVerifyDictionary) {
    verifyTree();
  }
  // starting at the root, work downwards trying to find match.
  // Remember the last node of size too great or too small.
  for (prevTL = curTL = root(); curTL != NULL;) {
    if (curTL->size() == size) {        // exact match
      break;
    } 
    prevTL = curTL;
    if (curTL->size() < size) {        // proceed to right sub-tree
      curTL = curTL->right();
    } else {                           // proceed to left sub-tree
      assert(curTL->size() > size, "size inconsistency");
      curTL = curTL->left();
    }
  }
  if (curTL == NULL) { // couldn't find exact match
    // try and find the next larger size by walking back up the search path
    for (curTL = prevTL; curTL != NULL;) {
      if (curTL->size() >= size) break;
      else curTL = curTL->parent();
    }
    assert(curTL == NULL || curTL->count() > 0,
      "An empty list should not be in the tree");
  }
  if (curTL != NULL) {
    assert(curTL->size() >= size, "size inconsistency");
    if (UseCMSAdaptiveFreeLists) {
  
      // A candidate chunk has been found.  If it is already under
      // populated, get a chunk associated with the hint for this
      // chunk.
      if (curTL->surplus() <= 0) {
        /* Use the hint to find a size with a surplus, and reset the hint. */
        TreeList* hintTL = curTL;
        while (hintTL->hint() != 0) {
  	  assert(hintTL->hint() == 0 || hintTL->hint() > hintTL->size(),
	    "hint points in the wrong direction");
          hintTL = findList(hintTL->hint());
  	  assert(curTL != hintTL, "Infinite loop");
          if (hintTL == NULL || 
	      hintTL == curTL /* Should not happen but protect against it */ ) {
  	    // No useful hint.  Set the hint to NULL and go on.
            curTL->set_hint(0);
            break;
          }
          assert(hintTL->size() > size, "hint is inconsistent");
          if (hintTL->surplus() > 0) {
  	    // The hint led to a list that has a surplus.  Use it.
  	    // Set the hint for the candidate to an overpopulated
  	    // size.  
            curTL->set_hint(hintTL->size());
            // Change the candidate.
            curTL = hintTL;
            break;
          }
  	  // The evm code reset the hint of the candidate as
  	  // at an interrim point.  Why?  Seems like this leaves
  	  // the hint pointing to a list that didn't work.
          // curTL->set_hint(hintTL->size());
        }
      }
    }
    // don't waste time splaying if chunk's singleton
    if (splay && curTL->head()->next() != NULL) {
      semiSplayStep(curTL);
    }
    retTC = curTL->first_available();
    assert((retTC != NULL) && (curTL->count() > 0),
      "A list in the binary tree should not be NULL");
    assert(retTC->size() >= size, 
      "A chunk of the wrong size was found");
    removeChunkFromTree(retTC);
    assert(retTC->isFree(), "Header is not marked correctly");
  }

  if (FLSVerifyDictionary) {
    verify();
  }
  return retTC;
}

TreeList* BinaryTreeDictionary::findList(size_t size) const {
  TreeList* curTL;
  for (curTL = root(); curTL != NULL;) {
    if (curTL->size() == size) {        // exact match
      break;
    } 
    
    if (curTL->size() < size) {        // proceed to right sub-tree
      curTL = curTL->right();
    } else {                           // proceed to left sub-tree
      assert(curTL->size() > size, "size inconsistency");
      curTL = curTL->left();
    }
  }
  return curTL;
}


#ifndef PRODUCT
bool BinaryTreeDictionary::verifyChunkInFreeLists(FreeChunk* tc) const {
  size_t size = tc->size();
  TreeList* tl = findList(size);
  if (tl == NULL) {
    return false;
  } else {
    return tl->verifyChunkInFreeLists(tc);
  }
}
#endif

FreeChunk* BinaryTreeDictionary::findLargestDict() const {
  TreeList *curTL = root();
  if (curTL != NULL) {
    while(curTL->right() != NULL) curTL = curTL->right();
    return curTL->first_available();
  } else {
    return NULL;
  }
}

// Remove the current chunk from the tree.  If it is not the last 
// chunk in a list on a tree node, just unlink it.
// If it is the last chunk in the list (the next link is NULL),
// remove the node and repair the tree.
TreeChunk*
BinaryTreeDictionary::removeChunkFromTree(TreeChunk* tc) {
  assert(tc != NULL, "Should not call with a NULL chunk");
  assert(tc->isFree(), "Header is not marked correctly");

  TreeList *newTL, *parentTL;
  TreeChunk* retTC;
  TreeList* tl = tc->list();
  assert(tl != NULL, "List should be set");
  assert(tl->parent() == NULL || tl == tl->parent()->left() || 
	 tl == tl->parent()->right(), "list is inconsistent");

  bool complicatedSplice = false;

  retTC = tc;
  // Removing this chunk can have the side effect of changing the node
  // (TreeList*) in the tree.  If the node is the root, update it.
  TreeList* replacementTL = tl->removeChunkReplaceIfNeeded(tc);
  assert(tc->isFree(), "Chunk should still be free");
  assert(replacementTL->parent() == NULL ||
	 replacementTL == replacementTL->parent()->left() || 
	 replacementTL == replacementTL->parent()->right(),
         "list is inconsistent");
  if (tl == root()) {
    assert(replacementTL->parent() == NULL, "Incorrectly replacing root");
    set_root(replacementTL);
  }
  debug_only(
    if (tl != replacementTL) {
      assert(replacementTL->head() != NULL, 
        "If the tree list was replaced, it should not be a NULL list");
      TreeList* rhl = replacementTL->head_as_TreeChunk()->list();
      TreeList* rtl = TreeChunk::as_TreeChunk(replacementTL->tail())->list();
      assert(rhl == replacementTL, "Broken head");
      assert(rtl == replacementTL, "Broken tail");
      assert(replacementTL->size() == tc->size(),  "Broken size");
    }
  )

  // Does the tree need to be repaired?
  if (replacementTL->count() == 0) {
    assert(replacementTL->head() == NULL && 
	   replacementTL->tail() == NULL, "list count is incorrect");
    // Find the replacement node for the (soon to be empty) node being removed.
    // if we have a single (or no) child, splice child in our stead
    if (replacementTL->left() == NULL) {
      // left is NULL so pick right.  right may also be NULL.
      newTL = replacementTL->right();
      debug_only(replacementTL->clearRight();)
    } else if (replacementTL->right() == NULL) {
      // right is NULL
      newTL = replacementTL->left();
      debug_only(replacementTL->clearLeft();)
    } else {  // we have both children, so, by patriarchal convention,
              // my replacement is least node in right sub-tree
      complicatedSplice = true;
      newTL = removeTreeMinimum(replacementTL->right());
      assert(newTL != NULL && newTL->left() == NULL &&
             newTL->right() == NULL, "sub-tree minimum exists");
    }
    // newTL is the replacement for the (soon to be empty) node.
    // newTL may be NULL.
    // should verify; we just cleanly excised our replacement
    if (FLSVerifyDictionary) {
      verifyTree();
    }
    // first make newTL my parent's child
    if ((parentTL = replacementTL->parent()) == NULL) {  
      // newTL should be root
      assert(tl == root(), "Incorrectly replacing root");
      set_root(newTL);
      if (newTL != NULL) {
        newTL->clearParent();
      }
    } else if (parentTL->right() == replacementTL) {   
      // replacementTL is a right child
      parentTL->setRight(newTL);
    } else {                                // replacementTL is a left child
      assert(parentTL->left() == replacementTL, "should be left child");
      parentTL->setLeft(newTL);
    }
    debug_only(replacementTL->clearParent();)
    if (complicatedSplice) {  // we need newTL to get replacementTL's 
			      // two children
      assert(newTL != NULL &&
             newTL->left() == NULL && newTL->right() == NULL,
            "newTL should not have encumbrances from the past");
      // we'd like to assert as below:
      // assert(replacementTL->left() != NULL && replacementTL->right() != NULL,
      //       "else !complicatedSplice");
      // ... however, the above assertion is too strong because we aren't
      // guaranteed that replacementTL->right() is still NULL. 
      // Recall that we removed
      // the right sub-tree minimum from replacementTL. 
      // That may well have been its right
      // child! So we'll just assert half of the above:
      assert(replacementTL->left() != NULL, "else !complicatedSplice");
      newTL->setLeft(replacementTL->left());
      newTL->setRight(replacementTL->right());
      debug_only(
        replacementTL->clearRight();
        replacementTL->clearLeft();
      )
    }
    assert(replacementTL->right() == NULL && 
	   replacementTL->left() == NULL && 
	   replacementTL->parent() == NULL,
        "delete without encumbrances");
  }

  assert(totalSize() >= retTC->size(), "Incorrect total size");
  set_totalSize(totalSize() - retTC->size());     // size book-keeping
  assert(totalFreeBlocks() > 0, "Incorrect total count");
  set_totalFreeBlocks(totalFreeBlocks() - 1);

  assert(retTC != NULL, "null chunk?");
  assert(retTC->prev() == NULL && retTC->next() == NULL,
         "should return without encumbrances");
  if (FLSVerifyDictionary) {
    verifyTree();
  }
  return TreeChunk::as_TreeChunk(retTC);
}

// Remove the leftmost node (lm) in the tree and return it.
// If lm has a right child, link it to the left node of
// the parent of lm.
TreeList* BinaryTreeDictionary::removeTreeMinimum(TreeList* tl) {
  assert(tl != NULL && tl->parent() != NULL, "really need a proper sub-tree");
  // locate the subtree minimum by walking down left branches
  TreeList* curTL = tl;
  for (; curTL->left() != NULL; curTL = curTL->left());
  // obviously curTL now has at most one child, a right child
  if (curTL != root()) {  // Should this test just be removed?
    TreeList* parentTL = curTL->parent();
    if (parentTL->left() == curTL) { // curTL is a left child
      parentTL->setLeft(curTL->right());
    } else {
      // If the list tl has no left child, then curTL may be
      // the right child of parentTL.
      assert(parentTL->right() == curTL, "should be a right child");
      parentTL->setRight(curTL->right());
    }
  } else {
    // The only use of this method would not pass the root of the
    // tree (as indicated by the assertion above that the tree list
    // has a parent) but the specification does not explicitly exclude the
    // passing of the root so accomodate it.
    set_root(NULL);
  }
  debug_only(
    curTL->clearParent();  // Test if this needs to be cleared
    curTL->clearRight();    // recall, above, left child is already null
  )
  // we just excised a (non-root) node, we should still verify all tree invariants
  if (FLSVerifyDictionary) {
    verifyTree();
  }
  return curTL;
}

// Based on a simplification of the algorithm by Sleator and Tarjan (JACM 1985).
// The simplifications are the following:
// . we splay only when we delete (not when we insert)
// . we apply a single spay step per deletion/access
// By doing such partial splaying, we reduce the amount of restructuring,
// while getting a reasonably efficient search tree (we think).
// [Measurements will be needed to (in)validate this expectation.]

void BinaryTreeDictionary::semiSplayStep(TreeList* tc) {
  // apply a semi-splay step at the given node:
  // . if root, norting needs to be done
  // . if child of root, splay once
  // . else zig-zig or sig-zag depending on path from grandparent
  if (root() == tc) return;
  warning("*** Splaying not yet implemented; "
          "tree operations may be inefficient ***");
}

void BinaryTreeDictionary::insertChunkInTree(FreeChunk* fc) {
  TreeList *curTL, *prevTL;
  size_t size = fc->size();

  assert(size >= MIN_TREE_CHUNK_SIZE, "too small to be a TreeList");
  if (FLSVerifyDictionary) {
    verifyTree();
  }
  // XXX: do i need to clear the FreeChunk fields, let me do it just in case
  // Revisit this later
  
  fc->clearNext();
  fc->linkPrev(NULL);
  
  // work down from the _root, looking for insertion point
  for (prevTL = curTL = root(); curTL != NULL;) {
    if (curTL->size() == size)  // exact match
      break;
    prevTL = curTL;
    if (curTL->size() > size) { // follow left branch
      curTL = curTL->left();
    } else {                    // follow right branch
      assert(curTL->size() < size, "size inconsistency");
      curTL = curTL->right();
    }
  }
  TreeChunk* tc = TreeChunk::as_TreeChunk(fc);
  // This chunk is being returned to the binary try.  It's embedded
  // TreeList should be unused at this point.
  tc->initialize();
  if (curTL != NULL) {          // exact match
    tc->set_list(curTL);
    curTL->returnChunkAtTail(tc);
  } else {                     // need a new node in tree
    tc->clearNext();
    tc->linkPrev(NULL);
    TreeList* newTL = TreeList::as_TreeList(tc);
    assert(((TreeChunk*)tc)->list() == newTL,
      "List was not initialized correctly");
    if (prevTL == NULL) {      // we are the only tree node
      assert(root() == NULL, "control point invariant");
      set_root(newTL);
    } else {                   // insert under prevTL ...
      if (prevTL->size() < size) {   // am right child
        assert(prevTL->right() == NULL, "control point invariant");
        prevTL->setRight(newTL);
      } else {                       // am left child
        assert(prevTL->size() > size && prevTL->left() == NULL, "cpt pt inv");
        prevTL->setLeft(newTL);
      }
    }
  }
  assert(tc->list() != NULL, "Tree list should be set");

  set_totalSize(totalSize() + size);
  assert(totalSizeInTree(root()) == totalSize(), "_totalSize inconsistency");
  set_totalFreeBlocks(totalFreeBlocks() + 1);
  if (FLSVerifyDictionary) {
    verifyTree();
  }
}

size_t BinaryTreeDictionary::maxChunkSize() const {
  verify_par_locked();
  TreeList* tc = root();
  if (tc == NULL) return 0;
  for (; tc->right() != NULL; tc = tc->right());
  return tc->size();
}

size_t BinaryTreeDictionary::totalListLength(TreeList* tl) const {
  size_t res;
  res = tl->count();
#ifdef ASSERT
  size_t cnt;
  FreeChunk* tc = tl->head();
  for (cnt = 0; tc != NULL; tc = tc->next(), cnt++);
  assert(res == cnt, "The count is not being maintained correctly");
#endif
  return res;
}

size_t BinaryTreeDictionary::totalSizeInTree(TreeList* tl) const {
  if (tl == NULL)
    return 0;
  return (tl->size() * totalListLength(tl)) +
         totalSizeInTree(tl->left())    +
         totalSizeInTree(tl->right());
}

double BinaryTreeDictionary::sum_of_squared_block_sizes(TreeList* const tl) const {
  if (tl == NULL) {
    return 0.0;
  }
  double size = (double)(tl->size());
  double curr = size * size * totalListLength(tl);
  curr += sum_of_squared_block_sizes(tl->left());
  curr += sum_of_squared_block_sizes(tl->right());
  return curr;
}

size_t BinaryTreeDictionary::totalFreeBlocksInTree(TreeList* tl) const {
  if (tl == NULL)
    return 0;
  return totalListLength(tl) +
         totalFreeBlocksInTree(tl->left()) +
         totalFreeBlocksInTree(tl->right());
}

size_t BinaryTreeDictionary::numFreeBlocks() const {
  assert(totalFreeBlocksInTree(root()) == totalFreeBlocks(), 
         "_totalFreeBlocks inconsistency");
  return totalFreeBlocks();
}

size_t BinaryTreeDictionary::treeHeightHelper(TreeList* tl) const {
  if (tl == NULL)
    return 0;
  return 1 + MAX2(treeHeightHelper(tl->left()),
                  treeHeightHelper(tl->right()));
}

size_t BinaryTreeDictionary::treeHeight() const {
  return treeHeightHelper(root());
}

size_t BinaryTreeDictionary::totalNodesHelper(TreeList* tl) const {
  if (tl == NULL) {
    return 0;
  }
  return 1 + totalNodesHelper(tl->left()) +
    totalNodesHelper(tl->right());
}

size_t BinaryTreeDictionary::totalNodesInTree(TreeList* tl) const {
  return totalNodesHelper(root());
}

void BinaryTreeDictionary::dictCensusUpdate(size_t size, bool split, bool birth){
  TreeList* nd = findList(size);
  if (nd) {
    if (split) {
      if (birth) {
        nd->increment_splitBirths();
        nd->increment_surplus();
      }  else {
        nd->increment_splitDeaths();
        nd->decrement_surplus();
      }
    } else {
      if (birth) {
        nd->increment_coalBirths();
        nd->increment_surplus();
      } else {
        nd->increment_coalDeaths();
        nd->decrement_surplus();
      }
    }
  }
  // A list for this size may not be found (nd == 0) if
  //   This is a death where the appropriate list is now
  //     empty and has been removed from the list.
  //   This is a birth associated with a LinAB.  The chunk
  //     for the LinAB is not in the dictionary.
}

bool BinaryTreeDictionary::coalDictOverPopulated(size_t size) {
  TreeList* list_of_size = findList(size);
  // None of requested size implies overpopulated.
  return list_of_size == NULL || list_of_size->coalDesired() <= 0 ||
    (int)list_of_size->count() > list_of_size->coalDesired();
}

// Closures for walking the binary tree.
//   do_list() walks the free list in a node applying the closure
//     to each free chunk in the list
//   do_tree() walks the nodes in the binary tree applying do_list()
//     to each list at each node.

class TreeCensusClosure : public StackObj {
 protected:
  virtual void do_list(FreeList* fl) = 0;
 public:
  virtual void do_tree(TreeList* tl) = 0;
};

class AscendTreeCensusClosure : public TreeCensusClosure {
 public:
  void do_tree(TreeList* tl) {
    if (tl != NULL) {
      do_tree(tl->left());
      do_list(tl);
      do_tree(tl->right());
    }
  }
};

class DescendTreeCensusClosure : public TreeCensusClosure {
 public:
  void do_tree(TreeList* tl) {
    if (tl != NULL) {
      do_tree(tl->right());
      do_list(tl);
      do_tree(tl->left());
    }
  }
};
       
// For each list in the tree, calculate the desired, desired
// coalesce, count before sweep, and surplus before sweep.
class BeginSweepClosure : public AscendTreeCensusClosure {
  double percentage;
 public:
  BeginSweepClosure(double v) { percentage = v; }
  void do_list(FreeList* fl) {
    double coalSurplusPercent = percentage;
    fl->set_desired(fl->prevSweep()   - fl->count() +
                    fl->splitBirths() - fl->splitDeaths());
    fl->set_coalDesired((int)((double)fl->desired() * coalSurplusPercent));
    fl->set_beforeSweep(fl->count());
    fl->set_bfrSurp(fl->surplus());
  }
};

void BinaryTreeDictionary::beginSweepDictCensus(double coalSurplusPercent) {
  BeginSweepClosure bsc(coalSurplusPercent);
  bsc.do_tree(root());
}

// Closures and methods for calculating total bytes returned to the
// free lists in the tree.
NOT_PRODUCT(
  class InitializeDictReturnedBytesClosure : public AscendTreeCensusClosure {
   public:
    void do_list(FreeList* fl) {
      fl->set_returnedBytes(0);
    }
  };
  
  void BinaryTreeDictionary::initializeDictReturnedBytes() {
    InitializeDictReturnedBytesClosure idrb;
    idrb.do_tree(root());
  }
  
  class ReturnedBytesClosure : public AscendTreeCensusClosure {
    size_t _dictReturnedBytes;
   public:
    ReturnedBytesClosure() { _dictReturnedBytes = 0; }
    void do_list(FreeList* fl) {
      _dictReturnedBytes += fl->returnedBytes();
    }
    size_t dictReturnedBytes() { return _dictReturnedBytes; }
  };
  
  size_t BinaryTreeDictionary::sumDictReturnedBytes() {
    ReturnedBytesClosure rbc;
    rbc.do_tree(root());
  
    return rbc.dictReturnedBytes();
  }

  // Count the number of entries in the tree.
  class treeCountClosure : public DescendTreeCensusClosure {
   public:
    uint count;
    treeCountClosure(uint c) { count = c; }
    void do_list(FreeList* fl) {
      count++;
    }
  };

  size_t BinaryTreeDictionary::totalCount() {
    treeCountClosure ctc(0);
    ctc.do_tree(root());
    return ctc.count;
  }
)

// Calculate surpluses for the lists in the tree.
class setTreeSurplusClosure : public AscendTreeCensusClosure {
  double percentage;
 public:
  setTreeSurplusClosure(double v) { percentage = v; }
  void do_list(FreeList* fl) {
    double splitSurplusPercent = percentage;
    fl->set_surplus(fl->count() -
                   (int)((double)fl->desired() * splitSurplusPercent));
  }
};

void BinaryTreeDictionary::setTreeSurplus(double splitSurplusPercent) {
  setTreeSurplusClosure sts(splitSurplusPercent);
  sts.do_tree(root());
}

// Set hints for the lists in the tree.
class setTreeHintsClosure : public DescendTreeCensusClosure {
  size_t hint;
 public:
  setTreeHintsClosure(size_t v) { hint = v; }
  void do_list(FreeList* fl) {
    fl->set_hint(hint);
    assert(fl->hint() == 0 || fl->hint() > fl->size(), 
      "Current hint is inconsistent");
    if (fl->surplus() > 0) {
      hint = fl->size();
    }
  }
};

void BinaryTreeDictionary::setTreeHints(void) {
  setTreeHintsClosure sth(0);
  sth.do_tree(root());
}

// Save count before previous sweep and splits and coalesces.
class clearTreeCensusClosure : public AscendTreeCensusClosure {
  void do_list(FreeList* fl) {
    fl->set_prevSweep(fl->count());
    fl->set_coalBirths(0);
    fl->set_coalDeaths(0);
    fl->set_splitBirths(0);
    fl->set_splitDeaths(0);
  }
};

void BinaryTreeDictionary::clearTreeCensus(void) {
  clearTreeCensusClosure ctc;
  ctc.do_tree(root());
}

// Do reporting and post sweep clean up.
void BinaryTreeDictionary::endSweepDictCensus(double splitSurplusPercent) {
  // Does walking the tree 3 times hurt?
  setTreeSurplus(splitSurplusPercent);
  setTreeHints();
  if (PrintGC && Verbose) {
    reportStatistics();
  }
  clearTreeCensus();
}
    
// Print summary statistics
void BinaryTreeDictionary::reportStatistics() const {
  verify_par_locked();
  gclog_or_tty->print("Statistics for BinaryTreeDictionary:\n"
         "------------------------------------\n");
  size_t totalSize = totalChunkSize(debug_only(NULL));
  size_t    freeBlocks = numFreeBlocks();
  gclog_or_tty->print("Total Free Space: %d\n", totalSize);
  gclog_or_tty->print("Max   Chunk Size: %d\n", maxChunkSize());
  gclog_or_tty->print("Number of Blocks: %d\n", freeBlocks);
  if (freeBlocks > 0) {
    gclog_or_tty->print("Av.  Block  Size: %d\n", totalSize/freeBlocks);
  }
  gclog_or_tty->print("Tree      Height: %d\n", treeHeight());
}

// Print census information - counts, births, deaths, etc.
// for each list in the tree.  Also print some summary
// information.
class printTreeCensusClosure : public AscendTreeCensusClosure {
  size_t _totalFree;
  AllocationStats _totals;
  size_t _count;
 
 public:
  printTreeCensusClosure() {
    _totalFree = 0;
    _count = 0;
    _totals.initialize();
  }
  AllocationStats* totals() { return &_totals; }
  size_t count() { return _count; }
  void increment_count_by(size_t v) { _count += v; }
  size_t totalFree() { return _totalFree; }
  void increment_totalFree_by(size_t v) { _totalFree += v; }
  void do_list(FreeList* fl) {
    bool nl = false; // "maybe this is not needed" isNearLargestChunk(fl->head());

    gclog_or_tty->print("%c %4d\t\t" "%7d\t" "%7d\t"
               "%7d\t"      "%7d\t" "%7d\t" "%7d\t"
               "%7d\t"      "%7d\t" "%7d\t"
               "%7d\t" "\n",
               " n"[nl], fl->size(), fl->bfrSurp(), fl->surplus(),
               fl->desired(), fl->prevSweep(), fl->beforeSweep(), fl->count(),
               fl->coalBirths(), fl->coalDeaths(), fl->splitBirths(),
               fl->splitDeaths());
  
    increment_totalFree_by(fl->count() * fl->size());
    increment_count_by(fl->count());
    totals()->set_bfrSurp(totals()->bfrSurp() + fl->bfrSurp());
    totals()->set_surplus(totals()->splitDeaths()     + fl->surplus());
    totals()->set_desired(totals()->desired()     + fl->desired());
    totals()->set_prevSweep(totals()->prevSweep()   + fl->prevSweep());
    totals()->set_beforeSweep(totals()->beforeSweep() + fl->beforeSweep());
    totals()->set_coalBirths(totals()->coalBirths()  + fl->coalBirths());
    totals()->set_coalDeaths(totals()->coalDeaths()  + fl->coalDeaths());
    totals()->set_splitBirths(totals()->splitBirths() + fl->splitBirths());
    totals()->set_splitDeaths(totals()->splitDeaths() + fl->splitDeaths());
  }
};

void BinaryTreeDictionary::printDictCensus(void) const {
  
  gclog_or_tty->print("\nBinaryTree\n");
  gclog_or_tty->print("%4s\t\t" "%7s\t"   "%7s\t"    "%7s\t"    "%7s\t"    "%7s\t"
             "%7s\t"   "%7s\t"   "%7s\t"    "%7s\t"    "%7s\t"     "\n",
             "size",  "bfrsurp", "surplus", "desired", "prvSwep", "bfrSwep",
             "count", "cBirths", "cDeaths", "sBirths", "sDeaths");

  printTreeCensusClosure ptc;
  ptc.do_tree(root());

  gclog_or_tty->print("%4s\t\t" "%7d\t" "%7d\t" "%7d\t" "%7d\t" "%7d\t" "%7d\t"
             "%7d\t"   "%7d\t" "%7d\t" "%7d\t"     "\n",
             "totl",
             ptc.totals()->bfrSurp(), 
	     ptc.totals()->surplus(), 
	     ptc.totals()->desired(), 
	     ptc.totals()->prevSweep(), 
	     ptc.totals()->beforeSweep(), 
	     ptc.count(), 
	     ptc.totals()->coalBirths(), 
	     ptc.totals()->coalDeaths(), 
	     ptc.totals()->splitBirths(), 
	     ptc.totals()->splitDeaths());
  gclog_or_tty->print("totalFree(words): %7d growth: %8.5f  deficit: %8.5f\n",
              ptc.totalFree(),
              (double)(ptc.totals()->splitBirths()+ptc.totals()->coalBirths()
                       -ptc.totals()->splitDeaths()-ptc.totals()->coalDeaths())
              /(ptc.totals()->prevSweep() != 0 ?
                (double)ptc.totals()->prevSweep() : 1.0),
             (double)(ptc.totals()->desired() - ptc.count())
             /(ptc.totals()->desired() != 0 ?
               (double)ptc.totals()->desired() : 1.0));
}

// Verify the following tree invariants:
// . _root has no parent
// . parent and child point to each other
// . each node's key correctly related to that of its child(ren)
void BinaryTreeDictionary::verifyTree() const {
  guarantee(root() == NULL || totalFreeBlocks() == 0 ||
    totalSize() != 0, "_totalSize should't be 0?");
  guarantee(root() == NULL || root()->parent() == NULL, "_root shouldn't have parent");
  verifyTreeHelper(root());
}

size_t BinaryTreeDictionary::verifyPrevFreePtrs(TreeList* tl) {
  size_t ct = 0;
  for (FreeChunk* curFC = tl->head(); curFC != NULL; curFC = curFC->next()) {
    ct++;
    assert(curFC->prev() == NULL || curFC->prev()->isFree(), 
      "Chunk should be free");
  }
  return ct;
}

// Note: this helper is recursive rather than iterative, so use with
// caution on very deep trees; and watch out for stack overflow errors;
// In general, to be used only for debugging.
void BinaryTreeDictionary::verifyTreeHelper(TreeList* tl) const {
  if (tl == NULL)
    return;
  guarantee(tl->size() != 0, "A list must has a size");
  guarantee(tl->left()  == NULL || tl->left()->parent()  == tl,
         "parent<-/->left");
  guarantee(tl->right() == NULL || tl->right()->parent() == tl,
         "parent<-/->right");;
  guarantee(tl->left() == NULL  || tl->left()->size()    <  tl->size(),
         "parent !> left");
  guarantee(tl->right() == NULL || tl->right()->size()   >  tl->size(), 
         "parent !< left");
  guarantee(tl->head() == NULL || tl->head()->isFree(), "!Free");
  guarantee(tl->head() == NULL || tl->head_as_TreeChunk()->list() == tl, 
    "list inconsistency");
  guarantee(tl->count() > 0 || (tl->head() == NULL && tl->tail() == NULL),
    "list count is inconsistent");
  guarantee(tl->count() > 1 || tl->head() == tl->tail(),
    "list is incorrectly constructed");
  size_t count = verifyPrevFreePtrs(tl);
  guarantee(count == (size_t)tl->count(), "Node count is incorrect");
  if (tl->head() != NULL) {
    tl->head_as_TreeChunk()->verifyTreeChunkList();
  }
  verifyTreeHelper(tl->left());
  verifyTreeHelper(tl->right());
}

#ifndef PRODUCT
void BinaryTreeDictionary::verify() const {
  verifyTree();
  assert(totalSize() == totalSizeInTree(root()), "Total Size inconsistency");
}
#endif // PRODUCT
