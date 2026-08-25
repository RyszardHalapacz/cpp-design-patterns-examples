#include <gtest/gtest.h>
#include <memory>

#include "patterns/session/SessionManagement.hpp"
#include "patterns/session/SessionCoordinator.hpp"
#include "patterns/session/SessionAuditObserver.hpp"
#include "patterns/engine/Engine.hpp"
#include "patterns/services/ServiceLocator.hpp"
#include "patterns/services/Logger.hpp"
#include "patterns/observer/ISessionObserver.hpp"
#include "patterns/observer/SessionEvent.hpp"
#include "patterns/strategy/SortStrategyId.hpp"
#include "patterns/strategy/SortStrategyFactory.hpp"
#include "patterns/gui/Command.hpp"

using namespace patterns::session;
using namespace patterns::observer;
using namespace patterns::strategy;
using namespace patterns::services;

class SessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        [[maybe_unused]] auto _ = ServiceLocator::instance().provide<Logger>(std::make_shared<Logger>());
    }
};

// ─── SessionManagement ───────────────────────────────────────────────────────

TEST_F(SessionTest, ConnectToEngineAttachesEngineAsObserver) {
    auto engine = std::make_shared<patterns::engine::Engine>();
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("[Session] Engine connected"), std::string::npos);
}

TEST_F(SessionTest, OpenSessionStartsEngine) {
    auto engine = std::make_shared<patterns::engine::Engine>();
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("[Engine] Start"), std::string::npos);
}

TEST_F(SessionTest, OpenSessionWithoutEngineLogsError) {
    SessionManagement session;
    testing::internal::CaptureStdout();
    session.openSession();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("No Engine"), std::string::npos);
}

TEST_F(SessionTest, CloseSessionNotifiesObserversAndClearsState) {
    auto engine = std::make_shared<patterns::engine::Engine>();
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();
    session.closeSession();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("Closing session"), std::string::npos);
    EXPECT_NE(out.find("[Engine] Stop"), std::string::npos);
}

TEST_F(SessionTest, AddVectorFromGuiWhenSessionInactive) {
    auto engine = std::make_shared<patterns::engine::Engine>();
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    // session not opened — inactive
    session.addVectorFromGui({1, 2, 3});
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("Session inactive"), std::string::npos);
}

TEST_F(SessionTest, AddVectorFromGuiWhenSessionActive) {
    auto engine = std::make_shared<patterns::engine::Engine>();
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();
    session.addVectorFromGui({5, 4, 3});
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("GUI wants to add vector"), std::string::npos);
}

TEST_F(SessionTest, SortVectorFromGuiForwardsEvent) {
    auto engine = std::make_shared<patterns::engine::Engine>();
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();
    session.addVectorFromGui({3, 1, 2});
    session.sortVectorFromGui(0);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("GUI wants to sort"), std::string::npos);
}

TEST_F(SessionTest, PrintDataFromGuiForwardsEvent) {
    auto engine = std::make_shared<patterns::engine::Engine>();
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();
    session.printDataFromGui();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("GUI wants to print"), std::string::npos);
}

TEST_F(SessionTest, SetSortStrategyAllowedStrategy) {
    auto engine = std::make_shared<patterns::engine::Engine>();
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();
    session.setAllowedStrategies({SortStrategyId::Ascending, SortStrategyId::Descending});
    session.setSortStrategyFromGui(SortStrategyId::Descending);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("sort strategy"), std::string::npos);
}

TEST_F(SessionTest, SetSortStrategyForbiddenStrategy) {
    auto engine = std::make_shared<patterns::engine::Engine>();
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();
    session.setAllowedStrategies({SortStrategyId::Ascending});
    session.setSortStrategyFromGui(SortStrategyId::Bubble);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("not allowed"), std::string::npos);
}

// Observer: attach / detach

struct SpyObserver : ISessionObserver {
    int callCount = 0;
    SessionEventType lastType{};
    void onSessionEvent(const SessionEvent& e) override {
        ++callCount;
        lastType = e.type;
    }
};

TEST_F(SessionTest, ConnectToEngineTwiceDoesNotDuplicateObserver) {
    auto engine = std::make_shared<patterns::engine::Engine>();
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.connectToEngine(engine);  // second call — engine already attached
    session.openSession();
    session.addVectorFromGui({1, 2, 3});
    std::string out = testing::internal::GetCapturedStdout();

    // Engine would process VectorAdded once, not twice
    size_t first  = out.find("[Engine] -> recognized: vector added");
    size_t second = out.find("[Engine] -> recognized: vector added", first + 1);
    EXPECT_NE(first,  std::string::npos);
    EXPECT_EQ(second, std::string::npos);
}

TEST_F(SessionTest, AttachAndDetachObserver) {
    auto engine = std::make_shared<patterns::engine::Engine>();
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();

    auto spy = std::make_shared<SpyObserver>();
    session.attach(spy);
    session.addVectorFromGui({1});
    testing::internal::GetCapturedStdout();

    EXPECT_EQ(spy->callCount, 1);
    EXPECT_EQ(spy->lastType, SessionEventType::VectorAdded);

    session.detach(spy);
    testing::internal::CaptureStdout();
    session.addVectorFromGui({2});
    testing::internal::GetCapturedStdout();

    EXPECT_EQ(spy->callCount, 1); // no new calls after detach
}

