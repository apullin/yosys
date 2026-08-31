#include <gtest/gtest.h>
#include <limits>
#include "kernel/rtlil.h"
#include "kernel/sigtools.h"

YOSYS_NAMESPACE_BEGIN

namespace RTLIL {

	struct SafePrintToStringParamName {
		template <class ParamType>
		std::string operator()(const testing::TestParamInfo<ParamType>& info) const {
			std::string name = testing::PrintToString(info.param);
			for (auto &c : name)
				if (!('0' <= c && c <= '9') && !('a' <= c && c <= 'z') && !('A' <= c && c <= 'Z')  ) c = '_';
			return name;
		}
	};

	class KernelRtlilTest : public testing::Test {
	protected:
		KernelRtlilTest() {
			if (logger().empty()) logger().add_sink<ConsoleLogSink>();
		}
		virtual void SetUp() override {
			StaticTwines::init();
		}
	};

	TEST_F(KernelRtlilTest, ConstAssignCompare)
	{
		Const c1;
		Const c2;
		c2 = c1;
		Const c3(c2);
		EXPECT_TRUE(c2 == c3);
		EXPECT_FALSE(c2 < c3);
	}

	TEST_F(KernelRtlilTest, ConstConvertibleToInt) {
		// A Const whose minimal unsigned size is exactly 32 bits (bit 31 set)
		// but whose full width is larger must not be reported as convertible
		// to a (signed) int when treated as unsigned
		{
			// 33 bits: bits 0..31 = 1, bit 32 = 0 => value 0xffffffff
			std::vector<State> v(33, S1);
			v[32] = S0;
			Const c(v);
			EXPECT_FALSE(c.convertible_to_int(false));
			EXPECT_EQ(c.as_int_saturating(false), std::numeric_limits<int>::max());
			EXPECT_FALSE(c.convertible_to_int(true));
			EXPECT_EQ(c.as_int_saturating(true), std::numeric_limits<int>::max());
		}

		{
			// 32-bit unsigned 0x7fffffff fits in a signed int
			std::vector<State> v(32, S1);
			v[31] = S0;
			Const c(v);
			EXPECT_TRUE(c.convertible_to_int(false));
			EXPECT_EQ(c.as_int(false), 0x7fffffff);
		}

		{
			// 32-bit unsigned 0xffffffff does not fit in a signed int
			std::vector<State> v(32, S1);
			Const c(v);
			EXPECT_FALSE(c.convertible_to_int(false));
			EXPECT_EQ(c.as_int_saturating(false), std::numeric_limits<int>::max());
		}
	}

