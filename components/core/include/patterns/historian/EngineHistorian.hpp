#pragma once

namespace patterns::historian {

struct CommandHistory
{
};

struct EngineSnapshot
{
};

class EngineHistorian
{
public:
    void recordCommand(const CommandHistory&) {}
    void publishSnapshot(const EngineSnapshot&) {}
};

} // namespace patterns::historian
