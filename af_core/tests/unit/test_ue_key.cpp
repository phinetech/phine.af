/**
 * @file test_ue_key.cpp
 * @brief Light smoke tests for UeKey — proves the af_core unit-test/ctest
 * wiring end-to-end. Extend with real coverage as af_core's unit suite grows.
 */

#include <gtest/gtest.h>

#include "models/ue_state.h"

TEST(UeKeySmoke, FromValidSupiProducesExpectedPrimaryKey) {
    Supi supi{"imsi-123456789012345"};
    UeKey key = UeKey::from_supi(supi);
    EXPECT_EQ(key.get_primary_key(), "supi:imsi-123456789012345");
}

TEST(UeKeySmoke, FromInvalidSupiThrows) {
    Supi supi{"not-a-valid-supi"};
    EXPECT_THROW(UeKey::from_supi(supi), std::invalid_argument);
}
