// ─────────────────────────────────────────────────────────────────────────────
// EstablishmentSpy — test funkcyjny wymiany sygnałów podczas establish()
//
// Idea:
//   ExtendedSessionManagement dziedziczy po SessionManagement i buduje
//   establish() jako mapę std::function (std::map zapewnia stałą kolejność).
//   Każdy krok jest gated przez dynamic_cast — krok odpala się tylko wtedy,
//   gdy klasa konkretna dziedziczy odpowiedni tag (INotifiesAudit itp.).
//   EstablishmentSpy<Base> opakowuje każdy krok i nagrywa jego nazwę do logu.
//   Własny iterator pozwala iterować po wszystkich krokach lub tylko po
//   sygnałowych (filtrowanie przez predykat).
// ─────────────────────────────────────────────────────────────────────────────

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "patterns/engine/Engine.hpp"
#include "patterns/services/Logger.hpp"
#include "patterns/services/ServiceLocator.hpp"
#include "patterns/session/SessionManagement.hpp"

using namespace testing;

// ─── 1. Tagi możliwości — co klasa konkretna "umie sygnalizować" ──────────────

struct IEstablishSignal { virtual ~IEstablishSignal() = default; };
struct INotifiesAudit  : virtual IEstablishSignal {};
struct INotifiesGui    : virtual IEstablishSignal {};
struct INotifiesLogger : virtual IEstablishSignal {};

// ─── 2. Interfejsy współpracowników (odbiorcy sygnałów) ───────────────────────

struct IAuditNotifier {
    virtual void onEngineConnected() = 0;
    virtual void onSessionOpened()   = 0;
    virtual ~IAuditNotifier()        = default;
};

struct IGuiNotifier {
    virtual void onSessionReady() = 0;
    virtual ~IGuiNotifier()       = default;
};

struct ILogNotifier {
    virtual void onEstablishLog(const std::string& msg) = 0;
    virtual ~ILogNotifier()                              = default;
};

// ─── 3. Mocki współpracowników ────────────────────────────────────────────────

struct MockAuditNotifier : IAuditNotifier {
    MOCK_METHOD(void, onEngineConnected, (),                   (override));
    MOCK_METHOD(void, onSessionOpened,   (),                   (override));
};

struct MockGuiNotifier : IGuiNotifier {
    MOCK_METHOD(void, onSessionReady, (), (override));
};

struct MockLogNotifier : ILogNotifier {
    MOCK_METHOD(void, onEstablishLog, (const std::string&), (override));
};

// ─── 4. ExtendedSessionManagement ────────────────────────────────────────────
//
// Dziedziczy po SessionManagement. Establish() to pętla po std::map<string,fn>.
// Klucze z prefiksem numerycznym ustalają kolejność (std::map = rosnąco).
// Każdy krok sprawdza dynamic_cast<INotifiesX*>(this) zanim odezwie się do
// współpracownika — jeśli klasa konkretna nie dziedziczy tagu, krok milczy.

class ExtendedSessionManagement
    : public patterns::session::SessionManagement,
      public virtual IEstablishSignal
{
public:
    ExtendedSessionManagement(
        std::shared_ptr<patterns::engine::Engine> engine,
        IAuditNotifier* audit  = nullptr,
        IGuiNotifier*   gui    = nullptr,
        ILogNotifier*   logger = nullptr)
        : engine_(std::move(engine))
        , audit_(audit), gui_(gui), logger_(logger)
    {
        steps_["1_connect"] = [this]() {
            connectToEngine(engine_);
        };
        steps_["2_audit_connected"] = [this]() {
            if (dynamic_cast<INotifiesAudit*>(this) && audit_)
                audit_->onEngineConnected();
        };
        steps_["3_log"] = [this]() {
            if (dynamic_cast<INotifiesLogger*>(this) && logger_)
                logger_->onEstablishLog("session establishing");
        };
        steps_["4_open_session"] = [this]() {
            openSession();
        };
        steps_["5_audit_opened"] = [this]() {
            if (dynamic_cast<INotifiesAudit*>(this) && audit_)
                audit_->onSessionOpened();
        };
        steps_["6_gui_ready"] = [this]() {
            if (dynamic_cast<INotifiesGui*>(this) && gui_)
                gui_->onSessionReady();
        };
    }

    void establish() {
        for (auto& [key, fn] : steps_) fn();
    }

protected:
    // Spy dostaje dostęp do mapy przez protected getter — może owinąć każdy krok.
    std::map<std::string, std::function<void()>>& steps() { return steps_; }

private:
    std::shared_ptr<patterns::engine::Engine>    engine_;
    IAuditNotifier*                              audit_;
    IGuiNotifier*                                gui_;
    ILogNotifier*                                logger_;
    std::map<std::string, std::function<void()>> steps_;
};

