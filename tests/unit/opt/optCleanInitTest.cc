#include <gtest/gtest.h>
#include <memory>
#include "kernel/register.h"
#include "passes/opt/clean/opt_clean.h"

YOSYS_NAMESPACE_BEGIN

namespace {

struct InitInjectionMonitor : RTLIL::Monitor {
	RTLIL::Module *module;
	RTLIL::Wire *wire;
	pool<RTLIL::Monitor*> &registry;
	RTLIL::Const value;
	bool unregister_on_callback;
	bool fired = false;

	InitInjectionMonitor(RTLIL::Module *module, RTLIL::Wire *wire,
			pool<RTLIL::Monitor*> &registry, RTLIL::Const value, bool unregister_on_callback)
		: module(module), wire(wire), registry(registry), value(std::move(value)),
		  unregister_on_callback(unregister_on_callback) {}

	~InitInjectionMonitor() override {
		registry.erase(this);
	}

	void notify_connect(RTLIL::Module *connected_module, const RTLIL::SigSig&) override {
		if (connected_module != module || fired)
			return;
		fired = true;
		EXPECT_EQ(wire->attributes.count(ID::init), 0);
		wire->attributes[ID::init] = value;
		if (unregister_on_callback) {
			// A sole-entry pool iterator advances from index 0 to end (-1)
			// after this callback, so erasing that entry is safe.
			ASSERT_EQ(registry.size(), 1);
			registry.erase(this);
		}
	}
};

class OptCleanInitTest : public testing::Test {
protected:
	std::unique_ptr<RTLIL::Design> design;
	RTLIL::Module *module;

	void SetUp() override {
		StaticTwines::init();
		design = std::make_unique<RTLIL::Design>();
		module = design->addModule("\\top");
	}

	bool clean_init() {
		ParallelDispatchThreadPool pool(ThreadPool::pool_size(0, 2));
		ParallelDispatchThreadPool::Subpool subpool(pool, 2);
		return rmunused_module_init(module, subpool, false);
	}

	void clean_signals_to_fixpoint(bool purge = false) {
		CleanRunContext clean_ctx(design.get(), {module}, {purge, false});
		ParallelDispatchThreadPool::Subpool subpool(clean_ctx.thread_pool, 0);
		for (int iteration = 0; iteration < 8; iteration++) {
			AnalysisContext actx(module, subpool);
			if (!rmunused_module_signals(module, actx, clean_ctx))
				return;
		}
		FAIL() << "Signal cleanup did not reach a fixpoint";
	}

	RTLIL::Cell *add_ff(RTLIL::Wire *q) {
		auto clk = module->addWire("\\clk");
		auto data = module->addWire("\\data", q->width);
		clk->port_input = data->port_input = q->port_output = true;
		clk->port_id = 1;
		data->port_id = 2;
		q->port_id = 3;
		module->fixup_ports();
		return module->addDff("\\ff", clk, data, q, true);
	}

	void expect_unchanged() {
		auto wires = module->wires_;
		auto cells = module->cells_;
		auto connections = module->connections();
		auto module_attrs = module->attributes;
		auto scratchpad = design->scratchpad;
		std::vector<dict<RTLIL::IdString, RTLIL::Const>> wire_attrs, cell_attrs, params;
		std::vector<dict<RTLIL::IdString, RTLIL::SigSpec>> ports;
		for (auto wire : module->wires())
			wire_attrs.push_back(wire->attributes);
		for (auto cell : module->cells()) {
			cell_attrs.push_back(cell->attributes);
			params.push_back(cell->parameters);
			ports.push_back(cell->connections());
		}

		EXPECT_FALSE(clean_init());
		EXPECT_EQ(module->wires_, wires);
		EXPECT_EQ(module->cells_, cells);
		EXPECT_EQ(module->connections(), connections);
		EXPECT_EQ(module->attributes, module_attrs);
		EXPECT_EQ(design->scratchpad, scratchpad);
		size_t i = 0;
		for (auto wire : module->wires())
			EXPECT_EQ(wire->attributes, wire_attrs.at(i++));
		i = 0;
		for (auto cell : module->cells()) {
			EXPECT_EQ(cell->attributes, cell_attrs.at(i));
			EXPECT_EQ(cell->parameters, params.at(i));
			EXPECT_EQ(cell->connections(), ports.at(i++));
		}
	}