	TEST_F(KernelRtlilTest, ConstStr) {
		// We have multiple distinct sections since it's annoying
		// to list multiple testcases as friends of Const in kernel/rtlil.h
		{
			std::string foo = "foo";
			Const c1 = foo;
			Const c2;
			c2 = c1;
			Const c3(c2);
			EXPECT_TRUE(c1.is_str());
			EXPECT_TRUE(c2.is_str());
			EXPECT_TRUE(c3.is_str());
		}

		{
			// A binary constant is bitvec backed
			Const cb1(0, 10);
			Const cb2(1, 10);
			Const cb3(cb2);
			std::vector<bool> v1 {false, true};
			std::vector<State> v2 {State::S0, State::S1};
			Const cb4(v1);
			Const cb5(v2);
			EXPECT_TRUE(cb4 == cb5);
			EXPECT_TRUE(cb1.is_bits());
			EXPECT_TRUE(cb2.is_bits());
			EXPECT_TRUE(cb3.is_bits());
			EXPECT_TRUE(cb4.is_bits());
			EXPECT_TRUE(cb5.is_bits());
			EXPECT_EQ(cb1.size(), 10);
			EXPECT_EQ(cb2.size(), 10);
			EXPECT_EQ(cb3.size(), 10);
		}

		{
			// A string constructed Const starts off packed
			std::string foo = "foo";
			Const cs1 = foo;
			EXPECT_TRUE(cs1.is_str());

			// It can be iterated without mutating
			int i = 0;
			for (auto bit : cs1) {
				i += bit;
			}
			EXPECT_EQ(i, 16);
			EXPECT_TRUE(cs1.is_str());

			// It can be mutated via bit iteration and decays into unpacked
			// when an non-defined bit is set.
			for (auto b : cs1) {
				b = State::Sx;
			}
			EXPECT_TRUE(cs1.is_bits());
		}

		{
			Const c(0x12345678);
			EXPECT_TRUE(c.is_str());
			EXPECT_EQ(c.as_int(), 0x12345678);
			EXPECT_TRUE(c.is_str());
		}

		{
			Const c(0xab, 8);
			EXPECT_TRUE(c.is_str());
			EXPECT_EQ(c.as_int(), 0xab);
			EXPECT_TRUE(c.is_str());
		}

		{
			Const c(0x12345678, 80);
			EXPECT_TRUE(c.is_str());
			EXPECT_EQ(c.as_int(), 0x12345678);
			EXPECT_EQ(c[79], S0);
			EXPECT_TRUE(c.is_str());
		}

		{
			Const c(-1, 80);
			EXPECT_TRUE(c.is_str());
			EXPECT_EQ(c.as_int(), -1);
			EXPECT_EQ(c[79], S1);
			EXPECT_TRUE(c.is_str());
		}

		{
			Const c(1 << 24);
			EXPECT_TRUE(c.is_str());
			EXPECT_TRUE(c.as_bool());
			EXPECT_TRUE(c.is_str());
		}

		{
			Const c(0x2, 8);
			EXPECT_TRUE(c.is_str());
			EXPECT_EQ(c.as_string(), "00000010");
			EXPECT_TRUE(c.is_str());
		}

		{
			Const c(" ");
			EXPECT_TRUE(c.is_str());
			EXPECT_EQ(c.decode_string(), " ");
			EXPECT_TRUE(c.is_str());
		}

		{
			Const c(" ");
			EXPECT_TRUE(c.is_str());
			EXPECT_EQ(c.decode_string(), " ");
			EXPECT_TRUE(c.is_str());
		}

		{
			std::vector<State> v = {S0, S0, S0, S0, S0, S1, S0, S0};
			Const c(v);
			EXPECT_EQ(c.decode_string(), " ");
		}

		{
			std::vector<State> v = {S0, S0, S0, S0, S0, S1, S0, Sx};
			Const c(v);
			EXPECT_EQ(c.decode_string(), " ");
		}

		{
			Const c(" ");
			EXPECT_TRUE(c.is_str());
			EXPECT_FALSE(c.is_fully_zero());
			EXPECT_TRUE(c.is_str());
		}

		{
			Const c(" ");
			EXPECT_TRUE(c.is_str());
			EXPECT_FALSE(c.is_fully_ones());
			EXPECT_TRUE(c.is_str());
		}

		{
			Const c(" ");
			EXPECT_TRUE(c.is_str());
			EXPECT_TRUE(c.is_fully_def());
			EXPECT_TRUE(c.is_str());
		}

		{
			Const c(" ");
			EXPECT_TRUE(c.is_str());
			EXPECT_FALSE(c.is_fully_undef());
			EXPECT_TRUE(c.is_str());
		}

		{
			Const c(" ");
			EXPECT_TRUE(c.is_str());
			EXPECT_FALSE(c.is_fully_undef_x_only());
			EXPECT_TRUE(c.is_str());
		}

		{
			Const c(" ");
			EXPECT_TRUE(c.is_str());
			int pos;
			EXPECT_TRUE(c.is_onehot(&pos));
			EXPECT_EQ(pos, 5);
			EXPECT_TRUE(c.is_str());
		}
	}

	TEST_F(KernelRtlilTest, ConstConstIteratorWorks) {
		const Const c(0x2, 2);
		Const::const_iterator it = c.begin();
		ASSERT_NE(it, c.end());
		EXPECT_EQ(*it, State::S0);
		++it;
		ASSERT_NE(it, c.end());
		EXPECT_EQ(*it, State::S1);
		++it;
		EXPECT_EQ(it, c.end());
	}

