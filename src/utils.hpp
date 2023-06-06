#ifndef UTILS
#define UTILS

#include <iostream>
#include <fstream>
#include <tuple>
#include <vector>
#include <chrono>
#include <string>
#include "Config.hpp"
#include "utils.hpp" 
#include <algorithm>

//  s > o > p
// auto order_L = [](auto &v1, auto &v2)
// {
//     return std::tie(std::get<0>(v1), std::get<2>(v1), std::get<1>(v1)) <
//            std::tie(std::get<0>(v2), std::get<2>(v2), std::get<1>(v2));
// };

bool sortby_sop(const spo_triple &a, const spo_triple &b)  
{
     if(get<0>(a) == get<0>(b)){
        if(get<2>(a) == get<2>(b)){
            return get<1>(a) < get<1>(b);
        }
        return get<2>(a) < get<2>(b);
    }
    return get<0>(a) < get<0>(b);
}

char* getCmdOption(char ** begin, char ** end, const std::string & option)
{
    char ** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        return *itr;
    }
    return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}


// Overload print for tuples
// template <class TupType, size_t... I>
// void print(const TupType &_tup, std::index_sequence<I...>)
// {
//     std::cout << "(";
//     (..., (std::cout << (I == 0 ? "" : ", ") << std::get<I>(_tup)));
//     std::cout << ")\n";
// }

// template <class... T>
// void print(const std::tuple<T...> &_tup)
// {
//     print(_tup, std::make_index_sequence<sizeof...(T)>());
// }

// template <typename T>
// void print(T t)
// {
//     std::cout << t << " ";
// }

// // Overload print for vectors
// template <class... T>
// void print(const std::vector<T...> &_vec)
// {
//     for (auto &a : _vec)
//         print(a);
// }

// // Overload print for arrays
// template <typename T>
// void print(const T *const array, int count)
// {
//     for (int i = 0; i < count; ++i)
//     {
//         std::cout << array[i] << " ";
//     }
//     std::cout << std::endl;
// }

#endif