// ─── 5. Warianty — które sygnały są aktywne zależy od dziedziczenia ───────────

class FullSessionEstablishment
    : public ExtendedSessionManagement,
      public INotifiesAudit,
      public INotifiesGui,
      public INotifiesLogger
{
    using ExtendedSessionManagement::ExtendedSessionManagement;
};

class AuditOnlyEstablishment
    : public ExtendedSessionManagement,
      public INotifiesAudit
{
    using ExtendedSessionManagement::ExtendedSessionManagement;
};

class SilentEstablishment : public ExtendedSessionManagement {
    using ExtendedSessionManagement::ExtendedSessionManagement;
};

// ─── 6. EstablishmentSpy<Base> ───────────────────────────────────────────────
//
// Generyczny wrapper: opakowuje każdy krok z mapy Base tak, żeby przed
// wywołaniem oryginalnej funkcji dopisał nazwę kroku do logu.
// Zawiera własny forward iterator z opcjonalnym filtrowaniem przez predykat —
// dzięki temu można iterować po wszystkich krokach lub tylko po "sygnałowych".

template <typename Base>
class EstablishmentSpy : public Base {
public:
    EstablishmentSpy(
        std::shared_ptr<patterns::engine::Engine> engine,
        IAuditNotifier* audit  = nullptr,
        IGuiNotifier*   gui    = nullptr,
        ILogNotifier*   logger = nullptr)
        : Base(engine, audit, gui, logger)
    {
        for (auto& [key, fn] : this->steps()) {
            auto        original = fn;
            std::string name     = key;
            fn = [this, name, original]() {
                log_.push_back(name);
                original();
            };
        }
    }

    // ── Iterator ──────────────────────────────────────────────────────────────

    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = std::string;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const std::string*;
        using reference         = const std::string&;

        explicit iterator(
            std::vector<std::string>::const_iterator cur,
            std::vector<std::string>::const_iterator end,
            std::function<bool(const std::string&)>  pred = nullptr)
            : cur_(cur), end_(end), pred_(std::move(pred))
        {
            skip();
        }

        reference operator*()  const { return *cur_; }
        pointer   operator->() const { return &*cur_; }

        iterator& operator++()    { ++cur_; skip(); return *this; }
        iterator  operator++(int) { auto tmp = *this; ++(*this); return tmp; }

        bool operator==(const iterator& o) const { return cur_ == o.cur_; }
        bool operator!=(const iterator& o) const { return cur_ != o.cur_; }

    private:
        void skip() {
            while (cur_ != end_ && pred_ && !pred_(*cur_)) ++cur_;
        }

        std::vector<std::string>::const_iterator cur_, end_;
        std::function<bool(const std::string&)>  pred_;
    };

    // Iteracja po wszystkich krokach
    iterator begin() const { return iterator(log_.cbegin(), log_.cend()); }
    iterator end()   const { return iterator(log_.cend(),   log_.cend()); }

    // Iteracja tylko po krokach pasujących do predykatu ("skakanie")
    iterator begin(std::function<bool(const std::string&)> pred) const {
        return iterator(log_.cbegin(), log_.cend(), std::move(pred));
    }
    iterator end(std::function<bool(const std::string&)> pred) const {
        return iterator(log_.cend(), log_.cend(), std::move(pred));
    }

    const std::vector<std::string>& log() const { return log_; }

private:
    std::vector<std::string> log_;
};

// ─── 7. Fixture ───────────────────────────────────────────────────────────────

class EstablishSignalsTest : public ::testing::Test {
protected:
    void SetUp() override {
        [[maybe_unused]] auto _ = patterns::services::ServiceLocator::instance()
            .provide<patterns::services::Logger>(
                std::make_shared<patterns::services::Logger>());
        engine_ = std::make_shared<patterns::engine::Engine>();
    }

    std::shared_ptr<patterns::engine::Engine> engine_;
    MockAuditNotifier audit_;
    MockGuiNotifier   gui_;
    MockLogNotifier   logger_;
};

// ─── 8. Testy ─────────────────────────────────────────────────────────────────

