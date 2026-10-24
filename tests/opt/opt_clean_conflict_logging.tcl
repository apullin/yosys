# Conflict filtering uses wire-only aliases rather than the constant-collapsing
# map used for cell reachability. Keep warning order and duplicate occurrences,
# including warnings from drivers that have been removed before they are printed.
set fixture [file join [file dirname [info script]] opt_clean_conflict_logging.il]
set expected [list \
    {Driver-driver conflict for \opaque_wire between cell opaque_driver.Y and constant 1'0 in top: Resolved using constant.} \
    {Driver-driver conflict for \live_wire between cell live_input_driver.Y and constant 1'0 in top: Resolved using constant.} \
    {Driver-driver conflict for \kept_wire between cell kept_driver.Y and constant 1'0 in top: Resolved using constant.} \
    {Driver-driver conflict for \used_wire between cell duplicate_driver.Y and constant 1'0 in top: Resolved using constant.} \
    {Driver-driver conflict for \used_wire between cell duplicate_driver.Y and constant 1'0 in top: Resolved using constant.}]

# Run this script in separate processes with YOSYS_MAX_THREADS=1 and 4. The
# thread cap is cached on first use, so changing it mid-process does not test
# another thread count. This override exercises multiple shards on a small input.
yosys setenv YOSYS_WORK_UNITS_PER_THREAD 1
foreach repetition {1 2} {
    yosys design -reset
    yosys read_rtlil $fixture
    yosys logger -expect-no-warnings
    yosys logger -expect warning {Driver-driver conflict for .* between cell duplicate_driver\.Y and constant} 2
    yosys logger -expect warning {Driver-driver conflict for .* between cell kept_driver\.Y and constant} 1
    yosys logger -expect warning {Driver-driver conflict for .* between cell live_input_driver\.Y and constant} 1
    yosys logger -expect warning {Driver-driver conflict for .* between cell opaque_driver\.Y and constant} 1

    set logfile opt_clean_conflict_logging_$repetition.log
    yosys tee -o $logfile opt_clean -purge
    yosys logger -check-expected
    yosys select -assert-none top/c:duplicate_driver top/c:kept_driver top/c:live_input_driver top/c:unused_driver top/c:opaque_driver
    yosys select -assert-count 1 top/c:live_consumer
    yosys select -assert-count 1 top/c:opaque_keeper
    yosys select -assert-count 1 no_conflicts/c:driver

    set fd [open $logfile r]
    set lines [split [read $fd] \n]
    close $fd
    set actual {}
    foreach line $lines {
        if {[regexp {^(?:Warning: )?(Driver-driver conflict .*)$} $line -> warning]} {
            lappend actual $warning
        }
    }
    if {$actual ne $expected} {
        error "Incorrect conflict warning text, order or filtering: expected $expected, got $actual"
    }
    file delete $logfile
}
