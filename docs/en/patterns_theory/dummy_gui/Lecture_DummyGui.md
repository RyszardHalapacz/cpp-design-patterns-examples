
# Lecture: The `DummyGui` Class

## Introduction

`DummyGui` is a simplified, educational imitation of a GUI layer.

It does not use Qt, ImGui, or any other windowing library. Instead of real buttons it has methods such as:

```cpp
clickAddVector(...);
clickSortVector(...);
clickPrintData();
clickSetSortStrategy(...);
```

These methods simulate user actions.

The most important characteristic of the class is that `DummyGui` does not execute business logic. It does not sort data, does not store vectors, and does not create strategies. It only forwards requests to `SessionManagement`.

---

# Core Mechanism: Null Pointers at the Start

At the very beginning `DummyGui` has no access to any `SessionManagement` operation.

It stores:

```cpp
SessionManagement* session_ = nullptr;
```

and several pointers to member functions:

```cpp
AddVectorFunc addVectorFunc_ = nullptr;
SortVectorFunc sortVectorFunc_ = nullptr;
PrintDataFunc printDataFunc_ = nullptr;
ExecuteBatchFunc executeBatchFunc_ = nullptr;
SetSortStrategyFunc setSortStrategyFunc_ = nullptr;
```

These are initially null, raw pointers.

It is important to note that these are not ordinary pointers to free functions. They are **pointers to member functions of `SessionManagement`**.

Example type:

```cpp
using AddVectorFunc =
    void (SessionManagement::*)(const std::vector<int>&);
```

This means:

> A pointer to a member function of class `SessionManagement` that takes `const std::vector<int>&` and returns `void`.

A member function pointer alone is not enough. To call it you also need a concrete `SessionManagement` object.

That is why `DummyGui` stores two things:

```text
address of a SessionManagement object
+
address of a selected SessionManagement method
```

Only the combination of these two elements allows an operation to be executed.

---

# Who Assigns the Real Functions?

`DummyGui` does not choose which methods it has access to.

That is done by another class:

```cpp
Configurator
```

Example:

```cpp
gui.connectAddVector(
    &session,
    &SessionManagement::addVectorFromGui
);
```

`Configurator` passes:

1. the address of a concrete `SessionManagement` object,
2. a pointer to a concrete method of that object.

It connects the remaining functions in the same way:

```cpp
gui.connectSortVector(
    &session,
    &SessionManagement::sortVectorFromGui
);

gui.connectPrintData(
    &session,
    &SessionManagement::printDataFromGui
);

gui.connectExecuteBatch(
    &session,
    &SessionManagement::executeBatch
);

gui.connectSetSortStrategy(
    &session,
    &SessionManagement::setSortStrategyFromGui
);
```

This gives `Configurator` full control over what `DummyGui` is allowed to do.

This is not just plain "wiring". It is also a simple permission-granting mechanism.

`DummyGui` can only invoke the operations that have been explicitly assigned to it.

---

# State Before Configuration

Without calling `Configurator::configureGui()` the object looks logically like this:

```text
DummyGui
├── session_ = nullptr
├── addVectorFunc_ = nullptr
├── sortVectorFunc_ = nullptr
├── printDataFunc_ = nullptr
├── executeBatchFunc_ = nullptr
└── setSortStrategyFunc_ = nullptr
```

In this state, attempting to click an operation does not cause a memory error.

The method first checks for access:

```cpp
if (!session_ || !addVectorFunc_) {
    appLogger().log("[GUI] No access to AddVector\n");
    return;
}
```

This allows the class to safely refuse to execute the operation.

---

# State After Configuration

After calling:

```cpp
configurator.configureGui(gui, session);
```

the object looks logically like this:

```text
DummyGui
├── session_ --------------------------> SessionManagement
├── addVectorFunc_ --------------------> addVectorFromGui
├── sortVectorFunc_ -------------------> sortVectorFromGui
├── printDataFunc_ --------------------> printDataFromGui
├── executeBatchFunc_ -----------------> executeBatch
└── setSortStrategyFunc_ --------------> setSortStrategyFromGui
```

