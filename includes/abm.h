#ifndef ABM_H
#define ABM_H

#include <cmath>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <random>
#include <thread>
#include <map>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <queue>
#include <tuple>
#include <omp.h>
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include "graph.h"
#include "pcg_random.hpp"
#include "utils.h"

enum Log {info, debug, error = -1};

class ABM {
    public:
        ABM(std::string edgelist, std::string nodelist, std::string out_degree_bag, std::string recency_table, std::string recency_bins, double alpha, double minimum_alpha, bool use_alpha, bool start_from_checkpoint, std::string planted_nodes, double fully_random_citations, double preferential_weight, double fitness_weight, double num_authors_weight, double author_reputation_weight, int fitness_value_min, int fitness_value_max, int fitness_lag_duration_min, int fitness_lag_duration_max, int fitness_peak_duration_min, int fitness_peak_duration_max, double minimum_preferential_weight, double minimum_fitness_weight, int in_degree_threshold, int fitness_threshold, int recency_threshold, double non_random_generator_probability, double growth_rate, int num_cycles, double same_year_citations, int neighborhood_sample, std::string num_authors_bag, int author_max_lifetime, double cartel_outdegree_proportion, std::string output_file, std::string auxiliary_information_file, std::string log_file, int num_processors, int log_level) : edgelist(edgelist), nodelist(nodelist), out_degree_bag(out_degree_bag), recency_table(recency_table), recency_bins(recency_bins), alpha(alpha), minimum_alpha(minimum_alpha), use_alpha(use_alpha), start_from_checkpoint(start_from_checkpoint), planted_nodes(planted_nodes), fully_random_citations(fully_random_citations), preferential_weight(preferential_weight), fitness_weight(fitness_weight), num_authors_weight(num_authors_weight), author_reputation_weight(author_reputation_weight), fitness_value_min(fitness_value_min), fitness_value_max(fitness_value_max), fitness_lag_duration_min(fitness_lag_duration_min), fitness_lag_duration_max(fitness_lag_duration_max), fitness_peak_duration_min(fitness_peak_duration_min), fitness_peak_duration_max(fitness_peak_duration_max), minimum_preferential_weight(minimum_preferential_weight), minimum_fitness_weight(minimum_fitness_weight), in_degree_threshold(in_degree_threshold), fitness_threshold(fitness_threshold), recency_threshold(recency_threshold), non_random_generator_probability(non_random_generator_probability), growth_rate(growth_rate), num_cycles(num_cycles), same_year_citations(same_year_citations), neighborhood_sample(neighborhood_sample), num_authors_bag(num_authors_bag), author_max_lifetime(author_max_lifetime), cartel_outdegree_proportion(cartel_outdegree_proportion), output_file(output_file), auxiliary_information_file(auxiliary_information_file), log_file(log_file), num_processors(num_processors), log_level(log_level) {
            // initial validation
            if (this->log_file == "") {
                std::cerr << "Log file is required" << std::endl;
                exit(1);
            }
            if (this->log_level == -42) {
                std::cerr << "Log level is required" << std::endl;
                exit(1);
            }
            if(this->log_level > -1) {
                this->start_time = std::chrono::steady_clock::now();
                this->log_file_handle.open(this->log_file);
                if (!this->log_file_handle) {
                    std::cerr << "Opening log file failed" << std::endl;
                    std::cerr << "Requested path: " + this->log_file << std::endl;
                    exit(1);
                }
                this->timing_file_handle.open(this->log_file + "_timing");
                this->timing_file_handle << "year,stage,time\n";
            }
            if (!this->ValidateArguments()) {
                exit(1);
            }
            this->num_calls_to_log_write = 0;
            this->ReadOutDegreeBag();
            this->ReadRecencyProbabilities();
            if (this->planted_nodes != "") {
                this->ReadPlantedNodes();
            }
            this->InitializeBinBoundaries();
            omp_set_num_threads(this->num_processors);
        };

