# Wzorce projektowe Facade oraz Template Method

# Wprowadzenie

W dużych systemach bardzo często spotykamy dwa problemy.

Pierwszy:

> Jak ukryć złożoność całego podsystemu przed użytkownikiem?

Drugi:

> Jak zapewnić, że pewien algorytm zawsze będzie wykonywany w tej samej
> kolejności, pozostawiając możliwość zmiany tylko wybranych kroków?

Na pierwszy problem odpowiada **Facade**.

Na drugi odpowiada **Template Method**.

Oba wzorce bardzo często występują razem.

------------------------------------------------------------------------

# Facade

## Problem

Wyobraźmy sobie system wysyłania wiadomości.

Do wysłania maila trzeba:

-   sprawdzić konfigurację,
-   połączyć się z serwerem,
-   zalogować użytkownika,
-   wysłać wiadomość,
-   rozłączyć połączenie.

Klient nie powinien znać wszystkich tych klas.

## Rozwiązanie

Tworzymy jedną klasę udostępniającą prosty interfejs.

``` cpp
class MailFacade
{
public:
    void sendMail();
};
```

Klient wykonuje jedynie:

``` cpp
MailFacade facade;
facade.sendMail();
```

Cała reszta dzieje się wewnątrz fasady.

## Zalety

-   prosty interfejs,
-   ukrycie złożoności,
-   mniejsze sprzężenie,
-   łatwiejsze użycie biblioteki.

## Wady

-   może urosnąć do zbyt dużej klasy,
-   nie zastępuje dobrej architektury.

------------------------------------------------------------------------

# Template Method

## Problem

Załóżmy, że każda sesja wygląda podobnie.

1.  Sprawdzenie konfiguracji.
2.  Połączenie.
3.  Inicjalizacja.
4.  Zakończenie.

Różne typy sesji różnią się jedynie szczegółami.

## Rozwiązanie

Klasa bazowa definiuje szkielet algorytmu.

``` cpp
class SessionEstablisher
{
public:

    void establish()
    {
        check();
        connect();
        initialize();
        finish();
    }

protected:

    virtual void connect() = 0;
    virtual void initialize() = 0;

    virtual void check() {}
    virtual void finish() {}
};
```

Metoda `establish()` nigdy się nie zmienia.

Zmieniają się tylko poszczególne kroki.

## Hook

Hook to metoda posiadająca domyślną implementację.

Podklasa może ją nadpisać, ale nie musi.

``` cpp
virtual void initialize()
{
}
```

## Metoda obowiązkowa

``` cpp
virtual void connect() = 0;
```

Każda podklasa musi ją zaimplementować.

## Zalety

-   wspólny algorytm,
-   eliminacja duplikacji,
-   kontrola kolejności wykonywania.

## Wady

-   dziedziczenie,
-   mniejsza elastyczność niż kompozycja.

------------------------------------------------------------------------

# Dlaczego współpracują?

Facade udostępnia prostą operację.

Template Method realizuje jej wewnętrzny przebieg.

    GUI
     │
     ▼
    Facade
     │
     ▼
    Template Method
     │
     ▼
    Concrete Implementation

------------------------------------------------------------------------

# Kompletny przykład

``` cpp
#include <iostream>

class SessionEstablisher
{
public:

    virtual ~SessionEstablisher() = default;

    void establish()
    {
        checkConfiguration();
        connect();
        initialize();
        finish();
    }

protected:

    virtual void connect() = 0;

    virtual void initialize()
    {
        std::cout << "Default initialization\n";
    }

    virtual void checkConfiguration()
    {
        std::cout << "Checking configuration\n";
    }

    virtual void finish()
    {
        std::cout << "Session established\n";
    }
};

class TcpSession : public SessionEstablisher
{
protected:

    void connect() override
    {
        std::cout << "Connecting TCP...\n";
    }

    void initialize() override
    {
        std::cout << "Initializing TCP session\n";
    }
};

class MailFacade
{
public:

    explicit MailFacade(SessionEstablisher& session)
        : session_(session)
    {
    }

    void sendMail()
    {
        session_.establish();
        std::cout << "Sending mail...\n";
    }

private:

    SessionEstablisher& session_;
};

int main()
{
    TcpSession session;

    MailFacade facade(session);

    facade.sendMail();
}
```

------------------------------------------------------------------------

# Podsumowanie

Facade upraszcza korzystanie z całego podsystemu.

Template Method definiuje szkielet algorytmu i pozwala zmieniać tylko
wybrane kroki.

Bardzo często Facade wywołuje metodę szablonową, dzięki czemu klient
widzi tylko jedną prostą operację, a cała złożoność pozostaje ukryta
wewnątrz systemu.
