#include "Counter.hpp"

Counter::Counter(uint64_t _width) {
    /* store width of counter in bits, maximum value, current value (weakly taken), and value when weakly taken */
    width = _width;
    max = pow(2, width) - 1;
    val = pow(2, width) / 2;
    weakly_taken = pow(2, width) / 2;
}

Counter::Counter(uint64_t _width, int _val) {
    /* store width of counter in bits, maximum value, current value, and value when weakly taken */
    width = _width;
    max = pow(2, width) - 1;
    val = _val;
    weakly_taken = pow(2, width) / 2;
}

void Counter::update(bool taken) {
    /* if taken, increment when less than max */
    if (taken) {
        if (val < max)
            val ++;
    }
    /* if not taken, decrement when value is greater than 0 */
    else {
        if (val > 0)
            val --;
    }
}

uint64_t Counter::get() {
    /* return current value of counter */
    return val;
}

bool Counter::isTaken() {
    /* if greater than or equal to weakly taken value, return taken */
    if (val >= weakly_taken)
        return true;
    /* not taken otherwise */
    else
        return false;
}

void Counter::setCount(uint64_t count) {
    /* set value of counter to new val */
    val = count;
}

bool Counter::isWeak() {
    /* if counter is current equal to weakly taken or not taken values, return true */
    if (val == weakly_taken || val == (weakly_taken - 1))
        return true;
    /* false otherwise */
    else
        return false;
}

void Counter::reset(bool taken) {
    /* set counter to weakly taken if branch was taken */
    if (taken)
        val = weakly_taken;
    /* set counter to weakly not taken if branch was not taken */
    else
        val = weakly_taken - 1;
}
