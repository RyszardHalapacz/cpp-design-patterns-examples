#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>  // EXPECT_NONFATAL_FAILURE
#include <any>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "patterns/engine/Engine.hpp"
#include "patterns/historian/IHistorian.hpp"
#include "patterns/services/ServiceLocator.hpp"
#include "patterns/services/Logger.hpp"
#include "patterns/strategy/SortStrategyId.hpp"
#include "patterns/strategy/SortStrategyFactory.hpp"

#include "scenario/Signal.hpp"
#include "scenario/ScenarioVerifier.hpp"
#include "scenario/Spies.hpp"
#include "scenario/Scenarios.hpp"
#include "EngineDriver.hpp"

using namespace patterns::services;
using namespace patterns::strategy;
using namespace patterns::historian;

// ─── NullHistorian ────────────────────────────────────────────────────────────
// No-op IHistorian for fixtures that do not inherit HistorianSpy.
// Prevents null-dereference when Engine calls historian and no spy is wired.

struct NullHistorian : patterns::historian::IHistorian {
    void recordCommand(const patterns::historian::CommandHistory&) override {}
    void publishSnapshot(const patterns::historian::EngineSnapshot&) override {}
};

// ═══════════════════════════════════════════════════════════════════════════════
// ScenarioFrameworkTest
// ─────────────────────────────────────────────────────────────────────────────
// Tests the verification framework itself using EXPECT_NONFATAL_FAILURE.
// Verifies that ScenarioVerifier correctly detects every class of contract
// violation — without involving a real Engine.
//
// These are regression tests for the framework, not for Engine behaviour.
// ═══════════════════════════════════════════════════════════════════════════════

class ScenarioFrameworkTest : public ::testing::Test {
protected:
    ScenarioVerifier verifier_;

    // ── Helpers: build SignalDescriptors with real payload types ───────────────

    static SignalDescriptor historianCommand(std::string name,
                                             std::vector<int> data = {}) {
        return {Endpoint::Engine, Endpoint::Historian, "recordCommand",
                std::any{CommandHistory{std::move(name), std::move(data)}}};
    }

    static SignalDescriptor historianSnapshot(size_t vectorCount = 0) {
        EngineSnapshot snap;
        snap.vectorCount = vectorCount;
        return {Endpoint::Engine, Endpoint::Historian, "publishSnapshot",
                std::any{snap}};
    }

    static SignalDescriptor factoryCreate(SortStrategyId id) {
        return {Endpoint::Engine, Endpoint::Factory, "create", std::any{id}};
    }
};

// Actual signal arrives with no expectation registered → Unexpected signal.
TEST_F(ScenarioFrameworkTest, UnexpectedSignal_NoExpectation) {
    verifier_.setExpected({});
    EXPECT_NONFATAL_FAILURE(
        verifier_.report(historianCommand("addVector")),
        "Unexpected signal"
    );
}

// Expected signal never arrives → Signal not received.
TEST_F(ScenarioFrameworkTest, SignalNotReceived) {
    verifier_.setExpected({expectHistorianCommand("addVector")});
    EXPECT_NONFATAL_FAILURE(
        verifier_.verifyComplete(),
        "Signal not received"
    );
}

// Signals arrive in wrong order within a step.
// Expected: recordCommand then publishSnapshot.
// Received: publishSnapshot first.
TEST_F(ScenarioFrameworkTest, WrongOrder) {
    verifier_.setExpected({
        expectHistorianCommand("addVector"),
        expectHistorianSnapshot(),
    });
    EXPECT_NONFATAL_FAILURE(
        verifier_.report(historianSnapshot(0)),
        "Unexpected signal"
    );
}

// Correct signal name and endpoint, but wrong payload data.
TEST_F(ScenarioFrameworkTest, PayloadMismatch) {
    verifier_.setExpected({expectHistorianCommand("addVector", {{1, 2, 3}})});
    EXPECT_NONFATAL_FAILURE(
        verifier_.report(historianCommand("addVector", {9, 9, 9})),
        "payload mismatch"
    );
}

