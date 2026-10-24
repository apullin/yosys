# Wires without init must not require a fabricated width-sized all-X constant,
# even when their aliases map to X. Real init attributes retain normalization.
set fixture [file join [file dirname [info script]] opt_clean_absent_init.il]

# Run in separate processes with YOSYS_MAX_THREADS=1 and 4. The thread cap is
# cached on first use; this override exercises multiple shards on a small design.
yosys setenv YOSYS_WORK_UNITS_PER_THREAD 1
set reference {}
foreach repetition {1 2} {
    yosys design -reset
    yosys read_rtlil $fixture
    yosys logger -expect-no-warnings
    yosys opt_clean -purge

    yosys select -assert-count 1 top/w:x_out
    yosys select -assert-count 1 top/w:kept_x
    yosys select -assert-count 1 top/w:all_x_port
    yosys select -assert-count 1 top/w:zero_port
    yosys select -assert-none top/w:x_alias top/w:unused_x top/w:unused_plain top/w:all_x_unused top/w:zero_unused top/w:zero_kept
    yosys select -assert-none top/w:x_out top/w:kept_x top/w:all_x_port top/w:zero_port %u %u %u top/a:init %i
    yosys select -assert-count 1 top/w:defined_port {top/a:init=4'b1010} %i
    yosys select -assert-count 1 top/w:short_port {top/a:init=4'bxx10} %i
    yosys select -assert-count 1 top/w:long_port {top/a:init=2'b11} %i
    yosys select -assert-count 4 {top/t:$not}

    set output opt_clean_absent_init_$repetition.out.il
    yosys write_rtlil -sort $output
    set fd [open $output r]
    set actual [read $fd]
    close $fd
    if {$repetition == 1} {
        set reference $actual
    } elseif {$actual ne $reference} {
        error "Repeated opt_clean changed normalized init handling or ordered output"
    }
    file delete $output
}
