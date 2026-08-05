# Modifying `ServiceLocator` to `std::expected` — Full C++23 Version

## Scope of This Lecture

This material is a continuation of the earlier `ServiceLocator` lecture.
We do not revisit:

- the idea of the Service Locator pattern,
- the idea of Singleton,
- the reasons for using `IService`,
- the basics of `std::unordered_map`, `std::type_index`, and RTTI.

We focus exclusively on rebuilding the existing class:

```cpp
class ServiceLocator;
```

from the version that signalled errors via `throw` to the C++23 version that
returns:

```cpp
std::expected<T, ServiceLocatorError>
```

We will rebuild all operations belonging to the class:

- `provide<TService>()`,
- `tryGet<TService>()`,
- `provideRuntime()`,
- `tryGetRuntime<TService>()`,
- `tryGetRuntime(const std::type_index&)`.

The full compilable class code and usage examples for each API variant are at
the end.

---

# 1. Problem in the Previous Version

In the original class, operations that retrieved a service returned a reference
or pointer directly:

```cpp
template <typename TService>
TService& get();

template <typename TService>
TService& getRuntime();

std::shared_ptr<IService> getRuntime(const std::type_index& key);
```

If the entry was not in the map, the method executed:

```cpp
throw std::runtime_error(...);
```

In the runtime variant a failed cast could also occur:

```cpp
auto casted = std::dynamic_pointer_cast<TService>(it->second);

if (!casted) {
    throw std::runtime_error(...);
}
```

After the modification, errors will no longer propagate outside the function as
exceptions. They become part of the return type.

The scheme of the change looks like this:

```text
old version:

result or throw

new version:

std::expected<result, ServiceLocatorError>
```

---

# 2. Required Headers

The full version of the class uses the following standard library elements:

```cpp
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
```

The important new headers are:

```cpp
#include <expected>
#include <functional>
```

`<expected>` provides:

```cpp
std::expected
std::unexpected
```

`<functional>` provides:

```cpp
std::reference_wrapper
std::ref
```

---

# 3. Error Type `ServiceLocatorError`

Before writing the class we must define the type that describes failure.

```cpp
enum class ServiceLocatorErrorCode
{
    NullService,
    ServiceNotFound,
    InvalidServiceType
};
```

The codes describe *what kind of problem occurred*:

- `NullService` — attempt to register an empty `shared_ptr`,
- `ServiceNotFound` — no entry in the map under the requested key,
- `InvalidServiceType` — an entry exists but the object cannot be safely cast to
  the requested type.

The error code alone does not say which type the operation involved. We therefore
use a struct:

```cpp
struct ServiceLocatorError
{
    ServiceLocatorErrorCode code;
    std::string serviceType;
};
```

The field:

```cpp
ServiceLocatorErrorCode code;
```

allows handling the error programmatically, e.g. via `switch`.

The field:

```cpp
std::string serviceType;
```

stores the name of the type or key involved in the error.

Example value:

```cpp
ServiceLocatorError{
    ServiceLocatorErrorCode::ServiceNotFound,
    typeid(Logger).name()
};
```

We do not store only a ready-made text message. The error code matters because
client code may react differently to different cases:

```cpp
switch (error.code)
{
case ServiceLocatorErrorCode::NullService:
    // registration error
    break;

case ServiceLocatorErrorCode::ServiceNotFound:
    // missing entry
    break;

case ServiceLocatorErrorCode::InvalidServiceType:
    // type mismatch
    break;
}
```

---

# 4. Full Code of the Modified Class

Below is the complete version of the class. This is the central listing of the
lecture.

