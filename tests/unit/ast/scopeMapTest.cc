#include <gtest/gtest.h>
#include "frontends/ast/ast.h"

YOSYS_NAMESPACE_BEGIN

namespace AST_INTERNAL {
namespace {

class ScopeMapTest : public ::testing::Test
{
protected:
	ScopeMap scope;
	std::unique_ptr<AST::AstNode> first, second, third;

	void SetUp() override
	{
		StaticTwines::init();
		AST::AstSrcLocType location;
		first = std::make_unique<AST::AstNode>(location, AST::AST_NONE);
		second = std::make_unique<AST::AstNode>(location, AST::AST_NONE);
		third = std::make_unique<AST::AstNode>(location, AST::AST_NONE);
	}
};

TEST_F(ScopeMapTest, RestoresOverwrittenBinding)
{
	scope["name"] = first.get();
	{
		ScopeMap::Frame frame(scope);
		scope["name"] = second.get();
		scope["name"] = third.get();
	}
	EXPECT_EQ(scope.at("name"), first.get());
}

TEST_F(ScopeMapTest, RemovesAddedBindings)
{
	{
		ScopeMap::Frame frame(scope);
		scope["local"] = first.get();
	}
	EXPECT_TRUE(scope.empty());
}

TEST_F(ScopeMapTest, PreservesExistingNullBinding)
{
	scope["null"] = nullptr;
	{
		ScopeMap::Frame frame(scope);
		scope["null"] = first.get();
		scope["missing"] = nullptr;
	}
	EXPECT_EQ(scope.count("null"), 1);
	EXPECT_EQ(scope.at("null"), nullptr);
	EXPECT_EQ(scope.count("missing"), 0);
}

TEST_F(ScopeMapTest, NestedFramesRestoreTheirCallers)
{
	scope["argument"] = first.get();
	{
		ScopeMap::Frame outer(scope);
		scope["argument"] = second.get();
		scope["outer"] = second.get();
		{
			ScopeMap::Frame inner(scope);
			scope["argument"] = third.get();
			scope["outer"] = third.get();
			scope["inner"] = third.get();
		}
		EXPECT_EQ(scope.at("argument"), second.get());
		EXPECT_EQ(scope.at("outer"), second.get());
		EXPECT_EQ(scope.count("inner"), 0);
	}
	EXPECT_EQ(scope.at("argument"), first.get());
	EXPECT_EQ(scope.size(), 1);
}

TEST_F(ScopeMapTest, RestoresClearedScope)
{
	scope["first"] = first.get();
	scope["second"] = second.get();
	{
		ScopeMap::Frame frame(scope);
		scope["first"] = third.get();
		scope["temporary"] = third.get();
		scope.clear();
		scope["replacement"] = third.get();
	}
	EXPECT_EQ(scope.size(), 2);
	EXPECT_EQ(scope.at("first"), first.get());
	EXPECT_EQ(scope.at("second"), second.get());
}

TEST_F(ScopeMapTest, ExplicitRestoreIsIdempotent)
{
	scope["name"] = first.get();
	ScopeMap::Frame frame(scope);
	scope["name"] = second.get();
	frame.restore();
	frame.restore();
	EXPECT_EQ(scope.at("name"), first.get());
}

TEST_F(ScopeMapTest, RestoresAfterRehashing)
{
	for (int i = 0; i < 100; ++i)
		scope["global" + std::to_string(i)] = first.get();
	{
		ScopeMap::Frame frame(scope);
		for (int i = 0; i < 100; ++i)
			scope["global" + std::to_string(i)] = second.get();
		for (int i = 0; i < 1000; ++i)
			scope["local" + std::to_string(i)] = third.get();
	}
	EXPECT_EQ(scope.size(), 100);
	for (int i = 0; i < 100; ++i)
		EXPECT_EQ(scope.at("global" + std::to_string(i)), first.get());
}

TEST_F(ScopeMapTest, RepeatedCallsDoNotGrowScope)
{
	scope["global"] = first.get();
	for (int i = 0; i < 100; ++i) {
		ScopeMap::Frame frame(scope);
		scope["local" + std::to_string(i)] = second.get();
	}
	EXPECT_EQ(scope.size(), 1);
	EXPECT_EQ(scope.at("global"), first.get());
}

} // namespace
} // namespace AST_INTERNAL

YOSYS_NAMESPACE_END
