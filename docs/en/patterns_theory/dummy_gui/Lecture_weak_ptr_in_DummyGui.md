# Lecture — Safe Lifetime Management of `SessionManagement` Using `std::weak_ptr`

## Introduction

In the original version of the project `DummyGui` stored a raw pointer to
`SessionManagement`:

```cpp
class DummyGui {
    SessionManagement* session_;
    ...
};
```

At first glance the solution appears correct — `Configurator` passes a pointer
to the object and pointers to the allowed methods, and the GUI only checks
whether the pointer is `nullptr` before calling.

The problem arises when the lifetime of the `SessionManagement` object ends
before the lifetime of `DummyGui`.

---

# Problem

The architecture looks like this:

```text
        Configurator
             |
             v
         DummyGui
             |
             | SessionManagement*
             v
     SessionManagement
```

The GUI receives the address of the `SessionManagement` object.

If the object is destroyed:

```cpp
delete session;
```

or

```cpp
sharedPtr.reset();
```

the memory is freed, but the pointer inside `DummyGui`
**does not change its value**.

The GUI still holds the same address.

This creates a **dangling pointer**.

---

# What Happens When a Button Is Clicked?

The GUI executes:

```cpp
(session_->*addVectorFunc_)(vec);
```

If `session_` points to a destroyed object, this results in **Undefined
Behavior**.

Possible consequences:

- segmentation fault,
- access violation,
- random errors,
- seemingly correct behavior.

---

# Why Does a `nullptr` Check Not Help?

The code:

```cpp
if (!session_)
    return;
```

will not detect the problem.

After the object is deleted:

```cpp
delete session;
```

the pointer value is still different from `nullptr`.

The address exists, but the object does not.

---

# Solution — `std::weak_ptr`

Instead of:

```cpp
SessionManagement* session_;
```

the GUI stores:

```cpp
std::weak_ptr<SessionManagement> session_;
```

`weak_ptr` does not own the object.

It only allows checking whether the object still exists.

---

# New Architecture

```text
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

The sole owner is the `shared_ptr`.

The GUI only observes the object.

---

# Calling a Method

Before executing an operation the GUI calls:

```cpp
auto session = session_.lock();
```

## When the Object Exists

`lock()` returns a `shared_ptr`.

```cpp
if (auto session = session_.lock())
{
    ((*session).*addVectorFunc_)(vec);
}
```

The operation is safe.

## When the Object Has Been Destroyed

`lock()` returns an empty `shared_ptr`.

```cpp
if (!session)
{
    appLogger().log(
        "[GUI] SessionManagement no longer exists\n");
    return;
}
```

The GUI does not dereference a dead object.

---

# Why Does `lock()` Return a `shared_ptr`?

After executing:

```cpp
auto session = session_.lock();
```

the reference count temporarily increases.

This ensures the object cannot be destroyed while the callback is executing.

After the function returns the local `shared_ptr` disappears and the count
returns to its previous value.

---

# What Changed in `Configurator`?

Very little.

Instead of passing:

```cpp
SessionManagement*
```

it passes:

```cpp
std::shared_ptr<SessionManagement>
```

`Configurator` still decides which methods the GUI is allowed to call.

Only the way object lifetime is managed changes.

---

# Are Member Function Pointers Still Relevant?

Yes.

The GUI still stores:

- `AddVectorFunc`
- `SortVectorFunc`
- `PrintDataFunc`
- `ExecuteBatchFunc`
- `SetSortStrategyFunc`

The permission-granting mechanism remains identical.

---

# What About Observer?

Observer solves a different problem.

It can notify the GUI of the event:

```text
SessionClosing
```

so the GUI can:

- disable buttons,
- inform the user,
- change the interface state.

It is not responsible for memory safety, however.

That responsibility is taken over by `std::weak_ptr`.

---

# Advantages of the Solution

- no dangling pointers,
- no Undefined Behavior,
- project architecture preserved,
- `Configurator` role preserved,
- member function pointers preserved,
- safe check of whether `SessionManagement` still exists.

---

# Summary

A raw pointer answers the question:

> "What is the address of the object?"

`std::weak_ptr` answers two questions:

> "What is the address of the object?"
> "Does this object still exist?"

As a result `DummyGui` cannot accidentally call a method on an object whose
lifetime has already ended, and the entire architecture remains consistent with
the original design intent of the project.
