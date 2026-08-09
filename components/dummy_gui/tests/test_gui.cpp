#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "patterns/gui/ICommand.hpp"
#include "patterns/gui/CommandBatchBuilder.hpp"
#include "patterns/gui/DummyGuiAdapter.hpp"
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
        engine_  = std::make_shared<patterns::engine::Engine>();
        session_ = std::make_shared<SessionManagement>();
        session_->connectToEngine(engine_);
        session_->openSession();
        session_->setAllowedStrategies({SortStrategyId::Ascending,
                                        SortStrategyId::Descending,
                                        SortStrategyId::Bubble});
        testing::internal::GetCapturedStdout();
    }

    std::shared_ptr<patterns::engine::Engine> engine_;
    std::shared_ptr<SessionManagement>        session_;
};

// ─── CommandBatchBuilder ──────────────────────────────────────────────────────

TEST(CommandBatchBuilderTest, BuildEmptyBatch) {
    CommandBatchBuilder builder;
    EXPECT_TRUE(builder.build().empty());
}

TEST(CommandBatchBuilderTest, AddVectorCommandExecutesCallback) {
    bool called = false;
    std::vector<int> received;
    CommandBatchBuilder builder;
    builder.addVector([&](const std::vector<int>& v){ called = true; received = v; }, {1, 2, 3});
    auto batch = builder.build();
    ASSERT_EQ(batch.size(), 1u);
    batch[0]->execute();
    EXPECT_TRUE(called);
    EXPECT_EQ(received, (std::vector<int>{1, 2, 3}));
}

TEST(CommandBatchBuilderTest, SortVectorCommandExecutesCallback) {
    size_t receivedIndex = 0;
    CommandBatchBuilder builder;
    builder.sortVector([&](size_t i){ receivedIndex = i; }, 5);
    auto batch = builder.build();
    ASSERT_EQ(batch.size(), 1u);
    batch[0]->execute();
    EXPECT_EQ(receivedIndex, 5u);
}

TEST(CommandBatchBuilderTest, PrintDataCommandExecutesCallback) {
    bool called = false;
    CommandBatchBuilder builder;
    builder.printData([&](){ called = true; });
    auto batch = builder.build();
    ASSERT_EQ(batch.size(), 1u);
    batch[0]->execute();
    EXPECT_TRUE(called);
}

TEST(CommandBatchBuilderTest, FluentChaining) {
    CommandBatchBuilder builder;
    auto batch = builder
        .addVector([](const std::vector<int>&){}, {1, 2})
        .sortVector([](size_t){}, 0)
        .printData([](){})
        .build();
    EXPECT_EQ(batch.size(), 3u);
}

TEST(CommandBatchBuilderTest, BuildClearsBuilderState) {
    CommandBatchBuilder builder;
    builder.addVector([](const std::vector<int>&){}, {1, 2, 3});
    builder.build();
    EXPECT_TRUE(builder.build().empty());
}

// ─── DummyGuiAdapter ─────────────────────────────────────────────────────────

TEST_F(GuiTest, ClickAddVectorCallsSession) {
    DummyGuiAdapter gui;
    Configurator cfg;
    cfg.configureGui(gui, session_);

    testing::internal::CaptureStdout();
    gui.clickAddVector({7, 8, 9});
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("GUI wants to add vector"), std::string::npos);
}

TEST_F(GuiTest, ClickSortVectorCallsSession) {
    DummyGuiAdapter gui;
    Configurator cfg;
    cfg.configureGui(gui, session_);

    testing::internal::CaptureStdout();
    session_->addVectorFromGui({3, 1, 2});
    gui.clickSortVector(0);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("GUI wants to sort"), std::string::npos);
}

TEST_F(GuiTest, ClickPrintDataCallsSession) {
    DummyGuiAdapter gui;
    Configurator cfg;
    cfg.configureGui(gui, session_);

    testing::internal::CaptureStdout();
    gui.clickPrintData();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("GUI wants to print"), std::string::npos);
}

TEST_F(GuiTest, ClickSetSortStrategyCallsSession) {
    DummyGuiAdapter gui;
    Configurator cfg;
    cfg.configureGui(gui, session_);

    testing::internal::CaptureStdout();
    gui.clickSetSortStrategy(SortStrategyId::Descending);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("sort strategy"), std::string::npos);
}

