#include <gtest/gtest.h>
#include <memory>

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

// ─── Owning GUI — unique_ptr<DummyGui> via std::default_delete specialisation ─
// These tests exercise the RAII wrapper used in main.cpp.
// Raw makeGUI/deleteGUI tests live in dummy_gui_tests (C-API simulation).

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

TEST_F(OwningGuiTest, MakeGUIReturnsUsableInstance) {
    std::unique_ptr<DummyGui> gui(makeGUI());
    ASSERT_NE(gui, nullptr);
}

TEST_F(OwningGuiTest, UniquePtrCallsDeleteGUIOnDestruction) {
    // Verify the object can be created and destroyed through unique_ptr
    // without crashing — default_delete<DummyGui> routes through deleteGUI.
    testing::internal::CaptureStdout();
    {
        std::unique_ptr<DummyGui> gui(makeGUI());
        ASSERT_NE(gui, nullptr);
    } // deleteGUI called here via default_delete
    testing::internal::GetCapturedStdout();
    SUCCEED();
}

TEST_F(OwningGuiTest, ConfiguratorWorksWithDereferencedUniquePtr) {
    std::unique_ptr<DummyGui> gui(makeGUI());
    Configurator cfg;
    cfg.configureGui(*gui, session_);

    testing::internal::CaptureStdout();
    gui->clickAddVector({1, 2, 3});
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("GUI wants to add vector"), std::string::npos);
}

TEST_F(OwningGuiTest, ResetTransfersOwnershipAndDestroysGui) {
    std::unique_ptr<DummyGui> gui(makeGUI());
    Configurator cfg;
    cfg.configureGui(*gui, session_);

    testing::internal::CaptureStdout();
    gui.reset();  // deleteGUI fires here
    EXPECT_EQ(gui, nullptr);
    testing::internal::GetCapturedStdout();
}

TEST_F(OwningGuiTest, MoveOwnershipPreservesUsability) {
    std::unique_ptr<DummyGui> gui(makeGUI());
    Configurator cfg;
    cfg.configureGui(*gui, session_);

    std::unique_ptr<DummyGui> moved = std::move(gui);
    EXPECT_EQ(gui, nullptr);
    ASSERT_NE(moved, nullptr);

    testing::internal::CaptureStdout();
    moved->clickPrintData();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("GUI wants to print"), std::string::npos);
}