// Regression test for rev 1 bug: signal produced during Step 1 must not
// satisfy an expectation belonging to Step 2.
//
// Simulates: publishSnapshot sent during addVector processing (wrong step).
// Step 1 knows only about recordCommand, so publishSnapshot is Unexpected.
TEST_F(ScenarioFrameworkTest, SignalFromWrongStep_RegressionRev1) {
    // Step 1 scope: only recordCommand expected.
    verifier_.setExpected({expectHistorianCommand("addVector")});

    // recordCommand arrives → OK.
    verifier_.report(historianCommand("addVector"));

    // publishSnapshot arrives still inside Step 1 — no expectation for it.
    EXPECT_NONFATAL_FAILURE(
        verifier_.report(historianSnapshot(0)),
        "Unexpected signal"
    );
}

// ─── Mode 2 parity tests ──────────────────────────────────────────────────────
// Verify that ScenarioVerifier Mode 2 (beginStep / matchExpectation / finalizeStep)
// enforces the same strict contract as Mode 1 (setExpected / report / verifyComplete).
// These tests call the verifier API directly, without EngineDriver or a real Engine.

// Actual arrives with no matching expectation → finalizeStep reports Unexpected signal.
TEST_F(ScenarioFrameworkTest, Mode2_UnexpectedSignal) {
    verifier_.beginStep();
    verifier_.report(historianCommand("addVector"));
    verifier_.endStepCollection();
    EXPECT_NONFATAL_FAILURE(
        verifier_.finalizeStep(),
        "Unexpected signal"
    );
}

// No actual arrives but expectation is declared → Signal not received.
TEST_F(ScenarioFrameworkTest, Mode2_SignalNotReceived) {
    verifier_.beginStep();
    // no report() calls — stepActuals_ stays empty
    verifier_.endStepCollection();
    EXPECT_NONFATAL_FAILURE(
        verifier_.matchExpectation(expectHistorianCommand("addVector")),
        "Signal not received"
    );
}

// Two actuals arrive in order A, B. Expectation for B declared first → wrong order.
TEST_F(ScenarioFrameworkTest, Mode2_WrongOrder) {
    verifier_.beginStep();
    verifier_.report(historianCommand("addVector"));   // actual[0]
    verifier_.report(historianSnapshot(0));            // actual[1]
    verifier_.endStepCollection();
    // Request second actual before first:
    EXPECT_NONFATAL_FAILURE(
        verifier_.matchExpectation(expectHistorianSnapshot()),
        "Unexpected signal"
    );
}

// Correct signal name and endpoint but wrong payload data.
TEST_F(ScenarioFrameworkTest, Mode2_PayloadMismatch) {
    verifier_.beginStep();
    verifier_.report(historianCommand("addVector", {9, 9, 9}));
    verifier_.endStepCollection();
    EXPECT_NONFATAL_FAILURE(
        verifier_.matchExpectation(expectHistorianCommand("addVector", {{1, 2, 3}})),
        "payload mismatch"
    );
}

// ═══════════════════════════════════════════════════════════════════════════════
// EngineTestBase
// ─────────────────────────────────────────────────────────────────────────────
// Shared base for all Engine component test fixtures.
//
// SetUp():
//   Detects active channels via dynamic_cast on the concrete fixture type.
//   Builds ActiveChannels and stores it in channels_ for use by EngineDriver.
//   Wires the spy (if active) or a real/null implementation.
//
//   Engine stores collaborators via weak_ptr. To keep control blocks alive,
//   historianKeeper_ and factoryKeeper_ hold the shared_ptrs for the fixture's
//   entire lifetime.
//
// TearDown():
//   Resets engine_ before spy sub-objects are destroyed by base-class dtors.
//   Ensures Engine is gone while all control blocks and spy objects still live.
//
// Ownership of spy sub-objects:
//   Spies are base-class sub-objects of the concrete fixture (same lifetime).
//   Engine receives a non-owning shared_ptr (no-op deleter) stored in a keeper.
// ═══════════════════════════════════════════════════════════════════════════════

class EngineTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        [[maybe_unused]] auto r = ServiceLocator::instance().provide<Logger>(
            std::make_shared<Logger>());

        engine_ = std::make_shared<patterns::engine::Engine>();

        auto* h = dynamic_cast<HistorianSpy*>(this);
        auto* f = dynamic_cast<FactorySpy*>(this);

        // Result of dynamic_cast defines the fixture topology — stored in channels_
        // for both Engine wiring and EngineDriver expectation filtering.
        channels_ = {h != nullptr, f != nullptr};

        // ── Historian channel ─────────────────────────────────────────────────
        // Engine stores historian via weak_ptr; keeper extends control block lifetime.
        if (h) {
            h->attachVerifier(verifier_);
            historianKeeper_ =
                std::shared_ptr<patterns::historian::IHistorian>(h, [](auto*){});
            engine_->setHistorian(historianKeeper_);
        } else {
            historianKeeper_ = std::make_shared<NullHistorian>();
            engine_->setHistorian(historianKeeper_);
        }

        // ── Factory channel ───────────────────────────────────────────────────
        // Engine stores factory via weak_ptr; keeper extends control block lifetime.
        // setFactory calls factory->create(Ascending) internally.
        // verifier_ not yet armed (armed_=false) → call silently ignored.
        if (f) {
            f->attachVerifier(verifier_);
            factoryKeeper_ =
                std::shared_ptr<patterns::strategy::ISortStrategyFactory>(f, [](auto*){});
            engine_->setFactory(factoryKeeper_);
        } else {
            factoryKeeper_ =
                std::make_shared<patterns::strategy::SortStrategyFactory>();
            engine_->setFactory(factoryKeeper_);
        }
    }

    void TearDown() override {
        // Destroy Engine while keepers and spy sub-objects are still alive.
        // Engine holds weak_ptrs to collaborators — must be destroyed before
        // the control blocks (keepers) disappear.
        engine_.reset();
    }

    ScenarioVerifier                                          verifier_;
    std::shared_ptr<patterns::engine::Engine>                 engine_;
    std::shared_ptr<patterns::historian::IHistorian>          historianKeeper_;
    std::shared_ptr<patterns::strategy::ISortStrategyFactory> factoryKeeper_;
    ActiveChannels                                            channels_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// EngineComponentTest — both historian and factory channels active
// ─────────────────────────────────────────────────────────────────────────────
// dynamic_cast<HistorianSpy*>(this) → non-null → HistorianSpy wired
// dynamic_cast<FactorySpy*>(this)   → non-null → FactorySpy   wired
// channels_ = { historian: true, factory: true }
//
// Every Engine→Historian and Engine→Factory call must be declared in the scenario.
// No expectation is skipped by the channel filter.
// ═══════════════════════════════════════════════════════════════════════════════

class EngineComponentTest : public EngineTestBase,
                             public HistorianSpy,
                             public FactorySpy {};

// ─── Framework self-check (uses real Engine + EngineDriver) ──────────────────

// Expectation before the first Stimulus is a malformed scenario.
// The framework must report this rather than silently ignore it.
TEST_F(EngineComponentTest, MalformedScenario_ExpectationBeforeStimulus) {
    EngineDriver driver(*engine_, verifier_, channels_);
    EXPECT_NONFATAL_FAILURE(
        driver.run({
            expectHistorianCommand("addVector"),  // ← before any Stimulus
            receiveVectorAdded({1, 2, 3}),
        }),
        "Malformed scenario"
    );
}

// ─── Engine component contracts ───────────────────────────────────────────────

// Contract: receiveAddVector → historian.recordCommand("addVector", data)
TEST_F(EngineComponentTest, AddVector) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::AddVector({1, 2, 3}));
}

// Contract: receiveSortRequested → historian.recordCommand("sortVector")
// Vector seeded before scenario — pre-run calls are not verified.
TEST_F(EngineComponentTest, SortVector) {
    engine_->addVector({5, 3, 1});
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::SortVector(0));
}

// Contract: receiveStrategyChange(Descending)
//   → factory.create(Descending)               } same step,
//   → historian.recordCommand("setSortStrategy") } checked in order
TEST_F(EngineComponentTest, SetStrategy) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::SetStrategy(SortStrategyId::Descending));
}

// Contract: receivePublishSnapshot → historian.publishSnapshot(vectorCount=1)
TEST_F(EngineComponentTest, PublishSnapshot) {
    engine_->addVector({1, 2, 3});
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::PublishSnapshot(1));
}

