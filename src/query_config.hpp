#ifndef CONFIG_Q
#define CONFIG_Q

#include <chrono>  
using namespace std::chrono;

high_resolution_clock::time_point query_start;
duration<double> time_span;
uint16_t targets_calls;
uint16_t sources_calls;
high_resolution_clock::time_point stop; // start, stop;




#endif
