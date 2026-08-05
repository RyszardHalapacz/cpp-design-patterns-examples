#pragma once
#include <vector>

namespace patterns::gui {

enum class CommandType {
    AddVector,
    SortVector,
    PrintData
};

struct Command {
    CommandType      type;
    std::vector<int> vectorData;
    size_t           index = 0;
};

using CommandBatch = std::vector<Command>;

} // namespace patterns::gui