	TEST_F(KernelRtlilTest, ConstConstIteratorPreincrement) {
		const Const c(0x2, 2);
		Const::const_iterator it = c.begin();
		EXPECT_EQ(*++it, State::S1);
	}

	TEST_F(KernelRtlilTest, ConstConstIteratorPostincrement) {
		const Const c(0x2, 2);
		Const::const_iterator it = c.begin();
		EXPECT_EQ(*it++, State::S0);
	}

	TEST_F(KernelRtlilTest, ConstIteratorWorks) {
		Const c(0x2, 2);
		Const::iterator it = c.begin();
		ASSERT_NE(it, c.end());
		EXPECT_EQ(*it, State::S0);
		++it;
		ASSERT_NE(it, c.end());
		EXPECT_EQ(*it, State::S1);
		++it;
		ASSERT_EQ(it, c.end());
	}

	TEST_F(KernelRtlilTest, ConstIteratorPreincrement) {
		Const c(0x2, 2);
		Const::iterator it = c.begin();
		EXPECT_EQ(*++it, State::S1);
	}

	TEST_F(KernelRtlilTest, ConstIteratorPostincrement) {
		Const c(0x2, 2);
		Const::iterator it = c.begin();
		EXPECT_EQ(*it++, State::S0);
	}

	TEST_F(KernelRtlilTest, ConstIteratorWriteWorks) {
		Const c(0x2, 2);
		Const::iterator it = c.begin();
		EXPECT_EQ(*it, State::S0);
		*it = State::S1;
		EXPECT_EQ(*it, State::S1);
	}

	TEST_F(KernelRtlilTest, ConstBuilder) {
		Const::Builder b;
		EXPECT_EQ(GetSize(b), 0);
		b.push_back(S0);
		EXPECT_EQ(GetSize(b), 1);
		b.push_back(S1);
		EXPECT_EQ(GetSize(b), 2);
		EXPECT_EQ(b.build(), Const(0x2, 2));
	}

	TEST_F(KernelRtlilTest, ConstSet) {
		Const c(0x2, 2);
		c.set(0, S1);
		EXPECT_EQ(c, Const(0x3, 2));
	}

	TEST_F(KernelRtlilTest, ConstResize) {
		Const c(0x2, 2);
		c.resize(4, S1);
		EXPECT_EQ(c, Const(0xe, 4));
	}

	TEST_F(KernelRtlilTest, ConstResizeWidthLimit) {
		Const c;
		EXPECT_DEATH(c.resize(RTLIL::WIDTH_LIMIT, Sx), "");
		EXPECT_NO_FATAL_FAILURE(c.resize(RTLIL::WIDTH_LIMIT - 1, Sx));
	}

	TEST_F(KernelRtlilTest, ConstFromLongLongWidthLimit) {
		EXPECT_DEATH(Const(0, RTLIL::WIDTH_LIMIT), "");
		EXPECT_NO_FATAL_FAILURE(Const(0, RTLIL::WIDTH_LIMIT - 1));
	}

	TEST_F(KernelRtlilTest, ConstFromStateWidthLimit) {
		EXPECT_DEATH(Const(Sx, RTLIL::WIDTH_LIMIT), "");
		EXPECT_NO_FATAL_FAILURE(Const(Sx, RTLIL::WIDTH_LIMIT - 1));
	}

	TEST_F(KernelRtlilTest, ModuleAddWireWidthLimit) {
		std::unique_ptr<Design> design = std::make_unique<Design>();
		Module *mod = design->addModule("$test_mod");
		EXPECT_DEATH(mod->addWire("\\test", RTLIL::WIDTH_LIMIT), "");
		EXPECT_NO_FATAL_FAILURE(mod->addWire("\\test2", RTLIL::WIDTH_LIMIT - 1));
	}

	TEST_F(KernelRtlilTest, ConstEqualStr) {
		EXPECT_EQ(Const("abc"), Const("abc"));
		EXPECT_NE(Const("abc"), Const("def"));
	}

