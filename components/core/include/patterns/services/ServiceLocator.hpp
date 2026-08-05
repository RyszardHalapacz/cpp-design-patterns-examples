#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>
#include "IService.hpp"
#include "Logger.hpp"
#include "FileLogger.hpp"
#include "DoSomething.hpp"

namespace patterns::services {

// ==================================
// SERVICE LOCATOR
// Generic service registry indexed by type (std::type_index).
// Singleton — one instance per program.
// ==================================
class ServiceLocator {
public:
    static ServiceLocator& instance() {
        static ServiceLocator locator;
        return locator;
    }

    // Registration by template parameter (compile time)
    template <typename TService>
    void provide(std::shared_ptr<TService> service) {
        static_assert(std::is_base_of<IService, TService>::value,
                      "TService must inherit from IService");
        services_[std::type_index(typeid(TService))] = std::move(service);
    }

    template <typename TService>
    TService& get() {
        auto it = services_.find(std::type_index(typeid(TService)));
        if (it == services_.end()) {
            throw std::runtime_error(
                std::string("ServiceLocator: no registered service of type ")
                + typeid(TService).name());
        }
        return *std::static_pointer_cast<TService>(it->second);
    }

    // Registration via RTTI (runtime) — key resolved dynamically
    void provideRuntime(std::shared_ptr<IService> service) {
        const IService& ref = *service;
        services_[std::type_index(typeid(ref))] = std::move(service);
    }

    template <typename TService>
    TService& getRuntime() {
        auto it = services_.find(std::type_index(typeid(TService)));
        if (it == services_.end()) {
            throw std::runtime_error(
                std::string("ServiceLocator: no registered service of type ")
                + typeid(TService).name());
        }
        auto casted = std::dynamic_pointer_cast<TService>(it->second);
        if (!casted) {
            throw std::runtime_error(
                std::string("ServiceLocator: service at this key is not of type ")
                + typeid(TService).name());
        }
        return *casted;
    }

    // Non-template variant — returns raw IService pointer
    std::shared_ptr<IService> getRuntime(const std::type_index& key) {
        auto it = services_.find(key);
        if (it == services_.end()) {
            throw std::runtime_error(
                std::string("ServiceLocator: no registered service of type ")
                + key.name());
        }
        return it->second;
    }

private:
    ServiceLocator() = default;
    std::unordered_map<std::type_index, std::shared_ptr<IService>> services_;
};

// ==================================
// Access shortcuts — used throughout the project instead of
// ServiceLocator::instance().get<...>()
// ==================================
inline Logger& appLogger() {
    return ServiceLocator::instance().get<Logger>();
}

inline FileLogger& appFileLogger() {
    return ServiceLocator::instance().get<FileLogger>();
}

inline DoSomething& appDoSomething() {
    return ServiceLocator::instance().get<DoSomething>();
}

inline DoSomething& appDoSomethingRuntime() {
    return ServiceLocator::instance().getRuntime<DoSomething>();
}

inline DoSomething& appDoSomethingByPointer() {
    auto service = ServiceLocator::instance().getRuntime(
        std::type_index(typeid(DoSomething)));
    auto* casted = dynamic_cast<DoSomething*>(service.get());
    if (!casted) {
        throw std::runtime_error(
            "appDoSomethingByPointer: service under this type is not DoSomething");
    }
    return *casted;
}

} // namespace patterns::services
