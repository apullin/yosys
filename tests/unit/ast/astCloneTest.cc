#include <gtest/gtest.h>

#include "frontends/ast/ast.h"

YOSYS_NAMESPACE_BEGIN

namespace {

size_t count_ast_nodes(const AST::AstNode &node)
{
	size_t count = 1;
	for (const auto &child : node.children)
		count += count_ast_nodes(*child);
	for (const auto &[key, attribute] : node.attributes) {
		(void)key;
		count += count_ast_nodes(*attribute);
	}
	return count;
}

void expect_cloned_payload(const AST::AstNode &source, const AST::AstNode &clone)
{
	EXPECT_EQ(source.type, clone.type);
	EXPECT_EQ(source.str, clone.str);
	EXPECT_EQ(source.bits, clone.bits);
	EXPECT_EQ(source.is_input, clone.is_input);
	EXPECT_EQ(source.is_output, clone.is_output);
	EXPECT_EQ(source.is_reg, clone.is_reg);
	EXPECT_EQ(source.is_logic, clone.is_logic);
	EXPECT_EQ(source.is_signed, clone.is_signed);
	EXPECT_EQ(source.is_string, clone.is_string);
	EXPECT_EQ(source.is_wand, clone.is_wand);
	EXPECT_EQ(source.is_wor, clone.is_wor);
	EXPECT_EQ(source.range_valid, clone.range_valid);
	EXPECT_EQ(source.range_swapped, clone.range_swapped);
	EXPECT_EQ(source.was_checked, clone.was_checked);
	EXPECT_EQ(source.is_unsized, clone.is_unsized);
	EXPECT_EQ(source.is_custom_type, clone.is_custom_type);
	EXPECT_EQ(source.port_id, clone.port_id);
	EXPECT_EQ(source.range_left, clone.range_left);
	EXPECT_EQ(source.range_right, clone.range_right);
	EXPECT_EQ(source.integer, clone.integer);
	EXPECT_DOUBLE_EQ(source.realvalue, clone.realvalue);
	EXPECT_EQ(source.is_enum, clone.is_enum);
	ASSERT_EQ(source.dimensions.size(), clone.dimensions.size());
	for (size_t i = 0; i < source.dimensions.size(); i++) {
		EXPECT_EQ(source.dimensions[i].range_right, clone.dimensions[i].range_right);
		EXPECT_EQ(source.dimensions[i].range_width, clone.dimensions[i].range_width);
		EXPECT_EQ(source.dimensions[i].range_swapped, clone.dimensions[i].range_swapped);
	}
	EXPECT_EQ(source.unpacked_dimensions, clone.unpacked_dimensions);
	EXPECT_EQ(source.id2ast, clone.id2ast);
	EXPECT_EQ(source.basic_prep, clone.basic_prep);
	EXPECT_EQ(source.lookahead, clone.lookahead);
	EXPECT_EQ(source.location.to_string(), clone.location.to_string());
}

void expect_deep_clone(const AST::AstNode &source, const AST::AstNode &clone)
{
	EXPECT_NE(&source, &clone);
	EXPECT_NE(source.hashidx_, clone.hashidx_);
	expect_cloned_payload(source, clone);

	ASSERT_EQ(source.children.size(), clone.children.size());
	for (size_t i = 0; i < source.children.size(); i++)
		expect_deep_clone(*source.children[i], *clone.children[i]);

	ASSERT_EQ(source.attributes.size(), clone.attributes.size());
	for (const auto &[key, source_attribute] : source.attributes) {
		auto clone_it = clone.attributes.find(key);
		ASSERT_NE(clone_it, clone.attributes.end());
		expect_deep_clone(*source_attribute, *clone_it->second);
	}
}

} // namespace