	TEST_F(KernelRtlilTest, ConstEqualBits) {
		std::vector<State> v1 = {S0, S1};
		std::vector<State> v2 = {S1, S0};
		EXPECT_EQ(Const(v1), Const(v1));
		EXPECT_NE(Const(v1), Const(v2));
	}

	TEST_F(KernelRtlilTest, ConstEqualStrBits) {
		std::vector<State> v1 = {S0, S0, S0, S0, S0, S1, S0, S0};
		EXPECT_EQ(Const(v1), Const(" "));
		EXPECT_NE(Const(v1), Const("a"));
	}

	static Hasher::hash_t hash(const Const &c) {
		Hasher h;
		h = c.hash_into(h);
		return h.yield();
	}

	TEST_F(KernelRtlilTest, ConstEqualHashStrBits) {
		std::vector<State> v1 = {S0, S0, S0, S0, S0, S1, S0, S0};
		EXPECT_EQ(hash(Const(v1)), hash(Const(" ")));
		EXPECT_NE(hash(Const(v1)), hash(Const("a")));
	}

	TEST_F(KernelRtlilTest, ConstIsFullyZero) {
		EXPECT_TRUE(Const(0, 8).is_fully_zero());
		EXPECT_FALSE(Const(8, 8).is_fully_zero());
		EXPECT_TRUE(Const().is_fully_zero());
	}

	TEST_F(KernelRtlilTest, ConstIsFullyOnes) {
		EXPECT_TRUE(Const(0xf, 4).is_fully_ones());
		EXPECT_FALSE(Const(3, 4).is_fully_ones());
		EXPECT_TRUE(Const().is_fully_ones());
	}

	TEST_F(KernelRtlilTest, ConstIsFullyDef) {
		EXPECT_TRUE(Const(0xf, 4).is_fully_def());
		std::vector<State> v1 = {S0, Sx};
		EXPECT_FALSE(Const(v1).is_fully_def());
		EXPECT_TRUE(Const().is_fully_def());
	}

	TEST_F(KernelRtlilTest, ConstIsFullyUndef) {
		std::vector<State> v1 = {S0, Sx};
		EXPECT_FALSE(Const(v1).is_fully_undef());
		EXPECT_TRUE(Const(Sz, 2).is_fully_undef());
		EXPECT_TRUE(Const().is_fully_undef());
	}

	TEST_F(KernelRtlilTest, ConstIsFullyUndefXOnly) {
		std::vector<State> v1 = {Sx, Sz};
		EXPECT_FALSE(Const(v1).is_fully_undef_x_only());
		EXPECT_TRUE(Const(Sx, 2).is_fully_undef_x_only());
		EXPECT_TRUE(Const().is_fully_undef_x_only());
	}

	TEST_F(KernelRtlilTest, ConstIsOnehot) {
		int pos;
		EXPECT_TRUE(Const(0x80, 8).is_onehot(&pos));
		EXPECT_EQ(pos, 7);
		EXPECT_FALSE(Const(0x82, 8).is_onehot(&pos));
		EXPECT_FALSE(Const(0, 8).is_onehot(&pos));
		EXPECT_TRUE(Const(1, 1).is_onehot(&pos));
		EXPECT_EQ(pos, 0);
		EXPECT_FALSE(Const(Sx, 1).is_onehot(&pos));
		std::vector<State> v1 = {Sx, S1};
		EXPECT_FALSE(Const(v1).is_onehot(&pos));
		EXPECT_FALSE(Const().is_onehot(&pos));
	}

	TEST_F(KernelRtlilTest, StrpoolAttribute) {
		AttrObject object;
		const IdString id = ID::src;

		EXPECT_TRUE(object.get_strpool_attribute(id).empty());

		object.attributes[id] = Const(std::string("alpha||beta|alpha"));
		EXPECT_EQ(object.get_strpool_attribute(id), pool<std::string>({"alpha", "beta"}));

		object.attributes[id] = Const(std::string("alpha\0beta|gamma", 16));
		EXPECT_EQ(object.get_strpool_attribute(id), pool<std::string>({"alpha", "beta", "gamma"}));

		object.attributes[id] = Const(std::string("\0|", 2));
		EXPECT_TRUE(object.get_strpool_attribute(id).empty());
	}

