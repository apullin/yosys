#include <gtest/gtest.h>

#include "kernel/rtlil.h"
#include "tests/unit/kernel/rtlilHelpers.h"

YOSYS_NAMESPACE_BEGIN

namespace RTLIL {
	TEST_F(SigSpecRepTest, MixedEquality)
	{
		Wire *wire = m->addWire("$wire", 8);
		Wire *other_wire = m->addWire("$other", 8);

		SigSpec wire_chunk(wire, 2, 4);
		SigSpec wire_bits = wire_chunk;
		wire_bits.unpack();
		ASSERT_EQ(wire_chunk.rep_, SigSpec::Representation::CHUNK);
		ASSERT_EQ(wire_bits.rep_, SigSpec::Representation::BITS);
		EXPECT_EQ(wire_chunk, wire_bits);
		EXPECT_EQ(wire_bits, wire_chunk);
		EXPECT_EQ(wire_chunk, wire_chunk);
		EXPECT_EQ(wire_bits, wire_bits);

		SigSpec early_offset_bits = wire_bits;
		early_offset_bits[0] = SigBit(wire, 3);
		EXPECT_NE(wire_chunk, early_offset_bits);
		EXPECT_NE(early_offset_bits, wire_chunk);

		SigSpec wrong_wire_bits = wire_bits;
		wrong_wire_bits[2] = SigBit(other_wire, 4);
		EXPECT_NE(wire_chunk, wrong_wire_bits);
		EXPECT_NE(wrong_wire_bits, wire_chunk);

		SigSpec wrong_offset_bits = wire_bits;
		wrong_offset_bits[3] = SigBit(wire, 6);
		EXPECT_NE(wire_chunk, wrong_offset_bits);
		EXPECT_NE(wrong_offset_bits, wire_chunk);

		SigSpec repeated_wire_bits = wire_bits;
		repeated_wire_bits[1] = repeated_wire_bits[0];
		EXPECT_NE(wire_chunk, repeated_wire_bits);

		SigSpec descending_wire_bits = wire_bits;
		for (int i = 0; i < descending_wire_bits.size(); i++)
			descending_wire_bits[i] = SigBit(wire, 5 - i);
		EXPECT_NE(wire_chunk, descending_wire_bits);

		SigSpec wire_constant_bits = wire_bits;
		wire_constant_bits[0] = State::S0;
		EXPECT_NE(wire_chunk, wire_constant_bits);
		EXPECT_NE(wire_constant_bits, wire_chunk);

		Const value(std::vector<State>{State::S0, State::S1, State::Sx, State::Sz, State::Sa, State::Sm});
		SigSpec const_chunk(value);
		SigSpec const_bits = const_chunk;
		const_bits.unpack();
		ASSERT_EQ(const_chunk.rep_, SigSpec::Representation::CHUNK);
		ASSERT_EQ(const_bits.rep_, SigSpec::Representation::BITS);
		EXPECT_EQ(const_chunk, const_bits);
		EXPECT_EQ(const_bits, const_chunk);
		EXPECT_EQ(const_chunk, const_chunk);
		EXPECT_EQ(const_bits, const_bits);

		SigSpec early_const_bits = const_bits;
		early_const_bits[0] = State::S1;
		EXPECT_NE(const_chunk, early_const_bits);
		EXPECT_NE(early_const_bits, const_chunk);

		SigSpec wrong_const_bits = const_bits;
		wrong_const_bits[4] = State::S1;
		EXPECT_NE(const_chunk, wrong_const_bits);
		EXPECT_NE(wrong_const_bits, const_chunk);

		SigSpec const_with_wire_bit = const_bits;
		const_with_wire_bit[0] = SigBit(wire, 0);
		EXPECT_NE(const_chunk, const_with_wire_bit);
		EXPECT_NE(const_with_wire_bit, const_chunk);

		EXPECT_NE(wire_chunk, const_bits.extract(0, wire_chunk.size()));
		EXPECT_NE(wire_chunk, SigSpec(wire, 2, 3));

		SigSpec malformed_const_offset(value);
		malformed_const_offset.chunk_.offset = 1;
		EXPECT_NE(malformed_const_offset, const_bits);
		EXPECT_NE(const_bits, malformed_const_offset);

		SigSpec malformed_const_data(value);
		malformed_const_data.chunk_.data.pop_back();
		EXPECT_NE(malformed_const_data, const_bits);
		EXPECT_NE(const_bits, malformed_const_data);

		SigSpec malformed_wire_data(wire, 2, 4);
		malformed_wire_data.chunk_.data.push_back(State::S0);
		EXPECT_NE(malformed_wire_data, wire_bits);
		EXPECT_NE(wire_bits, malformed_wire_data);

		SigSpec empty_bits;
		SigSpec malformed_empty_chunk(value);
		malformed_empty_chunk.chunk_.width = 0;
		EXPECT_EQ(malformed_empty_chunk, empty_bits);
		EXPECT_EQ(empty_bits, malformed_empty_chunk);
	}
};

YOSYS_NAMESPACE_END
