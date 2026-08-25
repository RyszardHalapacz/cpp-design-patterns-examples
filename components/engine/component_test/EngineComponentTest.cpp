#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "patterns/engine/Engine.hpp"
#include "patterns/historian/IHistorian.hpp"
#include "patterns/strategy/ISortStrategyFactory.hpp"
#include "patterns/observer/SessionEvent.hpp"
#include "patterns/strategy/SortStrategyFactory.hpp"
#include "patterns/services/ServiceLocator.hpp"
#include "patterns/services/Logger.hpp"

using namespace patterns::historian;
using namespace patterns::strategy;
using namespace patterns::observer;
using namespace patterns::services;

// ─── HistorianSpy ─────────────────────────────────────────────────────────────
// Implements IHistorian — same interface as the real EngineHistorian.
// Records every call so tests can assert on what Engine reported.

class HistorianSpy : public IHistorian {
public:
    void recordCommand(const CommandHistory& cmd) override {
        commands.push_back(cmd.commandName);
    }
    void publishSnapshot(const EngineSnapshot& snap) override {
        snapshots.push_back(snap);
    }

    std::vector<std::string>    commands;
    std::vector<EngineSnapshot> snapshots;
};

// ─── FactorySpy ──────────────────────────────────────────────────────────────
// Implements ISortStrategyFactory — same interface as the real SortStrategyFactory.
// Delegates to the real factory; records which strategy ids were requested.

class FactorySpy : public ISortStrategyFactory {
public:
    [[nodiscard]] std::expected<std::unique_ptr<ISortStrategy>, std::string>
    create(SortStrategyId id) override {
        requestedIds.push_back(id);
        return real_.create(id);
    }

    std::vector<SortStrategyId> requestedIds;

private:
    SortStrategyFactory real_;
};

// ─── EngineDriver ─────────────────────────────────────────────────────────────
// Holds a reference to Engine and exposes a table of signals.
// owner_ is typed as ::testing::Test* so that signal lambdas can cross-cast it
// to the specific stub type (HistorianSpy*, FactorySpy*) they want to observe.
// The cast succeeds only when the concrete fixture inherits from that stub.

class EngineDriver {
public:
    using Signal = std::pair<std::string, std::function<void()>>;

    EngineDriver(patterns::engine::Engine& engine, ::testing::Test* owner);

    void run() {
        for (auto& [name, fn] : signals)
            fn();
    }

    std::vector<Signal> signals;

private:
    patterns::engine::Engine& engine_;
    ::testing::Test*           owner_;
};

// ─── Fixture ──────────────────────────────────────────────────────────────────
// Inherits from HistorianSpy and FactorySpy — so this IS the spy and the stub.
// Non-owning shared_ptrs (spyKeeper_, factoryKeeper_) keep the engine's
// weak_ptrs alive for the duration of the test.

class EngineComponentTest : public ::testing::Test, public HistorianSpy, public FactorySpy {
protected:
    void SetUp() override {
        [[maybe_unused]] auto r = ServiceLocator::instance().provide<Logger>(
            std::make_shared<Logger>());

        engine_ = std::make_shared<patterns::engine::Engine>();

        spyKeeper_     = std::shared_ptr<HistorianSpy>(this, [](auto*) {});
        factoryKeeper_ = std::shared_ptr<FactorySpy>(this, [](auto*) {});

        engine_->setHistorian(spyKeeper_);
        engine_->setFactory(factoryKeeper_);
    }

    std::shared_ptr<patterns::engine::Engine> engine_;
    std::shared_ptr<HistorianSpy>             spyKeeper_;
    std::shared_ptr<FactorySpy>              factoryKeeper_;
};

// ─── EngineDriver constructor ─────────────────────────────────────────────────
// Defined here — after EngineComponentTest is complete — so that the
// cross-casts in signal lambdas operate on fully-defined types.

EngineDriver::EngineDriver(patterns::engine::Engine& engine, ::testing::Test* owner)
    : engine_(engine), owner_(owner)
{
    signals = {
        // ── addVector ────────────────────────────────────────────────────────
        // receive: forward VectorAdded event to Engine
        { "receiveAddVector",
          [this] {
              engine_.onSessionEvent({SessionEventType::VectorAdded, {1, 2, 3}});
          }
        },
        // send: Engine called addVector → historian recorded "addVector"
        { "sendAddVector",
          [this] {
              if (auto* spy = dynamic_cast<HistorianSpy*>(owner_)) {
                  ASSERT_FALSE(spy->commands.empty());
                  ASSERT_EQ(spy->commands.back(), "addVector");
              }
          }
        },

        // ── sortVector ───────────────────────────────────────────────────────
        // receive: forward SortRequested event to Engine (index 0)
        { "receiveSortVector",
          [this] {
              engine_.onSessionEvent({SessionEventType::SortRequested, {}, 0});
          }
        },
        // send: Engine called sortVector → historian recorded "sortVector"
        { "sendSortVector",
          [this] {
              if (auto* spy = dynamic_cast<HistorianSpy*>(owner_)) {
                  ASSERT_FALSE(spy->commands.empty());
                  ASSERT_EQ(spy->commands.back(), "sortVector");
              }
          }
        },

        // ── setSortStrategy ──────────────────────────────────────────────────
        // receive: forward StrategyChangeRequested event (Descending) to Engine
        { "receiveSetStrategy",
          [this] {
              engine_.onSessionEvent(
                  {SessionEventType::StrategyChangeRequested, {}, 0, SortStrategyId::Descending});
          }
        },
        // send: Engine asked factory for Descending strategy
        { "sendSetStrategy",
          [this] {
              if (auto* stub = dynamic_cast<FactorySpy*>(owner_)) {
                  ASSERT_FALSE(stub->requestedIds.empty());
                  ASSERT_EQ(stub->requestedIds.back(), SortStrategyId::Descending);
              }
          }
        },

        // ── publishSnapshot ──────────────────────────────────────────────────
        // receive: Engine publishes its current snapshot
        { "receiveSnapshot",
          [this] {
              engine_.publishSnapshot();
          }
        },
        // send: historian received at least one snapshot
        { "sendSnapshot",
          [this] {
              if (auto* spy = dynamic_cast<HistorianSpy*>(owner_)) {
                  ASSERT_FALSE(spy->snapshots.empty());
                  // no back() call — emptiness check is sufficient here
              }
          }
        },
    };
}

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_F(EngineComponentTest, RunExecutesAllSignals) {
    EngineDriver driver(*engine_, this);
    driver.run();

    EXPECT_FALSE(commands.empty());
    EXPECT_FALSE(requestedIds.empty());
    EXPECT_FALSE(snapshots.empty());
}
