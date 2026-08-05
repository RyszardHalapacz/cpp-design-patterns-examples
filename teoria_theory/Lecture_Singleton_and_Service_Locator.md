# Singleton — design pattern

## What is Singleton?

Singleton is a creational pattern whose goal is to guarantee that only
one instance of a given class exists in the program.

The class itself controls how its object is created and exposes a
global access point to that instance.

Conceptually it looks like this:

``` cpp
SomeClass::instance()
```

The code does not create an object directly — it retrieves a previously
prepared, shared instance.

------------------------------------------------------------------------

## What problem does it solve?

Singleton can be useful when the system genuinely needs exactly one
shared object, for example:

-   application configuration manager,
-   global settings registry,
-   object representing access to a specific device,
-   central diagnostics system,
-   process resource manager.

## Example implementation

``` cpp
class ApplicationConfig {
public:
    static ApplicationConfig& instance() {
        static ApplicationConfig object;
        return object;
    }

    ApplicationConfig(const ApplicationConfig&) = delete;
    ApplicationConfig& operator=(const ApplicationConfig&) = delete;

private:
    ApplicationConfig() = default;
};
```

Since C++11, initialisation of a local static variable is thread-safe.

## Advantages

-   single instance,
-   simple access,
-   lazy initialisation,
-   controlled object creation.

## Disadvantages

-   hides dependencies,
-   makes testing harder,
-   introduces global state.

## Alternative

Dependency Injection is the most common alternative, where dependencies
are passed through the constructor.

------------------------------------------------------------------------

# Service Locator — design pattern

## What is it?

Service Locator is a central registry of services.

A client can retrieve the required object:

``` cpp
locator.get<Logger>();
locator.get<Database>();
```

## Registering services

``` cpp
locator.provide<Logger>(logger);
locator.provide<Database>(database);
```

## Advantages

-   centralisation of services,
-   easy swapping of implementations,
-   convenient access to shared components.

## Disadvantages

-   hides dependencies,
-   makes testing harder,
-   can turn into a global service dump.

## Service Locator vs Dependency Injection

Service Locator:

``` cpp
auto& logger = locator.get<Logger>();
```

Dependency Injection:

``` cpp
class ReportGenerator {
public:
    explicit ReportGenerator(ILogger& logger)
        : logger_(logger) {}

private:
    ILogger& logger_;
};
```

Dependency Injection makes dependencies explicit, whereas Service Locator
hides them inside the implementation.

## Summary

Singleton is responsible for ensuring a single instance of an object.

Service Locator is responsible for locating services.

These are two distinct patterns that solve different problems.
