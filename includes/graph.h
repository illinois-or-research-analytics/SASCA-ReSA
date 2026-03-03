#ifndef GRAPH_H
#define GRAPH_H

#include <set>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <vector>
#include <string>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <queue>
#include <iostream>
#include <cmath>
#include <random>
#include "pcg_random.hpp"

class Graph {
    public:
        Graph(std::string edgelist, std::string nodelist, bool start_from_checkpoint, std::string num_authors_bag, int author_max_lifetime);
        void AddEdge(std::pair<int, int> edge);

        static inline char GetDelimiter(std::string filepath) {
            std::ifstream edgelist(filepath);
            std::string line;
            getline(edgelist, line);
            if (line.find(',') != std::string::npos) {
                return ',';
            } else if (line.find('\t') != std::string::npos) {
                return '\t';
            } else if (line.find(' ') != std::string::npos) {
                return ' ';
            }
            throw std::invalid_argument("Could not detect filetype for " + filepath);
        }

        static inline std::unordered_map<std::string, int> GetHeaderToIndexMap(char delimiter, std::string filepath) {
            std::unordered_map<std::string, int> header_to_index_map;
            std::ifstream input_nodelist(filepath);
            std::string line;
            std::getline(input_nodelist, line);
            std::stringstream ss(line);
            std::string current_value;
            int index = 0;
            while(std::getline(ss, current_value, delimiter)) {
                header_to_index_map[current_value] = index;
                index ++;
            }
            return header_to_index_map;
        }



        const std::set<int>& GetNodeSet() const;
        const std::unordered_map<int, std::vector<int>>& GetForwardAdjMap() const;
        const std::unordered_map<int, std::vector<int>>& GetBackwardAdjMap() const;
        void SetIntAttribute(std::string attribute_key, int node, int attribute_value);
        void SaveInitialAuthorReputations();
        int GetIntAttribute(std::string attribute_key, int node) const;
        void SetStringAttribute(std::string attribute_key, int node, std::string attribute_value);
        std::string GetStringAttribute(std::string attribute_key, int node) const;
        void SetDoubleAttribute(std::string attribute_key, int node, double attribute_value);
        double GetDoubleAttribute(std::string attribute_key, int node) const;
        bool HasIntAttribute(std::string attribute_key, int node) const;
        void ParseNodelist();
        void ParseEdgelist();
        int GetInDegree(int node) const;
        int GetOutDegree(int node) const;
        void ComputeAuthorReputations();
        int GetAuthorReputationForNode(int node) const;
        void AddNode(int u);
        void PrintGraph() const;
        void WriteGraph(std::string output_file) const;
        void WriteAttributes(std::string auxiliary_information_file) const;
        int GetNextAuthor(int current_year);
        int GetNextNumAuthors();
        void ReadNumAuthorsBag();
        int IsAuthorFunded(int author) const;
        void UpdateAuthorPublicationMap(int author, int node);
        void SetCartelID(int author, int cartel_id);
        int GetCartelID(int author) const;
        std::set<int> GetCartelAuthors(int cartel_id) const;
        std::vector<int> GetAuthorPublications(int author_id) const;
        void UpdateAuthorManual(int author_id);
        std::set<int> GetCartelSet() const;

    private:
        std::set<int> node_set;
        std::string edgelist;
        std::string nodelist;
        std::set<int> cartel_set;
        bool start_from_checkpoint;
        std::string num_authors_bag;
        int author_max_lifetime;
        int IsNextAuthorFunded();
        std::vector<int> num_authors_bag_vec;

    protected:
        std::unordered_map<int, std::vector<int>> publication_count_to_author_map;
        int next_author_id = 0;
        int lotka_exponent = 2;
        std::unordered_map<int, std::vector<int>> forward_adj_map;
        std::unordered_map<int, std::vector<int>> backward_adj_map;
        std::unordered_map<int, int> author_birth_year_map;
        std::unordered_map<int, std::vector<int>> author_publication_map;
        std::unordered_map<int, int> author_reputation_map;
        std::unordered_map<std::string, std::unordered_map<int, int>> int_attribute_map;
        std::unordered_map<std::string, std::unordered_map<int, std::string>> string_attribute_map;
        std::unordered_map<std::string, std::unordered_map<int, double>> double_attribute_map;
        std::unordered_map<int, int> author_cartel_map;
        std::unordered_map<int, std::set<int>> cartel_author_map;
};

#endif
