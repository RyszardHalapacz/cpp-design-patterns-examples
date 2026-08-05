# Wykład: `DummyGui` jako osobny komponent — biblioteka statyczna, C-style API i `unique_ptr`

---

## Wprowadzenie

W pierwotnej wersji projektu `DummyGui` był zwykłą klasą kompilowaną razem z resztą aplikacji.

W obecnej wersji `DummyGui` jest **osobnym komponentem** — statyczną biblioteką (`dummy_gui_lib`), która żyje w podkatalogu `components/dummy_gui/` i ma własny plik `CMakeLists.txt`.

Zmiana ta nie jest przypadkowa. Odzwierciedla realny podział odpowiedzialności: warstwa GUI jest niezależna od silnika, ma własny cykl życia i własną wersję.

---

## Struktura komponentu

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

Komponent buduje bibliotekę statyczną `dummy_gui_lib` oraz własny zestaw testów `dummy_gui_tests`.

---

## Dlaczego biblioteka statyczna?

Biblioteka statyczna (`add_library(dummy_gui_lib STATIC ...)`) pozwala:

- **izolować kod GUI** — zmiany w `DummyGui` nie wymagają przebudowania całego projektu,
- **testować niezależnie** — testy GUI żyją wewnątrz komponentu i nie mieszają się z testami silnika,
- **wersjonować osobno** — komponent ma własną wersję (`DUMMY_GUI_VERSION`) i generuje własny plik `dummy_gui.yml`,
- **przygotować grunt pod przyszłe wyodrębnienie** — w przyszłości komponent może trafić do osobnego repozytorium.

Główna aplikacja i testy linkują `dummy_gui_lib`:

```cmake
# apka
target_link_libraries(patterns PRIVATE dummy_gui_lib)

# testy główne
target_link_libraries(patterns_tests PRIVATE dummy_gui_lib GTest::gtest_main)
```

Nagłówki są dostępne przez include path ustawiony jako `PUBLIC`, dzięki czemu po zlinkopwaniu wystarczy pisać:

```cpp
#include "patterns/gui/DummyGui.hpp"
```

---

## Symulacja starego C API — `makeGUI` i `deleteGUI`

### Skąd ten pomysł?

Wiele starych bibliotek napisanych w języku C zarządza zasobami przez parę funkcji:

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

Wzorzec jest zawsze ten sam:

1. Funkcja tworząca zwraca surowy wskaźnik (`C*`).
2. Użytkownik jest odpowiedzialny za wywołanie funkcji niszczącej.
3. Brak wywołania funkcji niszczącej oznacza wyciek zasobów.

`DummyGui` celowo symuluje ten styl. Konstruktor jest **prywatny** — obiekt można uzyskać wyłącznie przez `makeGUI`.

```cpp
// Jedyna droga do stworzenia obiektu
DummyGui* gui = makeGUI();

// Jedyna droga do jego zniszczenia
deleteGUI(gui);
```

### Implementacja

```cpp
template<typename Writer = ComponentManifestWriter>
class BasicDummyGui {
public:
    friend BasicDummyGui* makeGUI(std::filesystem::path manifestPath);
    friend void           deleteGUI(BasicDummyGui* gui);

    // ... metody publiczne ...

private:
    explicit BasicDummyGui(std::filesystem::path manifestPath = {});
    // ... składowe ...
};

using DummyGui = BasicDummyGui<>;

inline DummyGui* makeGUI(std::filesystem::path manifestPath = {}) {
    return new DummyGui(std::move(manifestPath));
}

inline void deleteGUI(DummyGui* gui) {
    delete gui;
}
```

Deklaracja `friend` wewnątrz klasy sprawia, że tylko `makeGUI` i `deleteGUI` mogą wywoływać prywatny konstruktor i destruktor.

---

## Problem z surowym wskaźnikiem

Kod korzystający z C-style API wygląda następująco:

