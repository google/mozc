// Copyright 2010-2021, Google Inc.
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

#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"

namespace mozc {
namespace storage {

// Note: this class keeps some resources inside of the Key/Value, even if
// such a entry is erased. Be careful to use for such classes.
// TODO(yukawa): Make this class final once we stop supporting GCC 4.6.
template <typename Key, typename Value>
class LruCache {
 public:
  // Every Element is either on the free list or the lru list.  The
  // free list is singly-linked and only uses the next pointer, while
  // the LRU list is doubly-linked and uses both next and prev.
  struct Element {
    Element* next;
    Element* prev;
    Key key;
    Value value;
  };

  template <bool is_const>
  class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = std::conditional_t<is_const, const Element, Element>;
    using difference_type = ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    explicit Iterator(pointer element)
        : current_(element), next_(element ? element->next : nullptr) {}

    reference operator*() const { return *current_; }
    pointer operator->() const { return current_; }

    Iterator& operator++() {
      current_ = next_;
      // Capture `next` for when it's changed before the next increment.
      next_ = current_ ? current_->next : nullptr;
      return *this;
    }

    bool operator==(const Iterator& other) const {
      return current_ == other.current_;
    }
    bool operator!=(const Iterator& other) const {
      return current_ != other.current_;
    }

   private:
    pointer current_;
    pointer next_;
  };
  using iterator = Iterator</*is_const=*/false>;
  using const_iterator = Iterator</*is_const=*/true>;

  // Constructs a new LruCache that can hold at most max_elements
  explicit LruCache(size_t max_elements);
  ~LruCache() = default;

  LruCache(const LruCache&) = delete;
  LruCache& operator=(const LruCache&) = delete;

  // Iterators
  iterator begin() { return iterator{lru_head_}; }
  iterator end() { return iterator{nullptr}; }
  const_iterator begin() const { return const_iterator{lru_head_}; }
  const_iterator end() const { return const_iterator{nullptr}; }

  // Adds the specified key/value pair into the cache, putting it at the head
  // of the LRU list. Can pass rvalue reference to move the ownership.
  void Insert(const Key& key, Value value);

  // Adds the specified key and return the Element added to the cache.
  // Caller needs to set the value
  Element* Insert(const Key& key);

  // Returns the cached value associated with the key, or NULL if the cache
  // does not contain an entry for that key.  The caller does not assume
  // ownership of the returned value.  The reference returned by Lookup() could
  // be invalidated by a call to Insert(), so the caller must take care to not
  // access the value if Insert() could have been called after Lookup().
  const Value* absl_nullable Lookup(const Key& key) {
    return MutableLookup(key);
  }

  // return non-const Value
  Value* absl_nullable MutableLookup(const Key& key);

  // Lookup/MutableLookup don't change the LRU order.
  const Value* absl_nullable LookupWithoutInsert(const Key& key) const;

  // Returns non-const value.
  Value* absl_nullable MutableLookupWithoutInsert(const Key& key);

  // Removes the cache entry specified by key.  Returns true if the entry was
  // in the cache, otherwise returns false.
  bool Erase(const Key& key);

  // Removes all entries from the cache.  Note that this does not release the
  // memory associates with the blocks, but just pushes all the elements onto
  // the free list.
  void Clear();

  // Returns the number of entries currently in the cache.
  size_t Size() const { return table_.size(); }
  bool empty() const { return lru_head_ == nullptr; }

  bool HasKey(const Key& key) const { return table_.find(key) != table_.end(); }

  // Returns the head of LRU list
  const Element* Head() const { return lru_head_; }
  Element* MutableHead() { return lru_head_; }

  // Returns the tail of LRU list
  const Element* Tail() const { return lru_tail_; }
  Element* MutableTail() { return lru_tail_; }

  // Expose the free list only for testing purposes.
  const Element* FreeListForTesting() const { return free_list_; }

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
  Element* absl_nullable NextFreeElement();

  // Returns the Element* associated with key, or NULL if no element with this
  // key is found.
  Element* absl_nullable LookupInternal(const Key& key) const;

  // Removes the specified element from the LRU list.
  void RemoveFromLRU(Element* element);

  // Adds the specified element to the head of the LRU list.
  void PushLRUHead(Element* element);

  // Evict is similar to Erase, except that it adds the eviction callback and
  // element to a list of eviction calls, and takes an element so that another
  // lookup is not necessary.
  bool Evict(Element* element);