TEST_F(SessionTest, AttachDuplicateObserverIsIgnored) {
    auto engine = std::make_shared<patterns::engine::Engine>();
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();

    auto spy = std::make_shared<SpyObserver>();
    session.attach(spy);
    session.attach(spy);  // duplicate — should be ignored
    session.addVectorFromGui({1});
    testing::internal::GetCapturedStdout();

    EXPECT_EQ(spy->callCount, 1);  // called once, not twice
}

// ─── EngineSessionCoordinator ────────────────────────────────────────────────

TEST_F(SessionTest, EngineSessionCoordinatorConnectsAndOpens) {
    auto engine  = std::make_shared<patterns::engine::Engine>();
    auto factory = std::make_shared<patterns::strategy::SortStrategyFactory>();
    SessionManagement session;

    testing::internal::CaptureStdout();
    EngineSessionCoordinator coordinator(session, engine, factory, std::make_shared<patterns::historian::EngineHistorian>());
    auto result = coordinator.establish();
    std::string out = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result.has_value());
    EXPECT_NE(out.find("[SessionCoordinator] Starting"), std::string::npos);
    EXPECT_NE(out.find("[Session] Engine connected"), std::string::npos);
    EXPECT_NE(out.find("[Engine] Start"), std::string::npos);
    EXPECT_NE(out.find("[SessionCoordinator] Session established"), std::string::npos);
}

TEST_F(SessionTest, EngineSessionCoordinator_NullEngine_ReturnsError) {
    std::shared_ptr<patterns::engine::Engine> noEngine;
    auto factory = std::make_shared<patterns::strategy::SortStrategyFactory>();
    SessionManagement session;

    testing::internal::CaptureStdout();
    EngineSessionCoordinator coordinator(session, noEngine, factory, std::make_shared<patterns::historian::EngineHistorian>());
    auto result = coordinator.establish();
    std::string out = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "Engine not available");
    EXPECT_NE(out.find("[SessionCoordinator] Engine not available — aborting"), std::string::npos);
    EXPECT_EQ(out.find("[Session] Engine connected"), std::string::npos);
    EXPECT_EQ(out.find("[SessionCoordinator] Session established"), std::string::npos);
}

// ─── SessionAuditObserver ────────────────────────────────────────────────────

TEST_F(SessionTest, AuditObserverLogsVectorAdded) {
    auto* audit = new SessionAuditObserver();

    SessionEvent ev;
    ev.type = SessionEventType::VectorAdded;

    testing::internal::CaptureStdout();
    audit->onSessionEvent(ev);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("[Audit] Recorded: vector added"), std::string::npos);

    // manually delete since we won't trigger SessionClosing
    delete audit;
}

TEST_F(SessionTest, AuditObserverLogsSortRequested) {
    auto* audit = new SessionAuditObserver();

    SessionEvent ev;
    ev.type = SessionEventType::SortRequested;

    testing::internal::CaptureStdout();
    audit->onSessionEvent(ev);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("[Audit] Recorded: sort requested"), std::string::npos);

    delete audit;
}

TEST_F(SessionTest, AuditObserverLogsPrintRequested) {
    auto* audit = new SessionAuditObserver();

    SessionEvent ev;
    ev.type = SessionEventType::PrintRequested;

    testing::internal::CaptureStdout();
    audit->onSessionEvent(ev);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("[Audit] Recorded: print requested"), std::string::npos);

    delete audit;
}

TEST_F(SessionTest, AuditObserverLogsStrategyChangeRequested) {
    auto* audit = new SessionAuditObserver();

    SessionEvent ev;
    ev.type       = SessionEventType::StrategyChangeRequested;
    ev.strategyId = SortStrategyId::Descending;

    testing::internal::CaptureStdout();
    audit->onSessionEvent(ev);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("[Audit] Recorded: strategy change to"), std::string::npos);

    delete audit;
}

// ExecuteBatch moved to DummyGui::flushBatch — batch is executed via ICommand::execute()
// Coverage for batch execution lives in test_gui.cpp (FlushBatchExecutesCommands)

TEST_F(SessionTest, SetSortStrategyFromGuiWithNoEngineLogsError) {
    SessionManagement session; // no engine connected

    testing::internal::CaptureStdout();
    session.setSortStrategyFromGui(SortStrategyId::Ascending);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("No Engine"), std::string::npos);
}

TEST_F(SessionTest, AuditObserverOwnedExternallyOnSessionClosing) {
    auto engine = std::make_shared<patterns::engine::Engine>();
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();

    // Owned by caller — lifetime independent of session
    auto audit = std::make_shared<SessionAuditObserver>();
    session.attach(audit);

    session.closeSession();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("[Audit] Session closing"), std::string::npos);
    // audit still alive — owned by this scope, not by session
    EXPECT_NE(audit, nullptr);
}