// Full communication contract: AddVector → Sort → ChangeStrategy → Snapshot.
// Each sub-scenario is a separate step; no cross-step signal leakage possible.
TEST_F(EngineComponentTest, FullEngineFlow) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::FullEngineFlow());
}

// Ad-hoc local scenario — stimulus and expectation in separate run() calls.
// Demonstrates that run() is a fragment, not a scenario boundary.
TEST_F(EngineComponentTest, LocalScenario) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run({ receiveVectorAdded({10, 20, 30}) });
    driver.run({ expectHistorianCommand("addVector", {{10, 20, 30}}) });
}

// ─── Cross-run regression tests ───────────────────────────────────────────────
// Verify that run() is a scenario fragment, not a scenario boundary.

// Actual without expectation until EngineDriver destruction → Unexpected signal.
TEST_F(EngineComponentTest, SplitRun_ActualWithoutExpectation) {
    EXPECT_NONFATAL_FAILURE(
        [&]() {
            EngineDriver driver(*engine_, verifier_, channels_);
            driver.run({ receiveVectorAdded({10, 20, 30}) });
            // No expectation declared — destructor: finalizeStep() → "Unexpected signal"
        }(),
        "Unexpected signal"
    );
}

// Wrong order: two actuals from one stimulus, expectations declared in reverse.
// receiveStrategyChange generates in one step:
//   actual[0]: Factory.create(Descending)
//   actual[1]: Historian.recordCommand("setSortStrategy")
// Declaring Historian expectation first → mismatch → Unexpected signal.
TEST_F(EngineComponentTest, SplitRun_WrongOrder) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run({ receiveStrategyChange(SortStrategyId::Descending) });
    EXPECT_NONFATAL_FAILURE(
        driver.run({
            expectHistorianCommand("setSortStrategy"),        // expected 2nd, arrives 1st
            expectFactoryCreate(SortStrategyId::Descending),
        }),
        "Unexpected signal"
    );
}

// Payload mismatch in split scenario.
TEST_F(EngineComponentTest, SplitRun_PayloadMismatch) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run({ receiveVectorAdded({10, 20, 30}) });
    EXPECT_NONFATAL_FAILURE(
        driver.run({ expectHistorianCommand("addVector", {{99, 99, 99}}) }),
        "payload mismatch"
    );
}

// Multi-step split: each stimulus and expectation in a separate run().
TEST_F(EngineComponentTest, SplitRun_MultiStep) {
    engine_->addVector({3, 1, 2});
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run({ receiveVectorAdded({3, 1, 2}) });
    driver.run({ expectHistorianCommand("addVector", {{3, 1, 2}}) });
    driver.run({ receiveSortRequested(0) });
    driver.run({ expectHistorianCommand("sortVector") });
}

// Mixed fragment: expectation closing step A + new stimulus B + expectation B
// in a single run() call after a prior run() with stimulus A.
// Verifies that run() boundaries have no semantic role.
TEST_F(EngineComponentTest, SplitRun_MixedFragment) {
    engine_->addVector({5, 3, 1});
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run({ receiveVectorAdded({1, 2, 3}) });
    driver.run({
        expectHistorianCommand("addVector", {{1, 2, 3}}),  // closes step A
        receiveSortRequested(0),                            // opens step B
    //    expectHistorianCommand("sortVector"),               // closes step B
    });
     driver.run({expectHistorianCommand("sortVector"),   });
}

// ═══════════════════════════════════════════════════════════════════════════════
// HistorianOnlyTest — historian channel only
// ─────────────────────────────────────────────────────────────────────────────
// dynamic_cast<HistorianSpy*>(this) → non-null → HistorianSpy wired
// dynamic_cast<FactorySpy*>(this)   → null     → real SortStrategyFactory wired
// channels_ = { historian: true, factory: false }
//
// EngineDriver filters expectations by channels_:
//   expectHistorianCommand  → VERIFY
//   expectHistorianSnapshot → VERIFY
//   expectFactoryCreate     → SKIP  (not "Signal not received")
//
// The same pre-built Scenarios::* collections work without modification.
// ═══════════════════════════════════════════════════════════════════════════════

