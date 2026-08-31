#include <gtest/gtest.h>

#include "kernel/rtlil.h"
#include "tests/unit/kernel/rtlilHelpers.h"

YOSYS_NAMESPACE_BEGIN

namespace RTLIL {
	namespace {
		bool chunks_is_fully_const(const SigSpec &sig)
		{
			for (const SigChunk &chunk : sig.chunks())
				if (chunk.width > 0 && chunk.wire != nullptr)
					return false;
			return true;
		}

		bool chunks_is_fully_state(const SigSpec &sig, State expected)
		{
			for (const SigChunk &chunk : sig.chunks()) {
				if (chunk.width > 0 && chunk.wire != nullptr)
					return false;
				for (State state : chunk.data)
					if (state != expected)
						return false;
			}
			return true;
		}

		bool chunks_is_fully_either(const SigSpec &sig, State first, State second)
		{
			for (const SigChunk &chunk : sig.chunks()) {
				if (chunk.width > 0 && chunk.wire != nullptr)
					return false;
				for (State state : chunk.data)
					if (state != first && state != second)
						return false;
			}
			return true;
		}

		bool chunks_has_const(const SigSpec &sig)
		{
			for (const SigChunk &chunk : sig.chunks())
				if (chunk.width > 0 && chunk.wire == nullptr)
					return true;
			return false;
		}

		bool chunks_has_const(const SigSpec &sig, State expected)
		{
			for (const SigChunk &chunk : sig.chunks())
				if (chunk.width > 0 && chunk.wire == nullptr)
					for (State state : chunk.data)
						if (state == expected)
							return true;
			return false;
		}

		std::optional<Const> chunks_try_as_const(const SigSpec &sig)
		{
			auto chunks = sig.chunks();
			auto it = chunks.begin();
			if (it == chunks.end())
				return Const();
			SigChunk chunk = *it;
			if (chunk.wire != nullptr || ++it != chunks.end())
				return std::nullopt;
			return Const(std::move(chunk.data));
		}

		bool chunks_is_wire(const SigSpec &sig)
		{
			auto chunks = sig.chunks();
			auto it = chunks.begin();
			if (it == chunks.end())
				return false;
			const SigChunk &chunk = *it;
			return chunk.wire != nullptr && chunk.wire->width == sig.size() && ++it == chunks.end();
		}

		bool chunks_is_chunk(const SigSpec &sig)
		{
			auto chunks = sig.chunks();
			auto it = chunks.begin();
			if (it == chunks.end())
				return false;
			return ++it == chunks.end();
		}

		Wire *chunks_as_wire(const SigSpec &sig)
		{
			auto chunks = sig.chunks();
			return chunks.begin()->wire;
		}

		SigChunk chunks_as_chunk(const SigSpec &sig)
		{
			auto chunks = sig.chunks();
			return *chunks.begin();
		}

		Hasher::hash_t chunks_hash(const SigSpec &sig)
		{
			Hasher inner;
			for (const SigChunk &chunk : sig.chunks()) {
				if (chunk.wire == nullptr) {
					for (State state : chunk.data)
						inner.eat(state);
				} else {
					inner.eat(chunk.wire->name);
					inner.eat(chunk.offset);
					inner.eat(chunk.width);
				}
			}
			Hasher::hash_t value = inner.yield();
			if (value == 0)
				value = 1;
			Hasher outer;
			outer.eat(value);
			return outer.yield();
		}
	}

	TEST_F(SigSpecRepTest, OneBitConversion)
	{
		Wire *wire = m->addWire("$wire", 8);
		SigSpec wire_chunk(wire, 3, 1);
		SigSpec wire_bits = wire_chunk;
		(void)wire_bits[0];

		EXPECT_EQ(SigBit(wire_chunk), SigBit(wire, 3));
		EXPECT_EQ(SigBit(wire_bits), SigBit(wire, 3));

		for (State state : {State::S0, State::S1, State::Sx, State::Sz, State::Sa, State::Sm}) {
			SigSpec const_chunk(state);
			SigSpec const_bits = const_chunk;
			(void)const_bits[0];

			EXPECT_EQ(SigBit(const_chunk), SigBit(state));
			EXPECT_EQ(SigBit(const_bits), SigBit(state));
		}

		EXPECT_DEATH((void)SigBit(SigSpec()), "");
		EXPECT_DEATH((void)SigBit(SigSpec(wire, 0, 2)), "");
	}