        ~ABM() {
            if(this->log_level > -1) {
                this->log_file_handle.close();
            }
            this->timing_file_handle.close();
        }

        int main();
        int WriteToLogFile(std::string message, Log message_type);
        bool ValidateArguments();
        bool ValidateBinBoundaries();
        void ReadOutDegreeBag();
        void ReadRecencyProbabilities();
        void ReadPlantedNodes();
        std::unordered_map<int, int> BuildContinuousNodeMapping(Graph* graph);
        std::unordered_map<int, int> ReverseMapping(std::unordered_map<int, int> mapping);
        std::vector<int> GetComplement(Graph* graph, const std::vector<int>& base_vec, const std::unordered_map<int, int>& reverse_continuous_node_mapping);
        int GetFinalGraphSize(Graph* graph);
        std::vector<int> GetGeneratorNodes(Graph* graph, const std::unordered_map<int, int>& reverse_continuous_node_mapping);
        std::vector<int> GetEligibleGeneratorNodes(Graph* graph, int graph_size, const std::unordered_map<int, int>& reverse_continuous_node_mapping, int* in_degree_arr, int* fitness_arr, int in_degree_threshold, int fitness_threshold, int start_year, int current_year, int recency_threshold);
        std::vector<int> GetGeneratorNodesFromSet(std::vector<int>& eligible_generator_nodes);
        std::vector<int> GetGraphAttributesGeneratorNodes(Graph* graph, int new_node) const;
        std::vector<int> GetNeighborhood(Graph* graph, const std::vector<int>& generator_nodes, const std::unordered_map<int, int>& reverse_continuous_node_mapping);
        std::unordered_map<int, std::vector<int>> GetNeighborhoodMap(Graph* graph, int current_year, const std::vector<int>& generator_nodes, int num_hops);
        std::unordered_map<int, std::vector<int>> GetOneAndTwoDistanceNeighborhoods(Graph* graph, int current_year, const std::vector<int>& generator_nodes, int num_hops);
        std::unordered_map<int, std::vector<int>> GetNHopNeighborhood(Graph* graph, int current_year, const std::vector<int>& generator_nodes, int num_hops);
        int GetBinIndex(int year_diff);
        void InitializeBinBoundaries();
        int GetBinIndex(Graph* graph, int current_node, int current_year);
        std::unordered_map<int, std::vector<int>> BinNeighborhood(Graph* graph, int current_year, std::vector<int> n_hop_list);
        std::unordered_map<int, double> GetBinnedRecencyProbabilities();
        std::unordered_map<int, int> BinOutdegrees(const std::unordered_map<int, std::vector<int>>& binned_neighborhood, int total_outdegree, std::unordered_map<int, double> binned_recency_probabilities);
        std::unordered_map<int, int> GetNumCitationsPerNeighborhood(double alpha, int total_num_citations_neighborhood, const std::unordered_map<int, std::vector<int>>& n_hop_map);
        void FillInDegreeArr(Graph* graph, const std::unordered_map<int, int>& continuous_node_mapping, int* in_degree_arr);
        void InitializeFitness(Graph* graph);
        void FillFitnessArr(Graph* graph, const std::unordered_map<int, int>& continuous_node_mapping, int current_year, int* fitness_arr);
        // void FillNumAuthorsArr(Graph* graph, const std::unordered_map<int, int>& continuous_node_mapping, int* num_authors_arr);
        void FillAuthorReputationArr(Graph* graph, const std::unordered_map<int, int>& continuous_node_mapping, int* author_reputation_arr);
        void UpdateGraphAttributesInitialAuthorReputations(Graph* graph, const std::vector<int>& new_nodes_vec);
        void PopulateWeightArrs(double* pa_weight_arr, double* fit_weight_arr, double* num_authors_weight_arr, double* author_reputation_weight_arr, int len);
        int MakeCartelCitations(Graph* graph, int author_id, const std::unordered_map<int, int>& continuous_node_mapping, const std::unordered_map<int, std::vector<int>> n_hop_map, int* citations, int num_cartel_citations);
        int GetNumCartelCitations(Graph* graph, int author_id, const std::unordered_map<int, std::vector<int>>& n_hop_map, int total_num_citations_neighborhood);
        void PopulateAlphaArr(double* alpha_arr, int len);
        void PopulateFitnessArrs(int* fitness_lag_duration_arr, int* fitness_peak_value_arr, int* fitness_peak_duration_arr, int len);
        void PopulateNumAuthorsArr(Graph* graph, int* num_authors_arr, int len);
        int GetMaxYear(Graph* graph);
        int GetMaxNode(Graph* graph);
        void PopulateOutDegreeArr(int* out_degree_arr, int len);
        void CalculateTanhScores(std::unordered_map<int, double>& cached_results, int* src_arr, double* dst_arr, int len);
        void CalculateExpScores(std::unordered_map<int, double>& cached_results, int* src_arr, double* dst_arr, int len);
        void SampleFromNeighborhoods(std::unordered_map<int, std::vector<int>>& one_and_two_hop_neighborhood_map, int neighborhood_size_threshold, int max_neighborhood_size);
        int MakeCitations(Graph* graph, const std::unordered_map<int, int>& continuous_node_mapping, int current_year, const std::vector<int>& candidate_nodes, int* citations, double* pa_arr, double* fit_arr, double* na_arr, double* ar_arr, double pa_weight, double fit_weight, double num_authors_weight, double author_reputation_weight, int current_graph_size, int num_citations);
        int MakeUniformRandomCitations(Graph* graph, const std::unordered_map<int, int>& continuous_node_mapping, int current_year, const std::vector<int>& candidate_nodes, int* citations, int current_graph_size, int num_citations);
        void FillSameYearSourceNodes(std::set<int>& same_year_source_nodes, int current_year_new_nodes);
        int MakeUniformRandomCitationsFromGraph(Graph* graph, const std::unordered_map<int, int>& reverse_continuous_node_mapping, std::vector<int>& generator_nodes, int* citations, int num_cited_so_far, int num_citations);
        int MakeSameYearCitations(const std::set<int>& same_year_source_nodes, int num_new_nodes, const std::unordered_map<int, int>& reverse_continuous_node_mapping, int* citations, int current_graph_size);
        void UpdateGraphAttributesWeights(Graph* graph, int next_node_id, double* pa_weight_arr, double* fit_weight_arr, double* num_authors_weight_arr, double* author_reputation_weight_arr, int len);
        void UpdateGraphAttributesOutDegrees(Graph* graph, int next_node_id, int* out_degree_arr, int len);
        void UpdateGraphAttributesGeneratorNodes(Graph* graph, int new_node, const std::vector<int>& generator_nodes);
        void UpdateGraphAttributesAuthors(Graph* graph, int new_node, int author_id);
        void UpdateGraphAttributesAlphas(Graph* graph, int next_node_id, double* alpha_arr, int len);
        void UpdateGraphAttributesPlantedNodesLineNumbers(Graph* graph, int next_node_id, const std::unordered_map<int, int>& planted_nodes_line_number_map);
        void UpdateGraphAttributesFitnesses(Graph* graph, const std::vector<int>& new_nodes_vec, const std::unordered_map<int, int>& continuous_node_mapping, int* fitness_lag_duration_arr, int* fitness_peak_value_arr, int* fitness_peak_duration_arr, int initial_graph_size);
        void UpdateGraphAttributesNumAuthors(Graph* graph, const std::unordered_map<int, int>& continuous_node_mapping, int* num_authors_arr);


