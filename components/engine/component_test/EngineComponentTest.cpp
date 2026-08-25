#include <gtest/gtest.h>
#include <format>
#include <iostream>
#include <memory>
#include <sstream>
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

// ─── Endpoint ─────────────────────────────────────────────────────────────────
enum class Endpoint { Driver, Engine, Historian, Factory };

// ─── SequenceLog ──────────────────────────────────────────────────────────────
// Prints ASCII sequence-diagram rows with fixed lifeline columns.
//
// Fixed column layout:
//   col 0        col 36        col 72            col 92
//   [Driver]     [Engine]      [HistorianSpy]    # comment
//                [Engine]      [FactorySpy]      # comment
//
// Invariant: [Engine] always starts at kEngineCol regardless of direction.

class SequenceLog {
public:
    static constexpr int kDriverCol   =  0;
    static constexpr int kEngineCol   = 36;
    static constexpr int kObserverCol = 72;
    static constexpr int kCommentCol  = 92;

    // Draws one sequence-diagram row.
    // `captured` is the stdout produced during the signal; lines appear after '#'.
    static void logFlow(Endpoint           from,
                        Endpoint           to,
                        const std::string& signal,
                        const std::string& captured = {})
    {
        int  fromC = colOf(from);
        int  toC   = colOf(to);
        auto fromL = std::string(labelOf(from));
        auto toL   = std::string(labelOf(to));

        std::string line;

        if (fromC <= toC) {
            // ── left-to-right: [from] ---signal{fills}> [to] ────────────────
            // Arrow format: " ---{signal}{fills}> " = 6 + signal + fills chars
            int arrowLen = toC - (fromC + static_cast<int>(fromL.size()));
            int fills    = std::max(arrowLen - 6 - static_cast<int>(signal.size()), 0);
            std::string arrow = std::format(" ---{}{}>", signal, std::string(fills, '-')) + " ";
            line = std::format("{}{}{}{}", spaces(fromC), fromL, arrow, toL);
        } else {
            // ── right-to-left: [to] <---{signal}{fills} [from] ──────────────
            // Arrow format: " <---{signal}{fills} " = 6 + signal + fills chars
            int arrowLen = fromC - (toC + static_cast<int>(toL.size()));
            int fills    = std::max(arrowLen - 6 - static_cast<int>(signal.size()), 0);
            std::string arrow = std::format(" <---{}{} ", signal, std::string(fills, '-'));
            line = std::format("{}{}{}{}", spaces(toC), toL, arrow, fromL);
        }

        // ── split captured stdout into remark lines ───────────────────────────
        std::vector<std::string> remarks;
        {
            std::istringstream iss(captured);
            std::string ln;
            while (std::getline(iss, ln))
                if (!ln.empty()) remarks.push_back(ln);
        }

        // First remark on the same row; extra remarks on continuation rows.
        if (!remarks.empty()) {
            int gap = std::max(kCommentCol - static_cast<int>(line.size()), 2);
            line += std::format("{:<{}}# {}", "", gap, remarks[0]);
        }
        std::cout << line << '\n';

        for (std::size_t i = 1; i < remarks.size(); ++i)
            std::cout << std::format("{:<{}}# {}\n", "", kCommentCol, remarks[i]);
    }

private:
    static std::string spaces(int n) { return std::string(std::max(n, 0), ' '); }

    static constexpr std::string_view labelOf(Endpoint e) noexcept {
        switch (e) {
            case Endpoint::Driver:    return "[Driver]";
            case Endpoint::Engine:    return "[Engine]";
            case Endpoint::Historian: return "[HistorianSpy]";
            case Endpoint::Factory:   return "[FactorySpy]";
        }
        std::unreachable();
    }

    static constexpr int colOf(Endpoint e) noexcept {
        switch (e) {
            case Endpoint::Driver:    return kDriverCol;
            case Endpoint::Engine:    return kEngineCol;
            case Endpoint::Historian: return kObserverCol;
            case Endpoint::Factory:   return kObserverCol;
        }
        std::unreachable();
    }
};

// ─── HistorianSpy ─────────────────────────────────────────────────────────────
// Implements IHistorian — same interface as the real EngineHistorian.
// Records every call so tests can assert on what Engine reported.

class HistorianSpy : public IHistorian {
public:
    void recordCommand(const CommandHistory& cmd) override {
        commands.push_back(cmd);
    }
    void publishSnapshot(const EngineSnapshot& snap) override {
        snapshots.push_back(snap);
    }

    std::vector<CommandHistory> commands;
    std::vector<EngineSnapshot> snapshots;
};

// ─── FactorySpy ───────────────────────────────────────────────────────────────
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
// to the specific spy type they want to observe.
// The cast succeeds only when the concrete fixture inherits from that spy.

class EngineDriver {
public:
    struct Signal {
        std::string           name;
        Endpoint              from;
        Endpoint              to;
        std::function<bool()> fn;  // returns false → channel inactive, skip diagram
    };

    EngineDriver(patterns::engine::Engine& engine, ::testing::Test* owner);

