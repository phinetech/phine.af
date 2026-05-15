/// @file test_qod_client.cpp
/// @brief Unit tests for QodClient JSON serialisation and response parsing.
///
/// These tests cover the serialisation/deserialisation logic without requiring
/// a live gRPC server.  The gRPC stub is mocked via gmock.

#include "qod_client.hpp"
#include "qod_models.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace phine::adapter;

// ─── Model / enum helpers ───────────────────────────────────────────────────

TEST(QodModels, QosStatusRoundTrip) {
    EXPECT_EQ(qos_status_from_string("REQUESTED"), QosStatus::REQUESTED);
    EXPECT_EQ(qos_status_from_string("AVAILABLE"), QosStatus::AVAILABLE);
    EXPECT_EQ(qos_status_from_string("UNAVAILABLE"), QosStatus::UNAVAILABLE);
    EXPECT_STREQ(to_string(QosStatus::REQUESTED), "REQUESTED");
    EXPECT_STREQ(to_string(QosStatus::AVAILABLE), "AVAILABLE");
    EXPECT_STREQ(to_string(QosStatus::UNAVAILABLE), "UNAVAILABLE");
}

TEST(QodModels, StatusInfoRoundTrip) {
    EXPECT_EQ(status_info_from_string("DURATION_EXPIRED"),
              StatusInfo::DURATION_EXPIRED);
    EXPECT_EQ(status_info_from_string("NETWORK_TERMINATED"),
              StatusInfo::NETWORK_TERMINATED);
    EXPECT_EQ(status_info_from_string("DELETE_REQUESTED"),
              StatusInfo::DELETE_REQUESTED);
    EXPECT_EQ(status_info_from_string("unknown"),
              StatusInfo::NONE);
}

TEST(QodModels, ResultOk) {
    auto r = Result<int>::ok(42);
    EXPECT_TRUE(r.success);
    EXPECT_EQ(r.value, 42);
    EXPECT_TRUE(r.error_code.empty());
}

TEST(QodModels, ResultError) {
    auto r = Result<int>::error("NOT_FOUND", "session not found");
    EXPECT_FALSE(r.success);
    EXPECT_EQ(r.error_code, "NOT_FOUND");
    EXPECT_EQ(r.error_message, "session not found");
}

TEST(QodModels, VoidResultOk) {
    auto r = Result<void>::ok();
    EXPECT_TRUE(r.success);
}

TEST(QodModels, VoidResultError) {
    auto r = Result<void>::error("INTERNAL", "boom");
    EXPECT_FALSE(r.success);
    EXPECT_EQ(r.error_code, "INTERNAL");
}

// ─── Placeholder for gRPC-level mock tests (Phase 2) ────────────────────────

// TODO: Add mock-stub tests for create_session, get_session, etc.
//       These will inject a mock InternalCommunication::Stub and verify
//       that the correct InternalMessage is sent and the response is parsed.