TEST_F(GuiTest, QueueAndBuildBatch) {
    DummyGuiAdapter gui;
    Configurator cfg;
    cfg.configureGui(gui, session_);

    gui.queueAddVector({1, 2, 3});
    gui.queueSortVector(0);
    gui.queuePrintData();

    testing::internal::CaptureStdout();
    CommandBatch batch = gui.buildBatch();
    testing::internal::GetCapturedStdout();

    ASSERT_EQ(batch.size(), 3u);
}

TEST_F(GuiTest, BuildBatchClearsQueue) {
    DummyGuiAdapter gui;
    Configurator cfg;
    cfg.configureGui(gui, session_);

    testing::internal::CaptureStdout();
    gui.queueAddVector({1, 2});
    gui.buildBatch();
    CommandBatch second = gui.buildBatch();
    testing::internal::GetCapturedStdout();

    EXPECT_TRUE(second.empty());
}

TEST_F(GuiTest, FlushBatchExecutesCommands) {
    DummyGuiAdapter gui;
    Configurator cfg;
    cfg.configureGui(gui, session_);

    gui.queueAddVector({5, 3, 1});

    testing::internal::CaptureStdout();
    gui.flushBatch();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("command batch"), std::string::npos);
}

// ─── DummyGuiAdapter — no access paths ───────────────────────────────────────

TEST_F(GuiTest, ClickAddVectorWithoutAccessLogsError) {
    DummyGuiAdapter gui; // no configureGui — functions not connected
    testing::internal::CaptureStdout();
    gui.clickAddVector({1, 2});
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("No access to AddVector"), std::string::npos);
}

TEST_F(GuiTest, ClickSortVectorWithoutAccessLogsError) {
    DummyGuiAdapter gui;
    testing::internal::CaptureStdout();
    gui.clickSortVector(0);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("No access to SortVector"), std::string::npos);
}

TEST_F(GuiTest, ClickPrintDataWithoutAccessLogsError) {
    DummyGuiAdapter gui;
    testing::internal::CaptureStdout();
    gui.clickPrintData();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("No access to PrintData"), std::string::npos);
}

TEST_F(GuiTest, ClickSetSortStrategyWithoutAccessLogsError) {
    DummyGuiAdapter gui;
    testing::internal::CaptureStdout();
    gui.clickSetSortStrategy(SortStrategyId::Ascending);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("No access to SetSortStrategy"), std::string::npos);
}

TEST_F(GuiTest, QueueWithoutAccessLogsError) {
    DummyGuiAdapter gui; // no configureGui — callbacks not connected
    testing::internal::CaptureStdout();
    gui.queueAddVector({1, 2, 3});
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("No access to AddVector"), std::string::npos);
}

TEST_F(GuiTest, ClickWithExpiredSessionLogsError) {
    DummyGuiAdapter gui;
    {
        // Connect to a temporary session, then let it expire
        auto tempSession = std::make_shared<SessionManagement>();
        Configurator cfg;
        cfg.configureGui(gui, tempSession);
    } // tempSession destroyed — weak_ptr in gui is now expired

    testing::internal::CaptureStdout();
    gui.clickAddVector({1, 2, 3});
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("no longer exists"), std::string::npos);
}

// ─── Adapter wraps IGui ───────────────────────────────────────────────────────

TEST_F(GuiTest, AdapterImplementsIGui) {
    // Application holds IGui* — verify the adapter is usable through the interface
    std::unique_ptr<IGui> gui = std::make_unique<DummyGuiAdapter>();
    Configurator cfg;
    cfg.configureGui(static_cast<DummyGuiAdapter&>(*gui), session_);

    testing::internal::CaptureStdout();
    EXPECT_NO_THROW(gui->clickAddVector({1}));
    EXPECT_NO_THROW(gui->clickSortVector(0));
    EXPECT_NO_THROW(gui->clickPrintData());
    EXPECT_NO_THROW(gui->clickSetSortStrategy(SortStrategyId::Ascending));
    testing::internal::GetCapturedStdout();
}

// ─── Configurator ────────────────────────────────────────────────────────────

TEST_F(GuiTest, ConfigureGuiConnectsAllFunctions) {
    DummyGuiAdapter gui;
    Configurator cfg;
    cfg.configureGui(gui, session_);

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