    void run() {
        for (auto& [name, from, to, fn] : signals) {
            testing::internal::CaptureStdout();
            bool active = fn();
            std::string captured = testing::internal::GetCapturedStdout();
            if (active)
                SequenceLog::logFlow(from, to, name, captured);
        }
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

class EngineComponentTest : public ::testing::Test , public HistorianSpy, public FactorySpy {
protected:
    void SetUp() override {
        [[maybe_unused]] auto r = ServiceLocator::instance().provide<Logger>(
            std::make_shared<Logger>());

        engine_ = std::make_shared<patterns::engine::Engine>();

        if (auto* spy = dynamic_cast<HistorianSpy*>(this)) {
            spyKeeper_ = std::shared_ptr<HistorianSpy>(spy, [](auto*) {});
            engine_->setHistorian(spyKeeper_);
        }
        if (auto* spy = dynamic_cast<FactorySpy*>(this)) {
            factoryKeeper_ = std::shared_ptr<FactorySpy>(spy, [](auto*) {});
            engine_->setFactory(factoryKeeper_);
        }
    }

    std::shared_ptr<patterns::engine::Engine> engine_;
    std::shared_ptr<HistorianSpy>             spyKeeper_;
    std::shared_ptr<FactorySpy>               factoryKeeper_;
};

// ─── EngineDriver constructor ─────────────────────────────────────────────────
// Defined here — after EngineComponentTest is complete — so that the
// cross-casts in signal lambdas operate on fully-defined types.

EngineDriver::EngineDriver(patterns::engine::Engine& engine, ::testing::Test* owner)
    : engine_(engine), owner_(owner)
{
    signals = {
        // ── addVector ────────────────────────────────────────────────────────
        { "receiveAddVector", Endpoint::Driver, Endpoint::Engine,
          [this] {
              engine_.onSessionEvent({SessionEventType::VectorAdded, {1, 2, 3}});
              return true;
          }
        },
        // send: 1. historian recorded the command name
        //       2. historian recorded the exact vector payload
        //       3. snapshot confirms the vector was actually stored (vectorCount == 1)
        { "sendAddVector", Endpoint::Engine, Endpoint::Historian,
          [this] {
              auto* spy = dynamic_cast<HistorianSpy*>(owner_);
              if (!spy) return false;
              EXPECT_FALSE(spy->commands.empty());
              EXPECT_EQ(spy->commands.back().commandName, "addVector");
              EXPECT_EQ(spy->commands.back().data, (std::vector<int>{1, 2, 3}));
              engine_.publishSnapshot();
              EXPECT_FALSE(spy->snapshots.empty());
              EXPECT_EQ(spy->snapshots.back().vectorCount, std::size_t{1});
              return true;
          }
        },

        // ── sortVector ───────────────────────────────────────────────────────
        { "receiveSortVector", Endpoint::Driver, Endpoint::Engine,
          [this] {
              engine_.onSessionEvent({SessionEventType::SortRequested, {}, 0});
              return true;
          }
        },
        { "sendSortVector", Endpoint::Engine, Endpoint::Historian,
          [this] {
              auto* spy = dynamic_cast<HistorianSpy*>(owner_);
              if (!spy) return false;
              EXPECT_FALSE(spy->commands.empty());
              EXPECT_EQ(spy->commands.back().commandName, "sortVector");
              return true;
          }
        },

        // ── setSortStrategy ──────────────────────────────────────────────────
        { "receiveSetStrategy", Endpoint::Driver, Endpoint::Engine,
          [this] {
              engine_.onSessionEvent(
                  {SessionEventType::StrategyChangeRequested, {}, 0, SortStrategyId::Descending});
              return true;
          }
        },
        { "sendSetStrategy", Endpoint::Engine, Endpoint::Factory,
          [this] {
              auto* stub = dynamic_cast<FactorySpy*>(owner_);
              if (!stub) return false;
              EXPECT_FALSE(stub->requestedIds.empty());
              EXPECT_EQ(stub->requestedIds.back(), SortStrategyId::Descending);
              return true;
          }
        },

        // ── publishSnapshot ──────────────────────────────────────────────────
        { "receiveSnapshot", Endpoint::Driver, Endpoint::Engine,
          [this] {
              engine_.publishSnapshot();
              return true;
          }
        },
        { "sendSnapshot", Endpoint::Engine, Endpoint::Historian,
          [this] {
              auto* spy = dynamic_cast<HistorianSpy*>(owner_);
              if (!spy) return false;
              EXPECT_FALSE(spy->snapshots.empty());
              return true;
          }
        },
    };
}

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_F(EngineComponentTest, RunExecutesAllSignals) {
    EngineDriver driver(*engine_, this);
    driver.run();

    // Check only channels that are active (base class present).
    // Removing a base class disables the channel — the cast returns nullptr.
    if (auto* spy = dynamic_cast<HistorianSpy*>(this)) {
        EXPECT_FALSE(spy->commands.empty());
        EXPECT_FALSE(spy->snapshots.empty());
    }
    if (auto* spy = dynamic_cast<FactorySpy*>(this)) {
        EXPECT_FALSE(spy->requestedIds.empty());
    }
}
