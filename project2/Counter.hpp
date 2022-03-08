#ifndef COUNTER_H
#define COUNTER_H

#include <cstdint>
#include <math.h>

/* class Counter definition */
class Counter {
  
    private:
      uint64_t width, val, max, weakly_taken;

    public:      
      /* Initializes a counter of width-bits to weakly taken */
      Counter(uint64_t _width);

      /* Initializes a counter of width-bits to specified value */
      Counter(uint64_t _width, int _val);
      
      /* Transitions in the Saturating Counter state diagram according to this branch being taken or not taken */
      void update(bool taken);

      /* Returns current counter value */
      uint64_t get();

      /* Returns true if the current counter is weakly taken or greater */
      bool isTaken();

      /* Sets counter value */
      void setCount(uint64_t count);

      /* Returns true if the current counter is in the weakly taken or weakly not-taken states */
      bool isWeak();

      /* Sets counter to either weakly taken (if taken==true) or weakly not taken (if taken==false) -- used in TAGE-S */
      void reset(bool taken);
};

#endif
