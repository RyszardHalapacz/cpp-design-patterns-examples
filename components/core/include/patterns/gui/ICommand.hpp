#pragma once
#include <functional>
#include <memory>
#include <vector>

namespace patterns::gui {

// ==================================
// COMMAND — GoF Command pattern
// Encapsulates an operation as an object with virtual execute().
// Each concrete command captures its callback and data at construction time.
// ==================================
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void execute() = 0;
};

class AddVectorCommand : public ICommand {
public:
    using Fn = std::function<void(const std::vector<int>&)>;
    AddVectorCommand(Fn fn, std::vector<int> vec)
        : fn_(std::move(fn)), vec_(std::move(vec)) {}
    void execute() override { if (fn_) fn_(vec_); }
private:
    Fn               fn_;
    std::vector<int> vec_;
};

class SortVectorCommand : public ICommand {
public:
    using Fn = std::function<void(size_t)>;
    SortVectorCommand(Fn fn, size_t index)
        : fn_(std::move(fn)), index_(index) {}
    void execute() override { if (fn_) fn_(index_); }
private:
    Fn     fn_;
    size_t index_;
};

class PrintDataCommand : public ICommand {
public:
    using Fn = std::function<void()>;
    explicit PrintDataCommand(Fn fn) : fn_(std::move(fn)) {}
    void execute() override { if (fn_) fn_(); }
private:
    Fn fn_;
};

using CommandBatch = std::vector<std::unique_ptr<ICommand>>;

} // namespace patterns::gui
