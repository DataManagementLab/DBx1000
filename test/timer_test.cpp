#include <thread>
#include <chrono>
#include <iostream>
#include <sys/platform/ppc.h>

template<typename Clock_t>
void test_clock(){

    auto start = Clock_t::now();
    uint64_t reps = 100'000;
#pragma omp parallel num_threads(10)
    {    
        typename Clock_t::duration d;
        for(uint64_t i = 0; i < reps; ++i) {
            d += (Clock_t::now() - start);
        }
        std::cout << "Inner duration: " << std::chrono::nanoseconds(d).count() << "\n";
    }

    auto end = Clock_t::now();
    auto total = std::chrono::nanoseconds(end - start).count();
    auto each = total / reps;
    std::cout << "Total duration: " << total << " (each= " << each << ")" << std::endl;
}

uint64_t my_clock_get_time() {
        timespec tp ;
        clock_gettime(CLOCK_REALTIME, &tp);
        return tp.tv_sec * 1000000000 + tp.tv_nsec;
}

void test_clock_sys() {
        auto start = my_clock_get_time();
        uint64_t d = 0;
        uint64_t reps = 100'000;

#pragma omp parallel num_threads(10)
        {
            for(uint64_t i = 0; i < reps; ++i) {
                    d += (my_clock_get_time() - start);
            }
            std::cout << "Inner duration: " << d << "\n";
        }

        auto end = my_clock_get_time();
        auto total = (end - start);
        auto each = total / reps;
        std::cout << "Total duration: " << total << " (each= " << each << ")" << std::endl;
}

void test_timebase() {
        auto start = my_clock_get_time();
        uint64_t d = 0;
        uint64_t reps = 100'000;

#pragma omp parallel num_threads(10)
        {
            for(uint64_t i = 0; i < reps; ++i) {
                    d += __builtin_ppc_mftb();
		    //d += (my_clock_get_time() - start);
            }
            std::cout << "Inner duration: " << d << "\n";
        }

        auto end = my_clock_get_time();
        auto total = (end - start);
        auto each = total / reps;
        std::cout << "Total duration: " << total << " (each= " << each << ")" << std::endl;
}

int main(int, char**)
{
	std::cout << "Timebase frequency: " << __ppc_get_timebase_freq() << "\n";
	auto start1 = __builtin_ppc_mftb();
	auto start3 = __builtin_ppc_get_timebase();
	auto start2 = std::chrono::high_resolution_clock::now();
	auto start4 = __ppc_get_timebase();

	std::this_thread::sleep_for(std::chrono::seconds(5));
	
	auto end1 = __builtin_ppc_mftb();
	auto end3 = __builtin_ppc_get_timebase();
	auto end2 = std::chrono::high_resolution_clock::now();
	auto end4 = __ppc_get_timebase();

	static const uint64_t mask = ~(uint64_t(0xF) << 60);
	start1 = start1 & mask;
	end1 = end1 & mask;
	auto duration1 = end1 - start1;
	auto duration2 = end2 - start2;
	auto duration3 = end3 - start3;
	auto duration4 = (end4 - start4) *1'000'000'000 / __ppc_get_timebase_freq();

	std::cout << "Duration1: " << duration1 << "\n"
		<< "Duration2: " << std::chrono::nanoseconds(duration2).count() << "\n"
		<< "Duration3: " << duration3 << "\n"
		<< "Duration4: " << duration4 << "\n"
		<< std::endl;
        /*{
                std::cout << "Steady Clock" << std::endl;
                test_clock<std::chrono::steady_clock>();
        }
        {
                std::cout << "System Clock" << std::endl;
                test_clock<std::chrono::system_clock>();
        }
        {
                std::cout << "High_resolution_clock" << std::endl;
                test_clock<std::chrono::high_resolution_clock>();
        }
        {
                std::cout << "clock_gettime" << std::endl;
                test_clock_sys();
        }

        {
                std::cout << "Timebase register" << std::endl;
		test_timebase();
        }*/
}
