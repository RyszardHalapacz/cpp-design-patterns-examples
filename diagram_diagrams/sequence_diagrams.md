# UML Sequence Diagrams — program lifecycle

Six diagrams showing specific, chronologically ordered scenarios in the
program. Unlike the class diagram (which shows static structure), each of
these diagrams covers **one concrete execution flow over time** — which is
why no single diagram shows all classes at once, only those that actually
communicate with each other in a given scenario.

The blocks below are [mermaid.js](https://mermaid.js.org/) code — they render
automatically in GitHub, GitLab, Obsidian and VS Code (with the Markdown
Preview Mermaid Support extension).

---

## 1. GUI configuration at program startup

`Configurator` wires `DummyGui` to `SessionManagement` (five method pointers)
and sets the policy for allowed sort strategies.

```mermaid
sequenceDiagram
    autonumber
    participant Main as main()
    participant Cfg as Configurator
    participant Gui as DummyGui
    participant Session as SessionManagement

    Main->>Cfg: configureGui(gui, session)
    activate Cfg
    Cfg->>Gui: connectAddVector(session, ...)
    Cfg->>Gui: connectSortVector(session, ...)
    Cfg->>Gui: connectPrintData(session, ...)
    Cfg->>Gui: connectExecuteBatch(session, ...)
    deactivate Cfg
    Main->>Cfg: configureAllowedStrategies(session, ...)
    activate Cfg
    Cfg->>Session: setAllowedStrategies(allowed)
    deactivate Cfg
```

---

## 2. GUI click: add vector

`main()` calls `DummyGui.clickAddVector()`, which forwards the call
**via a method pointer** (set earlier by Configurator) — only then does it
reach `SessionManagement`, which broadcasts the event to `Engine`.

```mermaid
sequenceDiagram
    autonumber
    participant Main as main()
    participant Gui as DummyGui
    participant Session as SessionManagement
    participant Eng as Engine

    Main->>Gui: clickAddVector(vec)
    activate Gui
    Gui->>Gui: (session_->*addVectorFunc_)(vec)
    Note right of Gui: call via method pointer,<br/>set earlier by Configurator
    Gui->>Session: addVectorFromGui(vec)
    deactivate Gui
    activate Session
    Session->>Session: checkSession()
    Session->>Session: notify(VectorAdded)
    Session->>Eng: onSessionEvent(VectorAdded)
    activate Eng
    Eng->>Eng: addVector(vec)
    deactivate Eng
    deactivate Session
```

---

## 3. Session creation (`EngineSessionEstablisher.establish()`)

Template Method: fixed skeleton (`checkPreconditions()` → `connect()` →
`configure()` → `finalizeSetup()`). Note the `attach(&engine)` — it is a
self-call on `SessionManagement`, not a message to `Engine` (which at this
point receives nothing, it is only stored for later).

```mermaid
sequenceDiagram
    autonumber
    participant Main as main()
    participant Est as EngineSessionEstablisher
    participant Session as SessionManagement
    participant Eng as Engine

    Main->>Est: establish()
    activate Est
    Est->>Est: checkPreconditions()
    Est->>Session: connect()
    activate Session
    Session->>Session: connectToEngine(engine)
    Session->>Session: attach(&engine)
    Note right of Session: stores pointer in observers_,<br/>Engine receives nothing yet
    deactivate Session
    Est->>Est: configure()
    Est->>Session: finalizeSetup()
    activate Session
    Session->>Session: openSession()
    Session->>Eng: start()
    deactivate Session
    deactivate Est
```

---

## 4. Closing the session

`SessionManagement` broadcasts `SessionClosing` to both observers.
`AuditObserver` reacts by destroying itself (`delete this`) — hence the
**X** on its lifeline, exactly as in the classic UML destruction notation.

```mermaid
sequenceDiagram
    autonumber
    participant Main as main()
    participant Session as SessionManagement
    participant Eng as Engine
    participant Audit as AuditObserver

    Main->>Session: closeSession()
    activate Session
    Session->>Eng: onSessionEvent(SessionClosing)
    activate Eng
    Eng->>Eng: stop()
    deactivate Eng
    Session->>Audit: onSessionEvent(SessionClosing)
    activate Audit
    Audit->>Audit: delete this
    destroy Audit
    deactivate Session
```

---

## 5. Changing the sort strategy

Flow through `DummyGui` → `SessionManagement` → `notify()` (Observer) →
`Engine`, which **itself** calls `SortStrategyFactory` to build the new
strategy. `AuditObserver` receives the same event in parallel — so the
audit genuinely sees strategy-change requests.

```mermaid
sequenceDiagram
    autonumber
    participant Main as main()
    participant Gui as DummyGui
    participant Session as SessionManagement
    participant Eng as Engine
    participant Fac as SortStrategyFactory
    participant Audit as AuditObserver

    Main->>Gui: clickSetSortStrategy(Descending)
    activate Gui
    Gui->>Gui: (session_->*setSortStrategyFunc_)(id)
    Note right of Gui: call via method pointer,<br/>set earlier by Configurator
    Gui->>Session: setSortStrategyFromGui(id)
    deactivate Gui
    activate Session
    Session->>Session: isStrategyAllowed(id)
    Session->>Session: notify(StrategyChangeRequested)
    Session->>Eng: onSessionEvent(StrategyChangeRequested)
    activate Eng
    Eng->>Fac: create(id)
    activate Fac
    Fac-->>Eng: unique_ptr<DescendingSortStrategy>
    deactivate Fac
    Eng->>Eng: setSortStrategy(strategy)
    deactivate Eng
    Session->>Audit: onSessionEvent(StrategyChangeRequested)
    activate Audit
    Audit->>Audit: log("change to Descending")
    deactivate Audit
    deactivate Session
```

---

## 6. Program startup: service registration and simulated `FileLogger`

`FileLogger` does not actually write anything to disk — it only prints to
the console what command it would execute. This means its constructor
**cannot throw an exception** — there is no I/O operation that could fail.

```mermaid
sequenceDiagram
    autonumber
    participant Main as main()
    participant FL as FileLogger
    participant SL as ServiceLocator

    Main->>FL: new FileLogger("engine_log.txt")
    activate FL
    FL->>FL: std::cout << "command: create file" (simulation)
    deactivate FL
    Note right of Main: constructor CANNOT throw<br/>an exception — no real I/O

    Main->>SL: provide<FileLogger>(fileLogger)
    Main->>SL: appFileLogger()
    activate SL
    SL-->>Main: FileLogger&
    deactivate SL
    Main->>FL: log("=== Program start ===")
    activate FL
    FL->>FL: std::cout << "command: append to file" (simulation)
    deactivate FL
```

---

## UML notation legend (sequence diagram)

| Element | Meaning |
|---|---|
| Vertical dashed line | Object lifeline (from creation to destruction) |
| Narrow rectangle on the lifeline | Activation bar — the object is currently doing work |
| `A->>B: message` | Synchronous call from A to B |
| `A-->>B: message` | Return value (response) from A to B |
| `A->>A: message` | Self-call — a method called without an object qualifier (`this->method()`) |
| Circled number | Order of events over time (`autonumber`) |
| **X** at the end of the lifeline | Object destruction (`destroy`) |
| `Note right of X: ...` | Explanatory annotation, not a signal |