```cpp
class ServiceLocator
{
public:
    using ServicePtr = std::shared_ptr<IService>;
    using RuntimeGetResult =
        std::expected<ServicePtr, ServiceLocatorError>;

    static ServiceLocator& instance()
    {
        static ServiceLocator locator;
        return locator;
    }

    template <typename TService>
    std::expected<void, ServiceLocatorError>
    provide(std::shared_ptr<TService> service)
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService must inherit from IService"
        );

        if (!service)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::NullService,
                    typeid(TService).name()
                }
            );
        }

        const std::type_index key(typeid(TService));
        services_[key] = std::move(service);

        return {};
    }

    template <typename TService>
    std::expected<
        std::reference_wrapper<TService>,
        ServiceLocatorError
    >
    tryGet()
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService must inherit from IService"
        );

        const std::type_index key(typeid(TService));
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        return std::ref(
            *std::static_pointer_cast<TService>(it->second)
        );
    }

    std::expected<void, ServiceLocatorError>
    provideRuntime(ServicePtr service)
    {
        if (!service)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::NullService,
                    "IService"
                }
            );
        }

        const IService& serviceRef = *service;
        const std::type_index key(typeid(serviceRef));

        services_[key] = std::move(service);

        return {};
    }

    template <typename TService>
    std::expected<
        std::reference_wrapper<TService>,
        ServiceLocatorError
    >
    tryGetRuntime()
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService must inherit from IService"
        );

        const std::type_index key(typeid(TService));
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        auto casted =
            std::dynamic_pointer_cast<TService>(it->second);

        if (!casted)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::InvalidServiceType,
                    key.name()
                }
            );
        }

        return std::ref(*casted);
    }

    RuntimeGetResult
    tryGetRuntime(const std::type_index& key)
    {
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        return it->second;
    }

private:
    ServiceLocator() = default;

    std::unordered_map<
        std::type_index,
        ServicePtr
    > services_;
};
```

The following chapters discuss each part of this class separately.

---

# 5. Type Aliases

At the start of the class there are two aliases:

```cpp
using ServicePtr = std::shared_ptr<IService>;
```

and:

```cpp
using RuntimeGetResult =
    std::expected<ServicePtr, ServiceLocatorError>;
```

## `ServicePtr`

Instead of repeatedly writing:

```cpp
std::shared_ptr<IService>
```

the class uses the shorter name:

```cpp
ServicePtr
```

The alias does not create a new type. It is simply another name for the same
type.

## `RuntimeGetResult`

Without the alias the signature of the non-template method would be:

```cpp
std::expected<
    std::shared_ptr<IService>,
    ServiceLocatorError
>
tryGetRuntime(const std::type_index& key);
```

With the alias:

```cpp
RuntimeGetResult
tryGetRuntime(const std::type_index& key);
```

The alias precisely describes the contract of the method:

```text
success → shared_ptr<IService>
error   → ServiceLocatorError
```

---

# 6. `instance()`

The method remains unchanged:

```cpp
static ServiceLocator& instance()
{
    static ServiceLocator locator;
    return locator;
}
```

Migrating to `std::expected` does not affect how the `ServiceLocator` object is
accessed.

There is no result here that could represent a predictable failure. Therefore
this method should not return `std::expected`.

---

# 7. `provide<TService>()`

Full method:

```cpp
template <typename TService>
std::expected<void, ServiceLocatorError>
provide(std::shared_ptr<TService> service)
{
    static_assert(
        std::is_base_of_v<IService, TService>,
        "TService must inherit from IService"
    );

    if (!service)
    {
        return std::unexpected(
            ServiceLocatorError{
                ServiceLocatorErrorCode::NullService,
                typeid(TService).name()
            }
        );
    }

    const std::type_index key(typeid(TService));
    services_[key] = std::move(service);

    return {};
}
```

## 7.1. Return Type

Previously the method returned:

```cpp
void
```

The new version returns:

```cpp
std::expected<void, ServiceLocatorError>
```

`void` in the first parameter means:

> The operation can succeed, but on success there is no additional value to
> return.

We therefore have two states:

```text
success → expected holds the success state but no value
error   → expected holds ServiceLocatorError
```

## 7.2. `static_assert`

```cpp
static_assert(
    std::is_base_of_v<IService, TService>,
    "TService must inherit from IService"
);
```

This part is not replaced by `std::expected`.

The reason is fundamental: the absence of `IService` inheritance is an error
detected at compile time, while `std::expected` describes the result of an
operation executed at runtime.

## 7.3. Null `shared_ptr` Check

```cpp
if (!service)
```

In the previous version it was possible to call:

```cpp
locator.provide<Logger>(nullptr);
```

The map would then contain a `Logger` key but the assigned value would be a null
pointer. A subsequent retrieval and dereference could lead to undefined behavior.

The new version stops such an error at registration time:

```cpp
return std::unexpected(
    ServiceLocatorError{
        ServiceLocatorErrorCode::NullService,
        typeid(TService).name()
    }
);
```

