# Wzorzec projektowy Strategy oraz Factory

# Wprowadzenie

Bardzo często program musi wykonywać tę samą operację na różne sposoby.

Przykładowo chcemy skompresować plik.

Możemy użyć:

-   ZIP,
-   GZIP,
-   BZIP2,
-   LZ4,
-   ZSTD.

Każdy algorytm wykonuje to samo zadanie --- kompresuje dane --- ale robi
to w inny sposób.

Pojawia się więc pytanie:

**Jak zaprojektować program, aby łatwo dodawać kolejne algorytmy bez
modyfikowania istniejącego kodu?**

Odpowiedzią jest wzorzec **Strategy**.

Natomiast jeśli chcemy dodatkowo ukryć sposób tworzenia tych algorytmów,
wykorzystujemy **Factory**.

------------------------------------------------------------------------

# Strategy

## Problem

Pierwszym pomysłem początkujących programistów jest napisanie kodu
podobnego do tego:

``` cpp
if(type == ZIP)
{
    ...
}
else if(type == GZIP)
{
    ...
}
else if(type == BZIP2)
{
    ...
}
```

Na początku działa, ale z czasem kod staje się coraz większy i
trudniejszy do utrzymania.

## Rozwiązanie

Każdy algorytm umieszczamy w osobnej klasie implementującej wspólny
interfejs.

``` cpp
class ICompressionStrategy
{
public:
    virtual ~ICompressionStrategy() = default;
    virtual void compress(const std::string& filename) = 0;
};
```

### Przykładowe strategie

``` cpp
class ZipCompression : public ICompressionStrategy
{
public:
    void compress(const std::string& file) override
    {
        std::cout << "ZIP -> " << file << '\n';
    }
};

class GzipCompression : public ICompressionStrategy
{
public:
    void compress(const std::string& file) override
    {
        std::cout << "GZIP -> " << file << '\n';
    }
};

class BzipCompression : public ICompressionStrategy
{
public:
    void compress(const std::string& file) override
    {
        std::cout << "BZIP2 -> " << file << '\n';
    }
};
```

### Klasa korzystająca ze Strategy

``` cpp
class Compressor
{
public:
    explicit Compressor(std::unique_ptr<ICompressionStrategy> strategy)
        : strategy_(std::move(strategy))
    {
    }

    void compress(const std::string& file)
    {
        strategy_->compress(file);
    }

private:
    std::unique_ptr<ICompressionStrategy> strategy_;
};
```

## Zalety

-   łatwe dodawanie nowych algorytmów,
-   brak rozbudowanych instrukcji if/else,
-   zgodność z Open/Closed Principle,
-   łatwe testowanie.

## Wady

-   większa liczba klas,
-   klient musi posiadać strategię.

------------------------------------------------------------------------

# Factory

## Problem

Skąd wziąć odpowiednią strategię?

Nie chcemy, aby klient znał wszystkie klasy implementujące algorytmy.

## Rozwiązanie

Tworzymy Factory.

``` cpp
class CompressionFactory
{
public:
    static std::unique_ptr<ICompressionStrategy>
    create(const std::string& type)
    {
        if(type == "ZIP")
            return std::make_unique<ZipCompression>();

        if(type == "GZIP")
            return std::make_unique<GzipCompression>();

        if(type == "BZIP2")
            return std::make_unique<BzipCompression>();

        throw std::runtime_error("Unknown algorithm");
    }
};
```

Klient korzysta wyłącznie z Factory:

``` cpp
auto strategy = CompressionFactory::create("GZIP");
```

------------------------------------------------------------------------

# Strategy + Factory

Factory tworzy odpowiednią strategię.

Strategy wykonuje właściwy algorytm.

    Użytkownik
         │
         ▼
    Factory
         │
         ▼
    ICompressionStrategy
     ▲      ▲      ▲
     │      │      │
    ZIP   GZIP   BZIP2
         │
         ▼
    Compressor

------------------------------------------------------------------------

# Kompletny przykład

``` cpp
#include <iostream>
#include <memory>
#include <string>

class ICompressionStrategy
{
public:
    virtual ~ICompressionStrategy() = default;
    virtual void compress(const std::string& file) = 0;
};

class ZipCompression : public ICompressionStrategy
{
public:
    void compress(const std::string& file) override
    {
        std::cout << "ZIP -> " << file << '\n';
    }
};

class GzipCompression : public ICompressionStrategy
{
public:
    void compress(const std::string& file) override
    {
        std::cout << "GZIP -> " << file << '\n';
    }
};

class BzipCompression : public ICompressionStrategy
{
public:
    void compress(const std::string& file) override
    {
        std::cout << "BZIP2 -> " << file << '\n';
    }
};

class CompressionFactory
{
public:
    static std::unique_ptr<ICompressionStrategy>
    create(const std::string& algorithm)
    {
        if(algorithm == "ZIP")
            return std::make_unique<ZipCompression>();

        if(algorithm == "GZIP")
            return std::make_unique<GzipCompression>();

        if(algorithm == "BZIP2")
            return std::make_unique<BzipCompression>();

        throw std::runtime_error("Unknown algorithm");
    }
};

class Compressor
{
public:
    explicit Compressor(std::unique_ptr<ICompressionStrategy> strategy)
        : strategy_(std::move(strategy))
    {
    }

    void compress(const std::string& file)
    {
        strategy_->compress(file);
    }

private:
    std::unique_ptr<ICompressionStrategy> strategy_;
};

int main()
{
    auto strategy = CompressionFactory::create("GZIP");
    Compressor compressor(std::move(strategy));
    compressor.compress("report.pdf");
}
```

------------------------------------------------------------------------

# Podsumowanie

Strategy odpowiada za wybór sposobu wykonania operacji.

Factory odpowiada za utworzenie odpowiedniego obiektu.

Oba wzorce bardzo często współpracują ze sobą.
