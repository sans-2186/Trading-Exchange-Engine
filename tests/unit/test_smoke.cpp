#include <gtest/gtest.h>

#include "mercury/core/types.hpp"
#include "mercury/version.hpp"

TEST(SmokeTest, ProjectBuilds) {
    EXPECT_STREQ(mercury::version_string(), "0.1.0");
}

TEST(SmokeTest, StrongTypedefsAreDistinct) {
    mercury::Price price{100};
    mercury::Quantity quantity{100};

    EXPECT_EQ(price.ticks, 100);
    EXPECT_EQ(quantity.value, 100u);
}

TEST(SmokeTest, SideEnumValues) {
    EXPECT_NE(mercury::Side::Buy, mercury::Side::Sell);
}
