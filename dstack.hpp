#ifndef __DSTACK_H_
#define __DSTACK_H_

#include <stdint.h>

class DStack {
 public:
  // Represents the current top of the stack.
  // You can only roll back to a marker, not to an arbitrary
  // location within the stack
  typedef size_t marker;

  // Construct a double stack with a given size
  DStack(size_t size) noexcept;
  ~DStack();

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

    T *p = (T *)(mStack + mMarkerBottom);
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
