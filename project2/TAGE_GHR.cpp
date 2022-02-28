#include "TAGE_GHR.hpp"

#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <map>
#include <iostream>

// =====================================================================
//
// You do not need to modify this file. Please see TAGE_GHR.hpp for some
// explanation on what is going on in here.
//
// =====================================================================

TAGE_GHR::TAGE_GHR() {}

TAGE_GHR::TAGE_GHR(uint64_t len) : historyChunks(std::vector<uint64_t>(calcNumChunks(len), 0)), length(len) {}

size_t TAGE_GHR::calcNumChunks(uint64_t len) {
    // Round len up to a factor of 64
    if (len & ~((uint64_t)-1 << 6)) {
        len = (len & (-1 << 6)) + (1 << 6);
    }
    // Divide by 64
    return len >> 6;
}

void TAGE_GHR::shiftLeft(bool taken) {
    for (size_t i = 0; i < historyChunks.size(); i++) {
        unsigned char next_bit;
        if (i+1 < historyChunks.size()) {
            next_bit = historyChunks[i+1] >> 63;
        } else {
            next_bit = taken;
        }
        historyChunks[i] = (historyChunks[i] << 1) | next_bit;
    }
}

uint64_t TAGE_GHR::getHistory() {
    uint64_t history = historyChunks[historyChunks.size()-1];
    if (length < 64) {
        history = history & ~((uint64_t)-1 << length);
    }

    return history;
}

uint64_t TAGE_GHR::getCompressedHistory(uint64_t length, uint64_t width) {
    uint64_t base = 0;
    size_t rhs = 0;

    while (rhs < length) {
        // Need to extract width bits from array of 64-bit uints
        // Round down to a multiple of 64
        size_t rightChunkIdx = rhs >> 6;
        size_t rightChunkOffset = rhs & ~((uint64_t)-1 << 6);
        uint64_t rightChunk = historyChunks[historyChunks.size()-1 - rightChunkIdx];
        rightChunk = rightChunk >> rightChunkOffset;
        size_t rightBits = std::min(width, 64-rightChunkOffset);
        rightChunk = rightChunk & ~((uint64_t)-1 << rightBits);
        uint64_t piece = rightChunk;

        // Need a left chunk too
        if (width > 64-rightChunkOffset) {
            uint64_t leftChunk;
            if (rightChunkIdx + 1 >= historyChunks.size()) {
                // Edge case: if we are overflowing the history chunks array,
                // just use NT
                leftChunk = 0;
            } else {
                leftChunk = historyChunks[historyChunks.size()-1 - (rightChunkIdx+1)];
                size_t leftBits = width - rightBits;
                leftChunk = leftChunk & ~((uint64_t)-1 << leftBits);
            }
            piece = piece | (leftChunk << rightBits);
        }

        if (rhs + width > length) {
            // Need to be very careful here: we shouldn't use bits that go past
            // the requested history length. We can't let the students see the truth!!!
            piece = piece & ~((uint64_t)-1 << (length - rhs));
        }

        base = base ^ piece;

        rhs += width;
    }

    return base;
}
