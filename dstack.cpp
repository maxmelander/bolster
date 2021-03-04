#include "dstack.hpp"

DStack::DStack(size_t size) noexcept
    : mStack{new char[size]},
      mStackSize{size},
      mMarkerTop{},
      mMarkerBottom{size} {}

DStack::~DStack() { delete[] mStack; }

DStack::marker DStack::getMarkerTop() { return mMarkerTop; }
DStack::marker DStack::getMarkerBottom() { return mMarkerBottom; }

void DStack::freeTopToMarker(marker marker) { mMarkerTop = marker; }
void DStack::freeBottomToMarker(marker marker) { mMarkerBottom = marker; }

void DStack::clearTop() { mMarkerTop = 0; }
void DStack::clearBottom() { mMarkerBottom = mStackSize; }

size_t DStack::getSize() { return mStackSize; }