	TEST_F(KernelRtlilTest, LookupAutoidxId) {
		TwinePool twines;
		IdString id = twines.add(NEW_ID);
		EXPECT_EQ(twines.find(twines.str(id)), id);
	}

	TEST_F(KernelRtlilTest, NewIdBeginsWith) {
		TwinePool twines;
		std::string id = twines.str(twines.add(NEW_ID));
		EXPECT_TRUE(id.starts_with("$auto"));
		EXPECT_FALSE(id.starts_with("xyz"));
		EXPECT_TRUE(id.starts_with("$auto$"));
		EXPECT_FALSE(id.starts_with("abcdefghijklmn"));
		EXPECT_TRUE(id.starts_with("$auto$rtlilTest"));
		EXPECT_FALSE(id.starts_with("$auto$rtlilX"));
	}

	TEST_F(KernelRtlilTest, SigSpecForEachBitPair) {
		Design design;
		Module *mod = design.addModule("$test_mod");
		Wire *a = mod->addWire(ID(a), 6);
		Wire *b = mod->addWire(ID(b), 6);

		SigSpec chunk_a(a, 1, 4);
		SigSpec chunk_b(b, 2, 4);
		SigSpec bits_a(std::vector<SigBit>{SigBit(a, 5), SigBit(State::S0), SigBit(a, 0), SigBit(State::S1)});
		SigSpec bits_b(std::vector<SigBit>{SigBit(b, 1), SigBit(b, 5), SigBit(State::Sx), SigBit(b, 0)});
		SigSpec const_chunk(Const(std::vector<State>{State::S1, State::S0, State::Sx, State::Sz}));

		auto expect_pairs = [](const SigSpec &from, const SigSpec &to) {
			std::vector<std::pair<SigBit, SigBit>> pairs;
			from.for_each_bit_pair(to, [&](const SigBit &from_bit, const SigBit &to_bit) {
				pairs.emplace_back(from_bit, to_bit);
			});
			ASSERT_EQ(GetSize(pairs), GetSize(from));
			for (int i = 0; i < GetSize(pairs); i++) {
				EXPECT_EQ(pairs[i].first, from[i]);
				EXPECT_EQ(pairs[i].second, to[i]);
			}
		};

		expect_pairs(chunk_a, chunk_b);
		expect_pairs(chunk_a, bits_b);
		expect_pairs(bits_a, chunk_b);
		expect_pairs(bits_a, bits_b);
		expect_pairs(chunk_a, const_chunk);
		expect_pairs(bits_a, const_chunk);
		expect_pairs(const_chunk, chunk_b);
		expect_pairs(const_chunk, bits_b);
		expect_pairs(const_chunk, SigSpec(Const(std::vector<State>{State::Sz, State::Sx, State::S0, State::S1})));

		bool visited = false;
		SigSpec().for_each_bit_pair(SigSpec(), [&](const SigBit &, const SigBit &) { visited = true; });
		EXPECT_FALSE(visited);
	}

	TEST_F(KernelRtlilTest, SigMapVectorConnections) {
		Design design;
		Module *mod = design.addModule("$test_mod");
		Wire *a = mod->addWire(ID(a), 4);
		Wire *b = mod->addWire(ID(b), 4);

		SigSpec from(a);
		SigSpec to(std::vector<SigBit>{SigBit(b, 2), SigBit(State::S0), SigBit(b, 0), SigBit(State::S1)});
		SigMap sigmap;
		sigmap.add(from, to);
		for (int i = 0; i < GetSize(from); i++)
			EXPECT_EQ(sigmap(from[i]), to[i]);
	}

