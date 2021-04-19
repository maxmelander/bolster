#include "dstack.hpp"

#include <stdint.h>

#include <iostream>

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("DStack") {
  DStack stack{12};
  SECTION("init") { REQUIRE(stack.getSize() == 12); }

  SECTION("good alloc top") {
    void *p = stack.allocUnalignedTop<void>(10);
    REQUIRE(p != nullptr);
  }

  SECTION("good alloc bottom") {
    void *p = stack.allocUnalignedBottom<void>(10);
    REQUIRE(p != nullptr);
  }

  SECTION("overflow top") {
    void *p = stack.allocUnalignedTop<void>(13);
    REQUIRE(p == nullptr);
  }

  SECTION("overflow bottom") {
    void *p = stack.allocUnalignedBottom<void>(13);
    REQUIRE(p == nullptr);
  }

  SECTION("alloc stuff") {
    // 2 bytes
    struct Test {
      uint16_t c;
    };

    Test *s = stack.allocUnalignedTop<Test>();
    Test *s2 = stack.allocUnalignedTop<Test>();
    Test *bs = stack.allocUnalignedBottom<Test>();
    Test *bs2 = stack.allocUnalignedBottom<Test>();

    REQUIRE(s != nullptr);
    REQUIRE(s2 != nullptr);
    REQUIRE(bs != nullptr);

    s2->c = 264;
    s->c = UINT16_MAX;
    bs2->c = 666;
    bs->c = UINT16_MAX;

    REQUIRE(s->c == UINT16_MAX);
    REQUIRE(s2->c == 264);
    REQUIRE(bs->c == UINT16_MAX);
    REQUIRE(bs2->c == 666);
  }

  SECTION("overflow a thing") {
    // 10 bytes
    // NOTE: This struct would actually end up being
    // 12 bytes because of padding between c and x
    struct Test {
      uint16_t c;
      uint32_t x;
      uint32_t y;
    };

    Test *s = stack.allocUnalignedTop<Test>();
    Test *bs = stack.allocUnalignedBottom<Test>();
    Test *ss = stack.allocUnalignedTop<Test>();

    REQUIRE(s != nullptr);
    REQUIRE(bs == nullptr);
    REQUIRE(ss == nullptr);
  }

  SECTION("clear top") {
    void *p = stack.allocUnalignedTop<void>(10);
    void *p2 = stack.allocUnalignedTop<void>(1);
    REQUIRE(stack.getSize() == 12);
    REQUIRE(stack.getMarkerTop() == 11);

    stack.clearTop();
    REQUIRE(stack.getSize() == 12);
    REQUIRE(stack.getMarkerTop() == 0);
  }

  SECTION("clear bottom") {
    void *p = stack.allocUnalignedBottom<void>(10);
    void *p2 = stack.allocUnalignedBottom<void>(1);
    REQUIRE(stack.getSize() == 12);
    REQUIRE(stack.getMarkerBottom() == 12 - 11);

    stack.clearBottom();
    REQUIRE(stack.getSize() == 12);
    REQUIRE(stack.getMarkerBottom() == 12);
  }

  SECTION("free to marker top") {
    uint32_t *i = stack.allocUnalignedTop<uint32_t>();
    uint32_t *i2 = stack.allocUnalignedTop<uint32_t>();

    *i = 1;
    *i2 = 2;

    auto m = stack.getMarkerTop();

    uint32_t *i3 = stack.allocUnalignedTop<uint32_t>();

    *i3 = 3;

    REQUIRE(stack.getMarkerTop() == 12);

    stack.freeTopToMarker(m);

    REQUIRE(stack.getMarkerTop() == 8);
    *i2 = 12;

    // Ensure that we haven't messed up marker borders
    REQUIRE(*i == 1);
    REQUIRE(*i2 == 12);
    REQUIRE(*i3 == 3);
  }

  SECTION("free to marker bottom") {
    uint32_t *i = stack.allocUnalignedBottom<uint32_t>();
    uint32_t *i2 = stack.allocUnalignedBottom<uint32_t>();

    *i = 1;
    *i2 = 2;

    auto m = stack.getMarkerBottom();

    uint32_t *i3 = stack.allocUnalignedBottom<uint32_t>();

    *i3 = 3;

    REQUIRE(stack.getMarkerBottom() == 0);

    stack.freeBottomToMarker(m);

    REQUIRE(stack.getMarkerBottom() == 12 - 8);
    *i2 = 12;

    // Ensure that we haven't messed up marker borders
    REQUIRE(*i == 1);
    REQUIRE(*i2 == 12);
    REQUIRE(*i3 == 3);
  }

  SECTION("Alligned alloc top") {
    void *padding = stack.allocUnalignedTop<void>(3);
    uint16_t *unalignedBytes = stack.allocUnalignedTop<uint16_t>();

    size_t alignment = alignof(uint16_t);
    size_t mask = (alignment - 1);
    std::uintptr_t missalignment =
        (reinterpret_cast<std::uintptr_t>(unalignedBytes) & mask);

    REQUIRE(missalignment > 0);

    uint16_t *alignedBytes = stack.alloc<uint16_t, StackDirection::Top>();

    std::cout << stack.getMarkerTop() << std::endl;
    missalignment = (reinterpret_cast<std::uintptr_t>(alignedBytes) & mask);
    REQUIRE(missalignment == 0);

    REQUIRE(stack.getMarkerTop() == 8);
  }

  SECTION("Alligned alloc bottom") {
    void *padding = stack.allocUnalignedBottom<void>(3);
    uint16_t *unalignedBytes = stack.allocUnalignedBottom<uint16_t>();

    size_t alignment = alignof(uint16_t);
    size_t mask = (alignment - 1);
    std::uintptr_t missalignment =
        (reinterpret_cast<std::uintptr_t>(unalignedBytes) & mask);

    REQUIRE(missalignment > 0);

    uint16_t *alignedBytes = stack.alloc<uint16_t, StackDirection::Bottom>();

    missalignment = (reinterpret_cast<std::uintptr_t>(alignedBytes) & mask);

    REQUIRE(missalignment == 0);
  }

  SECTION("Alligned array") {
    uint16_t *array =
        stack.alloc<uint16_t, StackDirection::Top>(sizeof(uint16_t) * 4);

    array[3] = 3;
    array[2] = 2;
    array[1] = 1;
    array[0] = 0;

    REQUIRE(array[0] == 0);
    REQUIRE(array[1] == 1);
    REQUIRE(array[2] == 2);
    REQUIRE(array[3] == 3);
  }
}
