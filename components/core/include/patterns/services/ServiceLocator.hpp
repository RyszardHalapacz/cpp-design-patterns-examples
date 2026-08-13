#pragma once
#include <concepts>
#include <expected>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <typeindex>
#include <unordered_map>
#include "IService.hpp"
#include "Logger.hpp"
#include "FileLogger.hpp"
#include "DoSomething.hpp"

namespace patterns::services {

// Concept: TService must publicly derive from IService.
// Used in place of static_assert to catch constraint violations
// at the template declaration site with a cleaner compiler error.
template <typename TService>
concept ServiceType = std::derived_from<TService, IService>;

// ==================================
// SERVICE LOCATOR ERROR
// C++23: fallible operations return std::expected instead of throwing.
// ==================================
enum class ServiceLocatorErrorCode {
    NullService,
    ServiceNotFound,
    InvalidServiceType
};

struct ServiceLocatorError {
    ServiceLocatorErrorCode code;
    std::string serviceType;
};

inline const char* serviceLocatorErrorCodeName(ServiceLocatorErrorCode code) {
    switch (code) {
        case ServiceLocatorErrorCode::NullService:        return "NullService";
        case ServiceLocatorErrorCode::ServiceNotFound:    return "ServiceNotFound";
        case ServiceLocatorErrorCode::InvalidServiceType: return "InvalidServiceType";
    }
    return "UnknownServiceLocatorError";
}

inline std::string serviceLocatorErrorMessage(const ServiceLocatorError& error) {
    std::ostringstream oss;
    oss << "ServiceLocator: " << serviceLocatorErrorCodeName(error.code)
        << ", service type: " << error.serviceType;
    return oss.str();
}

// ==================================
// SERVICE LOCATOR
// Generic service registry indexed by type (std::type_index).
// Singleton — one instance per program.
//
// NOTE: Service Locator is widely considered an anti-pattern.
// The core problem is hidden dependencies: looking at Engine's constructor
// you cannot tell it needs a Logger — you only discover that by reading
// through the method bodies. This makes the class harder to understand,
// test (every test must pre-configure the global locator), and reuse.
// The idiomatic alternative is Dependency Injection — pass Logger (or
// ILogger) explicitly through the constructor so every dependency is
// visible at the call site.
// Service Locator is included here intentionally to demonstrate the
// pattern and contrast it with DI; it would not be the first choice
// in a production codebase.
// ==================================
class ServiceLocator {
public:
    using ServicePtr       = std::shared_ptr<IService>;
    using RuntimeGetResult = std::expected<ServicePtr, ServiceLocatorError>;

    static ServiceLocator& instance() {
        static ServiceLocator locator;
        return locator;
    }

    // Registration by template parameter (compile time).
    // Returns unexpected(NullService) if the shared_ptr is empty.
    template <ServiceType TService>
    std::expected<void, ServiceLocatorError>
    provide(std::shared_ptr<TService> service) {
        if (!service) {
            return std::unexpected(ServiceLocatorError{
                ServiceLocatorErrorCode::NullService,
                typeid(TService).name()
            });
        }
        services_[std::type_index(typeid(TService))] = std::move(service);
        return {};
    }

    // Retrieval by template parameter.
    // Returns unexpected(ServiceNotFound) when not registered.
    template <ServiceType TService>
    std::expected<std::reference_wrapper<TService>, ServiceLocatorError>
    tryGet() {
        const std::type_index key(typeid(TService));
        const auto it = services_.find(key);
        if (it == services_.end()) {
            return std::unexpected(ServiceLocatorError{
                ServiceLocatorErrorCode::ServiceNotFound,
                key.name()
            });
        }
        return std::ref(*std::static_pointer_cast<TService>(it->second));
    }

    // Registration via RTTI — key resolved from the actual dynamic type.
    // Returns unexpected(NullService) if the shared_ptr is empty.
    std::expected<void, ServiceLocatorError>
    provideRuntime(ServicePtr service) {
        if (!service) {
            return std::unexpected(ServiceLocatorError{
                ServiceLocatorErrorCode::NullService,
                "IService"
            });
        }
        const IService& ref = *service;
        services_[std::type_index(typeid(ref))] = std::move(service);
        return {};
    }

    // Runtime retrieval with dynamic_cast type check.
    // Returns unexpected(ServiceNotFound or InvalidServiceType).
    template <ServiceType TService>
    std::expected<std::reference_wrapper<TService>, ServiceLocatorError>
    tryGetRuntime() {
        const std::type_index key(typeid(TService));
        const auto it = services_.find(key);
        if (it == services_.end()) {
            return std::unexpected(ServiceLocatorError{
                ServiceLocatorErrorCode::ServiceNotFound,
                key.name()
            });
        }
        auto casted = std::dynamic_pointer_cast<TService>(it->second);
        if (!casted) {
            return std::unexpected(ServiceLocatorError{
                ServiceLocatorErrorCode::InvalidServiceType,
                key.name()
            });
        }
        return std::ref(*casted);
    }

    // Non-template variant — returns shared_ptr<IService> inside expected;
    // the caller is responsible for the final cast.
    RuntimeGetResult tryGetRuntime(const std::type_index& key) {
        const auto it = services_.find(key);
        if (it == services_.end()) {
            return std::unexpected(ServiceLocatorError{
                ServiceLocatorErrorCode::ServiceNotFound,
                key.name()
            });
        }
        return it->second;
    }

private:
    ServiceLocator() = default;
    std::unordered_map<std::type_index, ServicePtr> services_;
};

// ==================================
// Access shortcuts — return std::expected, not bare references.
// ==================================
inline auto appLogger() {
    return ServiceLocator::instance().tryGet<Logger>();
}

inline auto appFileLogger() {
    return ServiceLocator::instance().tryGet<FileLogger>();
}

inline auto appDoSomething() {
    return ServiceLocator::instance().tryGet<DoSomething>();
}

inline auto appDoSomethingRuntime() {
    return ServiceLocator::instance().tryGetRuntime<DoSomething>();
}

inline std::expected<std::reference_wrapper<DoSomething>, ServiceLocatorError>
appDoSomethingByPointer() {
    auto serviceResult = ServiceLocator::instance().tryGetRuntime(
        std::type_index(typeid(DoSomething)));
    if (!serviceResult) {
        return std::unexpected(serviceResult.error());
    }
    auto* casted = dynamic_cast<DoSomething*>(serviceResult->get());
    if (!casted) {
        return std::unexpected(ServiceLocatorError{
            ServiceLocatorErrorCode::InvalidServiceType,
            typeid(DoSomething).name()
        });
    }
    return std::ref(*casted);
}

// logApp — convenience wrapper used throughout the codebase instead of
// appLogger().log(...).  Handles a missing Logger gracefully: falls back to
// std::cerr so callers never need to repeat expected-handling boilerplate.
inline void logApp(const std::string& message) {
    auto result = appLogger();
    if (!result) {
        std::cerr << "[LOG ERROR] " << serviceLocatorErrorMessage(result.error()) << "\n";
        return;
    }
    result->get().log(message);
}

// logFile — logs via FileLogger using monadic transform; silently no-ops
// if FileLogger is not registered.
inline std::expected<void, ServiceLocatorError> logFile(const std::string& message) {
    return appFileLogger().transform(
        [&message](std::reference_wrapper<FileLogger> logger) {
            logger.get().log(message);
        });
}

} // namespace patterns::services
