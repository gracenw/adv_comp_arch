#include "Counter.hpp"

Counter::Counter(uint64_t width) {
    this->width = width;
    this->max = pow(2, width) - 1;
    this->val = pow(2, width) / 2;
    this->weakly_taken = pow(2, width) / 2;
}

void Counter::update(bool taken) {
    if (taken) {
        if (this->val < this->max)
            this->val ++;
    }
    else {
        if (this->val > 0)
            this->val --;
    }
}

uint64_t Counter::get() {
    return this->val;
}

bool Counter::isTaken() {
    if (this->val >= this->weakly_taken)
        return true;
    else
        return false;
}

void Counter::setCount(uint64_t count) {
    this->val = count;
}

bool Counter::isWeak() {
    if (this->val > 0 || this->val < this->max)
        return true;
    else
        return false;
}

void Counter::reset(bool taken) {
    if (taken)
        this->val = this->weakly_taken;
    else
        this->val = this->weakly_taken - 1;
}