	TEST_F(SigSpecRepTest, ConstantQueries)
	{
		Wire *wire = m->addWire("$wire", 8);
		std::vector<SigSpec> corpus;
		corpus.emplace_back();

		auto add_both_representations = [&](SigSpec sig) {
			corpus.push_back(sig);
			sig.unpack();
			corpus.push_back(std::move(sig));
		};

		add_both_representations(SigSpec(wire));
		add_both_representations(SigSpec(wire, 2, 4));
		for (State state : {State::S0, State::S1, State::Sx, State::Sz, State::Sa, State::Sm})
			add_both_representations(SigSpec(state, 3));
		add_both_representations(SigSpec(Const(std::vector<State>{
			State::S0, State::S1, State::Sx, State::Sz, State::Sa, State::Sm})));

		SigSpec mixed;
		mixed.append(SigBit(wire, 1));
		mixed.append(State::S0);
		mixed.append(State::Sm);
		corpus.push_back(std::move(mixed));

		SigSpec malformed_const(State::S1);
		malformed_const.chunk_.width = 0;
		corpus.push_back(std::move(malformed_const));
		SigSpec malformed_wire(wire, 0, 1);
		malformed_wire.chunk_.width = 0;
		corpus.push_back(std::move(malformed_wire));
		SigSpec malformed_const_data(State::S1, 2);
		malformed_const_data.chunk_.data.pop_back();
		corpus.push_back(std::move(malformed_const_data));

		for (const SigSpec &sig : corpus) {
			EXPECT_EQ(sig.is_fully_const(), chunks_is_fully_const(sig));
			EXPECT_EQ(sig.is_fully_zero(), chunks_is_fully_state(sig, State::S0));
			EXPECT_EQ(sig.is_fully_ones(), chunks_is_fully_state(sig, State::S1));
			EXPECT_EQ(sig.is_fully_def(), chunks_is_fully_either(sig, State::S0, State::S1));
			EXPECT_EQ(sig.is_fully_undef(), chunks_is_fully_either(sig, State::Sx, State::Sz));
			EXPECT_EQ(sig.has_const(), chunks_has_const(sig));
			for (State state : {State::S0, State::S1, State::Sx, State::Sz, State::Sa, State::Sm})
				EXPECT_EQ(sig.has_const(state), chunks_has_const(sig, state));
			EXPECT_EQ(sig.has_marked_bits(), chunks_has_const(sig, State::Sm));

			std::optional<Const> expected = chunks_try_as_const(sig);
			std::optional<Const> actual = sig.try_as_const();
			ASSERT_EQ(actual.has_value(), expected.has_value());
			if (actual.has_value())
				EXPECT_EQ(*actual, *expected);
		}
	}

