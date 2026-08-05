# Lecture: `DummyGui` as a Separate Component — Static Library, C-Style API, and `unique_ptr`

---

## Introduction

In the original version of the project `DummyGui` was a plain class compiled
together with the rest of the application.

In the current version `DummyGui` is a **separate component** — a static
library (`dummy_gui_lib`) that lives in the `components/dummy_gui/` subdirectory
and has its own `CMakeLists.txt`.

This change is intentional. It reflects a real separation of responsibilities:
the GUI layer is independent from the engine, has its own lifecycle, and its own
version.

---

## Component Structure

```
components/dummy_gui/
├── CMakeLists.txt
├── dummy_gui.yml.in
├── include/
│   └── patterns/
│       ├── config/
│       │   └── Configurator.hpp
│       └── gui/
│           ├── CommandBatchBuilder.hpp
│           └── DummyGui.hpp
├── src/
│   ├── CommandBatchBuilder.cpp
│   └── Configurator.cpp
└── tests/
    ├── CMakeLists.txt
    └── test_gui.cpp
```

The component builds a static library `dummy_gui_lib` and its own test suite
`dummy_gui_tests`.

---

## Why a Static Library?

A static library (`add_library(dummy_gui_lib STATIC ...)`) allows:

- **isolating GUI code** — changes to `DummyGui` do not require rebuilding the
  entire project,
- **testing independently** — GUI tests live inside the component and do not mix
  with engine tests,
- **versioning separately** — the component has its own version
  (`DUMMY_GUI_VERSION`) and generates its own `dummy_gui.yml` file,
- **laying groundwork for future extraction** — the component could eventually
  move to its own repository.

The main application and tests link `dummy_gui_lib`:

```cmake
# application
target_link_libraries(patterns PRIVATE dummy_gui_lib)

# main tests
target_link_libraries(patterns_tests PRIVATE dummy_gui_lib GTest::gtest_main)
```

Headers are available via include paths set as `PUBLIC`, so after linking it is
sufficient to write:

```cpp
#include "patterns/gui/DummyGui.hpp"
```

---

## Simulating a Legacy C API — `makeGUI` and `deleteGUI`

### Where Does This Idea Come From?

Many old C libraries manage resources through a pair of functions:

```c
// SDL
SDL_Window* window = SDL_CreateWindow(...);
SDL_DestroyWindow(window);

// libcurl
CURL* handle = curl_easy_init();
curl_easy_cleanup(handle);

// OpenAL
ALCdevice* device = alcOpenDevice(nullptr);
alcCloseDevice(device);
```

The pattern is always the same:

1. A factory function returns a raw pointer (`C*`).
2. The caller is responsible for calling the cleanup function.
3. Failing to call the cleanup function means a resource leak.

`DummyGui` deliberately simulates this style. The constructor is **private** —
an object can only be obtained through `makeGUI`.

```cpp
// The only way to create the object
DummyGui* gui = makeGUI();

// The only way to destroy it
deleteGUI(gui);
```

### Implementation

```cpp
template<typename Writer = ComponentManifestWriter>
class BasicDummyGui {
public:
    friend BasicDummyGui* makeGUI(std::filesystem::path manifestPath);
    friend void           deleteGUI(BasicDummyGui* gui);

    // ... public methods ...

private:
    explicit BasicDummyGui(std::filesystem::path manifestPath = {});
    // ... members ...
};

using DummyGui = BasicDummyGui<>;

inline DummyGui* makeGUI(std::filesystem::path manifestPath = {}) {
    return new DummyGui(std::move(manifestPath));
}

inline void deleteGUI(DummyGui* gui) {
    delete gui;
}
```

The `friend` declaration inside the class ensures that only `makeGUI` and
`deleteGUI` can call the private constructor and destructor.

---

## The Problem with Raw Pointers

Code using the C-style API looks like this:

```cpp
DummyGui* gui = makeGUI(path);

// ... use gui ...

deleteGUI(gui);  // easy to forget
```

If an exception is thrown between `makeGUI` and `deleteGUI` — or if the
programmer simply forgets to call `deleteGUI` — there is a **memory leak**.

This is exactly the same problem that affects old C libraries.

---

## Solution — `unique_ptr` with `std::default_delete`

Modern C++ solves this problem through **RAII** (Resource Acquisition Is
Initialization).

Instead of calling `deleteGUI` manually, we can wrap the raw pointer in a
`std::unique_ptr`.

### Specialising `std::default_delete`

`std::unique_ptr<T>` calls `delete ptr` by default on destruction.

We want it to call `deleteGUI(ptr)` instead.

To achieve this we define a `std::default_delete` specialisation at the point
of use (in `DummyGui.hpp`):

```cpp
namespace std {
template<>
struct default_delete<patterns::gui::DummyGui> {
    void operator()(patterns::gui::DummyGui* gui) const {
        patterns::gui::deleteGUI(gui);
    }
};
} // namespace std
```

From this point on `unique_ptr<DummyGui>` automatically calls `deleteGUI` on
destruction.

### Usage in the Application

```cpp
std::unique_ptr<patterns::gui::DummyGui> gui(
    patterns::gui::makeGUI(fs::path(DUMMY_GUI_MANIFEST_PATH))
);

// ... use gui via -> ...
gui->clickAddVector({3, 1, 2});

// deleteGUI called automatically — even on exception
```

There is no need to call `deleteGUI` manually. `unique_ptr` will do so when it
goes out of scope or when `reset()` is called.

---

## Separation of Responsibilities

```text
C-style API layer (dummy_gui_lib)
   makeGUI()    →  creates raw DummyGui*
   deleteGUI()  →  destroys raw DummyGui*

Application layer (main.cpp)
   unique_ptr<DummyGui>  →  RAII wrapper
   std::default_delete   →  bridges both worlds
```

The library remains written in C style — raw pointers, explicit resource
management.

The application uses it through a thin wrapper that eliminates the risk of a
leak.

---

## Why Is the `default_delete` Specialisation in the Header, Not in the Application?

The `std::default_delete` specialisation defines **how the library is used**, not
its implementation.

The library (`dummy_gui_lib`) should not dictate to consumers how they manage
resources.

Different consumers may have different needs:

- `main.cpp` uses `unique_ptr` — convenient RAII,
- unit tests in `dummy_gui_tests` use raw pointers — they directly test the
  C-style API,
- a future consumer might use a custom allocator.

Placing the specialisation in the header is a deliberate decision when the goal
is to make `unique_ptr<DummyGui>` work everywhere — in both the application and
the integration tests. This ensures all consumers automatically get safe
lifetime management without repeating the specialisation.

---

## Summary

| Mechanism | Role |
|---|---|
| Private constructor | Forces use of `makeGUI`/`deleteGUI` |
| `friend makeGUI` / `friend deleteGUI` | The only authorised creation and destruction points |
| C API simulation | Educational demonstration of raw-pointer problems |
| `std::default_delete<DummyGui>` | Bridges C-style API with modern RAII |
| `unique_ptr<DummyGui>` | Safe resource management in the application |
| Static library `dummy_gui_lib` | Component isolation, independent versioning and tests |
