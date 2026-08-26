#pragma once
#include <algorithm>    // std::max
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>  // std::string_view in labelOf
#include <utility>      // std::unreachable
#include <vector>
#include "Signal.hpp"

// ─── SequenceLog ──────────────────────────────────────────────────────────────
// Prints ASCII sequence-diagram rows with fixed lifeline columns.
//
//   col 0        col 36        col 72            col 92
//   [Driver]     [Engine]      [HistorianSpy]    # comment
//                [Engine]      [FactorySpy]      # comment

class SequenceLog {
public:
    static constexpr int kDriverCol   =  0;
    static constexpr int kEngineCol   = 36;
    static constexpr int kObserverCol = 72;
    static constexpr int kCommentCol  = 92;

    static void logFlow(Endpoint           from,
                        Endpoint           to,
                        const std::string& signal,
                        const std::string& captured = {})
    {
        int  fromC = colOf(from);
        int  toC   = colOf(to);
        auto fromL = std::string(labelOf(from));
        auto toL   = std::string(labelOf(to));

        std::string line;

        if (fromC <= toC) {
            int arrowLen = toC - (fromC + static_cast<int>(fromL.size()));
            int fills    = std::max(arrowLen - 6 - static_cast<int>(signal.size()), 0);
            line = std::format("{}{} ---{}{}> {}",
                spaces(fromC), fromL, signal, std::string(fills, '-'), toL);
        } else {
            int arrowLen = fromC - (toC + static_cast<int>(toL.size()));
            int fills    = std::max(arrowLen - 6 - static_cast<int>(signal.size()), 0);
            line = std::format("{}{} <---{}{} {}",
                spaces(toC), toL, signal, std::string(fills, '-'), fromL);
        }

        std::vector<std::string> remarks;
        {
            std::istringstream iss(captured);
            std::string ln;
            while (std::getline(iss, ln))
                if (!ln.empty()) remarks.push_back(ln);
        }

        if (!remarks.empty()) {
            int gap = std::max(kCommentCol - static_cast<int>(line.size()), 2);
            line += std::format("{:<{}}# {}", "", gap, remarks[0]);
        }
        std::cout << line << '\n';

        for (std::size_t i = 1; i < remarks.size(); ++i)
            std::cout << std::format("{:<{}}# {}\n", "", kCommentCol, remarks[i]);
    }

private:
    static std::string spaces(int n) { return std::string(std::max(n, 0), ' '); }

    static constexpr std::string_view labelOf(Endpoint e) noexcept {
        switch (e) {
            case Endpoint::Driver:    return "[Driver]";
            case Endpoint::Engine:    return "[Engine]";
            case Endpoint::Historian: return "[HistorianSpy]";
            case Endpoint::Factory:   return "[FactorySpy]";
        }
        std::unreachable();
    }

    static constexpr int colOf(Endpoint e) noexcept {
        switch (e) {
            case Endpoint::Driver:    return kDriverCol;
            case Endpoint::Engine:    return kEngineCol;
            case Endpoint::Historian: return kObserverCol;
            case Endpoint::Factory:   return kObserverCol;
        }
        std::unreachable();
    }
};
