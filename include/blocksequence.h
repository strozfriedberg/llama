#pragma once

#include <cstdint>
#include <string>
#include <utility>

class BlockSequence {
public:
  virtual ~BlockSequence() {}

  class Iterator {
  public:
    using value_type = std::pair<const uint8_t*, const uint8_t*>;
    using pointer = const value_type*;
    using reference = const value_type&;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::input_iterator_tag;

    Iterator(): seq(nullptr), cur(nullptr, nullptr) {
    }

    Iterator(BlockSequence* seq): seq(seq), cur(nullptr, nullptr) {
      ++*this;
    }

    Iterator(const Iterator&) = default;

    Iterator(Iterator&&) = default;

    Iterator& operator=(const Iterator&) = default;

    Iterator& operator=(Iterator&&) = default;

    reference operator*() const {
      return cur;
    }

    pointer operator->() const {
      return &cur;
    }

    Iterator& operator++() {
      if (seq->advance()) {
        cur = seq->cur();
      }
      else {
        seq = nullptr;
      }
      return *this;
    }

    Iterator operator++(int) {
      Iterator i(*this);
      ++*this;
      return i;
    }

    bool operator==(const Iterator& r) const {
      return seq ? r.seq && seq->offset() == r.seq->offset() : !r.seq;
    }

    bool operator!=(const Iterator& r) const { return !(r == *this); }

  private:
    BlockSequence* seq;
    value_type cur;
  };

  virtual Iterator begin() {
    return Iterator(this);
  }

  virtual Iterator end() const {
    return Iterator();
  }

protected:
  virtual bool advance() = 0;

  virtual std::pair<const uint8_t*, const uint8_t*> cur() const = 0;

  virtual size_t offset() const = 0;
};

class EmptyBlockSequence: public BlockSequence {
public:
  virtual ~EmptyBlockSequence() {}

  virtual Iterator begin() override {
    return end();
  }

protected:
  virtual bool advance() override {
    return false;
  }

  virtual std::pair<const uint8_t*, const uint8_t*> cur() const override {
    return std::make_pair(nullptr, nullptr);
  }

  virtual size_t offset() const override {
    return 0;
  }
};

/*
class SingleBufferBlockSequence: public BlockSequence {
public:
  SingleBufferBlockSequence(const uint8_t* beg, const uint8_t* end):
    beg(beg), pos(nullptr), end(end) {}

  virtual ~SingleBufferBlockSequence() {}

protected:
  virtual bool advance() {
    pos = pos ? end : beg;
    return pos != end;
  }

  virtual std::pair<const uint8_t*, const uint8_t*> cur() const {
    return std::make_pair(pos, end);
  }

  virtual size_t offset() const {
    return pos - beg;
  }

private:
  const uint8_t* beg;
  const uint8_t* pos;
  const uint8_t* end;
};
*/


