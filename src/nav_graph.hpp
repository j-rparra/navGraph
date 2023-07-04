#ifndef NGRAPH
#define NGRAPH

#include <vector>
#include <cstdint>
#include <chrono>
#include <set>
#include <unordered_set>
#include <stdio.h>
#include <stdlib.h>
#include <sdsl/init_array.hpp>
#include <algorithm>

#include "Config.hpp"
#include "query_config.hpp"
#include "utils.hpp"
#include "nav_tree.hpp"
#include "nav_bv.hpp"
#include "RpqAutomata.hpp"
#include "RpqTree.hpp"
#include "parse_query.cpp"

using namespace std;

typedef struct
{
    vector<uint64_t> ojects;
    word_t current_D;
} induction_data;

class nav_graph
{
    nav_tree L, N;
    nav_bv B, B_L;

public:
    uint64_t max_S, max_P, max_O, max_SO;
    uint64_t n;

    nav_graph() { ; }

    nav_graph(vector<spo_triple> &E, uint64_t max_s, uint64_t max_o, uint64_t max_p)
    {
        max_S = max_s, max_P = max_p, max_O = max_o;
        uint64_t i, tmp;
        std::fstream fp;
        vector<spo_triple>::iterator it, triple_begin = E.begin(), triple_end = E.end();
        n = triple_end - triple_begin;
        if (verbose)
            cout << "  > triples set = " << E.size() * sizeof(spo_triple) << " bytes" << endl;
        fflush(stdout);

        // sort triples by s > o > p
        cout << "  > sorting the triples" << endl;
        fflush(stdout);
        std::sort(E.begin(), E.end(), sortby_sop);

        // compute P_count and S_count (the starting points of the N_i and L_i)
        if (verbose)
            cout << "  > computing the starting points in N and L" << endl;
        fflush(stdout);
        int_vector<> P_count(max_P + 1); /// points N_i, used to create B_L
        int_vector<> S_count(max_S + 1); // points L_i, used to create B
        for (i = 0; i <= max_S; i++)
            S_count[i] = 0;
        for (i = 0; i <= max_P; i++)
            P_count[i] = 0;

        for (i = 0; i < n; i++)
        {
            P_count[std::get<1>(E[i])]++;
            S_count[std::get<0>(E[i])]++;
        }

        for (i = 1; i <= max_P; i++)
            P_count[i] = P_count[i] + P_count[i - 1];
        for (i = 1; i <= max_S; i++)
            S_count[i] = S_count[i] + S_count[i - 1];

        cout << "  > creating bitvector B" << endl;
        fflush(stdout);
        util::bit_compress(S_count);
        B = nav_bv(S_count, max_S + 1);

        cout << "  > creating bitvector B_L" << endl;
        fflush(stdout);
        util::bit_compress(P_count);
        B_L = nav_bv(P_count, max_P + 1);

        cout << "  > moving vector with triples to files L.tmp and N.tmp" << endl;
        fflush(stdout);
        uint64_t *L_data = new uint64_t[n + 1];
        uint64_t *N_data = new uint64_t[n + 1];
        if (!L_data or !N_data)
            cout << "ERROR: allocation of memory failed\n";

        std::vector<uint64_t> count_aux(max_P + 1);
        for (i = 0; i <= max_P; i++)
            count_aux[i] = 0;

        for (i = 0, it = triple_begin; it != triple_end; it++, i++)
        {
            tmp = std::get<1>(*it); // p
            L_data[i] = tmp;
            N_data[P_count[tmp - 1] + count_aux[tmp - 1]] = std::get<2>(*it);
            count_aux[tmp - 1]++;
        }
        E.clear();
        E.shrink_to_fit();

        cout << "  > writing L.tmp" << endl;
        fflush(stdout);
        fp.open("L.tmp", ios::out | ios::binary);
        for (i = 0; i < n; i++)
            fp.write((char *)&L_data[i], sizeof(uint64_t));
        fp.close();
        delete[] L_data;

        cout << "  > writing N.tmp" << endl;
        fflush(stdout);
        fp.open("N.tmp", ios::out | ios::binary);
        for (i = 0; i < n; i++)
            fp.write((char *)&N_data[i], sizeof(uint64_t));
        fp.close();
        delete[] N_data;

        cout << "  > creating tree N" << endl;
        fflush(stdout);
        int_vector<> wm_aux(n);
        fp.open("N.tmp", ios::in | ios::binary);
        for (i = 0; i < n; i++)
        {
            fp.read((char *)&tmp, sizeof(uint64_t));
            wm_aux[i] = tmp;
        }
        fp.close();
        util::bit_compress(wm_aux);
        std::remove("N.tmp");
        N = nav_tree(wm_aux, n);

        cout << "  > creating tree L" << endl;
        fflush(stdout);
        fp.open("L.tmp", ios::in | ios::binary);
        for (i = 0; i < n; i++)
        {
            fp.read((char *)&tmp, sizeof(uint64_t));
            wm_aux[i] = tmp;
        }
        fp.close();
        util::bit_compress(wm_aux);
        std::remove("L.tmp");
        L = nav_tree(wm_aux, n);
    }
    /* FUNCIONES GRAFO*/
    /*Primer grupo: para un vertice y el grafo sin filtrar*/

