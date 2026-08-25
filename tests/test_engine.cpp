#include <gtest/gtest.h>
#include <memory>
#include <algorithm>

#include "patterns/engine/Engine.hpp"
#include "patterns/services/ServiceLocator.hpp"
#include "patterns/services/Logger.hpp"
#include "patterns/strategy/AscendingSortStrategy.hpp"
#include "patterns/strategy/DescendingSortStrategy.hpp"
#include "patterns/strategy/SortStrategyFactory.hpp"
#include "patterns/observer/SessionEvent.hpp"

using namespace patterns::engine;
using namespace patterns::observer;
using namespace patterns::strategy;
using namespace patterns::services;

class EngineTest : public ::testing::Test {
protected:
    std::shared_ptr<SortStrategyFactory> factory = std::make_shared<SortStrategyFactory>();

    void SetUp() override {
        [[maybe_unused]] auto _ = ServiceLocator::instance().provide<Logger>(std::make_shared<Logger>());
    }

    Engine makeEngine() {
        Engine e;
        e.setFactory(factory);
        return e;
    }
};

// ─── start / stop — parameterized ───────────────────────────────────────────

struct EngineLifecycleParam {
    std::string                  name;
    std::function<void(Engine&)> action;
    std::string                  expectedOutput;
};

void PrintTo(const EngineLifecycleParam& p, std::ostream* os) { *os << p.name; }

class EngineLifecycleTest : public ::testing::TestWithParam<EngineLifecycleParam> {
protected:
    void SetUp() override {
        [[maybe_unused]] auto _ = ServiceLocator::instance().provide<Logger>(std::make_shared<Logger>());
    }
};

TEST_P(EngineLifecycleTest, PrintsExpectedMessage) {
    Engine e;
    testing::internal::CaptureStdout();
    GetParam().action(e);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find(GetParam().expectedOutput), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    Actions, EngineLifecycleTest,
    ::testing::Values(
        EngineLifecycleParam{"Start", [](Engine& e){ e.start(); }, "[Engine] Start"},
        EngineLifecycleParam{"Stop",  [](Engine& e){ e.stop();  }, "[Engine] Stop"}
    ),
    [](const ::testing::TestParamInfo<EngineLifecycleParam>& i){ return i.param.name; }
);

// ─── addVector / sortVector ───────────────────────────────────────────────────

TEST_F(EngineTest, AddVectorThenSort) {
    Engine e = makeEngine();
    e.addVector({5, 3, 1, 4, 2});

    testing::internal::CaptureStdout();
    e.sortVector(0);
    testing::internal::GetCapturedStdout();

    // verify sorted by printing and checking output contains sorted order
    testing::internal::CaptureStdout();
    e.printData();
    std::string out = testing::internal::GetCapturedStdout();
    // "1 2 3 4 5" should appear in output
    EXPECT_NE(out.find("1 2 3 4 5"), std::string::npos);
}

TEST_F(EngineTest, SortInvalidIndexLogsError) {
    Engine e;
    testing::internal::CaptureStdout();
    e.sortVector(99);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("Invalid"), std::string::npos);
}

TEST_F(EngineTest, PrintDataWithMultipleVectors) {
    Engine e;
    e.addVector({3, 1, 2});
    e.addVector({9, 8, 7});
    testing::internal::CaptureStdout();
    e.printData();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("[0]"), std::string::npos);
    EXPECT_NE(out.find("[1]"), std::string::npos);
}

// ─── setSortStrategy ─────────────────────────────────────────────────────────

TEST_F(EngineTest, SetDescendingStrategyThenSort) {
    Engine e;
    e.addVector({1, 3, 2, 5, 4});

    testing::internal::CaptureStdout();
    e.setSortStrategy(std::make_unique<DescendingSortStrategy>());
    testing::internal::GetCapturedStdout();

    testing::internal::CaptureStdout();
    e.sortVector(0);
    testing::internal::GetCapturedStdout();

    testing::internal::CaptureStdout();
    e.printData();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("5 4 3 2 1"), std::string::npos);
}

// ─── onSessionEvent ──────────────────────────────────────────────────────────

TEST_F(EngineTest, SessionEventVectorAdded) {
    Engine e;
    SessionEvent ev;
    ev.type       = SessionEventType::VectorAdded;
    ev.vectorData = {10, 20, 30};

    testing::internal::CaptureStdout();
    e.onSessionEvent(ev);
    testing::internal::GetCapturedStdout();

    // Verify the vector was added by sorting it
    testing::internal::CaptureStdout();
    e.printData();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("10 20 30"), std::string::npos);
}

TEST_F(EngineTest, SessionEventSortRequested) {
    Engine e = makeEngine();
    e.addVector({3, 1, 2});

    SessionEvent ev;
    ev.type  = SessionEventType::SortRequested;
    ev.index = 0;

    testing::internal::CaptureStdout();
    e.onSessionEvent(ev);
    testing::internal::GetCapturedStdout();

    testing::internal::CaptureStdout();
    e.printData();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("1 2 3"), std::string::npos);
}

// ─── onSessionEvent (simple output) — parameterized ─────────────────────────

struct SessionEventOutputParam {
    std::string                  name;
    std::function<void(Engine&)> setup;
    std::function<SessionEvent()> makeEvent;
    std::string                  expectedOutput;
};

void PrintTo(const SessionEventOutputParam& p, std::ostream* os) { *os << p.name; }

class EngineSessionEventOutputTest : public ::testing::TestWithParam<SessionEventOutputParam> {
protected:
    void SetUp() override {
        ServiceLocator::instance().provide<Logger>(std::make_shared<Logger>());
    }
};

TEST_P(EngineSessionEventOutputTest, LogsExpectedMessage) {
    Engine e;
    GetParam().setup(e);
    testing::internal::CaptureStdout();
    e.onSessionEvent(GetParam().makeEvent());
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find(GetParam().expectedOutput), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    Events, EngineSessionEventOutputTest,
    ::testing::Values(
        SessionEventOutputParam{
            "PrintRequested",
            [](Engine& e){ e.addVector({7, 8, 9}); },
            []{ SessionEvent ev; ev.type = SessionEventType::PrintRequested; return ev; },
            "[Engine] Data"
        },
        SessionEventOutputParam{
            "StrategyChangeRequested",
            [](Engine&){},
            []{ SessionEvent ev; ev.type = SessionEventType::StrategyChangeRequested;
                ev.strategyId = SortStrategyId::Descending; return ev; },
            "strategy"
        },
        SessionEventOutputParam{
            "SessionClosing",
            [](Engine& e){ e.start(); },
            []{ SessionEvent ev; ev.type = SessionEventType::SessionClosing; return ev; },
            "[Engine] Stop"
        }
    ),
    [](const ::testing::TestParamInfo<SessionEventOutputParam>& i){ return i.param.name; }
);
