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
#include "scenario/ScenarioExecutor.hpp"
#include "scenario/EngineEndpoint.hpp"
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
    // ── Public test API — accessible directly in TEST_F ──────────────────────
    // historian / factory available when the fixture inherits HistorianSpy / FactorySpy.
    EngineEndpoint engine;

    void SetUp() override {
        [[maybe_unused]] auto r = ServiceLocator::instance().provide<Logger>(
            std::make_shared<Logger>());

        engine_ = std::make_shared<patterns::engine::Engine>();

        auto* h = dynamic_cast<HistorianSpy*>(this);
        auto* f = dynamic_cast<FactorySpy*>(this);

        // Result of dynamic_cast defines the fixture topology — stored in channels_
        // for both Engine wiring and ScenarioExecutor expectation filtering.
        channels_ = {h != nullptr, f != nullptr};

        executor_ = std::make_unique<ScenarioExecutor>(verifier_, channels_);
        engine.attach(*executor_, *engine_);

        // ── Historian channel ─────────────────────────────────────────────────
        // Engine stores historian via weak_ptr; keeper extends control block lifetime.
        if (h) {
            h->attachVerifier(verifier_);
            h->attachEndpoint(*executor_);
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
        // verifier_ not yet collecting (collectingActuals_=false) → call silently ignored.
        if (f) {
            f->attachVerifier(verifier_);
            f->attachEndpoint(*executor_);
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
        // Order is critical:
        // 1. engine_.reset()       — Engine destroyed; cannot call spy via weak_ptr anymore.
        // 2. executor_->finalize() — explicit step finalization; failures reported here, not in dtor.
        // 3. executor_.reset()     — ScenarioExecutor dtor is a no-op (finalized_=true).
        // 4. spy sub-objects (HistorianSpy, FactorySpy) — destroyed by C++ after TearDown.
        engine_.reset();
        executor_->finalize();
        executor_.reset();
    }

    ScenarioVerifier                                          verifier_;
    std::shared_ptr<patterns::engine::Engine>                 engine_;
    std::unique_ptr<ScenarioExecutor>                         executor_;
    std::shared_ptr<patterns::historian::IHistorian>          historianKeeper_;
    std::shared_ptr<patterns::strategy::ISortStrategyFactory> factoryKeeper_;
    ActiveChannels                                            channels_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// EndpointApiTest — regression tests for the new endpoint API
// ─────────────────────────────────────────────────────────────────────────────
// Verify ScenarioExecutor + endpoint behaviour against a live Engine.
// ScenarioFrameworkTest covers ScenarioVerifier in isolation — these tests cover
// the layer above it: step lifecycle, declareExpectation, finalize().
// ═══════════════════════════════════════════════════════════════════════════════

class EndpointApiTest : public EngineTestBase,
                        public HistorianSpy,
                        public FactorySpy {};

// Expectation before the first stimulus → "Expectation ... before any stimulus".
TEST_F(EndpointApiTest, ExpectationBeforeStimulus)
{
    EXPECT_NONFATAL_FAILURE(
        historian.receive(historian.addVector({1, 2, 3})),
        "Expectation"
    );
}

// Stimulus with no expectations → "Unexpected signal" on the next stimulus.
TEST_F(EndpointApiTest, UnexpectedSignal_SecondStimulusClosesPreviousStep)
{
    EXPECT_NONFATAL_FAILURE(
        [&]() {
            engine.receive(engine.addVector({1, 2, 3}));
            // No historian.receive() → step remains open with unmatched actuals.
            engine.receive(engine.addVector({1, 2, 3}));  // closes step 1
        }(),
        "Unexpected signal"
    );
    historian.receive(historian.addVector({1, 2, 3}));
}

// Payload mismatch → "payload mismatch".
TEST_F(EndpointApiTest, PayloadMismatch)
{
    engine.receive(engine.addVector({1, 2, 3}));
    EXPECT_NONFATAL_FAILURE(
        historian.receive(historian.addVector({9, 9, 9})),
        "payload mismatch"
    );
}

// Expectations in wrong order → "Unexpected signal".
// strategyChange generates: actual[0]=factory.create, actual[1]=historian.recordCommand.
// Declaring historian before factory → mismatch.
TEST_F(EndpointApiTest, WrongOrder)
{
    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    EXPECT_NONFATAL_FAILURE(
        historian.receive(historian.setSortStrategy()),  // expected[0] but actual[0]=factory
        "Unexpected signal"
    );
}

// No actuals for a declared expectation → "Signal not received".
TEST_F(EndpointApiTest, SignalNotReceived)
{
    engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));
    EXPECT_NONFATAL_FAILURE(
        historian.receive(historian.sortVector()),  // no actual matches
        "Signal not received"
    );
}