    uint64_t outdegree(uint64_t v)
    {
        uint64_t b1 = B.select(v);
        uint64_t b2 = B.select(v + 1);
        return b2 - b1 - 1;
    }

    uint64_t indegree(uint64_t v)
    {
        return N.my_rank(n, v);
    }

    bool adj(uint64_t u, uint64_t v)
    {
        for (uint64_t i = 1; i <= max_P; i++)
        {
            if (adj_l(u, v, i))
                return true;
        }
        return false;
    }

    pair<uint64_t, uint64_t> neigh(uint64_t v, uint64_t j)
    { // assuming j <= outdegree(G, v)
        uint64_t p = B.select(v) - v + j;
        uint64_t l = L.access(p);
        uint64_t q = B_L.select(l) - l;
        return {N.access(q + L.my_rank(p, l)), l};
    }

    pair<uint64_t, uint64_t> rneigh(uint64_t v, uint64_t j)
    { // assuming j ≤ indegree(G, v)
        uint64_t p = N.select(j, v);
        uint64_t t = B_L.select0(p);
        uint64_t l = t - p;
        uint64_t s = L.select(t - B_L.pred(t), l);
        return {B.select0(s) - s, l};
    }

    std::vector<pair<uint64_t, uint64_t>> neigh_all(uint64_t v)
    {
        std::vector<pair<uint64_t, uint64_t>> neighboors;
        uint64_t outd = outdegree(v);
        for (uint64_t i = 1; i <= outd; i++)
            neighboors.emplace_back(neigh(v, i));
        return neighboors;
    }

    std::vector<pair<uint64_t, uint64_t>> rneigh_all(uint64_t v)
    {
        std::vector<pair<uint64_t, uint64_t>> rneighboors;
        int64_t ind = indegree(v);
        for (uint64_t i = 1; i <= ind; i++)
            rneighboors.emplace_back(rneigh(v, i));
        return rneighboors;
    }

    /*Segundo grupo: para un vertice y el subgrafo con labels l */

    bool adj_l(uint64_t u, uint64_t v, uint64_t l)
    {
        int64_t p1 = B.select(v) - v;
        int64_t p2 = B.select(v + 1) - (v + 1);
        int64_t r1 = B_L.select(l) - l;
        int64_t q1 = L.my_rank(p1, l);
        int64_t q2 = L.my_rank(p2, l);

        return (N.my_rank(r1 + q2, u) - N.my_rank(r1 + q1, u)) == 1;
    }

    uint64_t outdegree_l(uint64_t v, uint64_t l)
    {
        uint64_t b = B.select(v);
        uint64_t p1 = b - v;
        uint64_t p2 = B.succ(b + 1) - (v + 1);
        if (p2 <= p1)
            return 0;
        return L.my_rank(p2, l) - L.my_rank(p1, l);
    }

