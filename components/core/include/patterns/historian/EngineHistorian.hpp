#pragma once
#include "patterns/historian/IHistorian.hpp"

namespace patterns::historian {

class EngineHistorian : public IHistorian {
public:
    void recordCommand(const CommandHistory& cmd)    override {}
    void publishSnapshot(const EngineSnapshot& snap) override {}
};

} // namespace patterns::historian
