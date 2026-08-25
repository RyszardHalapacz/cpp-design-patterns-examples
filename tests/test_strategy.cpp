#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <memory>
#include <functional>

#include "patterns/strategy/SortStrategyId.hpp"
#include "patterns/strategy/AscendingSortStrategy.hpp"
#include "patterns/strategy/DescendingSortStrategy.hpp"
#include "patterns/strategy/BubbleSortStrategy.hpp"
#include "patterns/strategy/SortStrategyFactory.hpp"

using namespace patterns::strategy;

using StrategyFactory = std::function<std::unique_ptr<ISortStrategy>()>;

// ─── SortStrategyId / sortStrategyIdName ─────────────────────────────────────
// (only 2 unrelated assertions — not worth parameterizing)

TEST(SortStrategyIdTest, NamesAreNonEmpty) {
    EXPECT_STRNE("", sortStrategyIdName(SortStrategyId::Ascending));
    EXPECT_STRNE("", sortStrategyIdName(SortStrategyId::Descending));
    EXPECT_STRNE("", sortStrategyIdName(SortStrategyId::Bubble));
}

TEST(SortStrategyIdTest, NamesAreDifferent) {
    EXPECT_STRNE(sortStrategyIdName(SortStrategyId::Ascending),
                 sortStrategyIdName(SortStrategyId::Descending));
    EXPECT_STRNE(sortStrategyIdName(SortStrategyId::Ascending),
                 sortStrategyIdName(SortStrategyId::Bubble));
}

// ─── Strategy identity — parameterized (id + name) ───────────────────────────

struct StrategyIdentityParam {
    std::string     name;
    StrategyFactory create;
    SortStrategyId  expectedId;
};

void PrintTo(const StrategyIdentityParam& p, std::ostream* os) { *os << p.name; }

class StrategyIdentityTest : public ::testing::TestWithParam<StrategyIdentityParam> {};

TEST_P(StrategyIdentityTest, HasCorrectId) {
    EXPECT_EQ(GetParam().create()->id(), GetParam().expectedId);
}

TEST_P(StrategyIdentityTest, HasNonEmptyName) {
    EXPECT_NE(std::string(GetParam().create()->name()), "");
}

INSTANTIATE_TEST_SUITE_P(
    Strategies, StrategyIdentityTest,
    ::testing::Values(
        StrategyIdentityParam{"Ascending",  []{ return std::make_unique<AscendingSortStrategy>();  }, SortStrategyId::Ascending},
        StrategyIdentityParam{"Descending", []{ return std::make_unique<DescendingSortStrategy>(); }, SortStrategyId::Descending},
        StrategyIdentityParam{"Bubble",     []{ return std::make_unique<BubbleSortStrategy>();     }, SortStrategyId::Bubble}
    ),
    [](const ::testing::TestParamInfo<StrategyIdentityParam>& i){ return i.param.name; }
);

// ─── Strategy sorting behavior — parameterized ───────────────────────────────

struct SortBehaviorParam {
    std::string      name;
    StrategyFactory  create;
    std::vector<int> input;
    std::vector<int> expected;
};

void PrintTo(const SortBehaviorParam& p, std::ostream* os) { *os << p.name; }

class StrategyBehaviorTest : public ::testing::TestWithParam<SortBehaviorParam> {};

TEST_P(StrategyBehaviorTest, SortsCorrectly) {
    auto s = GetParam().create();
    auto v = GetParam().input;
    (*s)(v);
    EXPECT_EQ(v, GetParam().expected);
}

TEST_P(StrategyBehaviorTest, EmptyVectorNoCrash) {
    auto s = GetParam().create();
    std::vector<int> v;
    EXPECT_NO_THROW((*s)(v));
    EXPECT_TRUE(v.empty());
}

INSTANTIATE_TEST_SUITE_P(
    Strategies, StrategyBehaviorTest,
    ::testing::Values(
        SortBehaviorParam{"Ascending",  []{ return std::make_unique<AscendingSortStrategy>();  }, {5,3,1,4,2}, {1,2,3,4,5}},
        SortBehaviorParam{"Descending", []{ return std::make_unique<DescendingSortStrategy>(); }, {5,3,1,4,2}, {5,4,3,2,1}},
        SortBehaviorParam{"Bubble",     []{ return std::make_unique<BubbleSortStrategy>();     }, {9,1,8,2,7}, {1,2,7,8,9}}
    ),
    [](const ::testing::TestParamInfo<SortBehaviorParam>& i){ return i.param.name; }
);

// Standalone — specific to Ascending (single element preservation)
TEST(AscendingSortStrategyTest, SingleElement) {
    AscendingSortStrategy strat;
    std::vector<int> v = {42};
    strat(v);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], 42);
}

// Standalone — classic edge case: Bubble on already-sorted input
TEST(BubbleSortStrategyTest, AlreadySorted) {
    BubbleSortStrategy strat;
    std::vector<int> v = {1, 2, 3, 4, 5};
    strat(v);
    EXPECT_EQ(v, (std::vector<int>{1, 2, 3, 4, 5}));
}

// ─── SortStrategyFactory — parameterized ─────────────────────────────────────

struct FactoryParam {
    std::string    name;
    SortStrategyId id;
};

void PrintTo(const FactoryParam& p, std::ostream* os) { *os << p.name; }

class StrategyFactoryTest : public ::testing::TestWithParam<FactoryParam> {
protected:
    SortStrategyFactory factory;
};

TEST_P(StrategyFactoryTest, CreatesNonNull) {
    auto result = factory.create(GetParam().id);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

TEST_P(StrategyFactoryTest, CreatesCorrectType) {
    auto result = factory.create(GetParam().id);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->id(), GetParam().id);
}

INSTANTIATE_TEST_SUITE_P(
    Strategies, StrategyFactoryTest,
    ::testing::Values(
        FactoryParam{"Ascending",  SortStrategyId::Ascending},
        FactoryParam{"Descending", SortStrategyId::Descending},
        FactoryParam{"Bubble",     SortStrategyId::Bubble}
    ),
    [](const ::testing::TestParamInfo<FactoryParam>& i){ return i.param.name; }
);