// Wszystkie sygnały odpalone — pełna sekwencja w logu
TEST_F(EstablishSignalsTest, FullEstablishment_LogContainsAllStepsInOrder) {
    EstablishmentSpy<FullSessionEstablishment> spy(engine_, &audit_, &gui_, &logger_);

    EXPECT_CALL(audit_,  onEngineConnected()).Times(1);
    EXPECT_CALL(logger_, onEstablishLog(_)).Times(1);
    EXPECT_CALL(audit_,  onSessionOpened()).Times(1);
    EXPECT_CALL(gui_,    onSessionReady()).Times(1);

    spy.establish();

    EXPECT_EQ(spy.log(), (std::vector<std::string>{
        "1_connect",
        "2_audit_connected",
        "3_log",
        "4_open_session",
        "5_audit_opened",
        "6_gui_ready"
    }));
}

// InSequence — kolejność sygnałów jest kontraktem, nie przypadkiem
TEST_F(EstablishSignalsTest, FullEstablishment_SignalsFiredInSequence) {
    EstablishmentSpy<FullSessionEstablishment> spy(engine_, &audit_, &gui_, &logger_);

    InSequence seq;
    EXPECT_CALL(audit_,  onEngineConnected());
    EXPECT_CALL(logger_, onEstablishLog(_));
    EXPECT_CALL(audit_,  onSessionOpened());
    EXPECT_CALL(gui_,    onSessionReady());

    spy.establish();
}

// Tylko audit — GUI i logger milczą (brak tagu w hierarchii dziedziczenia)
TEST_F(EstablishSignalsTest, AuditOnly_GuiAndLoggerStepsSilent) {
    EstablishmentSpy<AuditOnlyEstablishment> spy(engine_, &audit_, nullptr, nullptr);

    EXPECT_CALL(audit_, onEngineConnected()).Times(1);
    EXPECT_CALL(audit_, onSessionOpened()).Times(1);
    EXPECT_CALL(gui_,   onSessionReady()).Times(0);

    spy.establish();

    // kroki są w logu (pętla je przeszła), ale nie wywołały mocków
    EXPECT_EQ(spy.log().size(), 6u);
}

// Żadnych tagów — żaden mock nie jest wywołany
TEST_F(EstablishSignalsTest, SilentEstablishment_NoCollaboratorsCalled) {
    EstablishmentSpy<SilentEstablishment> spy(engine_, nullptr, nullptr, nullptr);

    EXPECT_CALL(audit_,  onEngineConnected()).Times(0);
    EXPECT_CALL(audit_,  onSessionOpened()).Times(0);
    EXPECT_CALL(gui_,    onSessionReady()).Times(0);
    EXPECT_CALL(logger_, onEstablishLog(_)).Times(0);

    spy.establish();
}

// Iterator podstawowy — range-for po wszystkich krokach
TEST_F(EstablishSignalsTest, Iterator_RangeFor_AllSteps) {
    EstablishmentSpy<FullSessionEstablishment> spy(engine_, &audit_, &gui_, &logger_);

    EXPECT_CALL(audit_,  onEngineConnected());
    EXPECT_CALL(logger_, onEstablishLog(_));
    EXPECT_CALL(audit_,  onSessionOpened());
    EXPECT_CALL(gui_,    onSessionReady());

    spy.establish();

    std::vector<std::string> visited;
    for (const auto& step : spy) visited.push_back(step);

    EXPECT_EQ(visited, spy.log());
}

// Iterator z predykatem — "skakanie" tylko po krokach sygnałowych
TEST_F(EstablishSignalsTest, Iterator_FilteredToSignalStepsOnly) {
    EstablishmentSpy<FullSessionEstablishment> spy(engine_, &audit_, &gui_, &logger_);

    EXPECT_CALL(audit_,  onEngineConnected());
    EXPECT_CALL(logger_, onEstablishLog(_));
    EXPECT_CALL(audit_,  onSessionOpened());
    EXPECT_CALL(gui_,    onSessionReady());

    spy.establish();

    // kroki sygnałowe = te gdzie dynamic_cast może zadziałać (klucz zaczyna się od 2,3,5,6)
    auto isSignal = [](const std::string& s) {
        return s[0] == '2' || s[0] == '3' || s[0] == '5' || s[0] == '6';
    };

    std::vector<std::string> signals;
    for (auto it = spy.begin(isSignal); it != spy.end(isSignal); ++it)
        signals.push_back(*it);

    EXPECT_EQ(signals, (std::vector<std::string>{
        "2_audit_connected",
        "3_log",
        "5_audit_opened",
        "6_gui_ready"
    }));
}