    uint64_t indegree_l(uint64_t v, uint64_t l)
    {
        uint64_t b = B_L.select(l);
        uint64_t r1 = b - l;
        uint64_t r2 = B_L.succ(b + 1) - (l + 1);
        if (r2 <= r1)
            return 0;
        return N.my_rank(r2, v) - N.my_rank(r1, v);
    }

    uint64_t neigh_l(uint64_t v, uint64_t i, uint64_t l)
    {
        uint64_t p = B.select(v) - v;
        uint64_t r = B_L.select(l) - l;
        uint64_t q = L.my_rank(p, l);
        return N.access(r + q + i);
    }

    uint64_t rneigh_l(uint64_t v, uint64_t i, uint64_t l)
    {
        uint64_t r = B_L.select(l) - l;
        uint64_t t = N.select(N.my_rank(r, v) + i, v);
        uint64_t s = L.select(t - r, l);
        return B.select0(s) - s;
    }

    std::vector<uint64_t> neigh_l_all(uint64_t v, uint64_t l)
    {
        std::vector<uint64_t> neighboors;
        uint64_t p = B.select(v) - v;
        uint64_t r = B_L.select(l) - l;
        uint64_t q = L.my_rank(p, l);
        uint64_t outdegree = outdegree_l(v, l);
        for (uint64_t i = 1; i <= outdegree; i++)
            neighboors.emplace_back(N.access(r + q + i));
        return neighboors;
    }

    std::vector<uint64_t> rneigh_l_all(uint64_t v, uint64_t l)
    {
        std::vector<uint64_t> rneighboors;
        uint64_t t, s;
        uint64_t r = B_L.select(l) - l;
        uint64_t ind = indegree_l(v, l);
        for (uint64_t i = 1; i <= ind; i++)
        {
            t = N.select(N.my_rank(r, v) + i, v);
            s = L.select(t - r, l);
            rneighboors.emplace_back(B.select0(s) - s);
        }
        return rneighboors;
    }

    std::vector<uint64_t> old_rneigh_l_all(uint64_t v, uint64_t l)
    {
        std::vector<uint64_t> rneighboors;
        for (uint64_t i = 1; i <= indegree_l(v, l); i++)
            rneighboors.emplace_back(rneigh_l(v, i, l));
        return rneighboors;
    }

    /*Tercer grupo: para el subgrafo con labels l */

    uint64_t count_l(uint64_t l)
    {
        return L.my_rank(n, l);
    }

    pair<uint64_t, uint64_t> access_l(uint64_t i, uint64_t l)
    {
        uint64_t p = L.select(i, l);
        uint64_t u = B.select0(p) - p;
        uint64_t v = N.access(B_L.select(l) - l + L.my_rank(p, l));
        return {u, v};
    }

    std::vector<pair<uint64_t, uint64_t>> access_l_all(uint64_t l)
    {
        std::vector<pair<uint64_t, uint64_t>> edges;
        uint64_t count = count_l(l);
        for (uint64_t i = 1; i <= count; i++)
            edges.emplace_back(access_l(i, l));
        return edges;
    }

    std::vector<uint64_t> targets_l(uint64_t l) // OJO! entrega repetidos
    {
        // TODO: sacar repetidos usando bitvector como se hace en targets_l_bv, es mas rapido?
        std::vector<uint64_t> targets;
        uint64_t p1 = B_L.select(l) - l + 1;
        uint64_t p2 = B_L.select(l + 1) - (l + 1);
        // intersect solo implementabe para arboles traversables:
        //      std::vector<std::array<uint64_t, 2ul>> ranges;
        //      ranges.emplace_back({p1 - 1, p2 - 1});
        //      ranges.emplace_back({p1 - 1, p2 - 1});
        //      std::vector<std::pair<uint64_t, uint64_t>> values_freq = N.intersect(ranges);
        //      for (uint64_t i = 0; i < values_freq.size(); ++i)
        //          targets.emplace_back(values_freq[i].first);
        while (p1 - 1 < p2)
        {
            targets.emplace_back(N.access(p1));
            p1++;
        }
        return targets;
    }