	TEST_F(KernelRtlilTest, SigMapApplyWholeSigSpec) {
		Design design;
		Module *mod = design.addModule("$test_mod");
		Wire *a = mod->addWire(ID(a), 6);
		Wire *b = mod->addWire(ID(b), 6);
		Wire *c = mod->addWire(ID(c), 2);

		SigMap sigmap;
		SigSpec wire_input(a, 1, 4);
		SigSpec wire_expected(std::vector<SigBit>{SigBit(b, 5), SigBit(State::S0), SigBit(b, 1), SigBit(State::S1)});
		sigmap.add(wire_input, wire_expected);
		EXPECT_NE(wire_input.hash_into(Hasher()).yield(), wire_expected.hash_into(Hasher()).yield());
		sigmap.apply(wire_input);
		EXPECT_EQ(wire_input, wire_expected);
		EXPECT_EQ(wire_input.hash_into(Hasher()).yield(), wire_expected.hash_into(Hasher()).yield());

		SigSpec bits_input(std::vector<SigBit>{SigBit(c, 0), SigBit(a, 0), SigBit(State::S0), SigBit(a, 5), SigBit(c, 1)});
		sigmap.add(SigBit(a, 0), SigBit(b, 3));
		sigmap.add(SigBit(a, 5), SigBit(State::S1));
		sigmap.add(SigBit(c, 1), SigBit(b, 0));
		SigSpec bits_expected(std::vector<SigBit>{SigBit(c, 0), SigBit(b, 3), SigBit(State::S0), SigBit(State::S1), SigBit(b, 0)});
		EXPECT_NE(bits_input.hash_into(Hasher()).yield(), bits_expected.hash_into(Hasher()).yield());
		sigmap.apply(bits_input);
		EXPECT_EQ(bits_input, bits_expected);
		EXPECT_EQ(bits_input.hash_into(Hasher()).yield(), bits_expected.hash_into(Hasher()).yield());

		SigSpec constant(Const(std::vector<State>{State::S0, State::S1, State::Sx, State::Sz}));
		SigSpec constant_expected = constant;
		sigmap.apply(constant);
		EXPECT_EQ(constant, constant_expected);

		SigSpec empty;
		sigmap.apply(empty);
		EXPECT_TRUE(empty.empty());
	}

	TEST_F(KernelRtlilTest, SigValMapVectorConnectionsContinueAfterSameRoot) {
		Design design;
		Module *mod = design.addModule("$test_mod");
		Wire *a = mod->addWire(ID(a), 3);
		Wire *b = mod->addWire(ID(b), 3);

		SigValMap<unsigned int> sigmap;
		sigmap.addVal(SigBit(a, 1), 1);
		sigmap.addVal(SigBit(b, 1), 2);
		sigmap.add(SigSpec(std::vector<SigBit>{SigBit(a, 0), SigBit(a, 1), SigBit(a, 2)}),
		           SigSpec(std::vector<SigBit>{SigBit(a, 0), SigBit(b, 1), SigBit(b, 2)}));

		SigBit bit = SigBit(a, 1);
		EXPECT_EQ(sigmap.apply_and_get_value(bit), 3u);
		EXPECT_EQ(bit, SigBit(b, 1));
		EXPECT_EQ(sigmap(SigBit(a, 2)), SigBit(b, 2));
	}

	TEST_F(KernelRtlilTest, SigMapCanonicalRepresentatives) {
		Design design;
		Module *mod = design.addModule("$test_mod");
		Wire *a = mod->addWire(ID(a));
		Wire *b = mod->addWire(ID(b));
		Wire *c = mod->addWire(ID(c));
		Wire *d = mod->addWire("\\d");

		SigMap sigmap;
		sigmap.add(SigBit(a), SigBit(b));
		EXPECT_EQ(sigmap(SigBit(a)), SigBit(b));

		sigmap.add(SigBit(a), SigBit(c));
		EXPECT_EQ(sigmap(SigBit(a)), SigBit(c));
		EXPECT_EQ(sigmap(SigBit(b)), SigBit(c));

		sigmap.add(SigBit(c), SigBit(State::S1));
		EXPECT_EQ(sigmap(SigBit(a)), SigBit(State::S1));
		EXPECT_EQ(sigmap(SigBit(c)), SigBit(State::S1));

		SigMap reverse_constant;
		reverse_constant.add(SigBit(State::S0), SigBit(d));
		EXPECT_EQ(reverse_constant(SigBit(d)), SigBit(State::S0));
	}

