#!/bin/bash

for f in {4..5}; do
    for w in {4..7}; do
        for c in {13..15}; do
            for l in {2..3}; do
                for m in {1..2}; do
                    for s in {2..4}; do
                        for ((r = 32 ; r <= 96 ; r+=16)); do
                            ./procsim -I traces/gcc602_2M.trace -A 3 -R $r -S $s -C $c -M $m -L $l -F $f -W $w >> search_gcc.out
                        done
                    done
                done
            done
        done
    done
done

echo All done