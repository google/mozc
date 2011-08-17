// Copyright 2010-2011, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef MOZC_STORAGE_LRU_CACHE_H_
#define MOZC_STORAGE_LRU_CACHE_H_

#include <map>
#include <list>
#include <string>

#include "base/base.h"

namespace mozc {
template<typename Key, typename Value>
class LRUCache {
 public:
  // Constructs a new LRUCache that can hold at most max_elements
  explicit LRUCache(size_t max_elements);

  // Cleans up all allocated resources.
  virtual ~LRUCache();

  // Every Element is either on the free list or the lru list.  The
  // free list is singly-linked and only uses the next pointer, while
  // the LRU list is doubly-linked and uses both next and prev.
  struct Element {
    Element* next;
    Element* prev;
    Key key;
    Value value;
  };

  // Adds the specified key/value pair into the cache, putting it at the head
  // of the LRU list.
  void Insert(const Key &key, const Value& value);

  // Adds the specified key and return the Element added to the cache.
  // Caller needs to set the value
  Element* Insert(const Key &key);

  // Returns the cached value associated with the key, or NULL if the cache
  // does not contain an entry for that key.  The caller does not assume
  // ownership of the returned value.  The reference returned by Lookup() could
  // be invalidated by a call to Insert(), so the caller must take care to not
  // access the value if Insert() could have been called after Lookup().
  const Value* Lookup(const Key &key);

  // return non-const Value
  Value *MutableLookup(const Key &key);

  // Lookup/MutableLookup don't change the LRU order.
  const Value *LookupWithoutInsert(const Key &key) const;
  Value *MutableLookupWithoutInsert(const Key &key) const;

  // Removes the cache entry specified by key.  Returns true if the entry was
  // in the cache, otherwise returns false.
  bool Erase(const Key &key);

  // Removes all entries from the cache.  Note that this does not release the
  // memory associates with the blocks, but just pushes all the elements onto
  // the free list.
  void Clear();

  // Returns the number of entries currently in the cache.
  size_t Size() const;

  bool HasKey(const Key &key) const;

  // Returns the head of LRU list
  const Element *Head() const { return lru_head_; }

  // Returns the tail of LRU list
  const Element *Tail() const { return lru_tail_; }

 private:

  // Allocates a new block containing next_block_size_ elements, updates
  // next_block_size_ appropriately, and pushes the elements in the new block
  // onto the free list.
  void AddBlock();

  // Pushes an element onto the head of the free list.
  void PushFreeList(Element* element);

  // Pops an element from the head of the free list.
  Element* PopFreeList();

  // Returns a free element, popping from the free list if possible, or
  // allocating a new element if the free list is empty.  If there are already
  // max_elements_ in use this will return NULL.
  Element* NextFreeElement();

  // Returns the Element* associated with key, or NULL if no element with this
  // key is found.
  Element* LookupInternal(const Key &key) const;

  // Removes the specified element from the LRU list.
  void RemoveFromLRU(Element* element);

  // Adds the specified element to the head of the LRU list.
  void PushLRUHead(Element* element);

  // Evict is similar to Erase, except that it adds the eviction callback and
  // element to a list of eviction calls, and takes an element so that another
  // lookup is not necessary.
  bool Evict(Element* element);

  typedef map<Key, Element*> Table;

  Table* table_;
  Element* free_list_;     // singly linked list of Element
  Element* lru_head_;      // head of doubly linked list of Element
  Element* lru_tail_;      // tail of doubly linked list of Element
  Element* blocks_[10];    // blocks of Element, with each block being twice
  //   as big as the previous block.  This allows the
  //   cache to use a small amount of memory when it
  //   contains few items, but still have low malloc
  //   overhead per insert.  The number of blocks is
  //   arbitrary, but 10 blocks allows the largest
  //   block to be 2^10 times as large as the smallest
  //   block, which seems like more than enough.
  size_t block_count_;      // how many entries in blocks_ have been used
  size_t block_capacity_;   // how many Elements can be stored in current blocks
  size_t next_block_size_;  // size of the next block to allocate
  size_t max_elements_;     // maximum elements to hold

