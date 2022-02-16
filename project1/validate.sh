#!/bin/bash
set -e

student_stat_dir=student_outs
spotlight_benchmark=gcc
default_benchmarks=( gcc leela linpack matmul_naive matmul_tiled mcf )
config_flags_l1='-v 0 -D'
config_flags_l1_vc='-D'
config_flags_l1_l2='-v 0'
config_flags_l1_vc_l2=

banner() {
    local message=$1
    printf '%s\n' "$message"
    yes = | head -n ${#message} | tr -d '\n'
    printf '\n'
}

student_stat_path() {
    local config=$1
    local benchmark=$2

    printf '%s' "${student_stat_dir}/${config}_${benchmark}.out"
}

ta_stat_path() {
    local config=$1
    local benchmark=$2

    printf '%s' "ref_outs/${config}_${benchmark}.out"
}

human_friendly_flags() {
    local config=$1

    local config_flags_var=config_flags_$config
    local flags="${!config_flags_var}"
    if [[ -n $flags ]]; then
        printf '%s' "$flags"
    else
        printf '(none)'
    fi
}

generate_stats() {
    local config=$1
    local benchmark=$2

    local config_flags_var=config_flags_$config
    ./run.sh ${!config_flags_var} <"traces/$benchmark.trace" >"$(student_stat_path "$config" "$benchmark")"
}

generate_stats_and_diff() {
    local config=$1
    local benchmark=$2

    printf '==> Running %s...\n' "$benchmark"
    generate_stats "$config" "$benchmark"
    if diff -u "$(ta_stat_path "$config" "$benchmark")" "$(student_stat_path "$config" "$benchmark")"; then
        printf 'Matched!\n\n'
    else
        printf '\nPlease examine the differences printed above. Benchmark: %s. Config name: %s. Flags to cachesim used: %s\n\n' "$benchmark" "$config" "$(human_friendly_flags "$config")"
    fi
}

main() {
    mkdir -p "$student_stat_dir"

    banner "Testing only L1 cache..."
    generate_stats_and_diff l1 gcc

    banner "Testing only L1 and Victim Cache..."
    generate_stats_and_diff l1_vc gcc

    banner "Testing only L1 and L2..."
    generate_stats_and_diff l1_l2 gcc

    banner "Testing default configuration (L1, Victim Cache, and L2)..."
    for benchmark in "${default_benchmarks[@]}"; do
        generate_stats_and_diff l1_vc_l2 "$benchmark"
    done
}

main