## 7.4. `std::unexpected`

`std::expected` represents success. To explicitly create an error state we use:

```cpp
std::unexpected(error)
```

## 7.5. Returning Success

```cpp
return {};
```

For the type:

```cpp
std::expected<void, ServiceLocatorError>
```

an empty initialiser creates the success state.

## 7.6. Usage of `provide()`

```cpp
auto result = ServiceLocator::instance().provide<Logger>(
    std::make_shared<Logger>()
);

if (!result)
{
    const ServiceLocatorError& error = result.error();
    // handle error
}
```

On a valid service:

```cpp
result.has_value() == true
```

On a null pointer:

```cpp
result.has_value() == false
result.error().code == ServiceLocatorErrorCode::NullService
```

---

# 8. `tryGet<TService>()`

Full method:

```cpp
template <typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError
>
tryGet()
{
    static_assert(
        std::is_base_of_v<IService, TService>,
        "TService must inherit from IService"
    );

    const std::type_index key(typeid(TService));
    const auto it = services_.find(key);

    if (it == services_.end())
    {
        return std::unexpected(
            ServiceLocatorError{
                ServiceLocatorErrorCode::ServiceNotFound,
                key.name()
            }
        );
    }

    return std::ref(
        *std::static_pointer_cast<TService>(it->second)
    );
}
```

## 8.1. Name Change

The previous method was named:

```cpp
get<TService>()
```

The new method is named:

```cpp
tryGet<TService>()
```

The `try` prefix informs API clients that the operation may not return the
requested service and the result must be checked.

## 8.2. Success Type

The method cannot return:

```cpp
std::expected<TService&, ServiceLocatorError>
```

`std::expected` cannot hold a reference type as `T`.

Therefore we use:

```cpp
std::reference_wrapper<TService>
```

Meaning:

```text
success → reference wrapped in reference_wrapper
error   → ServiceLocatorError
```

## 8.3. Why Not Return `shared_ptr<TService>`?

This approach preserves the semantics of the previous `get()` method, which
returned:

```cpp
TService&
```

`std::reference_wrapper<TService>` is the closest equivalent of a reference
inside `std::expected`.

Semantic difference:

- `reference_wrapper` gives access to the object but does not share ownership,
- `shared_ptr` increases the owner count and can extend the service lifetime.

In our class the owner of the service is the `services_` map.

## 8.4. Classic Usage

```cpp
auto result =
    ServiceLocator::instance().tryGet<Logger>();

if (!result)
{
    const auto& error = result.error();
    // handle error
    return;
}

Logger& logger = result->get();
logger.log("Hello\n");
```

---

# 9. `provideRuntime()`

Full method:

```cpp
std::expected<void, ServiceLocatorError>
provideRuntime(ServicePtr service)
{
    if (!service)
    {
        return std::unexpected(
            ServiceLocatorError{
                ServiceLocatorErrorCode::NullService,
                "IService"
            }
        );
    }

    const IService& serviceRef = *service;
    const std::type_index key(typeid(serviceRef));

    services_[key] = std::move(service);

    return {};
}
```

## 9.1. Argument Type

The method accepts:

```cpp
ServicePtr service
```

i.e.:

```cpp
std::shared_ptr<IService> service
```

Unlike `provide<TService>()` the method does not receive a concrete type as a
template parameter.

## 9.2. Return Type

Like `provide<TService>()` the method returns:

```cpp
std::expected<void, ServiceLocatorError>
```

Registration produces no useful value. It can, however, fail if the pointer is
null.

## 9.3. Null Pointer Check

This check must occur before the dereference:

```cpp
if (!service)
```

Without it the next line:

```cpp
const IService& serviceRef = *service;
```

would dereference a null pointer.

In the error we record the type `"IService"` because the method has no `TService`
parameter, and for a null pointer the actual type of the object cannot be read.

## 9.4. Determining the Runtime Type

```cpp
const IService& serviceRef = *service;
const std::type_index key(typeid(serviceRef));
```

After confirming the pointer is not null, dereference is safe.

Because `IService` is polymorphic, `typeid(serviceRef)` returns the dynamic type
of the object.

For:

```cpp
std::shared_ptr<IService> service =
    std::make_shared<DoSomething>();
```

the key will be:

```cpp
typeid(DoSomething)
```

## 9.5. Usage

