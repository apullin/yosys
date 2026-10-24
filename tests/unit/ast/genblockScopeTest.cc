#include <gtest/gtest.h>
#include "frontends/ast/ast.h"

YOSYS_NAMESPACE_BEGIN

namespace AST_INTERNAL {
namespace {

class GenblockScopeTest : public ::testing::Test
{
protected:
	AST::AstSrcLocType location;
	std::unique_ptr<AST::AstNode> module;
	std::unique_ptr<ScopeMap::Frame> frame;
	AST::AstNode *previous_module;

	std::unique_ptr<AST::AstNode> node(AST::AstNodeType type, const std::string &name = {})
	{
		auto result = std::make_unique<AST::AstNode>(location, type);
		result->str = name;
		return result;
	}

	void SetUp() override
	{
		StaticTwines::init();
		module = node(AST::AST_MODULE, "\\top");
		previous_module = current_ast_mod;
		current_ast_mod = module.get();
		frame = std::make_unique<ScopeMap::Frame>(current_scope);
		current_scope.clear();
	}

	void TearDown() override
	{
		frame->restore();
		current_ast_mod = previous_module;
	}

	std::unique_ptr<AST::AstNode> reference(const std::string &name)
	{
		auto block = node(AST::AST_BLOCK);
		block->children.push_back(node(AST::AST_IDENTIFIER, name));
		return block;
	}
};

TEST_F(GenblockScopeTest, ResolvesDeclarationsOutsideCurrentScope)
{
	module->children.push_back(node(AST::AST_WIRE, "\\block.x"));
	module->children.push_back(node(AST::AST_LOCALPARAM, "\\block.y"));
	auto block = reference("\\x");
	block->children.push_back(node(AST::AST_IDENTIFIER, "\\y"));
	block->expand_genblock("\\block.");
	EXPECT_EQ(block->children[0]->str, "\\block.x");
	EXPECT_EQ(block->children[1]->str, "\\block.y");
	EXPECT_EQ(current_scope.at("\\block.x"), module->children[0].get());
	EXPECT_EQ(current_scope.at("\\block.y"), module->children[1].get());
}

TEST_F(GenblockScopeTest, RegistersEnumsEvenOnAnInnerScopeMiss)
{
	auto enumeration = node(AST::AST_ENUM, "\\enum_type");
	enumeration->children.push_back(node(AST::AST_ENUM_ITEM, "\\block.choice"));
	module->children.push_back(std::move(enumeration));
	auto block = reference("\\choice");
	block->expand_genblock("\\block.inner.");
	EXPECT_EQ(block->children[0]->str, "\\block.choice");
	EXPECT_EQ(current_scope.at("\\enum_type"), module->children[0].get());
	EXPECT_EQ(current_scope.at("\\block.choice"), module->children[0]->children[0].get());
}

TEST_F(GenblockScopeTest, PreservesDuplicateDeclarationOrder)
{
	module->children.push_back(node(AST::AST_WIRE, "\\block.x"));
	module->children.push_back(node(AST::AST_LOCALPARAM, "\\block.x"));
	auto block = reference("\\x");
	block->expand_genblock("\\block.");
	EXPECT_EQ(current_scope.at("\\block.x"), module->children[1].get());
}

TEST_F(GenblockScopeTest, PreservesLaterEnumNameWrites)
{
	module->children.push_back(node(AST::AST_WIRE, "\\block.x"));
	auto enumeration = node(AST::AST_ENUM, "\\block.x");
	enumeration->children.push_back(node(AST::AST_ENUM_ITEM, "\\other"));
	module->children.push_back(std::move(enumeration));
	auto block = reference("\\x");
	block->expand_genblock("\\block.");
	EXPECT_EQ(current_scope.at("\\block.x"), module->children[1].get());
}

TEST_F(GenblockScopeTest, IgnoresNonDeclarationNodes)
{
	module->children.push_back(node(AST::AST_WIRE, "\\block.x"));
	for (int i = 0; i < 1000; ++i)
		module->children.push_back(node(AST::AST_ASSIGN_EQ, "\\block.x"));
	module->children.push_back(node(AST::AST_CELL, "\\block.x"));
	auto block = reference("\\x");
	block->expand_genblock("\\block.");
	EXPECT_EQ(current_scope.at("\\block.x"), module->children[0].get());
}

TEST_F(GenblockScopeTest, RebuildsAfterEnclosingScopeChanges)
{
	module->children.push_back(node(AST::AST_WIRE, "\\block.x"));
	reference("\\x")->expand_genblock("\\block.");
	current_scope.clear();
	module->children.push_back(node(AST::AST_WIRE, "\\block.y"));
	auto block = reference("\\y");
	block->expand_genblock("\\block.");
	EXPECT_EQ(block->children[0]->str, "\\block.y");
	EXPECT_EQ(current_scope.at("\\block.y"), module->children[1].get());
}

TEST_F(GenblockScopeTest, IndexesAppendedDeclarationsWithoutARebuild)
{
	module->children.push_back(node(AST::AST_WIRE, "\\block.x"));
	reference("\\x")->expand_genblock("\\block.");
	module->children.push_back(node(AST::AST_WIRE, "\\block.y"));
	auto block = reference("\\y");
	block->expand_genblock("\\block.");
	EXPECT_EQ(block->children[0]->str, "\\block.y");
	EXPECT_EQ(current_scope.at("\\block.y"), module->children[1].get());
}

TEST_F(GenblockScopeTest, AppendedDuplicatesRetainDeclarationOrder)
{
	module->children.push_back(node(AST::AST_WIRE, "\\block.x"));
	module->children.push_back(node(AST::AST_WIRE, "\\block.y"));
	reference("\\x")->expand_genblock("\\block.");
	module->children.push_back(node(AST::AST_LOCALPARAM, "\\block.y"));
	auto block = reference("\\y");
	block->expand_genblock("\\block.");
	EXPECT_EQ(block->children[0]->str, "\\block.y");
	EXPECT_EQ(current_scope.at("\\block.y"), module->children[2].get());
}

TEST_F(GenblockScopeTest, RebuildsForADifferentModule)
{
	module->children.push_back(node(AST::AST_WIRE, "\\block.x"));
	module->children.push_back(node(AST::AST_WIRE, "\\block.y"));
	reference("\\x")->expand_genblock("\\block.");
	auto other = node(AST::AST_MODULE, "\\other");
	other->children.push_back(node(AST::AST_WIRE, "\\block.y"));
	other->children.push_back(node(AST::AST_WIRE, "\\block.z"));
	current_ast_mod = other.get();
	auto block = reference("\\y");
	block->expand_genblock("\\block.");
	EXPECT_EQ(block->children[0]->str, "\\block.y");
	EXPECT_EQ(current_scope.at("\\block.y"), other->children[0].get());
	current_ast_mod = module.get();
}

TEST_F(GenblockScopeTest, RebuildsAfterChildrenShrink)
{
	module->children.push_back(node(AST::AST_WIRE, "\\block.x"));
	module->children.push_back(node(AST::AST_WIRE, "\\block.y"));
	module->children.push_back(node(AST::AST_WIRE, "\\block.z"));
	reference("\\x")->expand_genblock("\\block.");
	module->children.resize(1);
	module->children.push_back(node(AST::AST_LOCALPARAM, "\\block.y"));
	auto block = reference("\\y");
	block->expand_genblock("\\block.");
	EXPECT_EQ(block->children[0]->str, "\\block.y");
	EXPECT_EQ(current_scope.at("\\block.y"), module->children[1].get());
}

TEST_F(GenblockScopeTest, SeesLocalDeclarationsAddedDuringExpansion)
{
	module->children.push_back(node(AST::AST_WIRE, "\\block.x"));
	auto block = reference("\\x");
	auto inner = reference("\\local");
	inner->children.insert(inner->children.begin(), node(AST::AST_WIRE, "\\local"));
	block->children.push_back(std::move(inner));
	block->expand_genblock("\\block.");
	EXPECT_EQ(block->children[1]->children[1]->str, "\\block.local");
	EXPECT_EQ(current_scope.at("\\block.local"), block->children[1]->children[0].get());
}

TEST_F(GenblockScopeTest, DoesNotBuildAnUnusedIndex)
{
	module->children.push_back(node(AST::AST_WIRE, "\\x"));
	current_scope["\\x"] = module->children[0].get();
	// A malformed enum would assert if the fallback were visited. Existing
	// bindings must remain on the cheap path without building the index.
	auto enumeration = node(AST::AST_ENUM, "\\unused");
	enumeration->children.push_back(node(AST::AST_WIRE, "\\not_an_enum_item"));
	module->children.push_back(std::move(enumeration));
	auto block = reference("\\x");
	block->expand_genblock("\\block.");
	EXPECT_EQ(block->children[0]->str, "\\x");
}

} // namespace
} // namespace AST_INTERNAL

YOSYS_NAMESPACE_END
