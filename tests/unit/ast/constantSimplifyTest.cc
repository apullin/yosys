#include <gtest/gtest.h>
#include "frontends/ast/ast.h"

YOSYS_NAMESPACE_BEGIN

namespace AST {
namespace {

class AstConstantSimplifyTest : public ::testing::Test
{
protected:
	AstSrcLocType location;

	void SetUp() override
	{
		StaticTwines::init();
		if (logger().empty())
			logger().add_sink<ConsoleLogSink>();
	}

	std::unique_ptr<AstNode> make_constant()
	{
		return AstNode::mkconst_int(location, 1, false, 1);
	}
};

TEST_F(AstConstantSimplifyTest, PreparesLeafConstant)
{
	auto constant = make_constant();
	ASSERT_FALSE(constant->basic_prep);

	EXPECT_FALSE(constant->simplify(true, 1, -1, false));
	EXPECT_TRUE(constant->basic_prep);
}

TEST_F(AstConstantSimplifyTest, PreparesAttributesBeforeReturning)
{
	auto constant = make_constant();
	auto attribute = make_constant();
	AstNode *attribute_ptr = attribute.get();
	constant->set_attribute(ID::keep, std::move(attribute));
	ASSERT_FALSE(attribute_ptr->basic_prep);

	EXPECT_FALSE(constant->simplify(true, 1, -1, false));
	EXPECT_TRUE(constant->basic_prep);
	EXPECT_TRUE(attribute_ptr->basic_prep);
}

TEST_F(AstConstantSimplifyTest, PreparesChildrenBeforeReturning)
{
	auto constant = make_constant();
	auto child = make_constant();
	AstNode *child_ptr = child.get();
	constant->children.push_back(std::move(child));
	ASSERT_FALSE(child_ptr->basic_prep);

	EXPECT_FALSE(constant->simplify(true, 1, -1, false));
	EXPECT_TRUE(constant->basic_prep);
	EXPECT_TRUE(child_ptr->basic_prep);
}

#ifndef NDEBUG
TEST_F(AstConstantSimplifyTest, PreservesStageZeroTypeAssertion)
{
	auto constant = make_constant();

	EXPECT_DEATH(constant->simplify(true, 0, -1, false), "");
}
#endif

} // namespace
} // namespace AST

YOSYS_NAMESPACE_END