```cpp
std::shared_ptr<IService> service =
    std::make_shared<DoSomething>();

auto result =
    ServiceLocator::instance().provideRuntime(service);

if (!result)
{
    // handle registration error
}
```

---

# 10. `tryGetRuntime<TService>()`

Full method:

```cpp
template <typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError
>
tryGetRuntime()
{
    static_assert(
        std::is_base_of_v<IService, TService>,
        "TService must inherit from IService"
    );

    const std::type_index key(typeid(TService));
    const auto it = services_.find(key);

    if (it == services_.end())
    {
        return std::unexpected(
            ServiceLocatorError{
                ServiceLocatorErrorCode::ServiceNotFound,
                key.name()
            }
        );
    }

    auto casted =
        std::dynamic_pointer_cast<TService>(it->second);

    if (!casted)
    {
        return std::unexpected(
            ServiceLocatorError{
                ServiceLocatorErrorCode::InvalidServiceType,
                key.name()
            }
        );
    }

    return std::ref(*casted);
}
```

## 10.1. Contract

The method returns the same type as `tryGet<TService>()`:

```cpp
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError
>
```

The difference is not in the result but in how type verification is performed.

## 10.2. Safe Runtime Cast

```cpp
auto casted =
    std::dynamic_pointer_cast<TService>(it->second);
```

Unlike `static_pointer_cast`, `dynamic_pointer_cast` performs a runtime type
check.

If the cast fails the pointer is null:

```cpp
if (!casted)
```

## 10.3. Two Possible Errors

```text
1. ServiceNotFound
2. InvalidServiceType
```

## 10.4. Usage

```cpp
auto result =
    ServiceLocator::instance()
        .tryGetRuntime<DoSomething>();

if (!result)
{
    const ServiceLocatorError& error = result.error();
    return;
}

result->get().do_();
```

---

# 11. Non-Template `tryGetRuntime(key)`

Full method:

```cpp
RuntimeGetResult
tryGetRuntime(const std::type_index& key)
{
    const auto it = services_.find(key);

    if (it == services_.end())
    {
        return std::unexpected(
            ServiceLocatorError{
                ServiceLocatorErrorCode::ServiceNotFound,
                key.name()
            }
        );
    }

    return it->second;
}
```

## 11.1. Return Type

The alias:

```cpp
using RuntimeGetResult =
    std::expected<ServicePtr, ServiceLocatorError>;
```

expands to:

```cpp
std::expected<
    std::shared_ptr<IService>,
    ServiceLocatorError
>
```

This method does not return `reference_wrapper` because its earlier version
returned:

```cpp
std::shared_ptr<IService>
```

Its previous semantics are therefore preserved.

## 11.2. Usage

```cpp
auto result =
    ServiceLocator::instance().tryGetRuntime(
        std::type_index(typeid(DoSomething))
    );

if (!result)
{
    return;
}

std::shared_ptr<IService> service = *result;

auto* concrete =
    dynamic_cast<DoSomething*>(service.get());

if (concrete)
{
    concrete->do_();
}
```

`ServiceLocator` does not perform the final cast to the concrete service here.
It only returns `shared_ptr<IService>`. Responsibility for further type
interpretation belongs to the client.

---

# 12. The `services_` Field

```cpp
std::unordered_map<
    std::type_index,
    ServicePtr
> services_;
```

After expanding the alias:

```cpp
std::unordered_map<
    std::type_index,
    std::shared_ptr<IService>
> services_;
```

The field itself does not need to change during the migration to `std::expected`.

`std::expected` changes how public methods return results and errors. It does not
change how services are stored.

---

# 13. Private Constructor

```cpp
ServiceLocator() = default;
```

This part also remains unchanged.

It returns no result and performs no operation that would require explicit error
handling via `std::expected`.

---

# 14. Full Code with Helper Types

The following listing contains everything required to use the modified class:

- headers,
- `IService`,
- error codes,
- error struct,
- complete `ServiceLocator` class.

