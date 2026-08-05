# Lecture — Evolution of Service Locator: From Exceptions to `std::expected`

## Goal

In the first version of the project `ServiceLocator` returns a service or throws
an exception. This solution is correct but not always optimal. In C++23
`std::expected` appeared, which allows returning an error as part of the
function's return value.

---

# Current Implementation

Interface:

```cpp
template<typename TService>
TService& get();
```

Implementation:

```cpp
template<typename TService>
TService& get()
{
    auto it = services_.find(std::type_index(typeid(TService)));

    if (it == services_.end())
    {
        throw std::runtime_error(
            std::string("ServiceLocator: no service registered for type ")
            + typeid(TService).name());
    }

    return *std::static_pointer_cast<TService>(it->second);
}
```

Usage:

```cpp
Logger& logger = ServiceLocator::instance().get<Logger>();
logger.log("Program start");
```

If the service is absent:

```text
get<Logger>()
      |
      v
throw std::runtime_error(...)
```

---

# Drawbacks of the Exception Approach

This API assumes that the absence of a service is an exceptional situation.

If a client only wants to check whether a service exists, it must use:

```cpp
try
{
    auto& logger = ServiceLocator::instance().get<Logger>();
    logger.log("Hello");
}
catch(...)
{
    ...
}
```

Exceptions start serving as ordinary control flow.

---

# What Is `std::expected`?

`std::expected<T, E>` means:

- success → an object of type `T`,
- error → a value of type `E`.

No exceptions. The client decides how to handle failure.

---

# New Interface

```cpp
template<typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError>
tryGet();
```

---

# Error Type

```cpp
enum class ServiceLocatorError
{
    ServiceNotFound,
    InvalidServiceType
};
```

---

# Implementation

```cpp
template<typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError>
tryGet()
{
    auto it = services_.find(std::type_index(typeid(TService)));

    if (it == services_.end())
        return std::unexpected(ServiceLocatorError::ServiceNotFound);

    auto service =
        std::dynamic_pointer_cast<TService>(it->second);

    if (!service)
        return std::unexpected(ServiceLocatorError::InvalidServiceType);

    return std::ref(*service);
}
```

---

# Usage

```cpp
auto logger =
    ServiceLocator::instance().tryGet<Logger>();

if (!logger)
{
    std::cout << "Logger does not exist\n";
    return;
}

logger->get().log("Program start");
```

No exceptions.

---

# Monadic Interface in C++23

`std::expected` offers the operations:

- `transform()`
- `and_then()`
- `or_else()`

Example:

```cpp
ServiceLocator::instance()
    .tryGet<Logger>()
    .transform([](auto logger)
    {
        logger.get().log("Program start");
    })
    .or_else([](auto)
    {
        std::cout << "Logger was not found\n";
    });
```

If retrieving the service succeeds, `transform()` executes.
If an error occurs, `or_else()` executes.

No multiple `if` statements are needed.

---

# Should `get()` Be Removed?

The best solution is to keep two methods.

## Mandatory Services

```cpp
template<typename TService>
TService& getRequired();
```

If the service is absent:

```text
throw
```

The program is misconfigured and should terminate.

---

## Optional Services

```cpp
template<typename TService>
std::expected<
    std::reference_wrapper<TService>,
    ServiceLocatorError>
tryGet();
```

The client decides what to do.

---

# Comparison

## Exceptions

```cpp
try
{
    auto& logger = locator.getRequired<Logger>();
    logger.log("Hello");
}
catch(...)
{
    ...
}
```

## `std::expected`

```cpp
auto logger = locator.tryGet<Logger>();

if (!logger)
    return;

logger->get().log("Hello");
```

## `std::expected` + Monads

```cpp
locator
    .tryGet<Logger>()
    .transform([](auto logger)
    {
        logger.get().log("Hello");
    })
    .or_else(reportError);
```

---

# Conclusions

- `throw` is a good choice for configuration errors and mandatory services.
- `std::expected` works well when the absence of a service is a predictable
  outcome of an operation.
- The monadic interface (`transform`, `and_then`, `or_else`) allows building
  readable operation chains without multiple `if` statements.
- Keeping both `getRequired()` and `tryGet()` gives two different API contracts:
  **fail fast** for required services and **explicit error handling** for
  optional services.
