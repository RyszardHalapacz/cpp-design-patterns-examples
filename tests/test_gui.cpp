#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "patterns/gui/Command.hpp"
#include "patterns/gui/CommandBatchBuilder.hpp"
#include "patterns/gui/DummyGui.hpp"
#include "patterns/config/Configurator.hpp"
#include "patterns/session/SessionManagement.hpp"
#include "patterns/engine/Engine.hpp"
#include "patterns/services/ServiceLocator.hpp"
#include "patterns/services/Logger.hpp"
#include "patterns/strategy/SortStrategyId.hpp"

using namespace patterns::gui;
using namespace patterns::config;
using namespace patterns::session;
using namespace patterns::strategy;
using namespace patterns::services;

class GuiTest : public ::testing::Test {
protected:
    void SetUp() override {
        ServiceLocator::instance().provide<Logger>(std::make_shared<Logger>());
        // Suppress all output by default
        testing::internal::CaptureStdout();
        engine_  = std::make_unique<patterns::engine::Engine>();
        session_ = std::make_unique<SessionManagement>();
        session_->connectToEngine(*engine_);
        session_->openSession();
        session_->setAllowedStrategies({SortStrategyId::Ascending,
                                        SortStrategyId::Descending,
                                        SortStrategyId::Bubble});
        testing::internal::GetCapturedStdout();
    }

    std::unique_ptr<patterns::engine::Engine> engine_;
    std::unique_ptr<SessionManagement>        session_;
};

// ─── CommandBatchBuilder — parameterized single-command tests ─────────────────

struct SingleCommandParam {
    std::string                              name;
    CommandType                              expectedType;
    std::function<void(CommandBatchBuilder&)> setup;
    std::vector<int>                         expectedData  = {};
    size_t                                   expectedIndex = 0;
};

void PrintTo(const SingleCommandParam& p, std::ostream* os) { *os << p.name; }

class CommandBatchBuilderCommandTest
    : public ::testing::TestWithParam<SingleCommandParam> {};

TEST_P(CommandBatchBuilderCommandTest, ProducesSingleCommandBatch) {
    const auto& p = GetParam();
    CommandBatchBuilder builder;
    p.setup(builder);
    CommandBatch batch = builder.build();

    ASSERT_EQ(batch.size(), 1u);
    EXPECT_EQ(batch[0].type, p.expectedType);
    if (!p.expectedData.empty()) {
        EXPECT_EQ(batch[0].vectorData, p.expectedData);
    }
    if (p.expectedIndex != 0) {
        EXPECT_EQ(batch[0].index, p.expectedIndex);
    }
}

INSTANTIATE_TEST_SUITE_P(
    Commands,
    CommandBatchBuilderCommandTest,
    ::testing::Values(
        SingleCommandParam{
            "AddVector",
            CommandType::AddVector,
            [](CommandBatchBuilder& b){ b.addVector({1, 2, 3}); },
            {1, 2, 3}, 0
        },
        SingleCommandParam{
            "SortVector",
            CommandType::SortVector,
            [](CommandBatchBuilder& b){ b.sortVector(5); },
            {}, 5
        },
        SingleCommandParam{
            "PrintData",
            CommandType::PrintData,
            [](CommandBatchBuilder& b){ b.printData(); },
            {}, 0
        }
    ),
    [](const ::testing::TestParamInfo<SingleCommandParam>& info) {
        return info.param.name;
    }
);

// ─── CommandBatchBuilder — remaining non-parameterized tests ──────────────────

TEST(CommandBatchBuilderTest, BuildEmptyBatch) {
    CommandBatchBuilder builder;
    CommandBatch batch = builder.build();
    EXPECT_TRUE(batch.empty());
}

TEST(CommandBatchBuilderTest, FluentChaining) {
    CommandBatchBuilder builder;
    CommandBatch batch = builder
        .addVector({1, 2})
        .sortVector(0)
        .printData()
        .build();
    EXPECT_EQ(batch.size(), 3u);
    EXPECT_EQ(batch[0].type, CommandType::AddVector);
    EXPECT_EQ(batch[1].type, CommandType::SortVector);
    EXPECT_EQ(batch[2].type, CommandType::PrintData);
}