```cpp
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>

class IService
{
public:
    virtual ~IService() = default;
};

enum class ServiceLocatorErrorCode
{
    NullService,
    ServiceNotFound,
    InvalidServiceType
};

struct ServiceLocatorError
{
    ServiceLocatorErrorCode code;
    std::string serviceType;
};

class ServiceLocator
{
public:
    using ServicePtr = std::shared_ptr<IService>;
    using RuntimeGetResult =
        std::expected<ServicePtr, ServiceLocatorError>;

    static ServiceLocator& instance()
    {
        static ServiceLocator locator;
        return locator;
    }

    template <typename TService>
    std::expected<void, ServiceLocatorError>
    provide(std::shared_ptr<TService> service)
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService must inherit from IService"
        );

        if (!service)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::NullService,
                    typeid(TService).name()
                }
            );
        }

        const std::type_index key(typeid(TService));
        services_[key] = std::move(service);

        return {};
    }

    template <typename TService>
    std::expected<
        std::reference_wrapper<TService>,
        ServiceLocatorError
    >
    tryGet()
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService must inherit from IService"
        );

        const std::type_index key(typeid(TService));
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        return std::ref(
            *std::static_pointer_cast<TService>(it->second)
        );
    }

    std::expected<void, ServiceLocatorError>
    provideRuntime(ServicePtr service)
    {
        if (!service)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::NullService,
                    "IService"
                }
            );
        }

        const IService& serviceRef = *service;
        const std::type_index key(typeid(serviceRef));

        services_[key] = std::move(service);

        return {};
    }

    template <typename TService>
    std::expected<
        std::reference_wrapper<TService>,
        ServiceLocatorError
    >
    tryGetRuntime()
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService must inherit from IService"
        );

        const std::type_index key(typeid(TService));
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        auto casted =
            std::dynamic_pointer_cast<TService>(it->second);

        if (!casted)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::InvalidServiceType,
                    key.name()
                }
            );
        }

        return std::ref(*casted);
    }

    RuntimeGetResult
    tryGetRuntime(const std::type_index& key)
    {
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        return it->second;
    }

private:
    ServiceLocator() = default;

    std::unordered_map<
        std::type_index,
        ServicePtr
    > services_;
};
```

---

# 15. Error-to-String Function

The class itself returns a structured error. Code that displays a message can
live outside the class:

```cpp
std::string toString(const ServiceLocatorError& error)
{
    switch (error.code)
    {
    case ServiceLocatorErrorCode::NullService:
        return "ServiceLocator: null service pointer for type "
            + error.serviceType;

    case ServiceLocatorErrorCode::ServiceNotFound:
        return "ServiceLocator: service not found for type "
            + error.serviceType;

    case ServiceLocatorErrorCode::InvalidServiceType:
        return "ServiceLocator: invalid service type for key "
            + error.serviceType;
    }

    return "ServiceLocator: unknown error";
}
```

Separating the error code from the text gives two possibilities:

```cpp
if (error.code == ServiceLocatorErrorCode::ServiceNotFound)
{
    // programmatic reaction
}
```

and:

```cpp
std::cerr << toString(error) << '\n';
```

`ServiceLocator` does not need to log its own errors. This is especially
important when the service that failed to be retrieved is the `Logger` itself.

---

# 16. Full Examples of All Methods

Assuming the following services:

```cpp
class Logger : public IService
{
public:
    void log(const std::string& message)
    {
        std::cout << message;
    }
};

class DoSomething : public IService
{
public:
    void do_()
    {
        std::cout << "do something\n";
    }
};
```

## 16.1. `provide<Logger>()`

```cpp
auto provideResult =
    ServiceLocator::instance().provide<Logger>(
        std::make_shared<Logger>()
    );

if (!provideResult)
{
    std::cerr
        << toString(provideResult.error())
        << '\n';
}
```

## 16.2. `tryGet<Logger>()`

```cpp
auto loggerResult =
    ServiceLocator::instance().tryGet<Logger>();

if (!loggerResult)
{
    std::cerr
        << toString(loggerResult.error())
        << '\n';
}
else
{
    loggerResult->get().log("Hello\n");
}
```

## 16.3. `provideRuntime()`

```cpp
std::shared_ptr<IService> runtimeService =
    std::make_shared<DoSomething>();

auto runtimeProvideResult =
    ServiceLocator::instance().provideRuntime(
        std::move(runtimeService)
    );

if (!runtimeProvideResult)
{
    std::cerr
        << toString(runtimeProvideResult.error())
        << '\n';
}
```

## 16.4. `tryGetRuntime<DoSomething>()`

