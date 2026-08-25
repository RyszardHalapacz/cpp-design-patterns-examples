# Wykład: `SessionEstablisher` i `EngineSessionEstablisher`

## Wprowadzenie

Para klas `SessionEstablisher` oraz `EngineSessionEstablisher` jest
praktycznym przykładem wzorca **Template Method**.

Zamiast sztucznego przykładu (np. przygotowanie kawy), wzorzec został
zastosowany do rzeczywistego problemu: **ustanowienia sesji pomiędzy
`SessionManagement` i `Engine`**.

------------------------------------------------------------------------

# Główna odpowiedzialność

`SessionEstablisher` definiuje **szkielet algorytmu inicjalizacji
sesji**, natomiast klasy pochodne dostarczają implementacje
poszczególnych kroków.

Stała kolejność wygląda następująco:

``` text
establish()
│
├── checkPreconditions()
├── connect()
├── configure()
└── finalizeSetup()
```

To właśnie jest istota wzorca **Template Method** -- kolejność kroków
jest niezmienna.

------------------------------------------------------------------------

# Dlaczego `establish()` nie jest virtual?

Algorytm ma być identyczny dla wszystkich implementacji.

Podklasy mogą zmieniać sposób wykonania pojedynczych kroków, ale nie
mogą zmienić ich kolejności.

------------------------------------------------------------------------

# Hook methods

`checkPreconditions()` oraz `configure()` posiadają domyślne
implementacje.

Są to tzw. **hook methods** -- podklasa może je nadpisać, ale nie musi.

Przykładowe zastosowania:

-   sprawdzenie dostępności silnika,
-   weryfikacja licencji,
-   konfiguracja timeoutów,
-   ustawienie parametrów pracy.

------------------------------------------------------------------------

# Operacje obowiązkowe

Metody:

-   `connect()`
-   `finalizeSetup()`

są metodami czysto abstrakcyjnymi.

Każda klasa pochodna musi określić:

-   jak nawiązać połączenie,
-   jak zakończyć inicjalizację.

------------------------------------------------------------------------

# `EngineSessionEstablisher`

Ta klasa jest konkretną implementacją algorytmu.

Realizuje:

-   połączenie `SessionManagement` z `Engine`,
-   opcjonalną konfigurację,
-   otwarcie sesji.

Przepływ:

``` text
main()
    │
    ▼
EngineSessionEstablisher
    │
    ▼
establish()
    │
    ├── checkPreconditions()
    ├── connect()
    ├── configure()
    └── finalizeSetup()
              │
              ▼
      SessionManagement
              │
              ▼
           Engine
```

------------------------------------------------------------------------

# Zalety

-   bardzo czytelny algorytm,
-   brak duplikacji kodu,
-   łatwe dodawanie nowych sposobów ustanawiania sesji,
-   zgodność z Open/Closed Principle,
-   praktyczny przykład wzorca Template Method.

------------------------------------------------------------------------

# Podsumowanie

`SessionEstablisher` odpowiada za **algorytm**, natomiast
`EngineSessionEstablisher` za jego konkretną realizację.

To bardzo dobry przykład zastosowania wzorca **Template Method** w
kodzie infrastrukturalnym, gdzie ważniejsza od pojedynczych operacji
jest ich niezmienna kolejność.

------------------------------------------------------------------------

# Pełny kod klasy `SessionEstablisher`

``` cpp
class SessionEstablisher {
public:
    virtual ~SessionEstablisher() = default;

    // TEMPLATE METHOD — stały szkielet algorytmu, nienadpisywalny
    void establish() {
        appLogger().log("[SessionEstablisher] Rozpoczynam ustanawianie sesji\n");

        if (!checkPreconditions()) {
            appLogger().log("[SessionEstablisher] Warunki wstępne niespełnione — przerywam\n");
            return;
        }

        connect();
        configure();      // hook — opcjonalny krok
        finalizeSetup();

        appLogger().log("[SessionEstablisher] Sesja ustanowiona\n");
    }

protected:
    // hooki — mają domyślną implementację, podklasa może je nadpisać
    virtual bool checkPreconditions() { return true; }
    virtual void configure() {}

    // kroki wymagane — podklasa musi je zaimplementować
    virtual void connect() = 0;
    virtual void finalizeSetup() = 0;
};
```

------------------------------------------------------------------------

# Pełny kod klasy `EngineSessionEstablisher`

``` cpp
class EngineSessionEstablisher : public SessionEstablisher {
public:
    EngineSessionEstablisher(SessionManagement& session, Engine& engine)
        : session_(session), engine_(engine) {}

protected:
    bool checkPreconditions() override {
        appLogger().log("[EngineSessionEstablisher] Sprawdzam dostępność silnika\n");
        return true;
    }

    void connect() override {
        session_.connectToEngine(engine_);
    }

    void configure() override {
        appLogger().log("[EngineSessionEstablisher] Konfiguracja domyślna\n");
    }

    void finalizeSetup() override {
        session_.openSession();
    }

private:
    SessionManagement& session_;
    Engine& engine_;
};
```