	void expect_monitor_fallback(bool design_monitor, bool unregister_on_callback, RTLIL::Const value) {
		auto source = module->addWire("\\source", 2);
		source->port_input = true;
		source->port_id = 1;
		module->fixup_ports();
		auto alias = module->addWire("\\alias", 2);
		module->connect(alias, source);
		design->scratchpad_set_bool("opt.did_something", false);

		auto &registry = design_monitor ? design->monitors : module->monitors;
		ASSERT_TRUE(module->monitors.empty());
		ASSERT_TRUE(design->monitors.empty());
		InitInjectionMonitor monitor(module, alias, registry, std::move(value), unregister_on_callback);
		registry.insert(&monitor);
		ASSERT_EQ(registry.size(), 1);

		// This fixture has no temporary cells or removable wires. The callback
		// first runs when signal cleanup reconnects the alias after normalizing
		// attributes, so only the original init phase can remove its injection.
		Pass::call(design.get(), "opt_clean");

		EXPECT_TRUE(monitor.fired);
		EXPECT_EQ(alias->attributes.count(ID::init), 0);
		EXPECT_TRUE(design->scratchpad_get_bool("opt.did_something"));
		EXPECT_EQ(module->wires_size(), 2);
		EXPECT_EQ(SigMap(module)(alias), RTLIL::SigSpec(source));
		EXPECT_EQ(registry.empty(), unregister_on_callback);
	}
};

TEST_F(OptCleanInitTest, EmptyModule)
{
	expect_unchanged();
	EXPECT_FALSE(design->scratchpad_get_bool("opt.did_something"));
}

TEST_F(OptCleanInitTest, NoInitPreservesFfConnectionsAndScratchpad)
{
	auto q = module->addWire("\\q", 2);
	add_ff(q);
	module->connect(module->addWire("\\alias", 2), q);
	module->connect(module->addWire("\\unknown", 2), RTLIL::Const(RTLIL::State::Sx, 2));
	for (bool did_something : {false, true}) {
		design->scratchpad_set_bool("opt.did_something", did_something);
		expect_unchanged();
		EXPECT_EQ(design->scratchpad_get_bool("opt.did_something"), did_something);
	}
}

TEST_F(OptCleanInitTest, IgnoresOtherAttributesAndNonWireInit)
{
	auto q = module->addWire("\\q");
	auto ff = add_ff(q);
	q->set_bool_attribute(ID::keep);
	q->set_string_attribute(ID::src, "unit.v:1");
	ff->attributes[ID::init] = RTLIL::Const(1, 1);
	module->attributes[ID::init] = RTLIL::Const(1, 1);
	expect_unchanged();
}

TEST_F(OptCleanInitTest, EmptyAndUndefInitAttributesStillGetRemoved)
{
	auto empty = module->addWire("\\empty", 2);
	auto unknown = module->addWire("\\unknown", 2);
	auto high_z = module->addWire("\\high_z", 2);
	empty->attributes[ID::init] = RTLIL::Const();
	unknown->attributes[ID::init] = RTLIL::Const(RTLIL::State::Sx, 2);
	high_z->attributes[ID::init] = RTLIL::Const(RTLIL::State::Sz, 2);
	EXPECT_TRUE(clean_init());
	for (auto wire : {empty, unknown, high_z})
		EXPECT_EQ(wire->attributes.count(ID::init), 0);
	EXPECT_EQ(module->wires_size(), 3);
	EXPECT_TRUE(design->scratchpad_get_bool("opt.did_something"));
	expect_unchanged();
}

TEST_F(OptCleanInitTest, DefinedFfOutputInitIsPreserved)
{
	auto q = module->addWire("\\q", 2);
	add_ff(q);
	q->attributes[ID::init] = RTLIL::Const(2, 2);
	expect_unchanged();
}

TEST_F(OptCleanInitTest, RedundantAliasInitIsRemovedButFfInitRemains)
{
	auto q = module->addWire("\\q");
	add_ff(q);
	auto alias = module->addWire("\\alias");
	module->connect(alias, q);
	q->attributes[ID::init] = alias->attributes[ID::init] = RTLIL::Const(1, 1);
	auto connections = module->connections();
	EXPECT_TRUE(clean_init());
	EXPECT_EQ(alias->attributes.count(ID::init), 0);
	ASSERT_EQ(q->attributes.count(ID::init), 1);
	EXPECT_EQ(q->attributes.at(ID::init), RTLIL::Const(1, 1));
	EXPECT_EQ(module->connections(), connections);
	EXPECT_TRUE(design->scratchpad_get_bool("opt.did_something"));
	expect_unchanged();
}

TEST_F(OptCleanInitTest, AliasInitWithoutFfInitIsPreserved)
{
	auto q = module->addWire("\\q");
	add_ff(q);
	auto alias = module->addWire("\\alias");
	module->connect(alias, q);
	alias->attributes[ID::init] = RTLIL::Const(1, 1);
	expect_unchanged();
	EXPECT_EQ(q->attributes.count(ID::init), 0);
}

TEST_F(OptCleanInitTest, ConflictingAliasInitIsPreserved)
{
	auto q = module->addWire("\\q");
	add_ff(q);
	auto alias = module->addWire("\\alias");
	module->connect(alias, q);
	q->attributes[ID::init] = RTLIL::Const(0, 1);
	alias->attributes[ID::init] = RTLIL::Const(1, 1);
	expect_unchanged();
}

TEST_F(OptCleanInitTest, NormalizedPartialAndPermutedAliasesNeedNoInitCleanup)
{
	auto q = module->addWire("\\q", 4);
	auto ff = add_ff(q);
	auto permuted = module->addWire("$permuted", 4);
	permuted->set_bool_attribute(ID::keep);
	RTLIL::SigSpec order(std::vector<RTLIL::SigBit>{
		RTLIL::SigBit(q, 3), RTLIL::SigBit(q, 1), RTLIL::SigBit(q, 2), RTLIL::SigBit(q, 0)});
	module->connect(permuted, order);
	ff->setPort(ID::Q, permuted);
	permuted->attributes[ID::init] = RTLIL::Const(std::vector<RTLIL::State>{
		RTLIL::State::S1, RTLIL::State::S0, RTLIL::State::Sx, RTLIL::State::Sz});
	auto partial = module->addWire("\\partial", 2);
	module->connect(partial, RTLIL::SigSpec(q, 1, 2));
	partial->attributes[ID::init] = RTLIL::Const(2, 2);

	clean_signals_to_fixpoint();

	EXPECT_EQ(ff->getPort(ID::Q), order);
	EXPECT_EQ(permuted->attributes.count(ID::init), 0);
	EXPECT_EQ(partial->attributes.count(ID::init), 0);
	ASSERT_EQ(q->attributes.count(ID::init), 1);
	EXPECT_EQ(q->attributes.at(ID::init), RTLIL::Const(std::vector<RTLIL::State>{
		RTLIL::State::Sz, RTLIL::State::S0, RTLIL::State::S1, RTLIL::State::S1}));
	expect_unchanged();
}

TEST_F(OptCleanInitTest, NormalizedZeroShortAndLongInitWidthsNeedNoInitCleanup)
{
	auto q = module->addWire("\\q", 4);
	add_ff(q);
	q->attributes[ID::init] = RTLIL::Const(1, 2);
	auto wide_init = module->addWire("\\wide_init", 2);
	wide_init->set_bool_attribute(ID::keep);
	wide_init->attributes[ID::init] = RTLIL::Const(14, 4);
	auto empty_init = module->addWire("\\empty_init", 2);
	empty_init->set_bool_attribute(ID::keep);
	empty_init->attributes[ID::init] = RTLIL::Const();
	auto zero_port = module->addWire("\\zero_port", 0);
	zero_port->port_output = true;
	zero_port->port_id = 4;
	zero_port->attributes[ID::init] = RTLIL::Const(1, 1);
	module->fixup_ports();
	module->addWire("\\zero_internal", 0)->attributes[ID::init] = RTLIL::Const(1, 1);
	module->addWire("$unused");

	clean_signals_to_fixpoint();

	EXPECT_EQ(module->wire("\\zero_internal"), nullptr);
	EXPECT_EQ(module->wire("$unused"), nullptr);
	EXPECT_EQ(zero_port->attributes.count(ID::init), 0);
	EXPECT_EQ(empty_init->attributes.count(ID::init), 0);
	ASSERT_EQ(q->attributes.count(ID::init), 1);
	EXPECT_EQ(q->attributes.at(ID::init), RTLIL::Const(std::vector<RTLIL::State>{
		RTLIL::State::S1, RTLIL::State::S0, RTLIL::State::Sx, RTLIL::State::Sx}));
	ASSERT_EQ(wide_init->attributes.count(ID::init), 1);
	EXPECT_EQ(wide_init->attributes.at(ID::init), RTLIL::Const(2, 2));
	expect_unchanged();
}

TEST_F(OptCleanInitTest, NormalizedMixedStatesAndConstantFfOutputsNeedNoInitCleanup)
{
	auto q = module->addWire("\\q", 8);
	auto ff = add_ff(q);
	q->attributes[ID::init] = RTLIL::Const(std::vector<RTLIL::State>{
		RTLIL::State::S1, RTLIL::State::S0, RTLIL::State::Sx, RTLIL::State::Sz,
		RTLIL::State::Sa, RTLIL::State::Sm, RTLIL::State::S0, RTLIL::State::S1});
	module->connect(RTLIL::SigSpec(q, 0, 2), RTLIL::Const(std::vector<RTLIL::State>{
		RTLIL::State::Sx, RTLIL::State::Sz}));
	auto undef = module->addWire("\\undef", 2);
	undef->set_bool_attribute(ID::keep);
	undef->attributes[ID::init] = RTLIL::Const(std::vector<RTLIL::State>{
		RTLIL::State::Sx, RTLIL::State::Sz});

	clean_signals_to_fixpoint();

	EXPECT_EQ(ff->getPort(ID::Q)[0], RTLIL::SigBit(RTLIL::State::Sx));
	EXPECT_EQ(ff->getPort(ID::Q)[1], RTLIL::SigBit(RTLIL::State::Sz));
	EXPECT_EQ(SigMap(module)(RTLIL::SigBit(q, 0)), RTLIL::SigBit(RTLIL::State::Sx));
	EXPECT_EQ(undef->attributes.count(ID::init), 0);
	ASSERT_EQ(q->attributes.count(ID::init), 1);
	EXPECT_EQ(q->attributes.at(ID::init), RTLIL::Const(std::vector<RTLIL::State>{
		RTLIL::State::Sx, RTLIL::State::Sx, RTLIL::State::Sx, RTLIL::State::Sz,
		RTLIL::State::Sa, RTLIL::State::Sm, RTLIL::State::S0, RTLIL::State::S1}));
	expect_unchanged();
}

TEST_F(OptCleanInitTest, ProductionCleanNormalizesInitWithoutChangingScratchpad)
{
	auto q = module->addWire("\\q", 2);
	add_ff(q);
	q->attributes[ID::init] = RTLIL::Const(1, 2);
	auto alias = module->addWire("\\alias", 2);
	module->connect(alias, RTLIL::SigSpec(std::vector<RTLIL::SigBit>{
		RTLIL::SigBit(q, 1), RTLIL::SigBit(q, 0)}));
	alias->attributes[ID::init] = RTLIL::Const(2, 2);
	design->scratchpad_set_bool("opt.did_something", false);

	Pass::call(design.get(), "opt_clean");

	EXPECT_EQ(alias->attributes.count(ID::init), 0);
	ASSERT_EQ(q->attributes.count(ID::init), 1);
	EXPECT_EQ(q->attributes.at(ID::init), RTLIL::Const(1, 2));
	EXPECT_FALSE(design->scratchpad_get_bool("opt.did_something"));
	expect_unchanged();
}

TEST_F(OptCleanInitTest, PurgedNormalizedAliasesNeedNoInitCleanup)
{
	auto q = module->addWire("\\q", 3);
	auto ff = add_ff(q);
	q->attributes[ID::init] = RTLIL::Const(5, 3);
	auto alias = module->addWire("\\public_alias", 2);
	module->connect(alias, RTLIL::SigSpec(std::vector<RTLIL::SigBit>{
		RTLIL::SigBit(q, 2), RTLIL::SigBit(q, 0)}));
	alias->attributes[ID::init] = RTLIL::Const(3, 2);

	clean_signals_to_fixpoint(true);

	EXPECT_EQ(module->wire("\\public_alias"), nullptr);
	EXPECT_EQ(ff->getPort(ID::Q), RTLIL::SigSpec(q));
	ASSERT_EQ(q->attributes.count(ID::init), 1);
	EXPECT_EQ(q->attributes.at(ID::init), RTLIL::Const(5, 3));
	expect_unchanged();
}

TEST_F(OptCleanInitTest, ProductionCleanLeavesPartiallySelectedModuleUntouched)
{
	auto q = module->addWire("\\q", 2);
	auto ff = add_ff(q);
	auto alias = module->addWire("\\alias", 2);
	module->connect(alias, q);
	alias->attributes[ID::init] = RTLIL::Const(1, 2);
	auto connections = module->connections();
	auto ports = ff->connections();
	design->scratchpad_set_bool("opt.did_something", false);

	Pass::call(design.get(), "opt_clean top/ff");

	ASSERT_EQ(alias->attributes.count(ID::init), 1);
	EXPECT_EQ(alias->attributes.at(ID::init), RTLIL::Const(1, 2));
	EXPECT_EQ(q->attributes.count(ID::init), 0);
	EXPECT_EQ(module->connections(), connections);
	EXPECT_EQ(ff->connections(), ports);
	EXPECT_EQ(module->wires_size(), 4);
	EXPECT_FALSE(design->scratchpad_get_bool("opt.did_something"));
}

TEST_F(OptCleanInitTest, ModuleMonitorKeepsOriginalInitCleanup)
{
	expect_monitor_fallback(false, false, RTLIL::Const(RTLIL::State::Sx, 2));
}

TEST_F(OptCleanInitTest, DesignMonitorKeepsOriginalInitCleanup)
{
	expect_monitor_fallback(true, false, RTLIL::Const());
}

TEST_F(OptCleanInitTest, UnregisteringModuleMonitorStillKeepsOriginalInitCleanup)
{
	expect_monitor_fallback(false, true, RTLIL::Const());
}

TEST_F(OptCleanInitTest, UnregisteringDesignMonitorStillKeepsOriginalInitCleanup)
{
	expect_monitor_fallback(true, true, RTLIL::Const(RTLIL::State::Sx, 2));
}

} // namespace

YOSYS_NAMESPACE_END
