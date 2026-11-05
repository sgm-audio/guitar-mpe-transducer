#include <catch2/catch_test_macros.hpp>

TEST_CASE("Catch2 smoke test", "[smoke]") {
    REQUIRE(1 + 1 == 2);
}