class HistorianOnlyTest : public EngineTestBase, public HistorianSpy {};

TEST_F(HistorianOnlyTest, AddVector) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::AddVector({1, 2, 3}));
}

TEST_F(HistorianOnlyTest, SortVector) {
    engine_->addVector({5, 3, 1});
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::SortVector(0));
}

// Scenarios::SetStrategy contains expectFactoryCreate + expectHistorianCommand.
// channels_.factory=false → expectFactoryCreate silently skipped.
// Only expectHistorianCommand("setSortStrategy") is verified.
// Same Scenarios::SetStrategy as EngineComponentTest — no modification needed.
TEST_F(HistorianOnlyTest, SetStrategy) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::SetStrategy(SortStrategyId::Descending));
}

TEST_F(HistorianOnlyTest, PublishSnapshot) {
    engine_->addVector({1, 2, 3});
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::PublishSnapshot(1));
}

// Scenarios::FullEngineFlow contains all expectFactory* and expectHistorian* signals.
// channels_.factory=false → all expectFactoryCreate silently skipped.
// Verified: expectHistorianCommand x3, expectHistorianSnapshot x1.
// Same Scenarios::FullEngineFlow as EngineComponentTest — no modification needed.
TEST_F(HistorianOnlyTest, FullEngineFlow) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::FullEngineFlow());
}

// Split run in Historian-only fixture: expectFactoryCreate filtered by channels_.
TEST_F(HistorianOnlyTest, SplitRun_HistorianOnly) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run({ receiveVectorAdded({5, 6, 7}) });
    driver.run({ expectHistorianCommand("addVector", {{5, 6, 7}}) });
}

// ═══════════════════════════════════════════════════════════════════════════════
// FactoryOnlyTest — factory channel only
// ─────────────────────────────────────────────────────────────────────────────
// dynamic_cast<HistorianSpy*>(this) → null     → NullHistorian wired
// dynamic_cast<FactorySpy*>(this)   → non-null → FactorySpy   wired
// channels_ = { historian: false, factory: true }
//
// EngineDriver filters expectations by channels_:
//   expectFactoryCreate     → VERIFY
//   expectHistorianCommand  → SKIP  (not "Signal not received")
//   expectHistorianSnapshot → SKIP
//
// The same pre-built Scenarios::* collections work without modification.
// ═══════════════════════════════════════════════════════════════════════════════

class FactoryOnlyTest : public EngineTestBase, public FactorySpy {};

// Scenarios::SetStrategy contains expectFactoryCreate + expectHistorianCommand.
// channels_.historian=false → expectHistorianCommand silently skipped.
// Only expectFactoryCreate(Descending) is verified.
// Same Scenarios::SetStrategy as EngineComponentTest — no modification needed.
TEST_F(FactoryOnlyTest, SetStrategy) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::SetStrategy(SortStrategyId::Descending));
}

// Scenarios::FullEngineFlow contains all expectHistorian* and expectFactory* signals.
// channels_.historian=false → all expectHistorian* silently skipped.
// Verified: expectFactoryCreate x1 (SetStrategy(Descending) inside FullEngineFlow).
// The initial create(Ascending) in SetUp() runs before beginStep() → collectingActuals_=false
// → silently ignored (verifier not collecting yet).
// AddVector, SortVector, PublishSnapshot steps have empty expectations after filtering.
// If Engine unexpectedly calls FactorySpy during those steps, finalizeStep() reports
// "Unexpected signal" — empty contract means zero outbound calls expected.
TEST_F(FactoryOnlyTest, FullEngineFlow) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run(Scenarios::FullEngineFlow());
}

// Signal not received: stimulus executes but does not trigger the Factory channel.
// receiveVectorAdded() does not call ISortStrategyFactory::create().
// stepActuals_ is empty for Factory → matchExpectation() reports "Signal not received".
TEST_F(FactoryOnlyTest, SplitRun_SignalNotReceived) {
    EngineDriver driver(*engine_, verifier_, channels_);
    driver.run({ receiveVectorAdded({1, 2, 3}) });
    EXPECT_NONFATAL_FAILURE(
        driver.run({ expectFactoryCreate(SortStrategyId::Ascending) }),
        "Signal not received"
    );
}
