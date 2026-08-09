#pragma once
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>
#include "patterns/gui/ICommand.hpp"
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
    using SetSortStrategyFunc = std::function<void(patterns::strategy::SortStrategyId)>;

    friend BasicDummyGui* makeGUI(std::filesystem::path manifestPath);
    friend void           deleteGUI(BasicDummyGui* gui);

    void connectAddVector      (AddVectorFunc       f) { addVectorFunc_       = std::move(f); }
    void connectSortVector     (SortVectorFunc      f) { sortVectorFunc_      = std::move(f); }
    void connectPrintData      (PrintDataFunc       f) { printDataFunc_       = std::move(f); }
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
        if (!addVectorFunc_) { patterns::services::logApp("[GUI] No access to AddVector\n"); return *this; }
        patterns::services::logApp("[GUI] Adding AddVector to command batch\n");
        batchBuilder_.addVector(addVectorFunc_, vec);
        return *this;
    }

    BasicDummyGui& queueSortVector(size_t index) {
        if (!sortVectorFunc_) { patterns::services::logApp("[GUI] No access to SortVector\n"); return *this; }
        patterns::services::logApp("[GUI] Adding SortVector to command batch\n");
        batchBuilder_.sortVector(sortVectorFunc_, index);
        return *this;
    }

    BasicDummyGui& queuePrintData() {
        if (!printDataFunc_) { patterns::services::logApp("[GUI] No access to PrintData\n"); return *this; }
        patterns::services::logApp("[GUI] Adding PrintData to command batch\n");
        batchBuilder_.printData(printDataFunc_);
        return *this;
    }

    CommandBatch buildBatch() {
        patterns::services::logApp("[GUI] Closing command batch, ready to send\n");
        return batchBuilder_.build();
    }

    void flushBatch() {
        CommandBatch batch = buildBatch();
        if (batch.empty()) return;
        patterns::services::logApp("[GUI] Executing command batch\n");
        for (auto& cmd : batch) cmd->execute();
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
    SetSortStrategyFunc setSortStrategyFunc_;

    CommandBatchBuilder batchBuilder_;
    Writer              manifestWriter_;
};

using DummyGui = BasicDummyGui<>;

// ─── C-style factory functions ────────────────────────────────────────────────
inline DummyGui* makeGUI(std::filesystem::path manifestPath = {}) {
    return new DummyGui(std::move(manifestPath));
}

inline void deleteGUI(DummyGui* gui) {
    delete gui;
}

} // namespace patterns::gui

// ─── unique_ptr support ───────────────────────────────────────────────────────
namespace std {
template<>
struct default_delete<patterns::gui::DummyGui> {
    void operator()(patterns::gui::DummyGui* gui) const {
        patterns::gui::deleteGUI(gui);
    }
};
} // namespace std
