# Design patterns: Strategy and Factory

# Introduction

Very often a program needs to perform the same operation in different ways.

For example, we want to compress a file.

We can use:

-   ZIP,
-   GZIP,
-   BZIP2,
-   LZ4,
-   ZSTD.

Each algorithm performs the same task — compresses data — but does it
in a different way.

So the question arises:

**How do we design the program so that new algorithms can be added
easily without modifying existing code?**

The answer is the **Strategy** pattern.

If we additionally want to hide how those algorithms are created, we use
**Factory**.

------------------------------------------------------------------------

# Strategy

## Problem

The first instinct of beginner programmers is to write code like this:

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

It works at first, but over time the code grows larger and harder to
maintain.

## Solution

Each algorithm is placed in a separate class that implements a common
interface.

``` cpp
class ICompressionStrategy
{
public:
    virtual ~ICompressionStrategy() = default;
    virtual void compress(const std::string& filename) = 0;
};
```

### Example strategies

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

### Class using Strategy

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

## Advantages

-   easy to add new algorithms,
-   no complex if/else chains,
-   complies with the Open/Closed Principle,
-   easy to test.

## Disadvantages

-   higher class count,
-   the client must hold a strategy.

------------------------------------------------------------------------

# Factory

## Problem

Where do we get the right strategy from?

We do not want the client to know all the classes that implement the
algorithms.

## Solution

We create a Factory.

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

The client only uses the Factory:

``` cpp
auto strategy = CompressionFactory::create("GZIP");
```

------------------------------------------------------------------------

# Strategy + Factory

Factory creates the appropriate strategy.

Strategy executes the actual algorithm.

    User
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

# Complete example

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

# Summary

Strategy is responsible for choosing how an operation is performed.

Factory is responsible for creating the right object.

Both patterns very frequently work together.