TEST(AstCloneTest, PreservesCloneAndCloneIntoInvariants)
{
	using namespace AST;

	StaticTwines::init();
	const auto initial_node_count = astnode_count();

	{
		auto filename = std::make_shared<std::string>("ast-clone-test.sv");
		AstSrcLocType location(Position(filename, 3, 5), Position(filename, 7, 11));

		auto declaration = std::make_unique<AstNode>(location, AST_WIRE);
		declaration->str = "\\target";

		auto lhs = std::make_unique<AstNode>(location, AST_IDENTIFIER);
		lhs->str = "\\lhs";
		auto add_left = std::make_unique<AstNode>(location, AST_CONSTANT);
		add_left->integer = 11;
		auto add_right = std::make_unique<AstNode>(location, AST_CONSTANT);
		add_right->integer = 17;
		auto rhs = std::make_unique<AstNode>(location, AST_ADD, std::move(add_left), std::move(add_right));
		auto source = std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::move(lhs), std::move(rhs));

		source->str = "\\source";
		source->bits = {RTLIL::S0, RTLIL::S1, RTLIL::Sx};
		source->is_input = true;
		source->is_output = true;
		source->is_reg = true;
		source->is_logic = true;
		source->is_signed = true;
		source->is_string = true;
		source->is_wand = true;
		source->is_wor = true;
		source->range_valid = true;
		source->range_swapped = true;
		source->was_checked = true;
		source->is_unsized = true;
		source->is_custom_type = true;
		source->port_id = 23;
		source->range_left = 31;
		source->range_right = 4;
		source->integer = 0x12345678;
		source->realvalue = 3.25;
		source->is_enum = true;
		source->dimensions.push_back({7, 8, false});
		source->dimensions.push_back({2, 3, true});
		source->unpacked_dimensions = 1;
		source->id2ast = declaration.get();
		source->basic_prep = true;
		source->lookahead = true;

		auto attribute = std::make_unique<AstNode>(location, AST_CONSTANT);
		attribute->integer = 99;
		const RTLIL::IdString attribute_id = intern_attr_name("\\clone_attribute");
		source->set_attribute(attribute_id, std::move(attribute));
		source->set_in_lvalue_flag(true);
		source->set_in_param_flag(true);

		EXPECT_EQ(initial_node_count + 1 + count_ast_nodes(*source), astnode_count());

		auto clone = source->clone();
		expect_deep_clone(*source, *clone);
		EXPECT_EQ(clone->id2ast, declaration.get());
		EXPECT_FALSE(clone->in_lvalue_from_above);
		EXPECT_FALSE(clone->in_param_from_above);
		EXPECT_FALSE(clone->in_lvalue);
		EXPECT_FALSE(clone->in_param);
		ASSERT_EQ(clone->children.size(), 2u);
		EXPECT_TRUE(clone->children[0]->in_lvalue_from_above);
		EXPECT_FALSE(clone->children[1]->in_lvalue_from_above);
		EXPECT_FALSE(clone->children[0]->in_param_from_above);
		EXPECT_FALSE(clone->children[1]->in_param_from_above);
		EXPECT_TRUE(clone->attributes.at(attribute_id)->in_param_from_above);
		EXPECT_EQ(initial_node_count + 1 + count_ast_nodes(*source) + count_ast_nodes(*clone), astnode_count());

		auto old_child = std::make_unique<AstNode>(location, AST_CONSTANT);
		auto target = std::make_unique<AstNode>(location, AST_BLOCK, std::move(old_child));
		const RTLIL::IdString old_attribute_id = intern_attr_name("\\old_attribute");
		target->set_attribute(old_attribute_id, std::make_unique<AstNode>(location, AST_CONSTANT));
		target->set_in_lvalue_flag(true);
		target->set_in_param_flag(true);
		const auto target_hash = target->hashidx_;

		EXPECT_EQ(initial_node_count + 1 + count_ast_nodes(*source) + count_ast_nodes(*clone) + count_ast_nodes(*target), astnode_count());

		source->cloneInto(*target);
		expect_deep_clone(*source, *target);
		EXPECT_EQ(target_hash, target->hashidx_);
		EXPECT_TRUE(target->in_lvalue_from_above);
		EXPECT_TRUE(target->in_param_from_above);
		EXPECT_TRUE(target->in_lvalue);
		EXPECT_TRUE(target->in_param);
		ASSERT_EQ(target->children.size(), 2u);
		EXPECT_TRUE(target->children[0]->in_lvalue_from_above);
		EXPECT_TRUE(target->children[1]->in_lvalue_from_above);
		EXPECT_TRUE(target->children[0]->in_param_from_above);
		EXPECT_TRUE(target->children[1]->in_param_from_above);
		EXPECT_TRUE(target->attributes.at(attribute_id)->in_param_from_above);
		EXPECT_EQ(target->attributes.count(old_attribute_id), 0u);
		EXPECT_EQ(initial_node_count + 1 + count_ast_nodes(*source) + count_ast_nodes(*clone) + count_ast_nodes(*target), astnode_count());
	}

	EXPECT_EQ(initial_node_count, astnode_count());
}

YOSYS_NAMESPACE_END
