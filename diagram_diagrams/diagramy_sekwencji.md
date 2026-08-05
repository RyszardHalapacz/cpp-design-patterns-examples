# Diagramy sekwencji UML — cykl życia programu

Sześć diagramów pokazujących konkretne, chronologicznie uporządkowane
scenariusze w programie. W odróżnieniu od diagramu klas (który pokazuje
statyczną strukturę), każdy z tych diagramów dotyczy **jednego konkretnego
przebiegu w czasie** — dlatego żaden pojedynczy diagram nie pokazuje
wszystkich klas naraz, tylko te, które faktycznie ze sobą rozmawiają
w danym scenariuszu.

Bloki poniżej to kod [mermaid.js](https://mermaid.js.org/) — renderuje się
automatycznie w GitHub, GitLab, Obsidian i VS Code (z wtyczką Markdown
Preview Mermaid Support).

---

## 1. Konfiguracja GUI na starcie programu

`Configurator` okablowuje `DummyGui` do `SessionManagement` (pięć wskaźników
do metod) i ustala politykę dozwolonych strategii sortowania.

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

## 2. Kliknięcie w GUI: dodanie wektora

`main()` woła `DummyGui.clickAddVector()`, które przekazuje wywołanie dalej
**przez wskaźnik do metody** (ustawiony wcześniej przez Configuratora) —
dopiero to trafia do `SessionManagement`, które rozsyła zdarzenie do `Engine`.

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
    Note right of Gui: wywołanie przez wskaźnik do metody,<br/>ustawiony wcześniej przez Configuratora
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

## 3. Tworzenie sesji (`EngineSessionEstablisher.establish()`)

Template Method: stały szkielet (`checkPreconditions()` → `connect()` →
`configure()` → `finalizeSetup()`). Zwróć uwagę na `attach(&engine)` —
to samowywołanie na `SessionManagement`, nie wiadomość do `Engine`
(które w tym momencie nic nie dostaje, tylko zostaje zapamiętane).

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
    Note right of Session: zapisuje wskaźnik do observers_,<br/>Engine nic nie dostaje w tym momencie
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

## 4. Zamykanie sesji

`SessionManagement` rozsyła `SessionClosing` do obu obserwatorów.
`AuditObserver` w reakcji sam się niszczy (`delete this`) — stąd znaczek
**X** na jego linii życia, dokładnie jak przy zniszczeniu obiektu w
klasycznej notacji UML.

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

## 5. Zmiana strategii sortowania

Przepływ przez `DummyGui` → `SessionManagement` → `notify()` (Observer) →
`Engine`, który **sam** woła `SortStrategyFactory`, żeby zbudować nową
strategię. `AuditObserver` dostaje to samo zdarzenie równolegle —
dzięki temu audyt faktycznie widzi żądania zmiany strategii.

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
    Note right of Gui: wywołanie przez wskaźnik do metody,<br/>ustawiony wcześniej przez Configuratora
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
    Audit->>Audit: log("zmiana na Descending")
    deactivate Audit
    deactivate Session
```

---

## 6. Start programu: rejestracja usług i symulowany `FileLogger`

`FileLogger` niczego realnie nie zapisuje na dysk — tylko wypisuje na
konsolę, jaką komendę by wykonał. Dzięki temu jego konstruktor **nie może
rzucić wyjątku** — nie ma żadnej operacji I/O, która mogłaby się nie udać.

```mermaid
sequenceDiagram
    autonumber
    participant Main as main()
    participant FL as FileLogger
    participant SL as ServiceLocator

    Main->>FL: new FileLogger("engine_log.txt")
    activate FL
    FL->>FL: std::cout << "wykonuję komendę: utwórz plik" (symulacja)
    deactivate FL
    Note right of Main: konstruktor NIE może już rzucić<br/>wyjątku — brak realnego I/O

    Main->>SL: provide<FileLogger>(fileLogger)
    Main->>SL: appFileLogger()
    activate SL
    SL-->>Main: FileLogger&
    deactivate SL
    Main->>FL: log("=== Start programu ===")
    activate FL
    FL->>FL: std::cout << "wykonuję komendę: dopisz do pliku" (symulacja)
    deactivate FL
```

---

## Legenda notacji UML (diagram sekwencji)

| Element | Znaczenie |
|---|---|
| Pionowa przerywana linia | Linia życia obiektu (od utworzenia do zniszczenia) |
| Wąski prostokąt na linii życia | Pasek aktywacji — obiekt aktualnie wykonuje pracę |
| `A->>B: wiadomość` | Wywołanie synchroniczne z A do B |
| `A-->>B: wiadomość` | Zwrot wartości (odpowiedź) z A do B |
| `A->>A: wiadomość` | Samowywołanie — metoda wołana bez kwalifikatora obiektu (`this->metoda()`) |
| Numer w kółku | Kolejność zdarzeń w czasie (`autonumber`) |
| **X** na końcu linii życia | Zniszczenie obiektu (`destroy`) |
| `Note right of X: ...` | Adnotacja wyjaśniająca, niebędąca sygnałem |
