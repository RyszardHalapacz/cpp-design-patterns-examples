# Wykład -- Bezpieczne zarządzanie żywotnością `SessionManagement` za pomocą `std::weak_ptr`

## Wprowadzenie

W pierwotnej wersji projektu `DummyGui` przechowywało surowy wskaźnik do
`SessionManagement`:

``` cpp
class DummyGui {
    SessionManagement* session_;
    ...
};
```

Na pierwszy rzut oka rozwiązanie wydaje się poprawne -- `Configurator`
przekazuje wskaźnik do obiektu oraz wskaźniki do dozwolonych metod, a
GUI przed wywołaniem sprawdza jedynie, czy wskaźnik nie jest `nullptr`.

Problem pojawia się wtedy, gdy czas życia obiektu `SessionManagement`
kończy się wcześniej niż czas życia `DummyGui`.

------------------------------------------------------------------------

# Problem

Architektura wygląda następująco:

``` text
        Configurator
             |
             v
         DummyGui
             |
             | SessionManagement*
             v
     SessionManagement
```

GUI otrzymuje adres obiektu `SessionManagement`.

Jeżeli obiekt zostanie zniszczony:

``` cpp
delete session;
```

lub

``` cpp
sharedPtr.reset();
```

to pamięć zostaje zwolniona, ale wskaźnik znajdujący się w `DummyGui`
**nie zmienia swojej wartości**.

GUI nadal posiada ten sam adres.

Powstaje tzw. **dangling pointer**.

------------------------------------------------------------------------

# Co dzieje się po kliknięciu przycisku?

GUI wykonuje:

``` cpp
(session_->*addVectorFunc_)(vec);
```

Jeżeli `session_` wskazuje na usunięty obiekt, dochodzi do **Undefined
Behavior**.

Możliwe skutki:

-   segmentation fault,
-   access violation,
-   losowe błędy,
-   pozornie poprawne działanie.

------------------------------------------------------------------------

# Dlaczego sprawdzenie `nullptr` nie pomaga?

Kod:

``` cpp
if (!session_)
    return;
```

nie wykryje problemu.

Po usunięciu obiektu:

``` cpp
delete session;
```

wartość wskaźnika nadal jest różna od `nullptr`.

Adres istnieje, ale obiekt już nie.

------------------------------------------------------------------------

# Rozwiązanie -- `std::weak_ptr`

Zamiast:

``` cpp
SessionManagement* session_;
```

GUI przechowuje:

``` cpp
std::weak_ptr<SessionManagement> session_;
```

`weak_ptr` nie jest właścicielem obiektu.

Pozwala jedynie sprawdzić, czy obiekt nadal istnieje.

------------------------------------------------------------------------

# Nowa architektura

``` text
                main()

                  |
            shared_ptr
                  |
                  v

        SessionManagement
               ^
               |
           weak_ptr
               |
               |
           DummyGui
```

Jedynym właścicielem jest `shared_ptr`.

GUI jedynie obserwuje obiekt.

------------------------------------------------------------------------

# Wywołanie metody

Przed wykonaniem operacji GUI wywołuje:

``` cpp
auto session = session_.lock();
```

## Gdy obiekt istnieje

`lock()` zwraca `shared_ptr`.

``` cpp
if (auto session = session_.lock())
{
    ((*session).*addVectorFunc_)(vec);
}
```

Operacja jest bezpieczna.

## Gdy obiekt został usunięty

`lock()` zwraca pusty `shared_ptr`.

``` cpp
if (!session)
{
    appLogger().log(
        "[GUI] SessionManagement już nie istnieje\n");
    return;
}
```

GUI nie dereferencjonuje martwego obiektu.

------------------------------------------------------------------------

# Dlaczego `lock()` zwraca `shared_ptr`?

Po wykonaniu:

``` cpp
auto session = session_.lock();
```

licznik referencji chwilowo wzrasta.

Dzięki temu obiekt nie może zostać usunięty w trakcie wykonywania
callbacka.

Po zakończeniu funkcji lokalny `shared_ptr` znika i licznik wraca do
poprzedniej wartości.

------------------------------------------------------------------------

# Co zmieniło się w `Configurator`?

Bardzo niewiele.

Zamiast przekazywać:

``` cpp
SessionManagement*
```

przekazuje:

``` cpp
std::shared_ptr<SessionManagement>
```

Nadal to `Configurator` decyduje, które metody GUI może wywoływać.

Zmienia się jedynie sposób zarządzania żywotnością obiektu.

------------------------------------------------------------------------

# Czy wskaźniki do metod nadal mają sens?

Tak.

GUI nadal przechowuje:

-   `AddVectorFunc`
-   `SortVectorFunc`
-   `PrintDataFunc`
-   `ExecuteBatchFunc`
-   `SetSortStrategyFunc`

Mechanizm przyznawania uprawnień pozostaje identyczny.

------------------------------------------------------------------------

# A co z Observerem?

Observer rozwiązuje inny problem.

Może poinformować GUI o zdarzeniu:

``` text
SessionClosing
```

dzięki czemu GUI może:

-   wyłączyć przyciski,
-   poinformować użytkownika,
-   zmienić stan interfejsu.

Nie odpowiada jednak za bezpieczeństwo pamięci.

To zadanie przejmuje `std::weak_ptr`.

------------------------------------------------------------------------

# Zalety rozwiązania

-   brak dangling pointerów,
-   brak Undefined Behavior,
-   zachowana architektura projektu,
-   zachowana rola `Configurator`,
-   zachowane wskaźniki do metod,
-   bezpieczne sprawdzanie, czy `SessionManagement` nadal istnieje.

------------------------------------------------------------------------

# Podsumowanie

Surowy wskaźnik odpowiada na pytanie:

> „Jaki jest adres obiektu?"

`std::weak_ptr` odpowiada na dwa pytania:

> „Jaki jest adres obiektu?"\
> „Czy ten obiekt nadal istnieje?"

Dzięki temu `DummyGui` nie może przypadkowo wywołać metody na obiekcie,
którego czas życia już się zakończył, a cała architektura pozostaje
zgodna z pierwotnym zamysłem projektu.