From this point on the `click...()` methods can forward requests to the session.

---

# Member Function Pointer Types

The class defines type aliases:

```cpp
using AddVectorFunc =
    void (SessionManagement::*)(const std::vector<int>&);

using SortVectorFunc =
    void (SessionManagement::*)(size_t);

using PrintDataFunc =
    void (SessionManagement::*)();

using ExecuteBatchFunc =
    void (SessionManagement::*)(const CommandBatch&);

using SetSortStrategyFunc =
    void (SessionManagement::*)(SortStrategyId);
```

Each alias describes the exact signature of the method that can be connected.

For example, a method taking `size_t` cannot be assigned to `AddVectorFunc` because its type is different.

The compiler therefore enforces type safety.

---

# The `connect...()` Methods

Each connection method stores:

- the address of the session,
- the pointer to the selected method.

Example:

```cpp
void connectAddVector(
    SessionManagement* session,
    AddVectorFunc func
) {
    session_ = session;
    addVectorFunc_ = func;
}
```

This method does not add a vector. It only prepares the connection that `clickAddVector()` will later use.

---

# Invoking a Member Function Pointer

The key syntax looks like this:

```cpp
(session_->*addVectorFunc_)(vec);
```

It can be read as follows:

1. `session_` — select the concrete `SessionManagement` object,
2. `addVectorFunc_` — select the assigned method,
3. `->*` — combine the object pointer with the member function pointer,
4. `(vec)` — call the method with the argument.

This is not an ordinary function pointer call. The `->*` operator is specifically designed for pointers to member functions.

---

# `clickAddVector()`

```cpp
void clickAddVector(const std::vector<int>& vec)
```

This method simulates clicking the add-data button.

It first checks whether:

- a session is connected,
- an add-vector method is assigned.

Then it calls the assigned function:

```cpp
(session_->*addVectorFunc_)(vec);
```

Full flow:

```text
User
    │
    ▼
DummyGui::clickAddVector()
    │
    ▼
SessionManagement::addVectorFromGui()
    │
    ▼
notify(VectorAdded)
    │
    ▼
Engine::onSessionEvent()
    │
    ▼
Engine::addVector()
```

---

# `clickSortVector()`

```cpp
void clickSortVector(size_t index)
```

Simulates clicking the sort button.

Does not sort directly. Calls the method assigned by `Configurator`:

```cpp
(session_->*sortVectorFunc_)(index);
```

---

# `clickPrintData()`

```cpp
void clickPrintData()
```

Forwards the print-data request to `SessionManagement`.

```cpp
(session_->*printDataFunc_)();
```

---

# `clickSetSortStrategy()`

```cpp
void clickSetSortStrategy(SortStrategyId id)
```

Forwards the strategy-change request.

`DummyGui` does not create strategies. It does not use `SortStrategyFactory` directly.

The flow looks like this:

```text
DummyGui
    │
    ▼
SessionManagement
    │
    ▼
Engine
    │
    ▼
SortStrategyFactory
    │
    ▼
ISortStrategy
```

---

# Cooperation with Builder

`DummyGui` also contains:

```cpp
CommandBatchBuilder batchBuilder_;
```

This allows it to not execute operations immediately but instead collect them into a batch.

## `queueAddVector()`

```cpp
DummyGui& queueAddVector(const std::vector<int>& vec)
```

Adds a command to the builder and returns `*this`.

This enables a fluent interface:

```cpp
gui
    .queueAddVector({3, 1, 2})
    .queueSortVector(0)
    .queuePrintData();
```

## `queueSortVector()`

Adds a sort command to the batch.

## `queuePrintData()`

Adds a print-data command.

## `buildBatch()`

```cpp
CommandBatch buildBatch()
```

Finalises the batch and returns the finished `CommandBatch`.

