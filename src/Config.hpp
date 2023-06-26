#ifndef CONFIG_BWT
#define CONFIG_BWT

#include <tuple>
#include <sdsl/vectors.hpp>
#include <sdsl/bit_vectors.hpp>
#include <sdsl/wavelet_trees.hpp>
#include <sdsl/wt_algorithm.hpp>
#include <sdsl/init_array.hpp>

using namespace sdsl;
using namespace std;


uint64_t BOUND = 0; // bound for the number of results, 0 for no bound
uint64_t TIME_OUT = 0;     // timeout for queries in seconds, 0 for no timeout
bool OUTPUT_PAIRS = true; // if true, -o also saves the pairs obtained

typedef std::tuple<uint64_t, uint64_t, uint64_t> spo_triple;
typedef uint16_t word_t;

typedef rank_support_v<> bv_rank_type;
typedef select_support_mcl<> bv_select_type;
typedef select_support_mcl<0> bv_select0_type;

// typedef sdsl::wm_int<bit_vector> wm_type;
typedef wt_gmr<int_vector<>,
                     inv_multi_perm_support<32, int_vector<>>,
                     bit_vector,
                     bv_select_type,
                     bv_select0_type>
    wm_type;

#endif
