# Observer — design pattern

# Introduction

Imagine a weather application.

When the temperature changes, many parts of the system should be
notified:

-   display,
-   log writer,
-   push notification,
-   database write.

We do not want the weather station to know the implementation details of
each of those components.

The **Observer** pattern solves this problem.

------------------------------------------------------------------------

# Problem

The simplest solution looks like this:

``` cpp
display.update();
logger.log();
database.save();
notification.send();
```

The source class knows all the other classes.

Adding any new receiver requires modifying existing code.

This creates tight coupling.

------------------------------------------------------------------------

# Solution

We create two elements:

-   **Subject** — the observed object,
-   **Observer** — the object that receives notifications.

The Subject holds a list of observers.

When an event occurs it calls `notify()`, and each observer decides for
itself what to do.

------------------------------------------------------------------------

# Observer interface

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

# Advantages

-   loose coupling between classes,
-   easy to add new observers,
-   event-driven architecture,
-   complies with the Open/Closed Principle.

------------------------------------------------------------------------

# Disadvantages

-   harder to trace program flow,
-   notification order may matter,
-   observer lifetimes must be managed carefully.

------------------------------------------------------------------------

# Complete example

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
        std::cout << "Logger: recording " << value << '\n';
    }
};

class Alarm : public IObserver
{
public:
    void update(int value) override
    {
        if(value > 30)
            std::cout << "Alarm: high temperature!\n";
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

# Modern C++

In modern C++ the classic Observer is often replaced by callbacks:

``` cpp
std::function<void(int)>
```

or signal/event libraries.

The idea remains the same — one object publishes an event and many
receivers react to it.

------------------------------------------------------------------------

# Typical use cases

-   GUI,
-   event systems,
-   logging,
-   telemetry,
-   plugins,
-   inter-module communication.

------------------------------------------------------------------------

# Summary

Observer enables **one-to-many** communication.

The Subject does not know the details of its observers — it only knows
that they can handle the event. This keeps the system flexible and easy
to extend without modifying existing code.
