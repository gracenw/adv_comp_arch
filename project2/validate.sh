#!/bin/bash
set -e

student_stat_dir=student_outs
benchmarks=( gcc mcf perlbench x264 )
configs=( gshare_1k_hash0 gshare_8k_hash1 gshare_64k_hash2 gshare_64k_hash2_deepN tage_1k_p3 tage_8k_p7 tage_64k_p10 )

flags_gshare_1k_hash0='-O 1 -S 13 -P 0'
flags_gshare_8k_hash1='-O 1 -S 16 -P 1'
flags_gshare_64k_hash2='-O 1 -S 19 -P 2'
flags_gshare_64k_hash2_deepN='-O 1 -S 19 -P 2 -N 50'
flags_tage_1k_p3='-O 2 -S 13 -P 3'
flags_tage_8k_p7='-O 2 -S 16 -P 4'
flags_tage_64k_p10='-O 2 -S 19 -P 10'

banner() {
    local message=$1
    printf '%s\n' "$message"
    yes = | head -n ${#message} | tr -d '\n'
    printf '\n'
}

student_stat_path() {
    local config=$1
    local benchmark=$2

    printf '%s' "${student_stat_dir}/${config}_${benchmark}.10m.out"
}

ta_stat_path() {
    local config=$1
    local benchmark=$2

    printf '%s' "ref_outs/${config}_${benchmark}.10m.out"
}

benchmark_path() {
    benchmark=$1
    printf '%s' "traces/$benchmark.10m.br.trace"
}

human_friendly_config() {
    local config=$1
    if [[ $config == gshare* ]]; then
        local size=${config#gshare_}
        local size=${size%k_*}
        local hash=${config#*_hash}
        local hash=${hash%_deepN}

        printf 'GShare %d KiB with hash #%d' "$size" "$hash"

        if [[ $config == *deepN ]]; then
            local flags_var=flags_$config
            local stages=${!flags_var#*-N }
            printf ' with a %d-stage pipeline' "$stages"
        fi
    else
        local size=${config#tage_}
        local size=${size%k_*}
        local p=${config#*_p}
        # P=N means there are N+1 tables
        local num_tables=$(( p + 1 ))

        printf 'TAGE %d KiB with %d tables' "$size" "$num_tables"
    fi
}

generate_stats() {
    local config=$1
    local benchmark=$2

    local flags_var=flags_$config
    bash run.sh ${!flags_var} -I "$(benchmark_path "$benchmark")" >"$(student_stat_path "$config" "$benchmark")"
}

generate_stats_and_diff() {
    local config=$1
    local benchmark=$2

    printf '==> Running %s... ' "$benchmark"
    generate_stats "$config" "$benchmark"
    if diff -u "$(ta_stat_path "$config" "$benchmark")" "$(student_stat_path "$config" "$benchmark")"; then
        printf 'Matched!\n'
    else
        local flags_var=flags_$config
        printf '\nPlease examine the differences printed above. Benchmark: %s. Config name: %s. Flags to branchsim used: %s\n\n' "$benchmark" "$config" "${!flags_var} -I $(benchmark_path "$benchmark")"
    fi
}

main() {
    mkdir -p "$student_stat_dir"
    # This is a little hacky, but it makes validation ~3x as fast:
    # around ~2min instead of ~6 min
    make clean
    make FAST=1

    if [[ $# -gt 0 ]]; then
        local use_configs=( "$@" )

        for config in "${use_configs[@]}"; do
            local flags_var=flags_$config
            if [[ -z ${!flags_var} ]]; then
                printf 'Unknown configuration %s. Available configurations:\n' "$config"
                printf '%s\n' "${configs[@]}"
                return 1
            fi
        done
    else
        local use_configs=( "${configs[@]}" )
    fi

    local first=1
    for config in "${use_configs[@]}"; do
        if [[ $first -eq 0 ]]; then
            printf '\n'
        else
            local first=0
        fi

        banner "Testing $(human_friendly_config "$config")..."
        for benchmark in "${benchmarks[@]}"; do
            generate_stats_and_diff "$config" "$benchmark"
        done
    done
}

main "$@"
