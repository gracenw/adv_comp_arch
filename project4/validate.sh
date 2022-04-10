#!/bin/bash
set -e

exe=./dirsim
student_stat_dir=student_outs
traces=( 4proc 8proc 16proc )
protocols=( MI MSI MESI MOSIF )

banner() {
    local message=$1
    printf '%s\n' "$message"
    yes = | head -n ${#message} | tr -d '\n'
    printf '\n'
}

student_stat_path() {
    local protocol=$1
    local trace=$2

    printf '%s' "${student_stat_dir}/${protocol}_${trace}.out"
}

ta_stat_path() {
    local protocol=$1
    local trace=$2

    printf '%s' "ref_outs/${protocol}_${trace}.out"
}

trace_path() {
    trace=$1
    printf '%s' "traces/${trace}"
}

generate_stats() {
    local protocol=$1
    local trace=$2

    if ! "$exe" -p "$protocol" -t "$(trace_path "$trace")" &>"$(student_stat_path "$protocol" "$trace")"; then
        printf 'Exited with nonzero status code. Please check the log file %s for details\n' "$(student_stat_path "$protocol" "$trace")"
    fi
}

generate_stats_and_diff() {
    local protocol=$1
    local trace=$2

    printf '==> Running %s... ' "$trace"
    generate_stats "$protocol" "$trace"
    if diff -u "$(ta_stat_path "$protocol" "$trace")" "$(student_stat_path "$protocol" "$trace")"; then
        printf 'Matched!\n'
    else
        printf '\nPlease examine the differences printed above. Trace: %s. Protocol: %s\n\n' "$trace" "$protocol"
    fi
}

main() {
    mkdir -p "$student_stat_dir"

    if [[ $# -gt 0 ]]; then
        local use_protocols=( "$@" )

        for protocol in "${use_protocols[@]}"; do
            local found=0
            # My grandpa says he could stop drinking whenever he wants, and I say
            # the same thing about O(n^2) algorithms
            for known_protocol in "${protocols[@]}"; do
                if [[ $known_protocol = $protocol ]]; then
                    found=1
                    break
                fi
            done

            if [[ $found -eq 0 ]]; then
                printf 'Unknown protocol %s. Available protocols:\n' "$protocol"
                printf '%s\n' "${protocols[@]}"
                return 1
            fi
        done
    else
        local use_protocols=( "${protocols[@]}" )
    fi

    local first=1
    for protocol in "${use_protocols[@]}"; do
        if [[ $first -eq 0 ]]; then
            printf '\n'
        else
            local first=0
        fi

        banner "Testing $protocol..."

        for trace in "${traces[@]}"; do
            generate_stats_and_diff "$protocol" "$trace"
        done
    done
}

main "$@"