```cpp
DummyGui* gui = makeGUI(path);

// ... używamy gui ...

deleteGUI(gui);  // łatwo zapomnieć
```

Jeżeli między `makeGUI` a `deleteGUI` zostanie rzucony wyjątek — lub programista po prostu zapomni wywołać `deleteGUI` — powstaje **wyciek pamięci**.

To dokładnie ten sam problem, który dotyczy starych bibliotek C.

---

## Rozwiązanie — `unique_ptr` z `std::default_delete`

Nowoczesne C++ rozwiązuje ten problem przez **RAII** (Resource Acquisition Is Initialization).

Zamiast ręcznie wołać `deleteGUI`, możemy opakować surowy wskaźnik w `std::unique_ptr`.

### Specjalizacja `std::default_delete`

`std::unique_ptr<T>` domyślnie wywołuje `delete ptr` przy destrukcji.

Chcemy jednak, żeby wywoływał `deleteGUI(ptr)`.

W tym celu w miejscu użycia (pliku `main.cpp`) definiujemy specjalizację `std::default_delete`:

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

Od tej chwili `unique_ptr<DummyGui>` automatycznie woła `deleteGUI` przy destrukcji.

### Użycie w aplikacji

```cpp
std::unique_ptr<patterns::gui::DummyGui> gui(
    patterns::gui::makeGUI(fs::path(DUMMY_GUI_MANIFEST_PATH))
);

// ... używamy gui przez -> ...
gui->clickAddVector({3, 1, 2});

// deleteGUI wołane automatycznie — nawet przy wyjątku
```

Nie ma potrzeby ręcznego wywoływania `deleteGUI`. `unique_ptr` zrobi to przy wyjściu ze scope'u lub przy wywołaniu `reset()`.

---

## Podział odpowiedzialności

```text
Warstwa C-style API (dummy_gui_lib)
   makeGUI()  →  tworzy surowy DummyGui*
   deleteGUI() →  niszczy surowy DummyGui*

Warstwa aplikacji (main.cpp)
   unique_ptr<DummyGui>  →  RAII wrapper
   std::default_delete   →  łączy oba światy
```

Biblioteka pozostaje napisana w stylu C — surowe wskaźniki, jawne zarządzanie zasobami.

Aplikacja używa jej przez cienki wrapper, który eliminuje ryzyko wycieku.

---

## Dlaczego specjalizacja `default_delete` jest w `main.cpp`, a nie w nagłówku biblioteki?

Specjalizacja `std::default_delete` definiuje **sposób użycia** biblioteki, nie jej implementację.

Biblioteka (`dummy_gui_lib`) nie powinna narzucać konsumentom, jak mają zarządzać zasobami.

Różni konsumenci mogą mieć różne potrzeby:

- `main.cpp` używa `unique_ptr` — wygodne RAII,
- testy jednostkowe `dummy_gui_tests` używają surowych wskaźników — testują wprost C-style API,
- przyszły konsument mógłby użyć niestandardowego alokatora.

Umieszczenie specjalizacji w `main.cpp` jest świadomą decyzją: **biblioteka jest niezależna, aplikacja decyduje o strategii zarządzania zasobami**.

---

## Podsumowanie

| Mechanizm | Rola |
|---|---|
| Prywatny konstruktor | Wymusza korzystanie z `makeGUI`/`deleteGUI` |
| `friend makeGUI` / `friend deleteGUI` | Jedyne uprawnione punkty tworzenia i niszczenia |
| Symulacja C API | Dydaktyczne pokazanie problemu surowych wskaźników |
| `std::default_delete<DummyGui>` | Łączy C-style API z nowoczesnym RAII |
| `unique_ptr<DummyGui>` | Bezpieczne zarządzanie zasobem w aplikacji |
| Biblioteka statyczna `dummy_gui_lib` | Izolacja komponentu, niezależne wersjonowanie i testy |