    std::vector<uint64_t> sources_l(uint64_t l)
    {
        std::vector<uint64_t> sources;
        uint64_t c = 0;
        uint64_t p = L.select(1, l);
        uint64_t max_l = L.my_rank(n, l); // condicion para el select
        uint64_t v;
        uint64_t aux;

        while (p < n + 1)
        {
            v = B.select0(p) - p;
            sources.emplace_back(v);
            c = B.succ(v + p) - (v + 1);
            aux = L.my_rank(c, l);
            if (aux == max_l)
                p = n + 1;
            else
                p = L.select(1 + aux, l);
        }
        return sources;
    }

    // modifica el bv targets tal que en la posicion i es 1 iff existe un triple (s,l,i) para algun s
    void targets_l_bv(uint64_t l, bit_vector &targets)
    {
        uint64_t p1 = B_L.select(l) - l + 1;
        uint64_t p2 = B_L.select(l + 1) - (l + 1);
        while (p1 - 1 < p2)
        {
            targets[N.access(p1)] = 1;
            p1++;
        }
        return;
    }

    // modifica el bv sources tal que en la posicion i es 1 iff existe un triple (i,l,s) para algun s
    void sources_l_bv(uint64_t l, bit_vector &sources)
    {
        uint64_t c = 0;
        uint64_t p = L.select(1, l);
        uint64_t max_l = L.my_rank(n, l); // condicion para el select
        uint64_t v;
        uint64_t aux;

        while (p < n + 1)
        {
            v = B.select0(p) - p;
            sources[v] = 1;
            c = B.succ(v + p) - (v + 1);
            aux = L.my_rank(c, l);
            if (aux == max_l)
                p = n + 1;
            else
                p = L.select(1 + aux, l);
        }
        return;
    }

    /*FUNCIONES AUXILIARES*/

    // Size of the index in bytes
    uint64_t size()
    {
        return L.size() + B.size() + B_L.size() + N.size();
    }

    void save(string filename)
    {
        L.save(filename + ".L");
        N.save(filename + ".N");
        B.save(filename + ".B");
        B_L.save(filename + ".BL");

        std::ofstream ofs(filename + ".sizes");
        ofs << n << endl;
        ofs << max_S << endl;
        ofs << max_P << endl;
        ofs << max_O << endl;
    };

    void load(string filename)
    {
        std::ifstream ifs(filename + ".sizes");
        ifs >> n;
        ifs >> max_S;
        ifs >> max_P;
        ifs >> max_O;
        max_SO = max(max_S, max_O);
        L.load(filename + ".L", n);
        N.load(filename + ".N", n);
        B.load(filename + ".B", max_S + n);
        B_L.load(filename + ".BL", max_P + n);
    };

    /*FUNCIONES RPQ*/

    // caso ?x rpq ?y
    void rpq_var_to_var(const std::string &rpq,
                            unordered_map<std::string, uint64_t> &predicates_map,
                            std::vector<std::pair<uint64_t, uint64_t>> &output)
    {

        // automata para query no reversa, se usa para obtener los sujetos candidatos
        std::string query;
        int64_t i = 0;
        query = parse(rpq, i, predicates_map, max_P);
        RpqAutomata A(query, predicates_map);

        // automata para query reversa, se usa para encontrar los objetos una vez se tienen los sujetos
        std::string query2;
        int64_t i2 = rpq.size() - 1;
        query2 = parse_reverse(rpq, i2, predicates_map, max_P);
        RpqAutomata A2(query2, predicates_map);

        // PASO 0: ver si se acepta solucion "todo objeto con si mismo"
        bool is_empty_path_solution = false;
        if (A.atFinal((word_t)1, FWD))
        {
            is_empty_path_solution = true;
            for (uint64_t obj = 1; obj <= max_SO; ++obj)
                output.emplace_back(obj, obj);
            if (BOUND && output.size() >= BOUND)
                return;
        }

        // PASO 1; obtener sujetos candidatos
        std::vector<uint64_t> subjects_var_to_var;
        rpq_var_to_var_get_s(A, predicates_map, subjects_var_to_var);

        std::unordered_map<word_t, std::vector<uint64_t>> Q;
        std::unordered_map<uint64_t, word_t> seen;

        for (uint64_t i = 0; i < subjects_var_to_var.size(); i++)
        {
            // Q.clear();
            if (is_empty_path_solution)
                seen[subjects_var_to_var[i]] = (1 << 15); // objeto ya se agrego como respuesta
            _rpq_one_const(A2, Q, seen, predicates_map, subjects_var_to_var[i], output, true);
            seen.clear(); // TODO a priori tiene que estar descomentado pero si se guarda que ''Ds'' son exitosos y a que sijeto final corresponden se podria ahorrar(?)

            if (BOUND && output.size() >= BOUND)
                return;
            // check timeout
            if (TIME_OUT)
            {
                time_span = duration_cast<microseconds>(high_resolution_clock::now() - query_start);
                if (time_span.count() > TIME_OUT)
                    return;
            }
        }
    }