## `flushBatch()`

```cpp
void flushBatch()
```

Builds the batch and then forwards it to `SessionManagement::executeBatch()`.

Here too the operation will only work if `Configurator` granted the GUI access to `ExecuteBatch`.

---

# Is `DummyGui` a Real GUI?

No.

It is a stub presentation layer used for educational and testing purposes.

A real interface could be implemented in:

- Qt,
- ImGui,
- a console application,
- a web interface,
- a REST API.

The important thing is that each of these layers could use a similar `SessionManagement` API.

---

# Is `DummyGui` Well Designed?

From an educational standpoint the mechanism is interesting because it demonstrates:

- pointers to member functions,
- manual wiring,
- control over available operations,
- separation of GUI from `Engine`,
- cooperation with Builder.

It should be noted, however, that this is a fairly artificial and low-level solution.

In a production GUI one would more commonly use:

- an explicit facade interface,
- a reference or pointer to that interface,
- `std::function`,
- a signal-and-slot system,
- commands,
- dependency injection.

The raw member-function pointers are used here deliberately to show that the scope of GUI capabilities can be granted only at configuration time.

---

# Which Patterns Does It Use?

`DummyGui` does not directly implement a single classic GoF pattern, but it cooperates with several:

- **Facade** — communicates with `SessionManagement`,
- **Builder** — creates `CommandBatch`,
- **Observer** — its requests are later converted into events,
- **Strategy** — indirectly requests a strategy change,
- **Factory** — indirectly causes a strategy to be created,
- **Dependency Injection / manual wiring** — `Configurator` assigns available operations to it.

---

# Key Advantage

`DummyGui` does not know `Engine` and does not execute business logic.

It only knows which operations were made available to it during configuration.

This means:

- GUI is separated from the engine,
- business logic does not leak into the presentation layer,
- available operations can be restricted,
- the GUI layer is easier to replace.

---

# Full Flow

```text
             Configurator
                  │
                  │ assigns session object
                  │ and method pointers
                  ▼
              DummyGui
                  │
             click...()
                  │
                  ▼
          SessionManagement
                  │
             notify(event)
                  │
                  ▼
               Engine
                  │
                  ▼
              Strategy
```

---

# Summary

`DummyGui` is a simplified presentation layer.

At the start it stores a null pointer to `SessionManagement` and null pointers to its methods. Only `Configurator` assigns the concrete functions that the GUI is allowed to use.

This means the GUI knows exclusively what the configuration allowed it. It does not know `Engine`, does not create strategies, and does not execute business logic.

This is a deliberately educational solution: it demonstrates manual wiring, pointers to member functions, operation delegation, and separation of the presentation layer from the rest of the application.

---

# Full Class Code

