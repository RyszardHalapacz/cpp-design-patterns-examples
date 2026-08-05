#include "patterns/gui/DummyGui.hpp"
#include "patterns/services/ServiceLocator.hpp"

namespace patterns::gui {

using patterns::strategy::SortStrategyId;
using patterns::services::appLogger;

void DummyGui::connectAddVector(patterns::session::SessionManagement* s, AddVectorFunc f) {
    session_ = s; addVectorFunc_ = f;
}
void DummyGui::connectSortVector(patterns::session::SessionManagement* s, SortVectorFunc f) {
    session_ = s; sortVectorFunc_ = f;
}
void DummyGui::connectPrintData(patterns::session::SessionManagement* s, PrintDataFunc f) {
    session_ = s; printDataFunc_ = f;
}
void DummyGui::connectExecuteBatch(patterns::session::SessionManagement* s, ExecuteBatchFunc f) {
    session_ = s; executeBatchFunc_ = f;
}
void DummyGui::connectSetSortStrategy(patterns::session::SessionManagement* s, SetSortStrategyFunc f) {
    session_ = s; setSortStrategyFunc_ = f;
}

void DummyGui::clickAddVector(const std::vector<int>& vec) {
    if (!session_ || !addVectorFunc_) {
        appLogger().log("[GUI] No access to AddVector\n"); return;
    }
    appLogger().log("[GUI] Clicked AddVector\n");
    (session_->*addVectorFunc_)(vec);
}

void DummyGui::clickSortVector(size_t index) {
    if (!session_ || !sortVectorFunc_) {
        appLogger().log("[GUI] No access to SortVector\n"); return;
    }
    appLogger().log("[GUI] Clicked SortVector\n");
    (session_->*sortVectorFunc_)(index);
}

void DummyGui::clickPrintData() {
    if (!session_ || !printDataFunc_) {
        appLogger().log("[GUI] No access to PrintData\n"); return;
    }
    appLogger().log("[GUI] Clicked PrintData\n");
    (session_->*printDataFunc_)();
}

void DummyGui::clickSetSortStrategy(SortStrategyId id) {
    if (!session_ || !setSortStrategyFunc_) {
        appLogger().log("[GUI] No access to SetSortStrategy\n"); return;
    }
    appLogger().log("[GUI] Clicked SetSortStrategy\n");
    (session_->*setSortStrategyFunc_)(id);
}

DummyGui& DummyGui::queueAddVector(const std::vector<int>& vec) {
    appLogger().log("[GUI] Adding AddVector to command batch\n");
    batchBuilder_.addVector(vec);
    return *this;
}

DummyGui& DummyGui::queueSortVector(size_t index) {
    appLogger().log("[GUI] Adding SortVector to command batch\n");
    batchBuilder_.sortVector(index);
    return *this;
}

DummyGui& DummyGui::queuePrintData() {
    appLogger().log("[GUI] Adding PrintData to command batch\n");
    batchBuilder_.printData();
    return *this;
}

CommandBatch DummyGui::buildBatch() {
    appLogger().log("[GUI] Closing command batch, ready to send\n");
    return batchBuilder_.build();
}

void DummyGui::flushBatch() {
    if (!session_ || !executeBatchFunc_) {
        appLogger().log("[GUI] No access to ExecuteBatch\n"); return;
    }
    CommandBatch batch = buildBatch();
    appLogger().log("[GUI] Sending command batch to session\n");
    (session_->*executeBatchFunc_)(batch);
}

} // namespace patterns::gui