```cpp
auto typedRuntimeResult =
    ServiceLocator::instance()
        .tryGetRuntime<DoSomething>();

if (!typedRuntimeResult)
{
    std::cerr
        << toString(typedRuntimeResult.error())
        << '\n';
}
else
{
    typedRuntimeResult->get().do_();
}
```

## 16.5. `tryGetRuntime(type_index)`

```cpp
auto untypedRuntimeResult =
    ServiceLocator::instance().tryGetRuntime(
        std::type_index(typeid(DoSomething))
    );

if (!untypedRuntimeResult)
{
    std::cerr
        << toString(untypedRuntimeResult.error())
        << '\n';
}
else
{
    std::shared_ptr<IService> service =
        *untypedRuntimeResult;

    auto* concrete =
        dynamic_cast<DoSomething*>(service.get());

    if (concrete)
    {
        concrete->do_();
    }
}
```

---

# 17. Using Monadic Operations

`std::expected` in C++23 provides monadic operations. The most natural in this
class are `transform()` and `or_else()`.

## 17.1. `transform()` for a Retrieved Service

```cpp
ServiceLocator::instance()
    .tryGet<Logger>()
    .transform([](std::reference_wrapper<Logger> logger)
    {
        logger.get().log("Hello\n");
    });
```

The lambda executes only if `tryGet<Logger>()` returns success.

If the result is `ServiceNotFound`, `transform()` does not call the lambda and
passes the error on.

## 17.2. `or_else()` for an Error

```cpp
ServiceLocator::instance()
    .tryGet<Logger>()
    .or_else([](const ServiceLocatorError& error)
        -> std::expected<
            std::reference_wrapper<Logger>,
            ServiceLocatorError
        >
    {
        std::cerr << toString(error) << '\n';
        return std::unexpected(error);
    });
```

The lambda passed to `or_else()` must return a type compatible with the required
operation result.

## 17.3. `and_then()`

`and_then()` is intended for a next step that also returns `std::expected`.

Example:

```cpp
std::expected<void, ServiceLocatorError>
useLogger(std::reference_wrapper<Logger> logger)
{
    logger.get().log("Hello\n");
    return {};
}

ServiceLocator::instance()
    .tryGet<Logger>()
    .and_then(useLogger);
```

Flow:

```text
tryGet<Logger>()
       |
       | success
       v
useLogger(...)

or

tryGet<Logger>()
       |
       | error
       v
useLogger is not called
```

## 17.4. `transform_error()`

C++23 also provides:

```cpp
transform_error()
```

which allows changing the type or form of an error.

Example — converting the struct to a string:

```cpp
auto result =
    ServiceLocator::instance()
        .tryGet<Logger>()
        .transform_error([](const ServiceLocatorError& error)
        {
            return toString(error);
        });
```

The type of `result` becomes:

```cpp
std::expected<
    std::reference_wrapper<Logger>,
    std::string
>
```

The success value stays the same, but the error changes from:

```cpp
ServiceLocatorError
```

to:

```cpp
std::string
```

---

# 18. Full Compilable Example Program

