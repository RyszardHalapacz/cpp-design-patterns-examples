#include <gtest/gtest.h>
#include <memory>

#include "patterns/gui/IGui.hpp"
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

// ─── Owning GUI — unique_ptr<DummyGuiAdapter> / unique_ptr<IGui> ─────────────
// DummyGuiAdapter owns the underlying DummyGui (C-style API) internally.
// Application stores unique_ptr<IGui> — the adapter is the owned entity.

class OwningGuiTest : public ::testing::Test {
protected:
    void SetUp() override {
        ServiceLocator::instance().provide<Logger>(std::make_shared<Logger>());
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

TEST_F(OwningGuiTest, AdapterCreatesUsableInstance) {
    auto gui = std::make_unique<DummyGuiAdapter>();
    ASSERT_NE(gui, nullptr);
}

TEST_F(OwningGuiTest, UniquePtrDestroysAdapterOnReset) {
    // Adapter manages its internal DummyGui — verify no crash on destruction.
    testing::internal::CaptureStdout();
    {
        auto gui = std::make_unique<DummyGuiAdapter>();
        ASSERT_NE(gui, nullptr);
    } // ~DummyGuiAdapter calls deleteGUI internally
    testing::internal::GetCapturedStdout();
    SUCCEED();
}

TEST_F(OwningGuiTest, ConfiguratorWorksWithDereferencedUniquePtr) {
    auto gui = std::make_unique<DummyGuiAdapter>();
    Configurator cfg;
    cfg.configureGui(*gui, session_);

    testing::internal::CaptureStdout();
    gui->clickAddVector({1, 2, 3});
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("GUI wants to add vector"), std::string::npos);
}

TEST_F(OwningGuiTest, ResetTransfersOwnershipAndDestroysAdapter) {
    auto gui = std::make_unique<DummyGuiAdapter>();
    Configurator cfg;
    cfg.configureGui(*gui, session_);

    testing::internal::CaptureStdout();
    gui.reset();  // ~DummyGuiAdapter fires here
    EXPECT_EQ(gui, nullptr);
    testing::internal::GetCapturedStdout();
}

TEST_F(OwningGuiTest, MoveOwnershipPreservesUsability) {
    auto gui = std::make_unique<DummyGuiAdapter>();
    Configurator cfg;
    cfg.configureGui(*gui, session_);

    std::unique_ptr<DummyGuiAdapter> moved = std::move(gui);
    EXPECT_EQ(gui, nullptr);
    ASSERT_NE(moved, nullptr);

    testing::internal::CaptureStdout();
    moved->clickPrintData();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("GUI wants to print"), std::string::npos);
}

TEST_F(OwningGuiTest, AdapterUsableAsIGui) {
    // Application stores unique_ptr<IGui> — verify virtual dispatch works
    std::unique_ptr<IGui> gui = std::make_unique<DummyGuiAdapter>();
    Configurator cfg;
    cfg.configureGui(static_cast<DummyGuiAdapter&>(*gui), session_);

    testing::internal::CaptureStdout();
    gui->clickAddVector({10, 20});
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("GUI wants to add vector"), std::string::npos);
}