// Multiple steps in one TEST_F — correct isolation between steps.
TEST_F(EndpointApiTest, MultiStep_StepsAreIsolated)
{
    engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));

    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    factory.receive(factory.create(SortStrategyId::Descending));
    historian.receive(historian.setSortStrategy());
}

// Inactive channel: factory.receive() in HistorianOnlyTest is silently skipped.
// Indirect test — verifies that absence of FactorySpy does not cause "Signal not received".
// ScenarioExecutor channel filtering (channels_.factory=false) is unchanged from EngineDriver.
// Direct topology test: see HistorianOnlyTest::SetStrategy below.

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

// ─── Framework self-check ─────────────────────────────────────────────────────

// Expectation before the first stimulus → error.
TEST_F(EngineComponentTest, MalformedScenario_ExpectationBeforeStimulus) {
    EXPECT_NONFATAL_FAILURE(
        historian.receive(historian.addVector({1, 2, 3})),
        "Expectation"
    );
}

// ─── Engine component contracts ───────────────────────────────────────────────

// Contract: addVector → historian.recordCommand("addVector", data)
TEST_F(EngineComponentTest, AddVector) {
    engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));
}

// Contract: sortVector → historian.recordCommand("sortVector")
// Vector seeded before scenario — pre-test calls are not verified.
TEST_F(EngineComponentTest, SortVector) {
    engine_->addVector({5, 3, 1});
    engine.receive(engine.sortVector(0));
    historian.receive(historian.sortVector());
}

// Contract: strategyChange(Descending)
//   → factory.create(Descending)        } same step,
//   → historian.setSortStrategy()        } checked in order
TEST_F(EngineComponentTest, SetStrategy) {
    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    factory.receive(factory.create(SortStrategyId::Descending));
    historian.receive(historian.setSortStrategy());
}

// Contract: publishSnapshot → historian.publishSnapshot(vectorCount=1)
TEST_F(EngineComponentTest, PublishSnapshot) {
    engine_->addVector({1, 2, 3});
    engine.receive(engine.publishSnapshot());
    historian.receive(historian.publishSnapshot(1));
}

// Full communication contract: AddVector → Sort → ChangeStrategy → Snapshot.
// Each engine.receive() is a separate step; no cross-step signal leakage possible.
TEST_F(EngineComponentTest, FullEngineFlow) {
    engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));

    engine.receive(engine.sortVector(0));
    historian.receive(historian.sortVector());

    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    factory.receive(factory.create(SortStrategyId::Descending));
    historian.receive(historian.setSortStrategy());

    engine.receive(engine.publishSnapshot());
    historian.receive(historian.publishSnapshot(1));
}

// Ad-hoc local scenario — in the new API the absence of a run() boundary is natural.
TEST_F(EngineComponentTest, LocalScenario) {
    engine.receive(engine.addVector({10, 20, 30}));
    historian.receive(historian.addVector({10, 20, 30}));
}

// ─── Regression tests — failure detection ─────────────────────────────────────
// Verify that the framework detects every class of contract violation.
// In the new API each line is an independent call — no run() boundary needed.

// Actual with no expectation → "Unexpected signal".
// Second stimulus closes the previous step inside EXPECT_NONFATAL_FAILURE — failure
// must occur in the same scope, not in TearDown().
TEST_F(EngineComponentTest, ActualWithoutExpectation)
{
    engine.receive(engine.addVector({10, 20, 30}));
    EXPECT_NONFATAL_FAILURE(
        engine.receive(engine.addVector({10, 20, 30})),  // closes step 1 → "Unexpected signal"
        "Unexpected signal"
    );
    historian.receive(historian.addVector({10, 20, 30}));
}

// Wrong order of expectations → "Unexpected signal".
// strategyChange generates within one step:
//   actual[0]: factory.create(Descending)
//   actual[1]: historian.recordCommand("setSortStrategy")
// Declaring historian before factory → mismatch.
TEST_F(EngineComponentTest, WrongOrder)
{
    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    EXPECT_NONFATAL_FAILURE(
        historian.receive(historian.setSortStrategy()),  // actual[0]=factory, not historian
        "Unexpected signal"
    );
}

// Payload mismatch.
TEST_F(EngineComponentTest, PayloadMismatch)
{
    engine.receive(engine.addVector({10, 20, 30}));
    EXPECT_NONFATAL_FAILURE(
        historian.receive(historian.addVector({99, 99, 99})),
        "payload mismatch"
    );
}

// Multiple steps — isolation between steps.
TEST_F(EngineComponentTest, MultiStep)
{
    engine_->addVector({3, 1, 2});
    engine.receive(engine.addVector({3, 1, 2}));
    historian.receive(historian.addVector({3, 1, 2}));
    engine.receive(engine.sortVector(0));
    historian.receive(historian.sortVector());
}

