#include "Counter.hpp"

Counter::Counter(uint64_t _width) {
    width = _width;
    max = pow(2, width) - 1;
    val = pow(2, width) / 2;
    weakly_taken = pow(2, width) / 2;
}

Counter::Counter(uint64_t _width, int _val) {
    width = _width;
    max = pow(2, width) - 1;
    val = _val;
    weakly_taken = pow(2, width) / 2;
}

void Counter::update(bool taken) {
    if (taken) {
        if (val < max)
            val ++;
    }
    else {
        if (val > 0)
            val --;
    }
}

uint64_t Counter::get() {
    return val;
}

bool Counter::isTaken() {
    if (val >= weakly_taken)
        return true;
    else
        return false;
}

void Counter::setCount(uint64_t count) {
    val = count;
}

bool Counter::isWeak() {
    if (val == weakly_taken || val == (weakly_taken - 1))
        return true;
    else
        return false;
}

void Counter::reset(bool taken) {
    if (taken)
        val = weakly_taken;
    else
        val = weakly_taken - 1;
}
