#include "dstack.hpp"

#include <stdint.h>

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("DStack") {
  DStack stack{12};
  SECTION("init") { REQUIRE(stack.getSize() == 12); }

  SECTION("good alloc top") {
    void *p = stack.allocTop<void>(10);
    REQUIRE(p != nullptr);
  }

  SECTION("good alloc bottom") {
    void *p = stack.allocBottom<void>(10);
    REQUIRE(p != nullptr);
  }

  SECTION("overflow top") {
    void *p = stack.allocTop<void>(13);
    REQUIRE(p == nullptr);
  }

  SECTION("overflow bottom") {
    void *p = stack.allocBottom<void>(13);
    REQUIRE(p == nullptr);
  }

  SECTION("alloc a thing") {
    // 2 bytes
    struct Test {
      uint16_t c;
    };

    Test *s = stack.allocTop<Test>();
    Test *bs = stack.allocBottom<Test>();

    REQUIRE(s != nullptr);
    REQUIRE(bs != nullptr);
  }

  SECTION("overflow a thing") {
    // 10 bytes
    struct Test {
      uint16_t c;
      uint32_t x;
      uint32_t y;
    };

    Test *s = stack.allocTop<Test>();
    Test *bs = stack.allocBottom<Test>();
    Test *ss = stack.allocTop<Test>();

    REQUIRE(s != nullptr);
    REQUIRE(bs == nullptr);
    REQUIRE(ss == nullptr);
  }

  SECTION("clear top") {
    void *p = stack.allocTop<void>(10);
    void *p2 = stack.allocTop<void>(1);
    REQUIRE(stack.getSize() == 12);
    REQUIRE(stack.getMarkerTop() == 11);

    stack.clearTop();
    REQUIRE(stack.getSize() == 12);
    REQUIRE(stack.getMarkerTop() == 0);
  }

  SECTION("clear bottom") {
    void *p = stack.allocBottom<void>(10);
    void *p2 = stack.allocBottom<void>(1);
    REQUIRE(stack.getSize() == 12);
    REQUIRE(stack.getMarkerBottom() == 12 - 11);

    stack.clearBottom();
    REQUIRE(stack.getSize() == 12);
    REQUIRE(stack.getMarkerBottom() == 12);
  }

  SECTION("free to marker top") {
    uint32_t *i = stack.allocTop<uint32_t>();
    uint32_t *i2 = stack.allocTop<uint32_t>();

    *i = 1;
    *i2 = 2;

    auto m = stack.getMarkerTop();

    uint32_t *i3 = stack.allocTop<uint32_t>();

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
    uint32_t *i = stack.allocBottom<uint32_t>();
    uint32_t *i2 = stack.allocBottom<uint32_t>();

    *i = 1;
    *i2 = 2;

    auto m = stack.getMarkerBottom();

    uint32_t *i3 = stack.allocBottom<uint32_t>();

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
}