    void rpq_var_to_var_get_s(RpqAutomata &A,
                                  unordered_map<std::string, uint64_t> &predicates_map,
                                  std::vector<uint64_t> &output)
    {
        word_t initial_D;
        initial_D = (word_t)1; // sudden eurobeat and the sound of car engines
        std::vector<uint64_t> initial_predicates;

        // PASO 1: ver que predicados salen del initial D
        for (auto &pred : predicates_map)
        {
            uint64_t pred_id = pred.second;
            word_t new_D = (word_t)A.next(initial_D, pred_id, FWD);
            if (new_D != 0) // es valido!
                initial_predicates.emplace_back(pred_id);
        }

        // PASO 2: obtener los ?x con sources o targets
        // TODO optimizable: se puede invertir la query segun ctdd preds negados para reemplazar algunos targets por sources

        bit_vector initial_subjects_bv = bit_vector(max_SO + 1, 0); // usar un bv ayuda a evitar sujetos repetidos
        for (uint64_t ini_pred : initial_predicates)
        {
            if (TIME_OUT)
            {
                time_span = duration_cast<microseconds>(high_resolution_clock::now() - query_start);
                if (time_span.count() > TIME_OUT)
                    return;
            }

            if (ini_pred <= max_P)
                targets_l_bv(ini_pred, initial_subjects_bv);
            else
                sources_l_bv(ini_pred - max_P, initial_subjects_bv);
        }

        bv_rank_type rankbv(&initial_subjects_bv);
        bv_select_type selectbv(&initial_subjects_bv);
        uint64_t ones = rankbv(max_SO + 1);

        // traducir objetos marcados en el bv a la id real
        uint64_t subj_i = 1;
        while (subj_i <= ones)
        {
            output.emplace_back(selectbv(subj_i));
            subj_i++;
        }
    };

    /*Casos con una constante*/

    // nuevo caso: el primer paso de la iteracion sea sobre objetos en vez de preds
    void rpq_one_const(const std::string &rpq,
                           unordered_map<std::string, uint64_t> &predicates_map,
                           uint64_t initial_object,
                           std::vector<std::pair<uint64_t, uint64_t>> &output, bool is_const_to_var)
    {

        std::string query, str_aux;

        if (is_const_to_var)
        {
            int64_t iii = rpq.size() - 1;
            query = parse_reverse(rpq, iii, predicates_map, max_P);
        }
        else
        {
            int64_t iii = 0;
            query = parse(rpq, iii, predicates_map, max_P);
        }
        RpqAutomata A(query, predicates_map);

        std::unordered_map<word_t, std::vector<uint64_t>> Q; // Q[word_t D] = lista con los predicados del automata que llegan a mis nodos activos. TODO: cambiar vector por bitmap
        std::unordered_map<uint64_t, word_t> seen;

        _rpq_one_const(A, Q, seen, predicates_map, initial_object, output, is_const_to_var);
    };

