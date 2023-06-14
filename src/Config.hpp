
#ifndef CONFIG_BWT
#define CONFIG_BWT

#include <set>
#include <map>
#include <tuple>
#include <vector>

#include <sdsl/suffix_arrays.hpp>
#include <sdsl/vectors.hpp>
#include <sdsl/bit_vectors.hpp>
#include <sdsl/wavelet_trees.hpp>
#include <sdsl/wt_algorithm.hpp>
#include <sdsl/init_array.hpp>

using namespace sdsl;
using namespace std;

typedef std::tuple<uint64_t, uint64_t, uint64_t> spo_triple;
typedef uint16_t word_t;

uint64_t bound = 1000000;



typedef sdsl::rank_support_v<> bv_rank_type;
typedef sdsl::select_support_mcl<> bv_select_type;
typedef sdsl::select_support_mcl<0> bv_select0_type;



// typedef sdsl::wm_int<bit_vector> wm_type;
typedef sdsl::wt_gmr<int_vector<>,
                     inv_multi_perm_support<32, int_vector<>>,
                     bit_vector,
                     bv_select_type,
                     bv_select0_type>
    wm_type;

#endif