  absl::flat_hash_map<Key, Element*> table_;
  Element* free_list_ = nullptr;  // singly linked list of Element
  Element* lru_head_ = nullptr;   // head of doubly linked list of Element
  Element* lru_tail_ = nullptr;   // tail of doubly linked list of Element
  std::unique_ptr<Element[]> blocks_[10];  // blocks of Element, with each
  //   block being twice as big as the previous block.  This allows the cache to
  //   use a small amount of memory when it contains few items, but still have
  //   low malloc overhead per insert.  The number of blocks is arbitrary, but
  //   10 blocks allows the largest block to be 2^10 times as large as the
  //   smallest block, which seems like more than enough.
  size_t block_count_ = 0;      // how many entries in blocks_ have been used
  size_t block_capacity_ = 0;   // num elements the current blocks can hold
  size_t next_block_size_ = 0;  // size of the next block to allocate
  const size_t max_elements_;   // maximum elements to hold
};

template <typename Key, typename Value>
void LruCache<Key, Value>::AddBlock() {
  const size_t max_blocks = sizeof(blocks_) / sizeof(blocks_[0]);
  if (block_count_ < max_blocks && block_capacity_ < max_elements_) {
    blocks_[block_count_] = std::make_unique<Element[]>(next_block_size_);
    block_capacity_ += next_block_size_;
    // Add new elements to free list
    for (size_t i = 0; i < next_block_size_; ++i) {
      Element* e = &((blocks_[block_count_])[i]);
      e->prev = nullptr;  // free list is not doubly linked
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

template <typename Key, typename Value>
void LruCache<Key, Value>::PushFreeList(Element* element) {
  element->prev = nullptr;
  element->next = free_list_;
  free_list_ = element;
}

template <typename Key, typename Value>
typename LruCache<Key, Value>::Element* LruCache<Key, Value>::PopFreeList() {
  Element* r = free_list_;
  if (r != nullptr) {
    CHECK(r->prev == nullptr);
    free_list_ = r->next;
    if (free_list_ != nullptr) {
      free_list_->prev = nullptr;
    }
    r->next = nullptr;
  }
  return r;
}

template <typename Key, typename Value>
typename LruCache<Key, Value>::Element* absl_nullable
LruCache<Key, Value>::NextFreeElement() {
  Element* r = PopFreeList();
  if (r == nullptr) {
    AddBlock();
    r = PopFreeList();
  }
  return r;
}

template <typename Key, typename Value>
typename LruCache<Key, Value>::Element* absl_nullable
LruCache<Key, Value>::LookupInternal(const Key& key) const {
  if (auto iter = table_.find(key); iter != table_.end()) {
    return iter->second;
  }
  return nullptr;
}

template <typename Key, typename Value>
void LruCache<Key, Value>::RemoveFromLRU(Element* element) {
  if (lru_head_ == element) {
    lru_head_ = element->next;
  }
  if (lru_tail_ == element) {
    lru_tail_ = element->prev;
  }
  if (element->prev != nullptr) {
    element->prev->next = element->next;
  }
  if (element->next != nullptr) {
    element->next->prev = element->prev;
  }
  element->prev = nullptr;
  element->next = nullptr;
}

template <typename Key, typename Value>
void LruCache<Key, Value>::PushLRUHead(Element* element) {
  if (lru_head_ == element) {
    // element is already at head, so do nothing.
    return;
  }
  RemoveFromLRU(element);
  element->next = lru_head_;
  lru_head_ = element;
  if (element->next != nullptr) {
    element->next->prev = element;
  }
  if (lru_tail_ == nullptr) {
    lru_tail_ = element;
  }
}

template <typename Key, typename Value>
bool LruCache<Key, Value>::Evict(Element* e) {
  if (e != nullptr) {
    const int erased = table_.erase(e->key);
    CHECK_EQ(erased, 1);
    RemoveFromLRU(e);
    PushFreeList(e);
    return true;
  }
  return false;
}

template <typename Key, typename Value>
LruCache<Key, Value>::LruCache(size_t max_elements)
    : max_elements_(max_elements) {
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

template <typename Key, typename Value>
void LruCache<Key, Value>::Insert(const Key& key, Value value) {
  Element* e = Insert(key);
  if (e != nullptr) {
    e->value = std::move(value);
  }
}

template <typename Key, typename Value>
typename LruCache<Key, Value>::Element* LruCache<Key, Value>::Insert(
    const Key& key) {
  bool erased = false;
  Element* e = LookupInternal(key);
  if (e != nullptr) {
    erased = Evict(e);
    CHECK(erased);
  }

  e = NextFreeElement();
  if (e == nullptr) {
    // no free elements, I have to replace an existing element
    e = lru_tail_;
    erased = Evict(e);
    CHECK(erased);
    e = NextFreeElement();
    CHECK(e != nullptr);
  }
  e->key = key;
  table_[key] = e;
  PushLRUHead(e);

  return e;
}

template <typename Key, typename Value>
Value* absl_nullable LruCache<Key, Value>::MutableLookup(const Key& key) {
  Element* e = LookupInternal(key);
  if (e != nullptr) {
    PushLRUHead(e);
    return &(e->value);
  }
  return nullptr;
}

template <typename Key, typename Value>
Value* absl_nullable LruCache<Key, Value>::MutableLookupWithoutInsert(
    const Key& key) {
  Element* e = LookupInternal(key);
  if (e != nullptr) {
    return &(e->value);
  }
  return nullptr;
}

template <typename Key, typename Value>
const Value* absl_nullable LruCache<Key, Value>::LookupWithoutInsert(
    const Key& key) const {
  Element* e = LookupInternal(key);
  if (e != nullptr) {
    return &(e->value);
  }
  return nullptr;
}

template <typename Key, typename Value>
bool LruCache<Key, Value>::Erase(const Key& key) {
  Element* e = LookupInternal(key);
  return Evict(e);
}

template <typename Key, typename Value>
void LruCache<Key, Value>::Clear() {
  table_.clear();
  for (Element& e : *this) {
    PushFreeList(&e);
  }
  lru_head_ = lru_tail_ = nullptr;
}

}  // namespace storage
}  // namespace mozc

#endif  // MOZC_STORAGE_LRU_CACHE_H_
