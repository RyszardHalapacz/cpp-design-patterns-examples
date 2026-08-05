# Wzorzec projektowy Observer

# Wprowadzenie

Wyobraź sobie aplikację pogodową.

Gdy zmieni się temperatura, wiele elementów systemu powinno zostać o tym
poinformowanych:

-   wyświetlacz,
-   zapis do logów,
-   wysyłka powiadomienia,
-   zapis do bazy danych.

Nie chcemy, aby stacja pogodowa znała szczegóły działania każdego z tych
elementów.

Do rozwiązania tego problemu służy **Observer**.

------------------------------------------------------------------------

# Problem

Najprostsze rozwiązanie wygląda następująco:

``` cpp
display.update();
logger.log();
database.save();
notification.send();
```

Klasa źródłowa zna wszystkie pozostałe klasy.

Każde dodanie nowego odbiorcy wymaga modyfikacji istniejącego kodu.

Powstaje silne sprzężenie.

------------------------------------------------------------------------

# Rozwiązanie

Tworzymy dwa elementy:

-   **Subject** -- obiekt obserwowany,
-   **Observer** -- obiekt otrzymujący powiadomienia.

Subject przechowuje listę obserwatorów.

Gdy nastąpi zdarzenie, wywołuje metodę `notify()`, a każdy obserwator
sam decyduje, co zrobić.

------------------------------------------------------------------------

# Interfejs obserwatora

``` cpp
class IObserver
{
public:
    virtual ~IObserver() = default;
    virtual void update(int value) = 0;
};
```

------------------------------------------------------------------------

# Subject

``` cpp
class Subject
{
public:
    void attach(IObserver* observer);
    void detach(IObserver* observer);
    void notify(int value);

private:
    std::vector<IObserver*> observers_;
};
```

------------------------------------------------------------------------

# Diagram

                  attach()
    Observer --------------------+
                                 |
                                 v
                            +-----------+
                            | Subject   |
                            +-----------+
                                 |
                              notify()
                                 |
            +----------+----------+----------+
            |          |                     |
            v          v                     v
       Display     Logger              Database

------------------------------------------------------------------------

# Zalety

-   luźne powiązanie klas,
-   łatwe dodawanie nowych obserwatorów,
-   architektura zdarzeniowa,
-   zgodność z Open/Closed Principle.

------------------------------------------------------------------------

# Wady

-   trudniej śledzić przepływ programu,
-   kolejność powiadomień może mieć znaczenie,
-   trzeba uważać na czas życia obserwatorów.

------------------------------------------------------------------------

# Kompletny przykład

``` cpp
#include <algorithm>
#include <iostream>
#include <vector>

class IObserver
{
public:
    virtual ~IObserver() = default;
    virtual void update(int value) = 0;
};

class Subject
{
public:
    void attach(IObserver* observer)
    {
        observers_.push_back(observer);
    }

    void detach(IObserver* observer)
    {
        observers_.erase(
            std::remove(observers_.begin(), observers_.end(), observer),
            observers_.end());
    }

    void notify(int value)
    {
        for(auto* observer : observers_)
            observer->update(value);
    }

    void setValue(int value)
    {
        value_ = value;
        notify(value_);
    }

private:
    int value_{};
    std::vector<IObserver*> observers_;
};

class Display : public IObserver
{
public:
    void update(int value) override
    {
        std::cout << "Display: " << value << '\n';
    }
};

class Logger : public IObserver
{
public:
    void update(int value) override
    {
        std::cout << "Logger: zapis " << value << '\n';
    }
};

class Alarm : public IObserver
{
public:
    void update(int value) override
    {
        if(value > 30)
            std::cout << "Alarm: wysoka temperatura!\n";
    }
};

int main()
{
    Subject weatherStation;

    Display display;
    Logger logger;
    Alarm alarm;

    weatherStation.attach(&display);
    weatherStation.attach(&logger);
    weatherStation.attach(&alarm);

    weatherStation.setValue(20);
    weatherStation.setValue(35);

    weatherStation.detach(&logger);

    weatherStation.setValue(15);
}
```

------------------------------------------------------------------------

# Nowoczesny C++

W nowoczesnym C++ klasyczny Observer często zastępowany jest
callbackami:

``` cpp
std::function<void(int)>
```

lub bibliotekami sygnałów i zdarzeń.

Idea pozostaje identyczna -- jeden obiekt publikuje zdarzenie, a wielu
odbiorców na nie reaguje.

------------------------------------------------------------------------

# Typowe zastosowania

-   GUI,
-   systemy zdarzeń,
-   logowanie,
-   telemetryka,
-   pluginy,
-   komunikacja między modułami.

------------------------------------------------------------------------

# Podsumowanie

Observer umożliwia komunikację typu **jeden → wielu**.

Subject nie zna szczegółów swoich obserwatorów -- wie jedynie, że
potrafią obsłużyć zdarzenie. Dzięki temu system pozostaje elastyczny i
łatwo go rozbudowywać bez modyfikowania istniejącego kodu.
