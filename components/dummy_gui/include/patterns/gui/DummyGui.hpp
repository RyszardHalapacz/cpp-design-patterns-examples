#pragma once
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>
#include "patterns/gui/Command.hpp"
#include "patterns/gui/CommandBatchBuilder.hpp"
#include "patterns/strategy/SortStrategyId.hpp"
#include "patterns/services/ServiceLocator.hpp"
#include "patterns/manifest/ManifestWriter.hpp"


namespace patterns::gui {

template<typename Writer = patterns::manifest::ComponentManifestWriter>
class BasicDummyGui {
public:
    using AddVectorFunc       = std::function<void(const std::vector<int>&)>;
    using SortVectorFunc      = std::function<void(size_t)>;
    using PrintDataFunc       = std::function<void()>;
    using ExecuteBatchFunc    = std::function<void(const CommandBatch&)>;
    using SetSortStrategyFunc = std::function<void(patterns::strategy::SortStrategyId)>;

    friend BasicDummyGui* makeGUI(std::filesystem::path manifestPath);
    friend void           deleteGUI(BasicDummyGui* gui);

    void connectAddVector      (AddVectorFunc       f) { addVectorFunc_       = std::move(f); }
    void connectSortVector     (SortVectorFunc      f) { sortVectorFunc_      = std::move(f); }
    void connectPrintData      (PrintDataFunc       f) { printDataFunc_       = std::move(f); }
    void connectExecuteBatch   (ExecuteBatchFunc    f) { executeBatchFunc_    = std::move(f); }
    void connectSetSortStrategy(SetSortStrategyFunc f) { setSortStrategyFunc_ = std::move(f); }

    void clickAddVector(const std::vector<int>& vec) {
        if (!addVectorFunc_) { patterns::services::logApp("[GUI] No access to AddVector\n"); return; }
        patterns::services::logApp("[GUI] Clicked AddVector\n");
        addVectorFunc_(vec);
    }

    void clickSortVector(size_t index) {
        if (!sortVectorFunc_) { patterns::services::logApp("[GUI] No access to SortVector\n"); return; }
        patterns::services::logApp("[GUI] Clicked SortVector\n");
        sortVectorFunc_(index);
    }

    void clickPrintData() {
        if (!printDataFunc_) { patterns::services::logApp("[GUI] No access to PrintData\n"); return; }
        patterns::services::logApp("[GUI] Clicked PrintData\n");
        printDataFunc_();
    }

    void clickSetSortStrategy(patterns::strategy::SortStrategyId id) {
        if (!setSortStrategyFunc_) { patterns::services::logApp("[GUI] No access to SetSortStrategy\n"); return; }
        patterns::services::logApp("[GUI] Clicked SetSortStrategy\n");
        setSortStrategyFunc_(id);
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
        CommandBatch batch = buildBatch();
        patterns::services::logApp("[GUI] Sending command batch to session\n");
        executeBatchFunc_(batch);
    }

private:
    explicit BasicDummyGui(std::filesystem::path manifestPath = {}) {
        if (!manifestPath.empty()) {
            manifestWriter_.write(manifestPath, "DummyGui", "DummyGui", DUMMY_GUI_VERSION_STR);
            manifestWriter_.printManifest(manifestPath);
        }
    }

    AddVectorFunc       addVectorFunc_;
    SortVectorFunc      sortVectorFunc_;
    PrintDataFunc       printDataFunc_;
    ExecuteBatchFunc    executeBatchFunc_;
    SetSortStrategyFunc setSortStrategyFunc_;

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
