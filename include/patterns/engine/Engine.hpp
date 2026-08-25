#pragma once
#include <filesystem>
#include <memory>
#include <sstream>
#include <vector>
#include "patterns/observer/ISessionObserver.hpp"
#include "patterns/strategy/ISortStrategy.hpp"
#include "patterns/strategy/ISortStrategyFactory.hpp"
#include "patterns/services/ServiceLocator.hpp"
#include "patterns/manifest/ManifestWriter.hpp"
#include "patterns/historian/EngineHistorian.hpp"


namespace patterns::engine {

template<typename Writer = patterns::manifest::ComponentManifestWriter>
class BasicEngine : public patterns::observer::ISessionObserver {
public:
    explicit BasicEngine(std::filesystem::path manifestPath = {}) {
        if (!manifestPath.empty()) {
            manifestWriter_.write(manifestPath, "Engine", "Engine", ENGINE_VERSION_STR);
            manifestWriter_.printManifest(manifestPath);
        }
    }

    void setHistorian(std::shared_ptr<patterns::historian::EngineHistorian> historian) {
        historian_ = historian;
        patterns::services::logApp("[Engine] Historian connected\n");
    }

    void setFactory(std::shared_ptr<patterns::strategy::ISortStrategyFactory> factory) {
        factory_ = factory;
        if (auto s = factory->create(patterns::strategy::SortStrategyId::Ascending))
            sortStrategy_ = std::move(*s);
        else
            patterns::services::logApp("[Engine] Failed to create default strategy\n");
    }

    void start() {
        running_ = true;
        patterns::services::logApp("[Engine] Start\n");
    }

    void stop() {
        running_ = false;
        patterns::services::logApp("[Engine] Stop\n");
    }

    void addVector(const std::vector<int>& vec) {
        data_.push_back(vec);
    }

    void setSortStrategy(std::unique_ptr<patterns::strategy::ISortStrategy> strategy) {
        if (!strategy) {
            patterns::services::logApp("[Engine] setSortStrategy: null strategy ignored\n");
            return;
        }
        sortStrategy_ = std::move(strategy);
        std::ostringstream oss;
        oss << "[Engine] Sort strategy set: " << sortStrategy_->name() << "\n";
        patterns::services::logApp(oss.str());
    }

    void sortVector(size_t index) {
        if (index >= data_.size()) {
            patterns::services::logApp("[Engine] Invalid vector index\n");
            return;
        }
        if (!sortStrategy_) {
            patterns::services::logApp("[Engine] No sort strategy — ignoring\n");
            return;
        }
        std::ostringstream oss;
        oss << "[Engine] Sorting with strategy: " << sortStrategy_->name() << "\n";
        patterns::services::logApp(oss.str());
        (*sortStrategy_)(data_[index]);
    }

    void printData() const {
        std::ostringstream oss;
        oss << "[Engine] Data:\n";
        for (size_t i = 0; i < data_.size(); ++i) {
            oss << "  [" << i << "]: ";
            for (int v : data_[i]) oss << v << " ";
            oss << "\n";
        }
        patterns::services::logApp(oss.str());
    }

    void onSessionEvent(const patterns::observer::SessionEvent& event) override {
        using patterns::observer::SessionEventType;

        patterns::services::logApp("[Engine] Event received: session state changed\n");

        switch (event.type) {
            case SessionEventType::VectorAdded:
                patterns::services::logApp("[Engine] -> recognized: vector added\n");
                addVector(event.vectorData);
                break;
            case SessionEventType::SortRequested:
                patterns::services::logApp("[Engine] -> recognized: sort requested\n");
                sortVector(event.index);
                break;
            case SessionEventType::PrintRequested:
                patterns::services::logApp("[Engine] -> recognized: print requested\n");
                printData();
                break;
            case SessionEventType::StrategyChangeRequested:
                patterns::services::logApp("[Engine] -> recognized: strategy change requested\n");
                if (auto f = factory_.lock()) {
                    if (auto s = f->create(event.strategyId))
                        setSortStrategy(std::move(*s));
                    else
                        patterns::services::logApp("[Engine] Unknown sort strategy — ignoring\n");
                } else {
                    patterns::services::logApp("[Engine] Factory not available — ignoring\n");
                }
                break;
            case SessionEventType::SessionClosing:
                patterns::services::logApp("[Engine] -> recognized: session closing, cleaning up and stopping\n");
                stop();
                break;
        }
    }

private:
    bool                                               running_ = false;
    std::vector<std::vector<int>>                      data_;
    std::unique_ptr<patterns::strategy::ISortStrategy> sortStrategy_;
    std::weak_ptr<patterns::strategy::ISortStrategyFactory>  factory_;
    std::weak_ptr<patterns::historian::EngineHistorian>      historian_;
    Writer                                                   manifestWriter_;
};

using Engine = BasicEngine<>;

} // namespace patterns::engine