// Expectation closing step A and new stimulus B in a continuous sequence of calls.
TEST_F(EngineComponentTest, MultiStep_MixedSequence)
{
    engine_->addVector({5, 3, 1});
    engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));  // closes step A
    engine.receive(engine.sortVector(0));                // opens step B
    historian.receive(historian.sortVector());
}

// ═══════════════════════════════════════════════════════════════════════════════
// HistorianOnlyTest — historian channel only
// ─────────────────────────────────────────────────────────────────────────────
// dynamic_cast<HistorianSpy*>(this) → non-null → HistorianSpy wired
// dynamic_cast<FactorySpy*>(this)   → null     → real SortStrategyFactory wired
// channels_ = { historian: true, factory: false }
//
// `factory` does not exist as a member — the compiler blocks factory.receive().
// No factory.receive() declaration = no expectation for the factory channel.
// ScenarioExecutor silently skips the factory channel (channels_.factory=false).
// ═══════════════════════════════════════════════════════════════════════════════

class HistorianOnlyTest : public EngineTestBase, public HistorianSpy {};

// In the new API, no factory.receive() = no expectation for the factory channel.
// factory does not exist in this fixture — the compiler blocks its use.

TEST_F(HistorianOnlyTest, AddVector) {
    engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));
}

TEST_F(HistorianOnlyTest, SortVector) {
    engine_->addVector({5, 3, 1});
    engine.receive(engine.sortVector(0));
    historian.receive(historian.sortVector());
}

// Factory channel inactive — no factory.receive() means no expectation.
// factory.create() is called by Engine but no one verifies it.
// This is intentional: the fixture declares contracts only for the channels it monitors.
TEST_F(HistorianOnlyTest, SetStrategy) {
    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    historian.receive(historian.setSortStrategy());
}

TEST_F(HistorianOnlyTest, PublishSnapshot) {
    engine_->addVector({1, 2, 3});
    engine.receive(engine.publishSnapshot());
    historian.receive(historian.publishSnapshot(1));
}

TEST_F(HistorianOnlyTest, FullEngineFlow) {


    engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));
    engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));
    engine.receive(engine.sortVector(0));
    historian.receive(historian.sortVector());
     engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));
    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    historian.receive(historian.setSortStrategy());

    engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));
    engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));
    engine.receive(engine.sortVector(0));
    historian.receive(historian.sortVector());
     engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));
    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    historian.receive(historian.setSortStrategy());


    engine.receive(engine.publishSnapshot());
    historian.receive(historian.publishSnapshot(6));
}

TEST_F(HistorianOnlyTest, HistorianOnly_PayloadVerified) {
    engine.receive(engine.addVector({5, 6, 7}));
    historian.receive(historian.addVector({5, 6, 7}));
}
// ═══════════════════════════════════════════════════════════════════════════════
// FactoryOnlyTest — factory channel only
// ─────────────────────────────────────────────────────────────────────────────
// dynamic_cast<HistorianSpy*>(this) → null     → NullHistorian wired
// dynamic_cast<FactorySpy*>(this)   → non-null → FactorySpy   wired
// channels_ = { historian: false, factory: true }
//
// `historian` does not exist as a member — the compiler blocks historian.receive().
// No historian.receive() declaration = no expectation for the historian channel.
// ScenarioExecutor silently skips the historian channel (channels_.historian=false).
// ═══════════════════════════════════════════════════════════════════════════════

class FactoryOnlyTest : public EngineTestBase, public FactorySpy {};

// historian does not exist in this fixture — the compiler blocks its use.

// Historian channel inactive — only factory.create() is verified.
TEST_F(FactoryOnlyTest, SetStrategy) {
    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    factory.receive(factory.create(SortStrategyId::Descending));
}

// AddVector, SortVector, PublishSnapshot do not call factory.create().
// Empty expectations for those steps = zero outbound calls expected on the Factory channel.
// If Engine unexpectedly called factory, finalizeStep() would report "Unexpected signal".
// SetStrategy calls factory.create(Descending) → verified.
TEST_F(FactoryOnlyTest, FullEngineFlow) {
    engine.receive(engine.addVector({1, 2, 3}));
    // no factory.receive() — addVector does not call factory

    engine.receive(engine.sortVector(0));
    // no factory.receive() — sortVector does not call factory

    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    factory.receive(factory.create(SortStrategyId::Descending));

    engine.receive(engine.publishSnapshot());
    // no factory.receive() — publishSnapshot does not call factory
}

// addVector does not call factory.create() → "Signal not received".
TEST_F(FactoryOnlyTest, SignalNotReceived) {
    engine.receive(engine.addVector({1, 2, 3}));
    EXPECT_NONFATAL_FAILURE(
        factory.receive(factory.create(SortStrategyId::Ascending)),
        "Signal not received"
    );
}