```cpp
#include <expected>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>

class IService
{
public:
    virtual ~IService() = default;
};

class Logger : public IService
{
public:
    void log(const std::string& message)
    {
        std::cout << message;
    }
};

class DoSomething : public IService
{
public:
    void do_()
    {
        std::cout << "do something\n";
    }
};

enum class ServiceLocatorErrorCode
{
    NullService,
    ServiceNotFound,
    InvalidServiceType
};

struct ServiceLocatorError
{
    ServiceLocatorErrorCode code;
    std::string serviceType;
};

std::string toString(const ServiceLocatorError& error)
{
    switch (error.code)
    {
    case ServiceLocatorErrorCode::NullService:
        return "ServiceLocator: null service pointer for type "
            + error.serviceType;

    case ServiceLocatorErrorCode::ServiceNotFound:
        return "ServiceLocator: service not found for type "
            + error.serviceType;

    case ServiceLocatorErrorCode::InvalidServiceType:
        return "ServiceLocator: invalid service type for key "
            + error.serviceType;
    }

    return "ServiceLocator: unknown error";
}

class ServiceLocator
{
public:
    using ServicePtr = std::shared_ptr<IService>;
    using RuntimeGetResult =
        std::expected<ServicePtr, ServiceLocatorError>;

    static ServiceLocator& instance()
    {
        static ServiceLocator locator;
        return locator;
    }

    template <typename TService>
    std::expected<void, ServiceLocatorError>
    provide(std::shared_ptr<TService> service)
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService must inherit from IService"
        );

        if (!service)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::NullService,
                    typeid(TService).name()
                }
            );
        }

        const std::type_index key(typeid(TService));
        services_[key] = std::move(service);

        return {};
    }

    template <typename TService>
    std::expected<
        std::reference_wrapper<TService>,
        ServiceLocatorError
    >
    tryGet()
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService must inherit from IService"
        );

        const std::type_index key(typeid(TService));
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        return std::ref(
            *std::static_pointer_cast<TService>(it->second)
        );
    }

    std::expected<void, ServiceLocatorError>
    provideRuntime(ServicePtr service)
    {
        if (!service)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::NullService,
                    "IService"
                }
            );
        }

        const IService& serviceRef = *service;
        const std::type_index key(typeid(serviceRef));

        services_[key] = std::move(service);

        return {};
    }

    template <typename TService>
    std::expected<
        std::reference_wrapper<TService>,
        ServiceLocatorError
    >
    tryGetRuntime()
    {
        static_assert(
            std::is_base_of_v<IService, TService>,
            "TService must inherit from IService"
        );

        const std::type_index key(typeid(TService));
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        auto casted =
            std::dynamic_pointer_cast<TService>(it->second);

        if (!casted)
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::InvalidServiceType,
                    key.name()
                }
            );
        }

        return std::ref(*casted);
    }

    RuntimeGetResult
    tryGetRuntime(const std::type_index& key)
    {
        const auto it = services_.find(key);

        if (it == services_.end())
        {
            return std::unexpected(
                ServiceLocatorError{
                    ServiceLocatorErrorCode::ServiceNotFound,
                    key.name()
                }
            );
        }

        return it->second;
    }

private:
    ServiceLocator() = default;

    std::unordered_map<
        std::type_index,
        ServicePtr
    > services_;
};

int main()
{
    ServiceLocator& locator =
        ServiceLocator::instance();

    auto loggerRegistration =
        locator.provide<Logger>(
            std::make_shared<Logger>()
        );

    if (!loggerRegistration)
    {
        std::cerr
            << toString(loggerRegistration.error())
            << '\n';
        return 1;
    }

    auto logger = locator.tryGet<Logger>();

    if (!logger)
    {
        std::cerr
            << toString(logger.error())
            << '\n';
        return 1;
    }

    logger->get().log("Logger retrieved via tryGet\n");

    std::shared_ptr<IService> runtimeService =
        std::make_shared<DoSomething>();

    auto runtimeRegistration =
        locator.provideRuntime(
            std::move(runtimeService)
        );

    if (!runtimeRegistration)
    {
        std::cerr
            << toString(runtimeRegistration.error())
            << '\n';
        return 1;
    }

    auto typedRuntime =
        locator.tryGetRuntime<DoSomething>();

    if (!typedRuntime)
    {
        std::cerr
            << toString(typedRuntime.error())
            << '\n';
        return 1;
    }

    typedRuntime->get().do_();

    auto untypedRuntime =
        locator.tryGetRuntime(
            std::type_index(typeid(DoSomething))
        );

    if (!untypedRuntime)
    {
        std::cerr
            << toString(untypedRuntime.error())
            << '\n';
        return 1;
    }

    auto* concrete =
        dynamic_cast<DoSomething*>(
            untypedRuntime->get()
        );

    if (!concrete)
    {
        std::cerr << "Client: DoSomething cast failed\n";
        return 1;
    }

    concrete->do_();

    locator.tryGet<Logger>()
        .transform([](std::reference_wrapper<Logger> service)
        {
            service.get().log(
                "Logger retrieved via transform\n"
            );
        });

    return 0;
}
```

Compile with C++23:

```bash
g++ -std=c++23 -Wall -Wextra -pedantic main.cpp
```

---

# 19. Old vs New API Comparison

## Template Registration

Old version:

```cpp
template <typename TService>
void provide(std::shared_ptr<TService> service);
```

New version:

```cpp
template <typename TService>
std::expected<void, ServiceLocatorError>
provide(std::shared_ptr<TService> service);
```

New version can explicitly report `NullService`.