	TEST_F(SigSpecRepTest, ShapeQueries)
	{
		Wire *wire = m->addWire("$wire", 8);
		Wire *other_wire = m->addWire("$other", 8);
		std::vector<SigSpec> corpus;
		corpus.emplace_back();

		auto add_both_representations = [&](SigSpec sig) {
			corpus.push_back(sig);
			sig.unpack();
			corpus.push_back(std::move(sig));
		};

		add_both_representations(SigSpec(wire));
		add_both_representations(SigSpec(wire, 2, 4));
		add_both_representations(SigSpec(Const(std::vector<State>{State::S0, State::Sx, State::S1})));

		SigSpec gapped(wire, 1, 3);
		gapped[1] = SigBit(wire, 4);
		corpus.push_back(std::move(gapped));

		SigSpec repeated(wire, 1, 3);
		repeated[1] = repeated[0];
		corpus.push_back(std::move(repeated));

		SigSpec descending(wire, 1, 3);
		for (int i = 0; i < descending.size(); i++)
			descending[i] = SigBit(wire, 3 - i);
		corpus.push_back(std::move(descending));

		SigSpec different_wire(wire, 1, 3);
		different_wire[1] = SigBit(other_wire, 2);
		corpus.push_back(std::move(different_wire));

		SigSpec mixed(wire, 1, 3);
		mixed[1] = State::S0;
		corpus.push_back(std::move(mixed));

		SigSpec malformed_empty(State::S1);
		malformed_empty.chunk_.width = 0;
		corpus.push_back(std::move(malformed_empty));

		for (const SigSpec &sig : corpus) {
			EXPECT_EQ(sig.is_wire(), chunks_is_wire(sig));
			EXPECT_EQ(sig.is_chunk(), chunks_is_chunk(sig));
			if (sig.is_wire() && chunks_is_wire(sig))
				EXPECT_EQ(sig.as_wire(), chunks_as_wire(sig));
			if (sig.is_chunk() && chunks_is_chunk(sig))
				EXPECT_EQ(sig.as_chunk(), chunks_as_chunk(sig));
		}

		SigSpec constant_bits(Const(std::vector<State>{State::S0, State::Sx, State::S1}));
		constant_bits.unpack();
		SigChunk constant_chunk = constant_bits.as_chunk();
		EXPECT_EQ(constant_chunk.wire, nullptr);
		EXPECT_EQ(constant_chunk.offset, 0);
		EXPECT_EQ(constant_chunk.width, 3);
		EXPECT_EQ(constant_chunk.data, (std::vector<State>{State::S0, State::Sx, State::S1}));

		SigSpec wire_bits(wire);
		wire_bits.unpack();
		SigChunk wire_chunk = wire_bits.as_chunk();
		EXPECT_EQ(wire_chunk.wire, wire);
		EXPECT_EQ(wire_chunk.offset, 0);
		EXPECT_EQ(wire_chunk.width, 8);
		EXPECT_TRUE(wire_chunk.data.empty());

		EXPECT_DEATH((void)SigSpec().as_wire(), "");
		EXPECT_DEATH((void)SigSpec().as_chunk(), "");
		EXPECT_DEATH((void)SigSpec(wire, 2, 4).as_wire(), "");
		EXPECT_DEATH((void)corpus[corpus.size() - 2].as_chunk(), "");
		EXPECT_DEATH((void)corpus.back().as_chunk(), "");
	}

	TEST_F(SigSpecRepTest, HashMatchesChunkView)
	{
		Wire *wire = m->addWire("$wire", 8);
		Wire *other_wire = m->addWire("$other", 8);
		std::vector<SigSpec> corpus;
		corpus.emplace_back();

		auto add_both_representations = [&](SigSpec sig) {
			corpus.push_back(sig);
			if (!sig.empty())
				(void)sig[0];
			corpus.push_back(std::move(sig));
		};

		add_both_representations(SigSpec(wire, 2, 4));
		add_both_representations(SigSpec(Const(std::vector<State>{
				State::S0, State::S1, State::Sx, State::Sz, State::Sa, State::Sm})));

		corpus.emplace_back(std::vector<SigBit>{
			SigBit(wire, 1), SigBit(wire, 2), SigBit(State::S0), SigBit(State::S1),
			SigBit(wire, 5), SigBit(wire, 5), SigBit(wire, 4), SigBit(other_wire, 0),
			SigBit(other_wire, 1), SigBit(State::Sx), SigBit(State::Sz)});
		SigSpec malformed_empty(State::S1);
		malformed_empty.chunk_.width = 0;
		corpus.push_back(std::move(malformed_empty));

		for (const SigSpec &sig : corpus)
			EXPECT_EQ(sig.hash_into(Hasher()).yield(), chunks_hash(sig));

		SigSpec changed(wire, 1, 4);
		(void)changed.hash_into(Hasher());
		changed[2] = State::Sx;
		EXPECT_EQ(changed.hash_into(Hasher()).yield(), chunks_hash(changed));
	}
};

YOSYS_NAMESPACE_END
