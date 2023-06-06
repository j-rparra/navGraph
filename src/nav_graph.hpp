/*
revisar targets!
TODO: eliminar cola de P_count y S_count
arreglar verbose
caso sujetos vacios, varios s y o vacios en el index
+usar clear, shrink to fit, bit_compress
TODO: revisar tiempo de sources en informe, +checkeo para el select
revisar tiempo de targets (+ access)
 // TODO: renombrar access a edges en codigo e informe
  limpiar los include
  sacar el count/indegree/oytdegree de los all? (se necesitan para que el select no caiga)
 quitar n_predicates y n_operators
  optimizar caso dos vaiables
 posible solucion para duplicados en la solucion, (caso new) el primer digito del seen para cada objeto indica si esta como solucion o no?
*/
/*
en version vartovar adrian: (1) mezclar el paso 2 dentro del paso 1 del var_to_var para no ''desperdiciar' objetos
                    (2) intentar hacer el paso 1 flojamente con todos los objetos asociados al pred (se puede?)
*/

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

#include "Config.hpp"
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

typedef struct
{
    uint64_t ojects;
    word_t current_D;
} induction_data2;

class nav_graph
{
    nav_tree L, N;
    nav_bv B, B_L;

public:
    uint64_t max_S, max_P, max_O;
    uint64_t n;

    nav_graph() { ; }

    nav_graph(vector<spo_triple> &E, uint64_t max_s, uint64_t max_o, uint64_t max_p, bool verbose = true)
    {
        uint64_t i, tmp;
        std::fstream fp;

        max_S = max_s, max_P = max_p, max_O = max_o;
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

        cout << "  > moving vector with triples to disk into L.tmp and N.tmp" << endl;
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
    /*Primer grupo de operaciones: para un vertice y grafo sin filtrar*/

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
    { // assuming j ≤ outdegree(G, v)
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
        for (uint64_t i = 1; i <= outdegree(v); i++)
            neighboors.emplace_back(neigh(v, i));
        return neighboors;
    }

    std::vector<pair<uint64_t, uint64_t>> rneigh_all(uint64_t v)
    {
        std::vector<pair<uint64_t, uint64_t>> rneighboors;
        for (uint64_t i = 1; i <= indegree(v); i++)
            rneighboors.emplace_back(rneigh(v, i));
        return rneighboors;
    }

    /*Segundo grupo de operaciones, para un vertice y un subgrafo con labels l */

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
        return L.my_rank(p2, l) - L.my_rank(p1, l);
    }