  DISALLOW_COPY_AND_ASSIGN(LRUCache);
};


template<typename Key, typename Value>
void LRUCache<Key, Value>::AddBlock() {
  const size_t max_blocks = sizeof(blocks_) / sizeof(blocks_[0]);
  if (block_count_ < max_blocks && block_capacity_ < max_elements_) {
    blocks_[block_count_] = new Element[next_block_size_];
    block_capacity_ += next_block_size_;
    // Add new elements to free list
    for (size_t i = 0; i < next_block_size_; ++i) {
      Element* e = &((blocks_[block_count_])[i]);
      e->prev = NULL;  // free list is not doubly linked
      e->next = free_list_;
      free_list_ = e;
    }
    block_count_++;
    size_t blocks_remaining = max_blocks - block_count_;
    if (blocks_remaining > 0) {
      next_block_size_ = (next_block_size_ << 1);
      size_t elements_remaining = max_elements_ - block_capacity_;
      if (next_block_size_ > (elements_remaining / blocks_remaining)) {
        next_block_size_ = elements_remaining / blocks_remaining;
      }
      if (block_capacity_ + next_block_size_ > max_elements_) {
        next_block_size_ = max_elements_ - block_capacity_;
      }
    }
  }
}

template<typename Key, typename Value>
void LRUCache<Key, Value>::PushFreeList(Element* element) {
  element->prev = NULL;
  element->next = free_list_;
  free_list_ = element;
}

template<typename Key, typename Value>
typename LRUCache<Key, Value>::Element*
LRUCache<Key, Value>::PopFreeList() {
  Element* r = free_list_;
  if (r != NULL) {
    CHECK(r->prev == NULL);
    free_list_ = r->next;
    if (free_list_ != NULL) {
      free_list_->prev = NULL;
    }
    r->next = NULL;
  }
  return r;
}

template<typename Key, typename Value>
typename LRUCache<Key, Value>::Element*
LRUCache<Key, Value>::NextFreeElement() {
  Element* r = PopFreeList();
  if (r == NULL) {
    AddBlock();
    r = PopFreeList();
  }
  return r;
}

template<typename Key, typename Value>
typename LRUCache<Key, Value>::Element*
LRUCache<Key, Value>::LookupInternal(const Key &key) const {
  typename Table::iterator iter = table_->find(key);
  if (iter != table_->end()) {
    return iter->second;
  }
  return NULL;
}

template<typename Key, typename Value>
void LRUCache<Key, Value>::RemoveFromLRU(Element* element) {
  if (lru_head_ == element) {
    lru_head_ = element->next;
  }
  if (lru_tail_ == element) {
    lru_tail_ = element->prev;
  }
  if (element->prev != NULL) {
    element->prev->next = element->next;
  }
  if (element->next != NULL) {
    element->next->prev = element->prev;
  }
  element->prev = NULL;
  element->next = NULL;
}

template<typename Key, typename Value>
void LRUCache<Key, Value>::PushLRUHead(Element* element) {
  if (lru_head_ == element) {
    // element is already at head, so do nothing.
    return;
  }
  RemoveFromLRU(element);
  element->next = lru_head_;
  lru_head_ = element;
  if (element->next != NULL) {
    element->next->prev = element;
  }
  if (lru_tail_ == NULL) {
    lru_tail_ = element;
  }
}

template<typename Key, typename Value>
bool LRUCache<Key, Value>::Evict(Element* e) {
  if (e != NULL) {
    int erased = table_->erase(e->key);
    CHECK_EQ(erased, 1);
    RemoveFromLRU(e);
    PushFreeList(e);
    return true;
  }
  return false;
}

template<typename Key, typename Value>
LRUCache<Key, Value>::LRUCache(size_t max_elements)
  : free_list_(NULL),
    lru_head_(NULL),
    lru_tail_(NULL),
    block_count_(0),
    block_capacity_(0),
    max_elements_(max_elements) {
  memset(blocks_, 0, sizeof(blocks_));
  table_ = new Table;
  CHECK(table_);
  if (max_elements_ <= 128) {
    next_block_size_ = max_elements_;
  } else {
    next_block_size_ = 64;
    size_t num_blocks = sizeof(blocks_) / sizeof(blocks_[0]);
    // The default starting block size is 64, which is 2^6.  Every block is
    // twice as big as the previous (see AddBlock()), so the size of the last
    // block would be 2^(6 + num_blocks) if the first block was of size 64.  If
    // max_elements is large enough that 64 is too low of a starting size,
    // figure that out here.
    while ((next_block_size_ << num_blocks) < max_elements) {
      next_block_size_ = (next_block_size_ << 1);
    }
  }
}

template<typename Key, typename Value>
LRUCache<Key, Value>::~LRUCache() {
  // To free all the memory that I have allocated I need to delete table_ and
  // any used entries in blocks_.
  delete table_;
  for (size_t i = 0; i < block_count_; ++i) {
    delete [] blocks_[i];
  }
}

template<typename Key, typename Value>
void LRUCache<Key, Value>::Insert(const Key& key,
                                  const Value& value) {
  Element *e = Insert(key);
  if (e != NULL) {
    e->value = value;
  }
}

template<typename Key, typename Value>
typename LRUCache<Key, Value>::Element *
LRUCache<Key, Value>::Insert(const Key& key) {
  bool erased = false;
  Element* e = LookupInternal(key);
  if (e != NULL) {
    erased = Evict(e);
    CHECK(erased);
  }

  e = NextFreeElement();
  if (e == NULL) {
    // no free elements, I have to replace an existing element
    e = lru_tail_;
    erased = Evict(e);
    CHECK(erased);
    e = NextFreeElement();
    CHECK(e != NULL);
  }
  e->key = key;
  (*table_)[key] = e;
  PushLRUHead(e);

  return e;
}

template<typename Key, typename Value>
Value* LRUCache<Key, Value>::MutableLookup(const Key& key) {
  Element* e = LookupInternal(key);
  if (e != NULL) {
    PushLRUHead(e);
    return &(e->value);
  }
  return NULL;
}

template<typename Key, typename Value>
const Value* LRUCache<Key, Value>::Lookup(const Key& key) {
  return MutableLookup(key);
}

template<typename Key, typename Value>
Value* LRUCache<Key, Value>::MutableLookupWithoutInsert(const Key& key) const {
  Element *e = LookupInternal(key);
  if (e != NULL) {
    return &(e->value);
  }
  return NULL;
}

template<typename Key, typename Value>
const Value* LRUCache<Key, Value>::LookupWithoutInsert(const Key& key) const {
  return MutableLookupWithoutInsert(key);
}

template<typename Key, typename Value>
bool LRUCache<Key, Value>::Erase(const Key& key) {
  Element* e = LookupInternal(key);
  return Evict(e);
}

template<typename Key, typename Value>
void LRUCache<Key, Value>::Clear() {
  table_->clear();
  Element* e = lru_head_;
  while (e != NULL) {
    Element* next = e->next;
    PushFreeList(e);
    e = next;
  }
  lru_head_ = lru_tail_ = NULL;
}

template<typename Key, typename Value>
bool LRUCache<Key, Value>::HasKey(const Key& key ) const {
  return (table_->find(key) != table_->end());
}

template<typename Key, typename Value>
size_t LRUCache<Key, Value>::Size() const {
  return table_->size();
}
}   // mozc
#endif  // MOZC_STORAGE_LRU_CACHE_H_