        void LogTime(int current_year, std::string label);
        void LogTime(int current_year, std::string label, int time_elapsed);
        std::chrono::time_point<std::chrono::steady_clock> LocalLogTime(std::vector<std::pair<std::string, int>>& local_parallel_stage_time_vec, std::chrono::time_point<std::chrono::steady_clock> local_prev_time, std::string label);
        void WriteTimingFile(int start_year, int end_year);
        std::unordered_map<int, int> PlantNodes(Graph* graph, double* pa_weight_arr, double* fit_weight_arr, double* num_authors_weight_arr, double* author_reputation_weight_arr, int* out_degree_arr, double* alpha_arr, int* fitness_lag_duration_arr, int* fitness_peak_value_arr, int* fitness_peak_duration_arr, int* num_authors_arr, int* planted_author_id_arr);

        std::vector<int> GetCartelGeneratorNodes(Graph* graph, int author_id);
        void InitializeSeedFitness(Graph* graph) {
            for(auto const& node : graph->GetNodeSet()) {
                int fitness_lag_uniform = 0; // MARK: hard coded to be static fitness
                int fitness_peak_uniform = 1000; // MARK: hard coded to be static fitness
                int fitness_power = 1;

                graph->SetIntAttribute("fitness_lag_duration", node, fitness_lag_uniform);
                graph->SetIntAttribute("fitness_peak_duration", node, fitness_peak_uniform);
                graph->SetIntAttribute("fitness_peak_value", node, fitness_power);
            }
        }