    bool _rpq_one_const(
        RpqAutomata &A, std::unordered_map<word_t, std::vector<uint64_t>> &Q,
        std::unordered_map<uint64_t, word_t> &seen,
        std::unordered_map<std::string, uint64_t> &predicates_map,
        uint64_t initial_object,
        std::vector<std::pair<uint64_t, uint64_t>> &solutions,
        bool is_const_to_var)
    {

        word_t current_D, filtered_D;             // palabra de maquina D, con los estados activos
        std::stack<induction_data> ist_container; // data induccion:  sujetos obtenidos + D
        current_D = (word_t)A.getFinalStates();

        // second condition should be false iff  the call is from the var_to_var case
        if (A.atFinal(current_D, BWD) && !(seen[initial_object] & (1 << 15)))
        {
            solutions.emplace_back(initial_object, initial_object);
            seen[initial_object] = seen[initial_object] | (1 << 15);
        }

        std::vector<uint64_t> initial_object_vec;
        initial_object_vec.emplace_back(initial_object);
        ist_container.push(induction_data{initial_object_vec, current_D});

        while (!ist_container.empty())
        {
            if (TIME_OUT)
            {
                time_span = duration_cast<microseconds>(high_resolution_clock::now() - query_start);
                if (time_span.count() > TIME_OUT)
                    return false;
            }

            induction_data ist_top = ist_container.top();
            ist_container.pop();

            current_D = ist_top.current_D;
            initial_object_vec = ist_top.ojects;

            // cout << "  >PART1: PARA CADA OBJETO EN DATA DE INDUCCION..." << endl;
            for (uint64_t obj : ist_top.ojects)
            {
                filtered_D = ~seen[obj] & current_D; // dejar los nodos del automata no vistos aun
                // filtered_D = filtered_D & ~(1 << 15); //da igual, el mayor bit de current_D nunca deberia ser 1 
                if (filtered_D) // quedan ''1s'' del automata por ver, else pasa al siguiente objeto
                {
                    seen[obj] = seen[obj] | current_D;

                    // PART2: CALCULAR Q SI NO ESTA PARA ESE D: que preds llegan a mis estados activos?
                    if (Q.count(filtered_D) == 0) // TODO: es count lo mas rapido?
                    {
                        std::vector<uint64_t> valid_preds;
                        for (auto &pred : predicates_map)
                        {
                            uint64_t pred_id = pred.second;
                            // cout << "condicion D AND B[p] es: " << std::bitset<16>(current_D & A.getB()[pred_id]) << endl;

                            if (filtered_D & A.getB()[pred_id])
                                valid_preds.emplace_back(pred_id);
                        }
                        Q[filtered_D] = valid_preds;
                    }

                    // PART3: FOR EACH PREDICATE THAT REACHES FILTERED_D
                    for (uint64_t pred : Q[filtered_D])
                    {
                        word_t new_D = (word_t)A.next(filtered_D, pred, BWD);

                        std::vector<uint64_t> subjects_valid;
                        if (new_D != 0)
                        {
                            // PART5: extraer los sujetos para ese pred + obj. OJO: PASO COSTOSO!
                            if (pred <= max_P)
                            {
                                // cout << "pred no es negado !!" << endl;
                                subjects_valid = neigh_l_all(obj, pred);
                            }
                            else
                            {
                                // cout << "pred negado" << endl;
                                subjects_valid = rneigh_l_all(obj, pred - max_P);
                            }

                            if (!subjects_valid.empty())
                            {
                                if (A.atFinal(new_D, BWD))
                                {
                                    for (uint64_t sub : subjects_valid)
                                    {
                                        if (~seen[sub] & (1 << 15))
                                        {
                                            seen[sub] = seen[sub] | (1 << 15); //evita pares repetidos
                                            if (is_const_to_var)
                                                solutions.emplace_back(initial_object, sub);
                                            else
                                                solutions.emplace_back(sub, initial_object);
                                        }
                                    }

                                    if (BOUND && solutions.size() >= BOUND)
                                        return false;
                                }
                                // cout << "actualizar container" << endl;
                                ist_container.push(induction_data{subjects_valid, new_D});
                            }
                        }
                    }
                }
            }
        }
        return false;
    };
};
#endif