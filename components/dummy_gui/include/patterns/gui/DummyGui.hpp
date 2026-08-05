#pragma once
#include <filesystem>
#include <memory>
#include <vector>
#include "patterns/gui/Command.hpp"
#include "patterns/gui/CommandBatchBuilder.hpp"
#include "patterns/session/SessionManagement.hpp"
#include "patterns/strategy/SortStrategyId.hpp"
#include "patterns/services/ServiceLocator.hpp"
#include "patterns/manifest/ManifestWriter.hpp"


namespace patterns::gui {

template<typename Writer = patterns::manifest::ComponentManifestWriter>
class BasicDummyGui {
public:
    using AddVectorFunc       = void (patterns::session::SessionManagement::*)(const std::vector<int>&);
    using SortVectorFunc      = void (patterns::session::SessionManagement::*)(size_t);
    using PrintDataFunc       = void (patterns::session::SessionManagement::*)();
    using ExecuteBatchFunc    = void (patterns::session::SessionManagement::*)(const CommandBatch&);
    using SetSortStrategyFunc = void (patterns::session::SessionManagement::*)(patterns::strategy::SortStrategyId);

    friend BasicDummyGui* makeGUI(std::filesystem::path manifestPath);
    friend void           deleteGUI(BasicDummyGui* gui);

    void connectAddVector      (const std::shared_ptr<patterns::session::SessionManagement>& s, AddVectorFunc       f) { session_ = s; addVectorFunc_       = f; }
    void connectSortVector     (const std::shared_ptr<patterns::session::SessionManagement>& s, SortVectorFunc      f) { session_ = s; sortVectorFunc_      = f; }
    void connectPrintData      (const std::shared_ptr<patterns::session::SessionManagement>& s, PrintDataFunc       f) { session_ = s; printDataFunc_       = f; }
    void connectExecuteBatch   (const std::shared_ptr<patterns::session::SessionManagement>& s, ExecuteBatchFunc    f) { session_ = s; executeBatchFunc_    = f; }
    void connectSetSortStrategy(const std::shared_ptr<patterns::session::SessionManagement>& s, SetSortStrategyFunc f) { session_ = s; setSortStrategyFunc_ = f; }

    void clickAddVector(const std::vector<int>& vec) {
        if (!addVectorFunc_) { patterns::services::logApp("[GUI] No access to AddVector\n"); return; }
        auto s = lockSession(); if (!s) return;
        patterns::services::logApp("[GUI] Clicked AddVector\n");
        ((*s).*addVectorFunc_)(vec);
    }

    void clickSortVector(size_t index) {
        if (!sortVectorFunc_) { patterns::services::logApp("[GUI] No access to SortVector\n"); return; }
        auto s = lockSession(); if (!s) return;
        patterns::services::logApp("[GUI] Clicked SortVector\n");
        ((*s).*sortVectorFunc_)(index);
    }

    void clickPrintData() {
        if (!printDataFunc_) { patterns::services::logApp("[GUI] No access to PrintData\n"); return; }
        auto s = lockSession(); if (!s) return;
        patterns::services::logApp("[GUI] Clicked PrintData\n");
        ((*s).*printDataFunc_)();
    }

    void clickSetSortStrategy(patterns::strategy::SortStrategyId id) {
        if (!setSortStrategyFunc_) { patterns::services::logApp("[GUI] No access to SetSortStrategy\n"); return; }
        auto s = lockSession(); if (!s) return;
        patterns::services::logApp("[GUI] Clicked SetSortStrategy\n");
        ((*s).*setSortStrategyFunc_)(id);
    }

    BasicDummyGui& queueAddVector(const std::vector<int>& vec) {
        patterns::services::logApp("[GUI] Adding AddVector to command batch\n");
        batchBuilder_.addVector(vec);
        return *this;
    }

    BasicDummyGui& queueSortVector(size_t index) {
        patterns::services::logApp("[GUI] Adding SortVector to command batch\n");
        batchBuilder_.sortVector(index);
        return *this;
    }

    BasicDummyGui& queuePrintData() {
        patterns::services::logApp("[GUI] Adding PrintData to command batch\n");
        batchBuilder_.printData();
        return *this;
    }

    CommandBatch buildBatch() {
        patterns::services::logApp("[GUI] Closing command batch, ready to send\n");
        return batchBuilder_.build();
    }

    void flushBatch() {
        if (!executeBatchFunc_) { patterns::services::logApp("[GUI] No access to ExecuteBatch\n"); return; }
        auto s = lockSession(); if (!s) return;
        CommandBatch batch = buildBatch();
        patterns::services::logApp("[GUI] Sending command batch to session\n");
        ((*s).*executeBatchFunc_)(batch);
    }

private:
    explicit BasicDummyGui(std::filesystem::path manifestPath = {}) {
        if (!manifestPath.empty()) {
            manifestWriter_.write(manifestPath, "DummyGui", "DummyGui", DUMMY_GUI_VERSION_STR);
            manifestWriter_.printManifest(manifestPath);
        }
    }

    std::shared_ptr<patterns::session::SessionManagement> lockSession() const {
        auto s = session_.lock();
        if (!s) patterns::services::logApp("[GUI] SessionManagement no longer exists — operation cancelled\n");
        return s;
    }

    std::weak_ptr<patterns::session::SessionManagement> session_;

    AddVectorFunc       addVectorFunc_       = nullptr;
    SortVectorFunc      sortVectorFunc_      = nullptr;
    PrintDataFunc       printDataFunc_       = nullptr;
    ExecuteBatchFunc    executeBatchFunc_    = nullptr;
    SetSortStrategyFunc setSortStrategyFunc_ = nullptr;

    CommandBatchBuilder batchBuilder_;
    Writer              manifestWriter_;
};

using DummyGui = BasicDummyGui<>;

// ─── C-style factory functions ────────────────────────────────────────────────
// Simulates the interface of legacy C libraries where opaque handles are
// created and destroyed via paired init/cleanup functions (e.g. SDL_Init /
// SDL_Quit, curl_easy_init / curl_easy_cleanup).  The private constructor
// ensures DummyGui can only be obtained through makeGUI.

inline DummyGui* makeGUI(std::filesystem::path manifestPath = {}) {
    return new DummyGui(std::move(manifestPath));
}

inline void deleteGUI(DummyGui* gui) {
    delete gui;
}

} // namespace patterns::gui

// ─── unique_ptr support ───────────────────────────────────────────────────────
// Specialising std::default_delete lets callers write
//   std::unique_ptr<DummyGui> gui(makeGUI(...));
// and have deleteGUI called automatically on destruction — no custom deleter
// lambda needed at every call site.

namespace std {
template<>
struct default_delete<patterns::gui::DummyGui> {
    void operator()(patterns::gui::DummyGui* gui) const {
        patterns::gui::deleteGUI(gui);
    }
};
} // namespace std