TEST(CommandBatchBuilderTest, BuildClearsBuilderState) {
    CommandBatchBuilder builder;
    builder.addVector({1, 2, 3});
    builder.build();
    CommandBatch second = builder.build();
    EXPECT_TRUE(second.empty());
}

// ─── DummyGui ────────────────────────────────────────────────────────────────

TEST_F(GuiTest, ClickAddVectorCallsSession) {
    DummyGui gui;
    Configurator cfg;
    cfg.configureGui(gui, *session_);

    testing::internal::CaptureStdout();
    gui.clickAddVector({7, 8, 9});
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("GUI wants to add vector"), std::string::npos);
}

TEST_F(GuiTest, ClickSortVectorCallsSession) {
    DummyGui gui;
    Configurator cfg;
    cfg.configureGui(gui, *session_);

    testing::internal::CaptureStdout();
    session_->addVectorFromGui({3, 1, 2});
    gui.clickSortVector(0);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("GUI wants to sort"), std::string::npos);
}

TEST_F(GuiTest, ClickPrintDataCallsSession) {
    DummyGui gui;
    Configurator cfg;
    cfg.configureGui(gui, *session_);

    testing::internal::CaptureStdout();
    gui.clickPrintData();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("GUI wants to print"), std::string::npos);
}

TEST_F(GuiTest, ClickSetSortStrategyCallsSession) {
    DummyGui gui;
    Configurator cfg;
    cfg.configureGui(gui, *session_);

    testing::internal::CaptureStdout();
    gui.clickSetSortStrategy(SortStrategyId::Descending);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("sort strategy"), std::string::npos);
}

TEST_F(GuiTest, QueueAndBuildBatch) {
    DummyGui gui;
    gui.queueAddVector({1, 2, 3});
    gui.queueSortVector(0);
    gui.queuePrintData();

    CommandBatch batch = gui.buildBatch();
    ASSERT_EQ(batch.size(), 3u);
    EXPECT_EQ(batch[0].type, CommandType::AddVector);
    EXPECT_EQ(batch[1].type, CommandType::SortVector);
    EXPECT_EQ(batch[2].type, CommandType::PrintData);
}

TEST_F(GuiTest, BuildBatchClearsQueue) {
    DummyGui gui;
    gui.queueAddVector({1, 2});
    gui.buildBatch();
    CommandBatch second = gui.buildBatch();
    EXPECT_TRUE(second.empty());
}

TEST_F(GuiTest, FlushBatchExecutesCommands) {
    DummyGui gui;
    Configurator cfg;
    cfg.configureGui(gui, *session_);

    gui.queueAddVector({5, 3, 1});

    testing::internal::CaptureStdout();
    gui.flushBatch();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("command batch"), std::string::npos);
}

// ─── Configurator ────────────────────────────────────────────────────────────

TEST_F(GuiTest, ConfigureGuiConnectsAllFunctions) {
    DummyGui gui;
    Configurator cfg;
    cfg.configureGui(gui, *session_);

    // All click methods should work without crashing
    testing::internal::CaptureStdout();
    EXPECT_NO_THROW(gui.clickAddVector({1}));
    EXPECT_NO_THROW(gui.clickSortVector(0));
    EXPECT_NO_THROW(gui.clickPrintData());
    EXPECT_NO_THROW(gui.clickSetSortStrategy(SortStrategyId::Ascending));
    testing::internal::GetCapturedStdout();
}

TEST_F(GuiTest, ConfigureAllowedStrategiesLimitsChoices) {
    Configurator cfg;
    cfg.configureAllowedStrategies(*session_, {SortStrategyId::Ascending});

    testing::internal::CaptureStdout();
    session_->setSortStrategyFromGui(SortStrategyId::Bubble);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("not allowed"), std::string::npos);
}

TEST_F(GuiTest, ConfigureAllowedStrategiesPermitsAllowed) {
    Configurator cfg;
    cfg.configureAllowedStrategies(*session_, {SortStrategyId::Bubble});

    testing::internal::CaptureStdout();
    session_->setSortStrategyFromGui(SortStrategyId::Bubble);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(out.find("not allowed"), std::string::npos);
}
