
# Wykład: Logger i FileLogger

## Wprowadzenie

W przykładzie występują dwie klasy odpowiedzialne za logowanie:

- `Logger`
- `FileLogger`

Obie dziedziczą po wspólnej klasie bazowej `IService`, dzięki czemu mogą być przechowywane przez `ServiceLocator`.

---

# Logger

```cpp
class Logger : public IService {
public:
    void log(const std::string& message) {
        std::cout << message;
    }
};
```

## Dziedziczenie

```cpp
class Logger : public IService
```

`Logger` dziedziczy po `IService`, aby wszystkie usługi mogły być przechowywane w jednej kolekcji:

```cpp
std::unordered_map<
    std::type_index,
    std::shared_ptr<IService>
>
```

Dzięki temu `ServiceLocator` może przechowywać jednocześnie:

- Logger
- FileLogger
- DoSomething
- dowolne przyszłe usługi

---

## Funkcja `log()`

```cpp
void log(const std::string& message)
{
    std::cout << message;
}
```

Metoda jedynie wypisuje przekazany tekst na standardowe wyjście.

Przykład:

```cpp
Logger logger;
logger.log("Hello\n");
```

Wynik:

```
Hello
```

Nie ma tutaj:

- timestampów,
- poziomów logowania,
- mutexów,
- kolorów,
- zapisu do pliku.

Jest to najprostszy możliwy logger.

---

# FileLogger

```cpp
class FileLogger : public IService {
public:
    explicit FileLogger(const std::string& filename)
        : filename_(filename)
    {
        std::cout
            << "[FileLogger] Wykonuję komendę: utwórz plik \""
            << filename_
            << "\" (symulacja — bez realnego zapisu na dysk)\n";
    }

    void log(const std::string& message)
    {
        std::cout
            << "[FileLogger] Wykonuję komendę: dopisz do pliku \""
            << filename_
            << "\": "
            << message;
    }

private:
    std::string filename_;
};
```

## Pole klasy

```cpp
std::string filename_;
```

Przechowuje nazwę pliku, np.:

```
engine_log.txt
```

---

## Konstruktor

```cpp
explicit FileLogger(const std::string& filename)
    : filename_(filename)
{
    ...
}
```

Najpierw wykonywana jest lista inicjalizacyjna:

```cpp
filename_(filename)
```

Dopiero później wykonywane jest ciało konstruktora.

### Dlaczego `explicit`?

Bez `explicit` można byłoby napisać:

```cpp
FileLogger logger = "engine_log.txt";
```

Autor wymusza jawne tworzenie obiektu:

```cpp
FileLogger logger("engine_log.txt");
```

lub

```cpp
FileLogger logger{"engine_log.txt"};
```

---

## Co robi konstruktor?

Konstruktor **nie tworzy prawdziwego pliku**.

Jedynie wypisuje komunikat:

```
[FileLogger] Wykonuję komendę:
utwórz plik "engine_log.txt"
(symulacja)
```

Dlaczego?

Przykład ma działać również w środowiskach takich jak Wandbox, Compiler Explorer czy sandboxy online, gdzie zapis na dysk może być zablokowany.

---

## Funkcja `log()`

Zamiast naprawdę otwierać i zapisywać plik, wypisuje komunikat:

```cpp
logger.log("Hello");
```

Wynik:

```
[FileLogger]
Wykonuję komendę:
dopisz do pliku
"engine_log.txt":
Hello
```

---

# Dlaczego istnieją dwie klasy?

Obie udostępniają podobny interfejs:

```cpp
Logger::log(...)
```

oraz

```cpp
FileLogger::log(...)
```

ale realizują różne zachowanie:

```
Logger
    ↓
konsola

FileLogger
    ↓
plik (symulowany)
```

Dzięki temu `ServiceLocator` może przechowywać obie usługi jednocześnie:

```cpp
ServiceLocator::instance().provide(consoleLogger);
ServiceLocator::instance().provide(fileLogger);
```

Później pobieramy konkretną usługę po jej typie:

```cpp
appLogger().log(...);
```

lub

```cpp
appFileLogger().log(...);
```

---

# Czy klasy są ze sobą powiązane?

Nie.

Nie dziedziczą po sobie.

```
        IService
        /      \
       /        \
 Logger      FileLogger
```

Łączy je jedynie wspólna klasa bazowa `IService`, dzięki której `ServiceLocator` może przechowywać je w jednej heterogenicznej kolekcji.

---

# Podsumowanie

- `Logger` wypisuje komunikaty na konsolę.
- `FileLogger` symuluje zapis do pliku.
- Obie klasy dziedziczą po `IService`.
- Dzięki temu `ServiceLocator` może przechowywać wiele różnych usług pod wspólnym interfejsem.
- W przykładzie najważniejszym celem nie jest samo logowanie, lecz pokazanie współpracy z wzorcem **Service Locator**.
