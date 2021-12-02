#include "global.h"
#include <iostream>
#include <omp.h>

#undef LATCH_T
//#define LATCH_T Latch_tbb<tbb::spin_mutex>
#define LATCH_T tbb::spin_mutex
//#define LATCH_T L_PTHREAD

int main(int, char**)
{
    LATCH_T* latch1 = new LATCH_T;
    LATCH_T* latch2 = new LATCH_T;

    latch1->lock();
    latch2->lock();
    latch2->unlock();
    latch1->unlock();
    
    latch1->lock();
    latch2->lock();
    latch2->unlock();
    latch1->unlock();

#pragma omp parallel num_threads(2)
    {

        if(omp_get_thread_num() == 0)
            latch1->lock();

#pragma omp barrier

        switch(omp_get_thread_num())
        {
            case 0:
            {
                latch2->lock();
                latch1->unlock();
                std::cout << "Unlocked l1" << std::endl;
                latch2->unlock();
            }
            break;
            case 1:
            {
                latch1->lock();
                latch2->lock();
                latch1->unlock();
                latch2->unlock();
            }
            break;
            default: ;
        }
    }
}