    uint64_t indegree_l(uint64_t v, uint64_t l)
    {
        uint64_t b = B_L.select(l);
        uint64_t r1 = b - l;
        uint64_t r2 = B_L.succ(b + 1) - (l + 1);
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

    std::vector<uint64_t> old_neigh_l_all(uint64_t v, uint64_t l)
    {
        std::vector<uint64_t> neighboors;
        for (uint64_t i = 1; i <= outdegree_l(v, l); i++)
            neighboors.emplace_back(neigh_l(v, i, l));
        return neighboors;
    }

    std::vector<uint64_t> neigh_l_all(uint64_t v, uint64_t l)
    {
        std::vector<uint64_t> neighboors;

        uint64_t p = B.select(v) - v;
        uint64_t r = B_L.select(l) - l;
        uint64_t q = L.my_rank(p, l);
        for (uint64_t i = 1; i <= outdegree_l(v, l); i++)
            neighboors.emplace_back(N.access(r + q + i));
        return neighboors;
    }

    std::vector<uint64_t> rneigh_l_all(uint64_t v, uint64_t l)
    {
        std::vector<uint64_t> rneighboors;
        uint64_t t, s;
        uint64_t r = B_L.select(l) - l;
        for (uint64_t i = 1; i <= indegree_l(v, l); i++)
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

    /*Tercer grupo de operaciones para un subgrafo con labels l */

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
        for (uint64_t i = 1; i <= count_l(l); i++)
            edges.emplace_back(access_l(i, l));
        return edges;
    }

    std::vector<uint64_t> targets_l(uint64_t l)
    {
        std::vector<uint64_t> targets;
        uint64_t p1 = B_L.select(l) - l + 1;
        uint64_t p2 = B_L.select(l + 1) - (l + 1);
        //     std::vector<std::array<uint64_t, 2ul>> ranges;
        //     ranges.emplace_back({p1 - 1, p2 - 1});
        //     ranges.emplace_back({p1 - 1, p2 - 1});
        //     std::vector<std::pair<uint64_t, uint64_t>> values_freq = N.intersect(ranges);
        //     for (uint64_t i = 0; i < values_freq.size(); ++i)
        //         targets.emplace_back(values_freq[i].first);
        while (p1 - 1 < p2)
        { // TODO optimizable
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

        L.load(filename + ".L", n);
        N.load(filename + ".N", n);
        B.load(filename + ".B", max_S + n);
        B_L.load(filename + ".BL", max_P + n);
    };

    /*FUNCIONES RPQ*/

    // nuevo caso ?x rpq ?y, los objetos son los targets del nodo inicial
    void new_rpq_var_to_var(const std::string &rpq,
                            unordered_map<std::string, uint64_t> &predicates_map, // ToDo: esto debería ser una variable miembro de la clase
                            std::vector<std::pair<uint64_t, uint64_t>> &output)
    {

        // automata para query no reversa, se usa para obtener los objetos
        std::string query;
        int64_t i = 0;
        query = parse(rpq, i, predicates_map, max_P);
        RpqAutomata A(query, predicates_map);

        // automata para query reversa,  se usa una vez se tienen los objetos
        std::string query2;
        int64_t i2 = rpq.size() - 1;
        query2 = parse_reverse(rpq, i2, predicates_map, max_P);
        RpqAutomata A2(query2, predicates_map);

        std::vector<uint64_t> objects_var_to_var;

        // PASO 1; obtener objetos candidatos
        new_rpq_var_to_var_get_o(A, predicates_map, objects_var_to_var);

        std::unordered_map<word_t, std::vector<uint64_t>> Q;
        std::unordered_map<uint64_t, word_t> seen;

        for (uint64_t i = 0; i < objects_var_to_var.size() and output.size() < bound; i++)
        {
            // Q.clear();
            seen.clear(); // a priori tiene que estar descomentado pero
            // si se guarda que ''Ds'' son exitosos y a que sijeto final corresponden se podria ahorrar(?)
            _new_rpq_one_const(A2, Q, seen, predicates_map, objects_var_to_var[i], output, true);
        }
    }

    // version eficiente
    void new_rpq_var_to_var_get_o(RpqAutomata &A,
                                  unordered_map<std::string, uint64_t> &predicates_map,
                                  std::vector<uint64_t> &output_objects)
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

        // PASO 2: obtener los ?x con sources o targets TODO optimizable: se peude invertir o no query segun ctdd preds negados para
        //reemplazar algunos targets por sources o al reves
        std::vector<uint64_t> initial_objects;
        for (uint64_t ini_pred : initial_predicates)
        {
            if (ini_pred <= max_P)
                initial_objects = targets_l(ini_pred);
            else
                initial_objects = sources_l(ini_pred - max_P);

            output_objects.insert(output_objects.end(), initial_objects.begin(), initial_objects.end());
        }
    };

    // viejo caso ?x rpq ?y, se iban a buscar los objetos iniciales considerando la query
    // reversa y se llegaba hasta el final en el paso inductivo
    /*void rpq_var_to_var(const std::string &rpq,
                        unordered_map<std::string, uint64_t> &predicates_map, // ToDo: esto debería ser una variable miembro de la clase
                        std::vector<std::pair<uint64_t, uint64_t>> &output)
    {

        // high_resolution_clock::time_point start, stop;
        // double total_time = 0.0;
        // duration<double> time_span;

        // start = high_resolution_clock::now();

        // automata para query no reversa, se usa una vez se tienen los objetos candidatos
        std::string query;
        int64_t i = 0;
        query = parse(rpq, i, predicates_map, max_P);
        RpqAutomata A(query, predicates_map);

        // automata para query reversa, se usa para obtener los objetos candidatos
        std::string query2;
        int64_t i2 = rpq.size() - 1;
        query2 = parse_reverse(rpq, i2, predicates_map, max_P);
        RpqAutomata A2(query2, predicates_map);

        std::vector<uint64_t> objects_var_to_var;
        std::unordered_map<word_t, std::vector<uint64_t>> Q2;
        std::set<std::string> seen2;

        // new_rpq_var_to_var_get_o(A2, Q2, seen2, predicates_map, objects_var_to_var);
        // objects_var_to_var = {2};
        // cout << " paso 1 listo, objetos obtenidos: " << endl;

        // for (auto objeto : objects_var_to_var)
        //     cout << " OBJETOS: " << objeto << endl;

        std::unordered_map<word_t, std::vector<uint64_t>> Q;
        std::unordered_map<uint64_t, word_t> seen;

        // std::chrono::high_resolution_clock::time_point start;
        // double total_time = 0.0;
        // std::chrono::duration<double> time_span;
        // start = std::chrono::high_resolution_clock::now();

        for (uint64_t i = 0; i < objects_var_to_var.size() and output.size() < bound; i++)
        {
            // Q.clear();
            seen.clear();
            _new_rpq_one_const(A, Q, seen, predicates_map, objects_var_to_var[i], output, true);
        }
    }*/

    // se calculan todos los preds que llegan al Dinicial, para cada predicado obtener sus targets e iniciar induccion con esa data
    /*void old_rpq_var_to_var_get_o(RpqAutomata &A, std::unordered_map<word_t, std::vector<uint64_t>> &Q,
                                  std::unordered_map<uint64_t, word_t> &seen,
                                  unordered_map<std::string, uint64_t> &predicates_map, // ToDo: esto debería ser una variable miembro de la clase
                                  std::vector<uint64_t> &output_objects)
    {
        bool is_const_to_var;
        is_const_to_var = true;
        word_t current_D; // palabra de maquina D, con los estados activos

        current_D = (word_t)A.getFinalStates();

        // PASO 1: calcular los predicados que llegan a D
        std::vector<uint64_t> initial_predicates;
        for (auto &pred : predicates_map) // OJO preds demas cuando hay uno negado, se puede optimizar
        {
            uint64_t pred_id = pred.second;

            // cout << " predicados del map: " << pred.first << "-" << pred.second << endl;
            // cout << "condicion D AND B[p] es: " << std::bitset<16>(current_D & A.getB()[pred_id]) << endl;

            if (current_D & A.getB()[pred_id])
                initial_predicates.emplace_back(pred_id);
        }
        Q[current_D] = initial_predicates;

        // PASO 2: para cada predicado obtener sus targets e iniciar induccion con esa data
        std::stack<induction_data> ist_container; // stack<  vector<uint64_t>, word_t>, objetos + D
        std::vector<uint64_t> initial_objects;
        std::vector<uint64_t> initial_objects_aux;

        for (uint64_t ini_pred : initial_predicates)
        {
            // cout << "INICIANDO CON PRED: " << ini_pred;
            if (ini_pred <= max_P)
                initial_objects = targets_l(ini_pred);
            else
                initial_objects = sources_l(ini_pred - max_P);

            // PASO 3: limpiar los objetos que ya estan como respuesta //OJO ver tiempo
            for (uint64_t obj : initial_objects)
            {
                boolean is_seen = std::find(output_objects.begin(), output_objects.end(), obj) != output_objects.end();
                if (!is_seen)
                    initial_objects_aux.emplace_back(obj);
            }
            initial_objects = initial_objects_aux;

            if (A.atFinal(current_D, BWD))
                for (uint64_t sub : initial_objects)
                {
                    boolean is_seen = std::find(output_objects.begin(), output_objects.end(), sub) != output_objects.end();
                    if (!is_seen)
                        output_objects.emplace_back(sub); // OJO usar emplace_back?
                }

            ist_container.push(induction_data{initial_objects, current_D});

            while (!ist_container.empty())
            {
                induction_data ist_top = ist_container.top();
                ist_container.pop();

                current_D = ist_top.current_D;

                // cout << "  >PART1: CALCULAR Q PARA D SI NO EXISTE" << endl;
                if (Q.count(current_D) == 0)
                {
                    std::vector<uint64_t> valid_preds;
                    for (auto &pred : predicates_map) // OJO preds demas cuando hay uno negado, se puede optimizar
                    {
                        uint64_t pred_id = pred.second;
                        if (current_D & A.getB()[pred_id])
                            valid_preds.emplace_back(pred_id);
                    }
                    Q[current_D] = valid_preds;
                }

                // cout << "  >PART2: For each predicate valido...." << endl;
                for (uint64_t pred : Q[current_D])
                {
                    word_t new_D = (word_t)A.next(current_D, pred, BWD);

                    // cout << " PART 2.2: compute pred's subjects" << endl;
                    std::vector<uint64_t> subjects_valid;
                    std::vector<uint64_t> subjects_valid_aux;
                    // OJO pueden haber rneigh vacios para varios objetos!!

                    for (uint64_t obj : ist_top.ojects)
                    {
                        // if (pred <= max_P)
                        //     subjects_valid_aux = is_const_to_var ? rneigh_l_all(obj, pred) : rneigh_l_all(obj, pred);
                        // else
                        //     subjects_valid_aux = is_const_to_var ? rneigh_l_all(obj, pred - max_P) : neigh_l_all(obj, pred - max_P);

                        if (pred <= max_P)
                        {
                            // cout << "pred <= max_P" << endl;

                            subjects_valid_aux = is_const_to_var ? neigh_l_all(obj, pred) : neigh_l_all(obj, pred);
                        }
                        else
                        {
                            // cout << "pred > max_P" << endl;
                            subjects_valid_aux = is_const_to_var ? rneigh_l_all(obj, pred - max_P) : rneigh_l_all(obj, pred - max_P);
                        }

                        // cout << "  >PART 3: iterar por los sujetos y dejar los no vistos antes" << endl;
                        std::vector<std::uint64_t> true_subjects_valid;
                        for (uint64_t sub : subjects_valid_aux) // OJO optimizar con erase?
                        // for (std::vector<std::uint64_t>::iterator sub = subjects_valid.begin(); sub != subjects_valid.end();)
                        {
                            boolean is_seen = seen.find(to_string(new_D) + to_string(sub)) != seen.end();

                            if (!is_seen) // OJO usar count()?
                            {
                                // cout << ">PART 3.2: si no se ha visto actualizat seen" << endl;
                                seen.insert(to_string(new_D) + to_string(sub));
                                subjects_valid.emplace_back(sub);
                            }
                        }

                        if (!subjects_valid.empty())
                        {
                            if (A.atFinal(new_D, BWD))
                            {
                                for (uint64_t sub : subjects_valid)
                                {
                                    boolean is_seen = std::find(output_objects.begin(), output_objects.end(), sub) != output_objects.end();
                                    if (!is_seen)
                                        output_objects.emplace_back(sub); // OJO usar emplace_back?
                                }
                            }
                            // cout << ">PART 5: actualizar container" << endl;
                            if (new_D != 0)
                                ist_container.push(induction_data{subjects_valid, new_D});
                        }
                    }
                }
            }
        }
    }; */

    /*Casos con una constante*/

    // nuevo caso: el primer paso de la iteracion sea sobre objetos en vez de preds
    void new_rpq_one_const(const std::string &rpq,
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

        // std::chrono::high_resolution_clock::time_point start;
        // double total_time = 0.0;
        // std::chrono::duration<double> time_span;
        // start = std::chrono::high_resolution_clock::now();
        _new_rpq_one_const(A, Q, seen, predicates_map, initial_object, output, is_const_to_var);
    };

    
    bool _new_rpq_one_const(
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

        if (A.atFinal(current_D, BWD))
            solutions.emplace_back(initial_object, initial_object); 

        std::vector<uint64_t> initial_object_vec;
        initial_object_vec.emplace_back(initial_object);
        ist_container.push(induction_data{initial_object_vec, current_D});

        while (!ist_container.empty())
        {
            induction_data ist_top = ist_container.top();
            ist_container.pop();

            current_D = ist_top.current_D;
            initial_object_vec = ist_top.ojects;

            // cout << "  >PART1: PARA CADA OBJETO EN OBJETOS DE INDUCCION..." << endl;  
            for (uint64_t obj : ist_top.ojects)
            {
                filtered_D = ~seen[obj] & current_D; // dejar los nodos del automata no vistos aun
                // cout << "filtered_D es: " << std::bitset<16>(filtered_D) << endl;

                if (filtered_D) // quedan ''1s'' del automata por ver, else pasa al siguiente objeto
                {
                    seen[obj] = seen[obj] | current_D;
                    
                    // TODO: SACAR DEL FOR ALGUNOS PASOS
                    // PART2: CALCULAR Q SI NO ESTA PARA ESE D, que preds llegan a mis estados activos?
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
                            // PART5: extraer los sujetos para ese pred + obj. OJO PASO COSTOSO!
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
                                    if (is_const_to_var)
                                        for (uint64_t sub : subjects_valid)
                                            solutions.emplace_back(initial_object, sub); 
                                                                                         // v1.insert( v1.end(), v2.begin(), v2.end() );
                                    else
                                        for (uint64_t sub : subjects_valid)
                                            solutions.emplace_back(sub, initial_object);

                                    if (solutions.size() >= bound)
                                        return false;
                                }
                                // cout << ">PART 5: actualizar container" << endl;
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