	TEST_F(KernelRtlilTest, SigValMapPreservesMergedValues) {
		Design design;
		Module *mod = design.addModule("$test_mod");
		Wire *a = mod->addWire(ID(a));
		Wire *b = mod->addWire(ID(b));

		SigValMap<unsigned int> sigmap;
		sigmap.addVal(SigBit(a), 1);
		sigmap.addVal(SigBit(b), 2);
		sigmap.add(SigBit(a), SigBit(b));

		SigBit bit = SigBit(a);
		EXPECT_EQ(sigmap.apply_and_get_value(bit), 3u);
		EXPECT_EQ(bit, SigBit(b));

		sigmap.add(SigBit(b), SigBit(State::S1));
		bit = SigBit(a);
		EXPECT_EQ(sigmap.apply_and_get_value(bit), 3u);
		EXPECT_EQ(bit, SigBit(State::S1));
	}

	class WireRtlVsHdlIndexConversionTest :
		public KernelRtlilTest,
		public testing::WithParamInterface<std::tuple<bool, int, int>>
	{};

	INSTANTIATE_TEST_SUITE_P(
		WireRtlVsHdlIndexConversionInstance,
		WireRtlVsHdlIndexConversionTest,
		testing::Values(
			std::make_tuple(false, 0, 10),
			std::make_tuple(true, 0, 10),
			std::make_tuple(false, 4, 10),
			std::make_tuple(true, 4, 10),
			std::make_tuple(false, -4, 10),
			std::make_tuple(true, -4, 10),
			std::make_tuple(false, 0, 1),
			std::make_tuple(true, 0, 1),
			std::make_tuple(false, 4, 1),
			std::make_tuple(true, 4, 1),
			std::make_tuple(false, -4, 1),
			std::make_tuple(true, -4, 1)
		),
		SafePrintToStringParamName()
	);

	TEST_P(WireRtlVsHdlIndexConversionTest, WireRtlVsHdlIndexConversion) {
		std::unique_ptr<Design> design = std::make_unique<Design>();
		Module *mod = design->addModule("$test_mod");
		Wire *wire = mod->addWire("\\test", 10);

		auto [upto, start_offset, width] = GetParam();

		wire->upto = upto;
		wire->start_offset = start_offset;
		wire->width = width;

		int smallest = INT_MAX;
		int largest = INT_MIN;

		for (int i = 0; i < wire->width; i++) {
			int j = wire->to_hdl_index(i);
			smallest = std::min(smallest, j);
			largest = std::max(largest, j);
			EXPECT_EQ(
				std::make_pair(wire->from_hdl_index(j), j),
				std::make_pair(i, wire->to_hdl_index(i))
			);
		}

		EXPECT_EQ(smallest, start_offset);
		EXPECT_EQ(largest, start_offset + wire->width - 1);

		for (int i = 1; i < wire->width; i++) {
			EXPECT_EQ(
				wire->to_hdl_index(i) - wire->to_hdl_index(i - 1),
				upto ? -1 : 1
			);
		}

		for (int j = smallest; j < largest; j++) {
			int i = wire->from_hdl_index(j);
			EXPECT_EQ(
				std::make_pair(wire->from_hdl_index(j), j),
				std::make_pair(i, wire->to_hdl_index(i))
			);
		}

		for (int i = -10; i < 0; i++)
			EXPECT_EQ(wire->to_hdl_index(i), INT_MIN);
		for (int i = wire->width; i < wire->width + 10; i++)
			EXPECT_EQ(wire->to_hdl_index(i), INT_MIN);
		for (int j = smallest - 10; j < smallest; j++)
			EXPECT_EQ(wire->from_hdl_index(j), INT_MIN);
		for (int j = largest + 1; j < largest + 11; j++)
			EXPECT_EQ(wire->from_hdl_index(j), INT_MIN);
	}

}

YOSYS_NAMESPACE_END