        template<typename T1, typename T2>
        bool ValidateArgument(std::string section, std::string argument_name, T1 argument_value, T2 default_value) {
            if (argument_value == default_value) {
                this->WriteToLogFile("Required parameter '" + argument_name + "' was not found in the '" + section + "' section", Log::error);
                return false;
            }
            if constexpr (std::is_same_v<T1, std::string>) {
                this->WriteToLogFile(argument_name + ": " + argument_value, Log::info);
            } else {
                this->WriteToLogFile(argument_name + ": " + std::to_string(argument_value), Log::info);
            }
            return true;
        }


        template<typename T>
        void AssignFitnessLagDuration(Graph* graph, const T& container) {
            for(auto const& node : container) {
                /* std::random_device rand_dev; */
                /* std::minstd_rand generator{rand_dev()}; */
                pcg_extras::seed_seq_from<std::random_device> rand_dev;
                pcg32 generator(rand_dev);
                int fitness_lag_uniform = this->fitness_lag_duration_uniform_distribution(generator);
                // int fitness_lag_uniform = 0; // MARK: hard coded to be static fitness
                graph->SetIntAttribute("fitness_lag_duration", node, fitness_lag_uniform);
            }
        }

        template<typename T>
        void AssignFitnessPeakDuration(Graph* graph, const T& container) {
            for(auto const& node : container) {
                /* std::random_device rand_dev; */
                /* std::minstd_rand generator{rand_dev()}; */
                pcg_extras::seed_seq_from<std::random_device> rand_dev;
                pcg32 generator(rand_dev);
                int fitness_peak_uniform = this->fitness_peak_duration_uniform_distribution(generator);
                // int fitness_peak_uniform = 1000; // MARK: hard coded to be static fitness
                graph->SetIntAttribute("fitness_peak_duration", node, fitness_peak_uniform);
            }
        }

