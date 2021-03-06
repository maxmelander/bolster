#ifndef __DSTACK_H_
#define __DSTACK_H_

#include <stdint.h>

#include <cassert>
#include <cstddef>
#include <cstdint>

enum class StackDirection { Top, Bottom };

class DStack {
 public:
  // Represents the current top of the stack.
  // You can only roll back to a marker, not to an arbitrary
  // location within the stack
  typedef size_t marker;

  // Construct a double stack with a given size
  DStack(size_t size) noexcept;
  ~DStack();

  // Aligned allocation
  template <typename T, StackDirection stackDirection>
  T *allocAligned(size_t size, size_t alignment) noexcept {
    assert((alignment & (alignment - 1)) == 0);  // pwr of 2

    // Determine the total amount of memory to allocate.
    size_t expandedSize = size + alignment;

    // Allocate unaligned block
    std::uintptr_t rawAddress = reinterpret_cast<std::uintptr_t>(
        stackDirection == StackDirection::Top ? allocTop<T>(expandedSize)
                                              : allocBottom<T>(expandedSize));

    // Calculate the adjustment by masking off the lower bits
    // of the address, to determine how "misaligned" it is
    size_t mask = (alignment - 1);
    std::uintptr_t missalignment = (rawAddress & mask);

    std::ptrdiff_t adjustment = alignment - missalignment;

    // Calculate the adjusted address, return as pointer
    std::uintptr_t alignedAddress = rawAddress + adjustment;

    return reinterpret_cast<T *>(alignedAddress);
  }

  // Aligned allocation from top of stack
  // NOTE: For structs, use explicit alignment set to the
  // sizeof the biggest member of the struct.
  template <typename T, StackDirection stackDirection>
  T *allocAligned() noexcept {
    return allocAligned<T, stackDirection>(sizeof(T), sizeof(T));
  }

  // Allocate new block of the given size from the stack top
  template <typename T>
  T *allocTop(size_t size) noexcept {
    if (mMarkerTop + size > mMarkerBottom) {
      // Overflow
      return nullptr;
    }

    T *p = (T *)(mStack + mMarkerTop);
    mMarkerTop += size;

    return p;
  };

  // Allocate new block with size of T rom the stack top
  template <typename T>
  T *allocTop() noexcept {
    return allocTop<T>(sizeof(T));
  };

  // Allocate new block of the given size from the stack bottom
  template <typename T>
  T *allocBottom(size_t size) noexcept {
    if (mMarkerBottom < size || mMarkerBottom - size < mMarkerTop) {
      // Overflow
      return nullptr;
    }

    T *p = (T *)(mStack + (mMarkerBottom - size));
    mMarkerBottom -= size;

    return p;
  };

  // Allocate new block with size of T rom the stack bottom
  template <typename T>
  T *allocBottom() noexcept {
    return allocBottom<T>(sizeof(T));
  };

  // Returns a marker to the current stack top
  marker getMarkerTop();

  // Returns a marker to the current stack bottom
  marker getMarkerBottom();

  // Rolls the stack top back to the previous marker
  void freeTopToMarker(marker marker);
  // Rolls the stack bottom back to the previous marker
  void freeBottomToMarker(marker marker);

  // Clears the entire stack top
  void clearTop();
  // Clears the entire stack bottom
  void clearBottom();

  size_t getSize();

 private:
  char *mStack;
  size_t mStackSize;
  marker mMarkerTop;
  marker mMarkerBottom;
};

#endif  // __DSTACK_H_
