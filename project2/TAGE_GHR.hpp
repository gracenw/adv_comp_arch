#ifndef GHR_H
#define GHR_H

#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * This class helps track global history and calculate compressed history for
 * TAGE. We have implemented it fully for you in TAGE_GHR.cpp! Internally, it
 * maintains a large GHR of arbitrary length which you can update for each
 * branch with shiftLeft(). You can query up to the last 64 bits of the full
 * history with getHistory(). Or you can call getCompressedHistory() to
 * generate compressed history of arbitrary widths as if that compressed
 * history was derived from a shorter history than the longer history actually
 * stored in this class.
 */
class TAGE_GHR {

private:

    std::vector<uint64_t> historyChunks;
    uint64_t length;

static size_t calcNumChunks(uint64_t);

public:

TAGE_GHR();

/**
 * Constructor. The `length' here specifies the length of the GHR this class
 * stores. Tip: `length' should be at least the length of the longest history
 * needed across all TAGE tables.
 */
TAGE_GHR(uint64_t length);

/**
 * Shifts history left and ORs in the new taken bit. For example, if length==4
 * and GHR==0b1111, calling shiftLeft(0) will change the GHR to 0b1110.
 *
 * You need to call this in your code! Note that updating the GHR is the _last_
 * step in updating TAGE; see the end of the "Updating TAGE" section of the PDF.
 */
void shiftLeft(bool taken);

/**
 * Returns the integer form of GHR[length-1:0] where length (specified earlier
 * to the constructor) is strictly less than or equal to 64.
 *
 * Where is this useful? Partial tag calculation (see the PDF).
 */
uint64_t getHistory();

/**
 * Compress the last `length' bits of the GHR by breaking it into chunks of
 * `width' bits and XORing these chunks together to get a final result of width
 * bits. We handle the nasty cases such as width not being a multiple of length
 * by padding with zeros. However, it is required that width <= 64 and
 * length <= this->length (passed to constructor).
 *
 * Where is this useful? Indexing into TAGE tables (see the PDF).
 */
uint64_t getCompressedHistory(uint64_t length, uint64_t width);

};




#endif
