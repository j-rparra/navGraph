#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <string>
#include <filesystem>

#include "utils.hpp"
#include "nav_graph.hpp"
#include "Config.hpp"

using namespace std;
using namespace std::chrono;

using timer = chrono::high_resolution_clock;
int main(int argc, char **argv)
{
    bool verbose = true;
 

    std::ifstream ifs(argv[1]);


    if (verbose)
        cout << "\n====>Reading data in " << argv[1] << endl;
    fflush(stdout);

    uint64_t s, p, o;
    vector<spo_triple> E;
    uint64_t max_S = 0, max_P = 0, max_O = 0; 
    do
    {
        ifs >> s >> p >> o;
        if (s > max_S)
            max_S = s;
        if (p > max_P)
            max_P = p;
        if (o > max_O)
            max_O = o; 
        E.emplace_back(spo_triple(s, p, o));
    } while (!ifs.eof());

    if (E[E.size() - 1] == E[E.size() - 2])
        E.pop_back();

    E.shrink_to_fit();
    uint64_t n = E.size();

    cout << "  > number of triples: " << n << endl;
    cout << "  > max subject/object: " << max_S << "/" << max_O << endl;
    cout << "  > number of predicates: " << max_P << endl;

    cout << "\n====>Indexing the " << n << " triples" << endl;
    fflush(stdout);

    memory_monitor::start();
    auto start = timer::now();
    nav_graph NG(E, max_S, max_O, max_P);
    auto stop = timer::now();
    memory_monitor::stop();

    cout << "\n====>Index built" << endl;
    fflush(stdout);
    cout << "  > " << (float)NG.size() / n << " bytes per triple" << endl;
    cout << "  > TIME USED: " << duration_cast<seconds>(stop - start).count() << " seconds." << endl;
    cout << "  > MEMORY PEAK: " << memory_monitor::peak() << " bytes." << endl;

    cout << "\n====>Saving the index" << endl;
    NG.save(string(argv[1]));
    cout << "  > index saved" << endl;
}