## Template Retrieval

Old version:

```cpp
template <typename TService>
TService& get();
```

New version:

```cpp
template <typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError
>
tryGet();
```

New version returns `ServiceNotFound` instead of throwing.

## Runtime Registration

Old version:

```cpp
void provideRuntime(std::shared_ptr<IService> service);
```

New version:

```cpp
std::expected<void, ServiceLocatorError>
provideRuntime(ServicePtr service);
```

New version checks for a null pointer before `typeid(*service)`.

## Templated Runtime Retrieval

Old version:

```cpp
template <typename TService>
TService& getRuntime();
```

New version:

```cpp
template <typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError
>
tryGetRuntime();
```

Possible errors:

```text
ServiceNotFound
InvalidServiceType
```

## Non-Template Runtime Retrieval

Old version:

```cpp
std::shared_ptr<IService>
getRuntime(const std::type_index& key);
```

New version:

```cpp
RuntimeGetResult
tryGetRuntime(const std::type_index& key);
```

i.e.:

```cpp
std::expected<
    std::shared_ptr<IService>,
    ServiceLocatorError
>
```

---

# 20. Key Design Decisions in This Modification

## 20.1. `static_assert` Remains

Errors detectable at compile time are still compile-time errors.
`std::expected` does not replace the type system.

## 20.2. Registration Also Returns `expected`

The original `provide()` and `provideRuntime()` returned `void` but accepted a
`shared_ptr`. A null pointer is a predictable input error, so the full migration
covers these methods too.

## 20.3. Reference Retrieval Uses `reference_wrapper`

`std::expected<T&, E>` is not allowed. `std::reference_wrapper<T>` preserves
reference-access semantics without transferring co-ownership to the client.

## 20.4. Non-Template Runtime Still Returns `shared_ptr`

This method previously transferred co-ownership to the client. The migration to
`std::expected` should not unnecessarily change its semantics.

## 20.5. Error Has a Code and Data

`ServiceLocatorError` is not just a string. The error code enables programmatic
reaction while `serviceType` provides diagnostic context.

## 20.6. The Class Does Not Log Its Own Errors

`ServiceLocator` returns the error to the client. It does not attempt to retrieve
`Logger` in order to report the absence of `Logger`, as that would create a
circular dependency.

## 20.7. `value()` Should Not Replace Result Checking

It is possible to write:

```cpp
locator.tryGet<Logger>().value().get().log("Hello");
```

but on error `value()` will throw `std::bad_expected_access`.

This negates the main benefit of the migration. A correct client should:

- check the result via `if`, or
- use monadic operations.

---

# 21. Final Form of the Contracts

After migration the public API of the class has the following contracts:

```cpp
// Register a type known at compile time.
// Success: no value.
// Error: NullService.
template <typename TService>
std::expected<void, ServiceLocatorError>
provide(std::shared_ptr<TService> service);
```

```cpp
// Retrieve a type known at compile time.
// Success: reference to the service.
// Error: ServiceNotFound.
template <typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError
>
tryGet();
```

```cpp
// Register by dynamic type of the object.
// Success: no value.
// Error: NullService.
std::expected<void, ServiceLocatorError>
provideRuntime(ServicePtr service);
```

```cpp
// Retrieve with runtime type checking.
// Success: reference to the service.
// Errors: ServiceNotFound, InvalidServiceType.
template <typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError
>
tryGetRuntime();
```

```cpp
// Non-template retrieval by a given key.
// Success: shared pointer to IService.
// Error: ServiceNotFound.
RuntimeGetResult
tryGetRuntime(const std::type_index& key);
```

---

# Summary

The modification does not change how services are stored or the lookup mechanism
via `std::type_index`. It changes the contract of all operations that can fail.

Key results of the refactoring:

```text
throw runtime_error
        ↓
std::unexpected(ServiceLocatorError)
```

```text
TService&
        ↓
expected<reference_wrapper<TService>, Error>
```

```text
shared_ptr<IService>
        ↓
expected<shared_ptr<IService>, Error>
```

```text
void provide(...)
        ↓
expected<void, Error> provide(...)
```

After the change every predictable failure is visible directly in the function
signature. The client can handle the result classically via `if`, transform the
error via `transform_error()`, or build an operation chain using `transform()`
and `and_then()`.
