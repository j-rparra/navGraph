#ifndef NAVBV
#define NAVBV

#include "Config.hpp"

using namespace std;

class nav_bv
{
    bit_vector *B;
    uint64_t B_size;
    uint64_t ones, zeroes;

    bv_rank_type bv_rank;
    bv_select_type bv_select;
    bv_select0_type bv_select0;

public:
    nav_bv() { ; }

    nav_bv(int_vector<> &M, uint64_t size_M)
    {
        uint64_t i;
        B_size = M[size_M - 1] + size_M - 1;
        bit_vector B_aux = bit_vector(B_size, 0);

        for (i = 0; i < size_M - 1; i++)
        {
            B_aux[M[i] + i] = 1;
        }
        
        B = new bit_vector(B_aux);
        util::init_support(bv_rank, B);
        util::init_support(bv_select, B);
        util::init_support(bv_select0, B);

        ones = my_rank(B_size);
        zeroes = B_size - my_rank(B_size);
    }

    ~nav_bv() { ; }

    uint64_t size()
    {
        return sdsl::size_in_bytes(*B) + sdsl::size_in_bytes(bv_rank) + sdsl::size_in_bytes(bv_select) + sdsl::size_in_bytes(bv_select0);
    }

    uint64_t access(uint64_t i)
    {
        return (*B)[i - 1];
    }

    uint64_t my_rank(uint64_t i)
    {
        return bv_rank(i);
    }

    uint64_t select(uint64_t i)
    {
        if (i > ones)
            return B_size + 1;
        return bv_select(i) + 1;
    }
    uint64_t select0(uint64_t i)
    {
        if (i > zeroes)
            return B_size + 1;
        return bv_select0(i) + 1;
    }

    uint64_t pred(uint64_t i)
    {
        return select(my_rank(i));
    }

    uint64_t succ(uint64_t i)
    {
        return select(my_rank(i - 1) + 1);
    }

    void print()
    {
        for (size_t i = 0; i < B_size; i++)
            cout << (*B)[i] << " - ";
        cout << "\nsize: " << B_size << endl;
    }

    void save(string filename)
    {
        sdsl::store_to_file(*B, filename + ".bv");
    }

    void load(string filename, uint64_t n)
    {
        B = new bit_vector;
        sdsl::load_from_file(*B, filename + ".bv");
        util::init_support(bv_rank, B);
        util::init_support(bv_select, B);
        util::init_support(bv_select0, B);
        B_size = n; 
        ones = my_rank(B_size); 
        zeroes = B_size - my_rank(B_size); 
    }
};
#endif
