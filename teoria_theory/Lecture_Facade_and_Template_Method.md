# Design patterns: Facade and Template Method

# Introduction

In large systems we very often encounter two problems.

The first:

> How do we hide the complexity of an entire subsystem from the user?

The second:

> How do we ensure that a certain algorithm is always executed in the
> same order, while still allowing selected steps to be changed?

The first problem is addressed by **Facade**.

The second is addressed by **Template Method**.

Both patterns appear together very frequently.

------------------------------------------------------------------------

# Facade

## Problem

Imagine a message-sending system.

To send an email you need to:

-   check the configuration,
-   connect to the server,
-   log the user in,
-   send the message,
-   disconnect.

The client should not need to know all these classes.

## Solution

We create a single class that exposes a simple interface.

``` cpp
class MailFacade
{
public:
    void sendMail();
};
```

The client only does:

``` cpp
MailFacade facade;
facade.sendMail();
```

Everything else happens inside the facade.

## Advantages

-   simple interface,
-   hidden complexity,
-   reduced coupling,
-   easier library usage.

## Disadvantages

-   can grow into an oversized class,
-   does not replace good architecture.

------------------------------------------------------------------------

# Template Method

## Problem

Suppose every session looks similar:

1.  Check configuration.
2.  Connect.
3.  Initialise.
4.  Finish.

Different session types differ only in the details.

## Solution

The base class defines the algorithm skeleton.

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

The `establish()` method never changes.

Only the individual steps change.

## Hook

A hook is a method that has a default implementation.

A subclass may override it, but does not have to.

``` cpp
virtual void initialize()
{
}
```

## Mandatory step

``` cpp
virtual void connect() = 0;
```

Every subclass must implement this.

## Advantages

-   shared algorithm,
-   elimination of duplication,
-   controlled execution order.

## Disadvantages

-   relies on inheritance,
-   less flexible than composition.

------------------------------------------------------------------------

# Why do they work together?

Facade exposes a simple operation.

Template Method implements its internal flow.

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

# Complete example

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

# Summary

Facade simplifies the use of an entire subsystem.

Template Method defines the algorithm skeleton and allows only selected
steps to be changed.

Very often Facade calls the template method, so the client sees only
one simple operation while all the complexity remains hidden inside the
system.
