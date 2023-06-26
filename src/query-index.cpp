//./query-index /home/bob/pyProjects/data/100k.dat  /home/bob/pyProjects/data/paths -o /home/bob/pyProjects/data/navGraphOutput
//./query-index /home/bob/pyProjects/data/metro/metro.dat  /home/bob/pyProjects/data/metro/paths -o /home/bob/pyProjects/data/metro/navGraphOutput

//./query-index ~/pyProjects/data/grafo/grafo.dat  ~/pyProjects/data/grafo/paths -o ~/pyProjects/data/grafo/NAV_TEST

//./query-index ~/pyProjects/data/30k.dat  ~/pyProjects/data/paths -o ~/pyProjects/data/grafo/NAVOLD30k
//~/cppProjects/clean/NavarroGraph/build/build-index /home/bob/pyProjects/data/30k.dat
//./build-index /home/bob/pyProjects/data/grafo/grafo.dat

#include <iostream>
#include <fstream>
#include <sdsl/construct.hpp>
#include "nav_graph.hpp"
#include "query_config.hpp"
#include "Config.hpp"

using namespace std;

using namespace std::chrono;
using timer = std::chrono::high_resolution_clock;

int main(int argc, char **argv)
{

    if (argc < 3 || cmdOptionExists(argv, argv + argc, "-h"))
    {
        cout << "  Usage: " << argv[0] << " <ring-index-file> <queries-file>" << endl;
        cout << " -o <output-file>:  saves in <output-file>  the time used + the number of pairs obtained for each query" << endl;
        exit(1);
    }

    // handle -o
    ofstream out;
    char *output_file = getCmdOption(argv, argv + argc, "-o");
    string output_file_pairs;
    if (output_file && OUTPUT_PAIRS)
    {
        output_file_pairs = (string)output_file;
        output_file_pairs.append("_pairs");
        out.open(output_file_pairs, ofstream::out | ofstream::trunc);
        out.close();
    }
    if (output_file)
    {
        out.open(output_file, ofstream::out | ofstream::trunc);
        out.close();
    }

    nav_graph graph;
    graph.load(string(argv[1]));

    cout << "  > Graph Loaded!" << endl;
    cout << "   > number of triples: " << graph.n << endl;
    cout << "   > max subject/object: " << graph.max_S << "/" << graph.max_O << endl;
    cout << "   > number of predicates: " << graph.max_P << endl;

    std::ifstream ifs_SO(string(argv[1]) + ".SO", std::ifstream::in);
    std::ifstream ifs_P(string(argv[1]) + ".P", std::ifstream::in);
    std::ifstream ifs_q(argv[2], std::ifstream::in);

    std::unordered_map<string, uint64_t> map_SO;
    std::unordered_map<string, uint64_t> map_P;

    uint64_t id;
    string s_aux, data;

    while (std::getline(ifs_SO, data))
    {
        auto space = data.find(' ');
        id = std::stoull(data.substr(0, space));
        s_aux = data.substr(space + 1);
        map_SO[s_aux] = id;
    }

    while (std::getline(ifs_P, data))
    {
        auto space = data.find(' ');
        id = std::stoull(data.substr(0, space));
        s_aux = data.substr(space + 1);
        map_P[s_aux] = id;
    }

    typedef struct
    {
        uint64_t id;
        uint64_t beg;
        uint64_t end;
    } rpq_predicate;

    std::unordered_map<std::string, uint64_t> pred_map;
    std::string query, line;
    uint64_t s_id, o_id, n_line = 0, q = 0;
    bool flag_s, flag_o, skip_flag;
    std::vector<std::pair<uint64_t, uint64_t>> query_output;
    std::vector<word_t> B_array(4 * graph.n, 0);

    high_resolution_clock::time_point stop; //start, stop;
    double total_time = 0.0; 

    uint64_t n_predicates, n_operators;
    // bool is_negated_pred, is_a_path, is_or, is_const_to_var;
    // std::string query_type;
    // uint64_t first_pred_id, last_pred_id;

    do
    {
        getline(ifs_q, line);
        pred_map.clear();
        query.clear();
        // query_type.clear();
        query_output.clear();
        flag_s = flag_o = false;
        skip_flag = false;
        n_line++;

        stringstream X(line);
        q++;

        // n_predicates = 0;
        // is_negated_pred = false;
        // n_operators = 0;
        // is_a_path = true;
        // is_or = true;
        // is_const_to_var = false;

        if (line.at(0) == '?')
        {
            flag_s = false;
            X >> s_aux;
            if (line.at(line.size() - 3) == '?')
            {
                flag_o = false;
                line.pop_back();
                line.pop_back();
                line.pop_back();
            }
            else
            {
                flag_o = true;
                string s_aux_2;
                uint64_t i = line.size() - 1;

                while (line.at(i) != ' ')
                    i--;
                i++;
                while (i < line.size() - 1 /*line.at(i) != '>'*/)
                    s_aux_2 += line.at(i++);

                if (map_SO.find(s_aux_2) != map_SO.end())
                {
                    o_id = map_SO[s_aux_2];
                    i = 0;
                    // ofs_SO << o_id << " " << s_aux_2 << endl;
                    while (i < s_aux_2.size() + 1)
                    {
                        line.pop_back();
                        i++;
                    }
                }
                else
                    skip_flag = true;
            }
        }
        else
        {
            flag_s = true;
            X >> s_aux;
            if (map_SO.find(s_aux) != map_SO.end())
            {
                s_id = map_SO[s_aux];
            }
            else
                skip_flag = true;

            if (line.at(line.size() - 3) == '?')
            {
                // is_const_to_var = true;
                flag_o = false;
                line.pop_back();
                line.pop_back();
                line.pop_back();
            }
            else
            {
                flag_o = true;
                string s_aux_2;
                uint64_t i = line.size() - 2;
                while (line.at(i) != ' ')
                    i--;
                i++;
                while (i < line.size() - 1 /*line.at(i) != '>'*/)
                    s_aux_2 += line.at(i++);

                if (map_SO.find(s_aux_2) != map_SO.end())
                {
                    o_id = map_SO[s_aux_2];
                    i = 0;
                    while (i < s_aux_2.size() + 1)
                    {
                        line.pop_back();
                        i++;
                    }
                }
                else
                    skip_flag = true;
            }
        }

        stringstream X_a(line);
        X_a >> s_aux;

        if (!skip_flag)
        {
            X_a >> s_aux;
            do
            {
                for (uint64_t i = 0; i < s_aux.size(); i++)
                {
                    if (s_aux.at(i) == '<')
                    {
                        std::string s_aux_2, s_aux_3;
                        s_aux_2.clear();

                        while (s_aux.at(i) != '>')
                        {
                            s_aux_2 += s_aux.at(i++);
                        }
                        s_aux_2 += s_aux.at(i);
                        if (s_aux_2[1] == '%')
                        {
                            s_aux_3 = "<" + s_aux_2.substr(2, s_aux_2.size() - 1);
                            // is_negated_pred = true;
                        }
                        else
                        {
                            s_aux_3 = s_aux_2;
                        }

                        if (map_P.find(s_aux_3) != map_P.end())
                        {
                            query += s_aux_2;
                            // last_pred_id = pred_map[s_aux_3] = map_P[s_aux_3];
                            pred_map[s_aux_3] = map_P[s_aux_3];
                            // TODO: check preds no usados, ctdd de elementos en pred_map debe ser igual al m del automata

                            n_predicates++;
                            // if (n_predicates == 1)
                            //     first_pred_id = pred_map[s_aux_3];
                        }
                        else
                        {
                            cout << q << ";0;0" << endl;
                            skip_flag = true;
                            break;
                        }
                    }
                    else
                    {
                        if (s_aux.at(i) != '/' and s_aux.at(i) != ' ')
                        {
                            n_operators++;
                            if (s_aux.at(i) == '^')
                                query += '%';
                            else
                            {
                                query += s_aux.at(i);
                                // if (s_aux.at(i) != '(' and s_aux.at(i) != ')')
                                //     query_type += s_aux.at(i);
                                // is_a_path = false;
                                // if (s_aux.at(i) != '|')
                                //     is_or = false;
                            }
                        }
                        else if (s_aux.at(i) == '/')
                        {
                            n_operators++;
                            // query_type += s_aux.at(i);
                        }
                    }
                }
            } while (!skip_flag and X_a >> s_aux);

            if (!skip_flag)
            {
                query_start = high_resolution_clock::now();
                if (!flag_s and !flag_o)
                {
                    // cout << " ?x ?y " << endl;
                    graph.new_rpq_var_to_var(query, pred_map, query_output);
                }
                else
                {
                    if (flag_s)
                    {
                        // cout << " ?y " << endl;
                        graph.new_rpq_one_const(query, pred_map, s_id, query_output, true);
                        // graph.rpq_one_const(query, pred_map, s_id, query_output, true);
                    }
                    else
                    {
                        // cout << " ?x " << endl;
                        graph.new_rpq_one_const(query, pred_map, o_id, query_output, false);
                        // graph.rpq_one_const(query, pred_map, o_id, query_output, false);
                    }
                }
                stop = high_resolution_clock::now();
                time_span = duration_cast<microseconds>(stop - query_start);
                total_time = time_span.count();


                cout << q << ";" << query_output.size() << ";" << (uint64_t)(total_time * 1000000000ULL) << endl;

                if (output_file)
                {
                    // write times + number of results
                    out.open(output_file, std::ios::app);
                    out << query_output.size() << "," << (uint64_t)(total_time * 1000000000ULL) << endl;
                    out.close();

                    // write times + number of results + pairs obtained
                    if (OUTPUT_PAIRS)
                    {
                        out.open(output_file_pairs, std::ios::app);
                        out <<endl << q << ": " << line << endl;
                        // out << query_output.size() << "," << (uint64_t)(total_time * 1000000000ULL) << endl;
                        for (pair<uint64_t, uint64_t> pair : query_output)
                            out << pair.first << "-" << pair.second << endl;
                        out.close();
                    }
                }
            }
            else
                skip_flag = false;
        }
        else
        {
            cout << q << ";0;0" << endl;
            skip_flag = false;
        }
    } while (!ifs_q.eof());

    return 0;
}
