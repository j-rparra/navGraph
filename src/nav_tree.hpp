#ifndef NAVT
#define NAVT

#include "Config.hpp"

using namespace std;

class nav_tree
{
    wm_type L;
    uint64_t L_size;

public:
    nav_tree() { ; }

    nav_tree(int_vector<> &L_aux, uint64_t l_size)
    {
        uint64_t i; 
        L_size = l_size;
        construct_im(L, L_aux);
    }

    ~nav_tree() { ; }

    uint64_t access(uint64_t i)
    {
        return L[i - 1];
    }

    uint64_t my_rank(uint64_t pos, uint64_t val)
    {
        return L.rank(pos, val);
    }

    uint64_t select(uint64_t occurrence, uint64_t val) 
    {
        return L.select(occurrence, val) + 1;
    }

    uint64_t size()
    {
        return sdsl::size_in_bytes(L);
    }

    void save(string filename)
    {
        sdsl::store_to_file(L, filename + ".tree");
    }

    void load(string filename, uint64_t n)
    {
        sdsl::load_from_file(L, filename + ".tree");
        L_size = n;
    }

    // std::vector<std::pair<uint64_t, uint64_t>>
    // intersect(const std::vector<std::array<uint64_t, 2ul>> &ranges)
    // {
    //     return sdsl::intersect<wm_type>(L, ranges);
    // };
};
#endif