        template<typename T>
        void AssignPeakFitnessValues(Graph* graph, const T& container) {
            pcg_extras::seed_seq_from<std::random_device> rand_dev;
            pcg32 generator(rand_dev);
            /*
            std::vector<double> fitness_probabilities;
            double fitness_probabilities_sum = 0.0;
            for(int i = this->fitness_value_min; i <  this->fitness_value_max + 1; i ++) {
                double scale_factor = 6.3742991333;
                double constant = 0.072;
                double exponent = -1.634;
                double current_fitness_probability = scale_factor * constant * pow(i, exponent);
                fitness_probabilities.push_back(current_fitness_probability);
                fitness_probabilities_sum += current_fitness_probability;
            }

            for(size_t i = 0; i < fitness_probabilities.size(); i ++) {
                fitness_probabilities[i] /= fitness_probabilities_sum;
            }

            std::discrete_distribution<int> int_discrete_distribution(fitness_probabilities.begin(), fitness_probabilities.end());
            */
            for(auto const& node : container) {
                /*
                int current_fitness = int_discrete_distribution(generator) + 1;
                graph->SetIntAttribute("fitness_peak_value", node, current_fitness);
                */
                /* graph->SetIntAttribute("fitness_peak_value", node, 1); */
                double fitness_uniform = this->fitness_value_uniform_distribution(generator);
                double adjusted_alpha = this->fitness_alpha + 1;
                double base_left = (pow(this->fitness_value_max, adjusted_alpha) - pow(this->fitness_value_min, adjusted_alpha)) * fitness_uniform;
                double base_right = pow(this->fitness_value_min, adjusted_alpha);
                double exponent = 1.0/adjusted_alpha;
                int fitness_power = std::round(pow(base_left + base_right ,exponent));
                graph->SetIntAttribute("fitness_peak_value", node, fitness_power);
            }
        }



    protected:
        std::string edgelist;
        std::string nodelist;
        std::string out_degree_bag;
        std::string recency_table;
        std::string recency_bins;
        double alpha;
        double minimum_alpha;
        bool use_alpha;
        bool start_from_checkpoint;
        std::string planted_nodes;
        double fully_random_citations;
        double preferential_weight;
        double fitness_weight;
        double num_authors_weight;
        double author_reputation_weight;
        int fitness_value_min;
        int fitness_value_max;
        int fitness_lag_duration_min;
        int fitness_lag_duration_max;
        int fitness_peak_duration_min;
        int fitness_peak_duration_max;
        double minimum_preferential_weight;
        double minimum_fitness_weight;
        int in_degree_threshold;
        int fitness_threshold;
        int recency_threshold;
        double non_random_generator_probability;
        double growth_rate;
        int num_cycles;
        double same_year_citations;
        int neighborhood_sample;
        std::string num_authors_bag;
        int author_max_lifetime;
        double cartel_outdegree_proportion;
        std::string output_file;
        std::string auxiliary_information_file;
        std::string log_file;
        int num_processors;
        int log_level;
        std::chrono::time_point<std::chrono::steady_clock> start_time;
        std::ofstream log_file_handle;
        std::ofstream timing_file_handle;
        int num_calls_to_log_write;
        const int fitness_alpha = -3;
        const int fitness_decay_alpha = 3;
        const int gamma = 3;
        const int max_author_lifetime = 30;
        const int k = 2;
        const int recency_limit = 3;
        const int peak_constant = 2;
        const int delay_constant = 500;
        const int max_out_degree = 249;
        int next_author_id = 0;
        int num_bins;
        std::uniform_real_distribution<double> fitness_value_uniform_distribution{0, 1};
        std::uniform_real_distribution<double> weights_uniform_distribution{0, 1};
        std::uniform_real_distribution<double> wrs_uniform_distribution{0, 1};
        std::uniform_real_distribution<double> alpha_uniform_distribution{0, 1};
        std::uniform_int_distribution<int> fitness_lag_duration_uniform_distribution{fitness_lag_duration_min, fitness_lag_duration_max};
        std::uniform_int_distribution<int> fitness_peak_duration_uniform_distribution{fitness_peak_duration_min, fitness_peak_duration_max};
        std::vector<int> out_degree_bag_vec;
        std::vector<int> bin_boundaries;
        /* std::unordered_map<int, double> recency_probabilities_map; */
        std::unordered_map<int, int> recency_counts_map;
        std::unordered_map<int, std::unordered_map<int, std::unordered_map<std::string, std::string>>> planted_nodes_map;
        std::unordered_map<int, std::vector<std::pair<std::string, int>>> timing_map;
        std::chrono::time_point<std::chrono::steady_clock> prev_time;
};

#endif