```cpp
class DummyGui {
public:
    using AddVectorFunc = void (SessionManagement::*)(const std::vector<int>&);
    using SortVectorFunc = void (SessionManagement::*)(size_t);
    using PrintDataFunc = void (SessionManagement::*)();
    using ExecuteBatchFunc = void (SessionManagement::*)(const CommandBatch&);
    using SetSortStrategyFunc = void (SessionManagement::*)(SortStrategyId);

    void connectAddVector(SessionManagement* session, AddVectorFunc func) {
        session_ = session;
        addVectorFunc_ = func;
    }

    void connectSortVector(SessionManagement* session, SortVectorFunc func) {
        session_ = session;
        sortVectorFunc_ = func;
    }

    void connectPrintData(SessionManagement* session, PrintDataFunc func) {
        session_ = session;
        printDataFunc_ = func;
    }

    void connectExecuteBatch(SessionManagement* session, ExecuteBatchFunc func) {
        session_ = session;
        executeBatchFunc_ = func;
    }

    void connectSetSortStrategy(SessionManagement* session, SetSortStrategyFunc func) {
        session_ = session;
        setSortStrategyFunc_ = func;
    }

    void clickAddVector(const std::vector<int>& vec) {
        if (!session_ || !addVectorFunc_) {
            appLogger().log("[GUI] No access to AddVector\n");
            return;
        }
        appLogger().log("[GUI] Clicked AddVector\n");
        (session_->*addVectorFunc_)(vec);
    }

    void clickSortVector(size_t index) {
        if (!session_ || !sortVectorFunc_) {
            appLogger().log("[GUI] No access to SortVector\n");
            return;
        }
        appLogger().log("[GUI] Clicked SortVector\n");
        (session_->*sortVectorFunc_)(index);
    }

    void clickPrintData() {
        if (!session_ || !printDataFunc_) {
            appLogger().log("[GUI] No access to PrintData\n");
            return;
        }
        appLogger().log("[GUI] Clicked PrintData\n");
        (session_->*printDataFunc_)();
    }

    void clickSetSortStrategy(SortStrategyId id) {
        if (!session_ || !setSortStrategyFunc_) {
            appLogger().log("[GUI] No access to SetSortStrategy\n");
            return;
        }
        appLogger().log("[GUI] Clicked SetSortStrategy\n");
        (session_->*setSortStrategyFunc_)(id);
    }

    // ==================================
    // BUILDER — GUI collects commands instead of sending them immediately
    // ==================================
    DummyGui& queueAddVector(const std::vector<int>& vec) {
        appLogger().log("[GUI] Adding AddVector to command batch\n");
        batchBuilder_.addVector(vec);
        return *this;
    }

    DummyGui& queueSortVector(size_t index) {
        appLogger().log("[GUI] Adding SortVector to command batch\n");
        batchBuilder_.sortVector(index);
        return *this;
    }

    DummyGui& queuePrintData() {
        appLogger().log("[GUI] Adding PrintData to command batch\n");
        batchBuilder_.printData();
        return *this;
    }

    CommandBatch buildBatch() {
        appLogger().log("[GUI] Closing command batch, ready to send\n");
        return batchBuilder_.build();
    }

    // Sends the built batch to the session — only if Configurator granted access
    void flushBatch() {
        if (!session_ || !executeBatchFunc_) {
            appLogger().log("[GUI] No access to ExecuteBatch\n");
            return;
        }
        CommandBatch batch = buildBatch();
        appLogger().log("[GUI] Sending command batch to session\n");
        (session_->*executeBatchFunc_)(batch);
    }

private:
    SessionManagement* session_ = nullptr;

    AddVectorFunc addVectorFunc_ = nullptr;
    SortVectorFunc sortVectorFunc_ = nullptr;
    PrintDataFunc printDataFunc_ = nullptr;
    ExecuteBatchFunc executeBatchFunc_ = nullptr;
    SetSortStrategyFunc setSortStrategyFunc_ = nullptr;

    CommandBatchBuilder batchBuilder_;
};
```

---

# `Configurator` Code Responsible for Connecting Functions

The following code shows the class that assigns real `SessionManagement` methods to `DummyGui`.

```cpp
class Configurator {
public:
    void configureGui(DummyGui& gui, SessionManagement& session) {
        appLogger().log("[Configurator] Granting GUI access to selected functions\n");

        gui.connectAddVector(&session, &SessionManagement::addVectorFromGui);
        gui.connectSortVector(&session, &SessionManagement::sortVectorFromGui);
        gui.connectPrintData(&session, &SessionManagement::printDataFromGui);
        gui.connectExecuteBatch(&session, &SessionManagement::executeBatch);
        gui.connectSetSortStrategy(&session, &SessionManagement::setSortStrategyFromGui);
    }

    // Configurator is the single place that sets policy: which sort strategies
    // are allowed to be swapped at runtime
    void configureAllowedStrategies(SessionManagement& session,
                                     std::vector<SortStrategyId> allowed) {
        appLogger().log("[Configurator] Setting allowed sort strategies\n");
        session.setAllowedStrategies(std::move(allowed));
    }
};
```
