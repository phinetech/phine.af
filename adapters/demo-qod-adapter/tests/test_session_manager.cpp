/// @file test_session_manager.cpp
/// @brief Unit tests for SessionManager using a mock IQodClient.

#include "session_manager.hpp"
#include "qod_client.hpp"
#include "qod_models.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace phine::adapter;
using ::testing::_;
using ::testing::Return;

// ─── Mock QoD client ────────────────────────────────────────────────────────

class MockQodClient : public IQodClient {
   public:
    MOCK_METHOD(bool, wait_for_ready, (int), (override));
    MOCK_METHOD(Result<SessionInfo>, create_session,
                (const CreateSessionRequest&), (override));
    MOCK_METHOD(Result<SessionInfo>, get_session, (const std::string&),
                (override));
    MOCK_METHOD(Result<void>, delete_session, (const std::string&),
                (override));
    MOCK_METHOD(Result<SessionInfo>, extend_session,
                (const std::string&, const ExtendSessionRequest&),
                (override));
    MOCK_METHOD(Result<std::vector<SessionInfo>>, retrieve_sessions,
                (const Device&), (override));
};

// ─── Helpers ────────────────────────────────────────────────────────────────

static StreamConfig make_stream(const std::string& name,
                                const std::string& profile, int port) {
    StreamConfig sc;
    sc.name = name;
    sc.qos_profile = profile;
    sc.duration_seconds = 3600;
    sc.device.ipv4_address = DeviceIpv4Addr{"10.60.0.1", port};
    sc.app_server.ipv4_address = "0.0.0.0/0";
    if (port > 0) {
        PortsSpec ps;
        ps.ports.push_back(port);
        sc.device_ports = ps;
    }
    return sc;
}

static SessionInfo make_session(const std::string& id, QosStatus status) {
    SessionInfo si;
    si.session_id = id;
    si.qos_status = status;
    si.duration = 3600;
    si.qos_profile = "QOS_L";
    return si;
}

// ─── Tests ──────────────────────────────────────────────────────────────────

TEST(SessionManager, CreateAllSessions_Success) {
    auto mock = std::make_shared<MockQodClient>();
    SessionManager mgr(mock);

    EXPECT_CALL(*mock, create_session(_))
        .WillOnce(Return(Result<SessionInfo>::ok(make_session("s1", QosStatus::REQUESTED))))
        .WillOnce(Return(Result<SessionInfo>::ok(make_session("s2", QosStatus::REQUESTED))))
        .WillOnce(Return(Result<SessionInfo>::ok(make_session("s3", QosStatus::REQUESTED))));

    std::vector<StreamConfig> streams = {
        make_stream("video", "QOS_L", 8554),
        make_stream("webrtc", "QOS_E", 8443),
        make_stream("general", "QOS_S", 0),
    };

    mgr.create_all_sessions(streams);
    EXPECT_EQ(mgr.tracked_sessions().size(), 3u);
}

TEST(SessionManager, CreateAllSessions_PartialFailure) {
    auto mock = std::make_shared<MockQodClient>();
    SessionManager mgr(mock);

    EXPECT_CALL(*mock, create_session(_))
        .WillOnce(Return(Result<SessionInfo>::ok(make_session("s1", QosStatus::REQUESTED))))
        .WillOnce(Return(Result<SessionInfo>::error("CONFLICT", "already exists")))
        .WillOnce(Return(Result<SessionInfo>::ok(make_session("s3", QosStatus::REQUESTED))));

    std::vector<StreamConfig> streams = {
        make_stream("video", "QOS_L", 8554),
        make_stream("webrtc", "QOS_E", 8443),
        make_stream("general", "QOS_S", 0),
    };

    mgr.create_all_sessions(streams);
    EXPECT_EQ(mgr.tracked_sessions().size(), 2u);
}

TEST(SessionManager, MonitorSessions_DetectsStatusChange) {
    auto mock = std::make_shared<MockQodClient>();
    SessionManager mgr(mock);

    EXPECT_CALL(*mock, create_session(_))
        .WillOnce(Return(Result<SessionInfo>::ok(make_session("s1", QosStatus::REQUESTED))));

    mgr.create_all_sessions({make_stream("video", "QOS_L", 8554)});

    // Monitoring returns AVAILABLE
    EXPECT_CALL(*mock, get_session("s1"))
        .WillOnce(Return(Result<SessionInfo>::ok(make_session("s1", QosStatus::AVAILABLE))));

    mgr.monitor_sessions();

    auto it = mgr.tracked_sessions().find("s1");
    ASSERT_NE(it, mgr.tracked_sessions().end());
    EXPECT_EQ(it->second.session_info->qos_status, QosStatus::AVAILABLE);
    EXPECT_EQ(it->second.check_count, 1);
}

TEST(SessionManager, CleanupAllSessions_Success) {
    auto mock = std::make_shared<MockQodClient>();
    SessionManager mgr(mock);

    EXPECT_CALL(*mock, create_session(_))
        .WillOnce(Return(Result<SessionInfo>::ok(make_session("s1", QosStatus::REQUESTED))))
        .WillOnce(Return(Result<SessionInfo>::ok(make_session("s2", QosStatus::REQUESTED))));

    mgr.create_all_sessions({
        make_stream("video", "QOS_L", 8554),
        make_stream("webrtc", "QOS_E", 8443),
    });

    EXPECT_CALL(*mock, delete_session("s1")).WillOnce(Return(Result<void>::ok()));
    EXPECT_CALL(*mock, delete_session("s2")).WillOnce(Return(Result<void>::ok()));

    mgr.cleanup_all_sessions();
    EXPECT_TRUE(mgr.tracked_sessions().empty());
}

TEST(SessionManager, CleanupAllSessions_PartialFailure) {
    auto mock = std::make_shared<MockQodClient>();
    SessionManager mgr(mock);

    EXPECT_CALL(*mock, create_session(_))
        .WillOnce(Return(Result<SessionInfo>::ok(make_session("s1", QosStatus::REQUESTED))))
        .WillOnce(Return(Result<SessionInfo>::ok(make_session("s2", QosStatus::REQUESTED))));

    mgr.create_all_sessions({
        make_stream("video", "QOS_L", 8554),
        make_stream("webrtc", "QOS_E", 8443),
    });

    EXPECT_CALL(*mock, delete_session("s1")).WillOnce(Return(Result<void>::ok()));
    EXPECT_CALL(*mock, delete_session("s2"))
        .WillOnce(Return(Result<void>::error("INTERNAL", "server error")));

    mgr.cleanup_all_sessions();
    // s1 deleted, s2 still tracked due to failure
    EXPECT_EQ(mgr.tracked_sessions().size(), 1u);
    EXPECT_TRUE(mgr.tracked_sessions().count("s2"));
}

TEST(SessionManager, DeleteSession_RemovesFromTracking) {
    auto mock = std::make_shared<MockQodClient>();
    SessionManager mgr(mock);

    EXPECT_CALL(*mock, create_session(_))
        .WillOnce(Return(Result<SessionInfo>::ok(make_session("s1", QosStatus::REQUESTED))));

    mgr.create_all_sessions({make_stream("video", "QOS_L", 8554)});
    EXPECT_EQ(mgr.tracked_sessions().size(), 1u);

    EXPECT_CALL(*mock, delete_session("s1")).WillOnce(Return(Result<void>::ok()));
    mgr.delete_session("s1");
    EXPECT_TRUE(mgr.tracked_sessions().empty());
}
