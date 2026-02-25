#include "abm.h"
#include <iomanip>
#pragma omp declare reduction(merge_int_pair_vecs : std::vector<std::pair<int, int>> : omp_out.insert(omp_out.end(), omp_in.begin(), omp_in.end()))
#pragma omp declare reduction(merge_str_int_pair_vecs : std::vector<std::pair<std::string, int>> : omp_out.insert(omp_out.end(), omp_in.begin(), omp_in.end()))
#pragma omp declare reduction(merge_int_vecs : std::vector<int> : omp_out.insert(omp_out.end(), omp_in.begin(), omp_in.end()))

#pragma omp declare reduction(custom_merge_vec_int : std::vector<std::pair<int, int>> : omp_out.insert(omp_out.end(), omp_in.begin(), omp_in.end())) initializer(omp_priv = decltype(omp_orig){})

int ABM::WriteToLogFile(std::string message, Log message_type) {
    if(this->log_level >= message_type) {
        std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
        std::string log_message_prefix;
        if(message_type == Log::info) {
            log_message_prefix = "[INFO]";
        } else if(message_type == Log::debug) {
            log_message_prefix = "[DEBUG]";
        } else if(message_type == Log::error) {
            log_message_prefix = "[ERROR]";
        }
        auto days_elapsed = std::chrono::duration_cast<std::chrono::days>(now - this->start_time);
        auto hours_elapsed = std::chrono::duration_cast<std::chrono::hours>(now - this->start_time - days_elapsed);
        auto minutes_elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - this->start_time - days_elapsed - hours_elapsed);
        auto seconds_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - this->start_time - days_elapsed - hours_elapsed - minutes_elapsed);
        auto total_seconds_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - this->start_time);
        log_message_prefix += "[";
        log_message_prefix += std::to_string(days_elapsed.count());
        log_message_prefix += "-";
        log_message_prefix += std::to_string(hours_elapsed.count());
        log_message_prefix += ":";
        log_message_prefix += std::to_string(minutes_elapsed.count());
        log_message_prefix += ":";
        log_message_prefix += std::to_string(seconds_elapsed.count());
        log_message_prefix += "]";

        log_message_prefix += "(t=";
        log_message_prefix += std::to_string(total_seconds_elapsed.count());
        log_message_prefix += "s)";
        this->log_file_handle << log_message_prefix << " " << message << '\n';

        if(this->num_calls_to_log_write % 1 == 0) {
            std::flush(this->log_file_handle);
        }
        this->num_calls_to_log_write ++;
    }
    return 0;
}

void ABM::ReadPlantedNodes() {
    char delimiter = Utils::GetDelimiter(this->planted_nodes);
    std::unordered_map<int, std::string> index_to_header_map = Utils::GetIndexToHeaderMap(delimiter, this->planted_nodes);
    std::unordered_map<std::string, int> header_to_index_map = Utils::GetHeaderToIndexMap(delimiter, this->planted_nodes);
    std::ifstream planted_nodes_stream(this->planted_nodes);
    std::string line;
    int line_no = 1;
    while(std::getline(planted_nodes_stream, line)) {
        std::stringstream ss(line);
        std::string current_value;
        std::vector<std::string> current_line;
        while(std::getline(ss, current_value, delimiter)) {
            current_line.push_back(current_value);
        }
        if(current_line.size() == 0) {
            break;
        }
        if (line_no > 1) {
            int year_header_index = header_to_index_map["year"];
            int current_year = std::stoi(current_line[year_header_index]);
            for(size_t i = 0; i < current_line.size(); i ++) {
                if ((int)i != year_header_index) {
                    this->planted_nodes_map[current_year][line_no][index_to_header_map[i]] = current_line[i];
                }
            }
        }
        line_no += 1;
    }
}

void ABM::ReadOutDegreeBag() {
    char delimiter = ',';
    std::ifstream out_degree_bag_stream(this->out_degree_bag);
    std::string line;
    int line_no = 0;
    while(std::getline(out_degree_bag_stream, line)) {
        std::stringstream ss(line);
        std::string current_value;
        std::vector<std::string> current_line;
        while(std::getline(ss, current_value, delimiter)) {
            current_line.push_back(current_value);
        }
        if(current_line.size() == 0) {
            break;
        }
        if(line_no != 0) {
            this->out_degree_bag_vec.push_back(std::stoi(current_line[1]));
        }
        line_no ++;
    }
}


std::unordered_map<int, double> ABM::GetBinnedRecencyProbabilities() {
    std::unordered_map<int, double> binned_recency_probabilities;
    double binned_recency_sum = 0;
    for(const auto& [year_diff, count] : this->recency_counts_map) {
        int current_bin_index = this->GetBinIndex(year_diff);
        binned_recency_probabilities[current_bin_index] += count;
        binned_recency_sum += count;
    }
    for(const auto& recency_pair : binned_recency_probabilities) {
        binned_recency_probabilities[recency_pair.first] /= binned_recency_sum;
    }
    return binned_recency_probabilities;
}


void ABM::ReadRecencyProbabilities() {
    char delimiter = ',';
    std::ifstream recency_probabilities_stream(this->recency_table);
    std::string line;
    int line_no = 0;
    while(std::getline(recency_probabilities_stream, line)) {
        std::stringstream ss(line);
        std::string current_value;
        std::vector<std::string> current_line;
        while(std::getline(ss, current_value, delimiter)) {
            current_line.push_back(current_value);
        }
        if(current_line.size() == 0) {
            break;
        }
        if(line_no != 0) {
            int integer_year_diff = std::stoi(current_line[0]);
            /* double probability = std::stod(current_line[1]); */
            int count = std::stoi(current_line[1]);
            if(integer_year_diff > 0) {
                this->recency_counts_map[integer_year_diff] = count;
            }
        }
        line_no ++;
    }
}

std::unordered_map<int, int> ABM::BuildContinuousNodeMapping(Graph* graph) {
    int next_node_id = 0;
    std::unordered_map<int, int> continuous_node_mapping;
    for(auto const& node : graph->GetNodeSet()) {
        continuous_node_mapping[node] = next_node_id;
        next_node_id ++;
    }
    return continuous_node_mapping;
}

std::unordered_map<int, int> ABM::ReverseMapping(std::unordered_map<int, int> mapping) {
    std::unordered_map<int, int> reverse_mapping;
    for(auto const& [key,val] : mapping) {
        reverse_mapping[val] = key;
    }
    return reverse_mapping;
}


void ABM::FillInDegreeArr(Graph* graph, const std::unordered_map<int, int>& continuous_node_mapping, int* in_degree_arr) {
    for(auto const& node: graph->GetNodeSet()) {
        if (!continuous_node_mapping.contains(node)) {
            std::cerr << std::to_string(node) << " not in continuous node mapping " << std::endl;
        }
        int continuous_id = continuous_node_mapping.at(node);
        in_degree_arr[continuous_id] = graph->GetInDegree(node);
    }
}

// void ABM::FillNumAuthorsArr(Graph* graph, const std::unordered_map<int, int>& continuous_node_mapping, int* num_authors_arr) {
//     for(auto const& node: graph->GetNodeSet()) {
//         if (!continuous_node_mapping.contains(node)) {
//             std::cerr << std::to_string(node) << " not in continuous node mapping " << std::endl;
//         }
//         int continuous_id = continuous_node_mapping.at(node);
//         num_authors_arr[continuous_id] = graph->GetNextNumAuthors();
//     }
// }

void ABM::FillAuthorReputationArr(Graph* graph, const std::unordered_map<int, int>& continuous_node_mapping, int* author_reputation_arr) {
    graph->ComputeAuthorReputations();
    for(auto const& node: graph->GetNodeSet()) {
        if (!continuous_node_mapping.contains(node)) {
            std::cerr << std::to_string(node) << " not in continuous node mapping " << std::endl;
        }
        int continuous_id = continuous_node_mapping.at(node);
        author_reputation_arr[continuous_id] = graph->GetAuthorReputationForNode(node);
    }
}

void ABM::InitializeFitness(Graph* graph) {
    this->AssignPeakFitnessValues(graph, graph->GetNodeSet());
    this->AssignFitnessLagDuration(graph, graph->GetNodeSet());
    this->AssignFitnessPeakDuration(graph, graph->GetNodeSet());
}

std::unordered_map<int, int> ABM::PlantNodes(Graph* graph, double* pa_weight_arr, double* fit_weight_arr, double* num_authors_weight_arr, double* author_reputation_weight_arr, int* out_degree_arr, double* alpha_arr, int* fitness_lag_duration_arr, int* fitness_peak_value_arr, int* fitness_peak_duration_arr, int* num_authors_arr) {
    int current_graph_size = graph->GetNodeSet().size();
    int initial_graph_size = current_graph_size;
    const std::unordered_map<std::string, std::pair<std::string, void*>> column_header_to_type_arr_map = {
        {"pa_weight", {"double", (void*)pa_weight_arr}},
        {"fit_weight", {"double", (void*)fit_weight_arr}},
        {"num_authors_weight", {"double", (void*)num_authors_weight_arr}},
        {"author_reputation_weight", {"double", (void*)author_reputation_weight_arr}},
        {"out_degree", {"int", (void*)out_degree_arr}},
        {"alpha", {"double", (void*)alpha_arr}},
        {"fit_lag_duration", {"int", (void*)fitness_lag_duration_arr}},
        {"fit_peak_value", {"int", (void*)fitness_peak_value_arr}},
        {"fit_peak_duration", {"int", (void*)fitness_peak_duration_arr}},
        {"num_authors", {"int", (void*)(num_authors_arr + initial_graph_size)}}
    };
    std::unordered_map<int, int> planted_nodes_line_number_map;
    pcg_extras::seed_seq_from<std::random_device> rand_dev;
    pcg32 generator(rand_dev);
    int previous_graph_size = 0;
    for(int current_relative_year = 0; current_relative_year < this->num_cycles + 1; current_relative_year ++) {
        int num_new_nodes = std::ceil(current_graph_size * this->growth_rate);
        if (this->planted_nodes_map.count(current_relative_year)) {
            std::unordered_set<int> selected;
            std::uniform_int_distribution<int> new_nodes_distribution{previous_graph_size, current_graph_size - 1};
            std::unordered_map<int, std::unordered_map<std::string, std::string>> current_year_map = this->planted_nodes_map.at(current_relative_year);
            for(auto const& [line_no, line_map] : current_year_map) {
                int chosen_agent_index = new_nodes_distribution(generator);
                while(selected.contains(chosen_agent_index)) {
                    chosen_agent_index = new_nodes_distribution(generator);
                }
                selected.insert(chosen_agent_index);
                planted_nodes_line_number_map[chosen_agent_index - initial_graph_size] = line_no;
                for(auto const& [planted_feature_name, planted_feature_value] : line_map) {
                    std::string current_variable_type = column_header_to_type_arr_map.at(planted_feature_name).first;
                    void* current_variable_arr = column_header_to_type_arr_map.at(planted_feature_name).second;
                    if (current_variable_type == "double") {
                        ((double*)current_variable_arr)[chosen_agent_index - initial_graph_size] = std::stod(planted_feature_value);
                    } else if (current_variable_type == "int") {
                        ((int*)current_variable_arr)[chosen_agent_index - initial_graph_size] = std::stoi(planted_feature_value);
                    }
                }
                // std::string current_fitness_lag_duration = line_map.at("fitness_lag_duration");
                // std::string current_fitness_peak_value = line_map.at("fitness_peak_value");
                // std::string current_fitness_peak_duration = line_map.at("fitness_peak_duration");
                // std::string current_preferential_attachment_weight = line_map.at("pa_weight");
                // std::string current_fitness_weight = line_map.at("fit_weight");
                // std::string current_num_authors_weight = line_map.at("num_authors_weight");
                // std::string current_author_reputation_weight = line_map.at("author_reputation_weight");
                // std::string current_alpha = line_map.at("alpha");
                // std::string current_out_degree = line_map.at("out_degree");
                // while(selected.contains(chosen_agent_index)) {
                //     chosen_agent_index = new_nodes_distribution(generator);
                // }
                // selected.insert(chosen_agent_index);
                // if (current_fitness_lag_duration != "") {
                //     fitness_lag_duration_arr[chosen_agent_index - initial_graph_size] = std::stoi(current_fitness_lag_duration);
                // }
                // if (current_fitness_peak_value != "") {
                //     fitness_peak_value_arr[chosen_agent_index - initial_graph_size] = std::stoi(current_fitness_peak_value);
                // }
                // if (current_fitness_peak_duration != "") {
                //     fitness_peak_duration_arr[chosen_agent_index - initial_graph_size] = std::stoi(current_fitness_peak_duration);
                // }
                // if (current_preferential_attachment_weight != "") {
                //     pa_weight_arr[chosen_agent_index - initial_graph_size] = std::stod(current_preferential_attachment_weight);
                // }
                // if (current_fitness_weight != "") {
                //     fit_weight_arr[chosen_agent_index - initial_graph_size] = std::stod(current_fitness_weight);
                // }
                // if (current_num_authors_weight != "") {
                //     num_authors_weight_arr[chosen_agent_index - initial_graph_size] = std::stod(current_num_authors_weight);
                // }
                // if (current_author_reputation_weight != "") {
                //     author_reputation_weight_arr[chosen_agent_index - initial_graph_size] = std::stod(current_author_reputation_weight);
                // }
                // if (current_out_degree != "") {
                //     out_degree_arr[chosen_agent_index - initial_graph_size] = std::stoi(current_out_degree);
                // }
                // if (current_alpha != "") {
                //     alpha_arr[chosen_agent_index - initial_graph_size] = std::stod(current_alpha);
                // }
                // planted_nodes_line_number_map[chosen_agent_index - initial_graph_size] = line_no;
            }
        }
        previous_graph_size = current_graph_size;
        current_graph_size += num_new_nodes;
    }
    return planted_nodes_line_number_map;
}

void ABM::FillFitnessArr(Graph* graph, const std::unordered_map<int, int>& continuous_node_mapping, int current_year, int* fitness_arr) {
    for(auto const& node : graph->GetNodeSet()) {
        int fitness_peak_value = graph->GetIntAttribute("fitness_peak_value", node);
        int fitness_lag_duration = graph->GetIntAttribute("fitness_lag_duration", node);
        int fitness_peak_duration = graph->GetIntAttribute("fitness_peak_duration", node);
        int published_year = graph->GetIntAttribute("year", node);
        int continuous_index = continuous_node_mapping.at(node);
        if (published_year + fitness_lag_duration > current_year) {
            fitness_arr[continuous_index] = 1;
        } else if (published_year + fitness_lag_duration + fitness_peak_duration >= current_year) {
            fitness_arr[continuous_index] = fitness_peak_value;
        } else {
            double decayed_fitness_value = fitness_peak_value / pow(current_year - published_year - fitness_lag_duration - fitness_peak_duration + 1, this->fitness_decay_alpha);
            fitness_arr[continuous_index] = decayed_fitness_value;
        }
    }
}

int ABM::GetMaxYear(Graph* graph) {
    int max_year = -1;
    bool is_first = true;
    for(auto const& node : graph->GetNodeSet()) {
        int current_node_year = graph->GetIntAttribute("year", node);
        if (is_first) {
            max_year = current_node_year;
            is_first = false;
        }
        if (current_node_year > max_year) {
            max_year = current_node_year;
        }
    }
    return max_year;
}

int ABM::GetMaxNode(Graph* graph) {
    int max_node = -1;
    bool is_first = true;
    for(auto const& node : graph->GetNodeSet()) {
        if (is_first) {
            max_node = node;
            is_first = false;
        }
        if (node > max_node) {
            max_node = node;
        }
    }
    return max_node;
}

int ABM::GetFinalGraphSize(Graph* graph) {
    int current_graph_size = graph->GetNodeSet().size();
    for(int i = 0; i < this->num_cycles; i ++) {
        int num_new_nodes = std::ceil(current_graph_size * this->growth_rate);
        current_graph_size += num_new_nodes;
    }
    return current_graph_size;
}

void ABM::PopulateWeightArrs(double* pa_weight_arr, double* fit_weight_arr, double* num_authors_weight_arr, double* author_reputation_weight_arr, int len) {
    pcg_extras::seed_seq_from<std::random_device> rand_dev;
    pcg32 generator(rand_dev);
    if(this->preferential_weight != -1 &&  this->fitness_weight != -1 && this->num_authors_weight != -1 && this->author_reputation_weight != -1) {
        for(int i = 0; i < len; i ++) {
            double pa_uniform = this->preferential_weight;
            double fit_uniform = this->fitness_weight;
            double num_authors_uniform = this->num_authors_weight;
            double author_reputation_uniform = this->author_reputation_weight;
            double sum = pa_uniform + fit_uniform + num_authors_uniform + author_reputation_uniform;
            pa_weight_arr[i] = (double)pa_uniform / sum;
            fit_weight_arr[i] = (double)fit_uniform / sum;
            num_authors_weight_arr[i] = (double)num_authors_uniform / sum;
            author_reputation_weight_arr[i] = (double)author_reputation_uniform / sum;
        }
    } else {
        for(int i = 0; i < len; i ++) {
            std::uniform_real_distribution<double> weights_uniform_distribution{0, 1};
            double pa_uniform = weights_uniform_distribution(generator);
            double fit_uniform = weights_uniform_distribution(generator);
            double num_authors_uniform = weights_uniform_distribution(generator);
            double author_reputation_uniform = weights_uniform_distribution(generator);
            double sum = pa_uniform + fit_uniform + num_authors_uniform + author_reputation_uniform;
            pa_weight_arr[i] = (double)pa_uniform / sum;
            fit_weight_arr[i] = (double)fit_uniform / sum;
            num_authors_weight_arr[i] = (double)num_authors_uniform / sum;
            author_reputation_weight_arr[i] = (double)author_reputation_uniform / sum;
            /*
            std::uniform_real_distribution<double> first_weights_uniform_distribution{0, 1};
            double first_uniform = first_weights_uniform_distribution(generator);
            std::vector<double> current_weight_array{first_uniform, 1 - first_uniform};
            std::ranges::shuffle(current_weight_array, generator);
            std::ranges::shuffle(current_weight_array, generator);
            std::ranges::shuffle(current_weight_array, generator);
            for(size_t j = 0; j < current_weight_array.size(); j ++) {
                current_weight_array[j] = std::round(current_weight_array[j] * 1000.0) / 1000.0;
            }
            pa_weight_arr[i] = current_weight_array[0];
            fit_weight_arr[i] = current_weight_array[1];
            */
        }
    }
}

void ABM::PopulateNumAuthorsArr(Graph* graph, int* num_authors_arr, int len) {
    for(int i = 0; i < len; i ++) {
        num_authors_arr[i] = graph->GetNextNumAuthors();
    }
}

void ABM::PopulateFitnessArrs(int* fitness_lag_duration_arr, int* fitness_peak_value_arr, int* fitness_peak_duration_arr, int len) {
    pcg_extras::seed_seq_from<std::random_device> rand_dev;
    pcg32 generator(rand_dev);
    for(int i = 0; i < len; i ++) {
        fitness_lag_duration_arr[i] = this->fitness_lag_duration_uniform_distribution(generator);
        double fitness_uniform = this->fitness_value_uniform_distribution(generator);
        double adjusted_alpha = this->fitness_alpha + 1;
        double base_left = (pow(this->fitness_value_max, adjusted_alpha) - pow(this->fitness_value_min, adjusted_alpha)) * fitness_uniform;
        double base_right = pow(this->fitness_value_min, adjusted_alpha);
        double exponent = 1.0/adjusted_alpha;
        int fitness_power = pow(base_left + base_right ,exponent);
        fitness_peak_value_arr[i] = fitness_power;
        fitness_peak_duration_arr[i] = this->fitness_peak_duration_uniform_distribution(generator);
    }
}

void ABM::PopulateAlphaArr(double* alpha_arr, int len) {
    pcg_extras::seed_seq_from<std::random_device> rand_dev;
    pcg32 generator(rand_dev);

    if(!this->use_alpha) {
        for(int i = 0; i < len; i ++) {
            alpha_arr[i] = -1;
        }
    } else if(this->alpha == -1) {
        for(int i = 0; i < len; i ++) {
            double alpha_uniform = this->alpha_uniform_distribution(generator);
            alpha_uniform = std::round(alpha_uniform * 1000.0) / 1000.0;
            alpha_arr[i] = alpha_uniform;
        }
    } else if(this->minimum_alpha > 0) {
        for(int i = 0; i < len; i ++) {
            std::uniform_real_distribution<double> minimum_alpha_uniform_distribution{minimum_alpha, 1};
            double alpha_uniform = minimum_alpha_uniform_distribution(generator);
            alpha_arr[i] = alpha_uniform;
        }
    } else {
        for(int i = 0; i < len; i ++) {
            alpha_arr[i] = this->alpha;
        }
    }
}

void ABM::PopulateOutDegreeArr(int* out_degree_arr, int len) {
    std::uniform_int_distribution<int> outdegree_index_uniform_distribution{0, (int)(this->out_degree_bag_vec.size() - 1)};
    pcg_extras::seed_seq_from<std::random_device> rand_dev;
    pcg32 generator(rand_dev);
    for(int i = 0; i < len; i ++) {
        int index_uniform = outdegree_index_uniform_distribution(generator);
        out_degree_arr[i] = this->out_degree_bag_vec[index_uniform];
    }
}

void ABM::UpdateGraphAttributesWeights(Graph* graph, int next_node_id, double* pa_weight_arr, double* fit_weight_arr, double* num_authors_weight_arr, double* author_reputation_weight_arr, int len) {
    for(int i = 0; i < len; i ++) {
        int current_node_id = next_node_id + i;
        graph->SetDoubleAttribute("pa_weight", current_node_id, pa_weight_arr[i]);
        graph->SetDoubleAttribute("fit_weight", current_node_id, fit_weight_arr[i]);
        graph->SetDoubleAttribute("num_authors_weight", current_node_id, num_authors_weight_arr[i]);
        graph->SetDoubleAttribute("author_reputation_weight", current_node_id, author_reputation_weight_arr[i]);
    }
}

void ABM::UpdateGraphAttributesNumAuthors(Graph* graph, const std::unordered_map<int, int>& continuous_node_mapping, int* num_authors_arr) {
    for(auto const& node_id : graph->GetNodeSet()) {
        int continuous_id = continuous_node_mapping.at(node_id);
        graph->SetIntAttribute("num_authors", node_id, num_authors_arr[continuous_id]);
    }
}

void ABM::UpdateGraphAttributesInitialAuthorReputations(Graph* graph, const std::vector<int>& new_nodes_vec) {
    for(size_t i = 0; i < new_nodes_vec.size(); i ++) {
        int current_node_id = new_nodes_vec.at(i);
        // int current_weight_arr_index = continuous_node_mapping.at(current_node_id) - initial_graph_size;
        // int continuous_id = continuous_node_mapping.at(current_node_id);
        // if (author_reputation_arr[continuous_id] > 10000 || author_reputation_arr[continuous_id] < 0) {
        //     std::cerr << "author reputation for node " << current_node_id << std::endl;
        // }
        int current_author_reputation = graph->GetAuthorReputationForNode(current_node_id);
        graph->SetIntAttribute("initial_author_reputation", current_node_id, current_author_reputation);
    }
}

void ABM::UpdateGraphAttributesFitnesses(Graph* graph, const std::vector<int>& new_nodes_vec, const std::unordered_map<int,int>& continuous_node_mapping, int* fitness_lag_duration_arr, int* fitness_peak_value_arr, int* fitness_peak_duration_arr, int initial_graph_size) {
    for(size_t i = 0; i < new_nodes_vec.size(); i ++) {
        int current_node_id = new_nodes_vec.at(i);
        int current_weight_arr_index = continuous_node_mapping.at(current_node_id) - initial_graph_size;
        graph->SetIntAttribute("fitness_lag_duration", current_node_id, fitness_lag_duration_arr[current_weight_arr_index]);
        graph->SetIntAttribute("fitness_peak_value", current_node_id, fitness_peak_value_arr[current_weight_arr_index]);
        graph->SetIntAttribute("fitness_peak_duration", current_node_id, fitness_peak_duration_arr[current_weight_arr_index]);
    }
}

void ABM::UpdateGraphAttributesPlantedNodesLineNumbers(Graph* graph, int next_node_id, const std::unordered_map<int, int>& planted_nodes_line_number_map) {
    for(auto const& [weight_arr_index, line_no] : planted_nodes_line_number_map) {
        int current_node_id = next_node_id + weight_arr_index;
        graph->SetIntAttribute("planted_nodes_line_number", current_node_id, line_no);
    }
}

void ABM::UpdateGraphAttributesAlphas(Graph* graph, int next_node_id, double* alpha_arr, int len) {
    for(int i = 0; i < len; i ++) {
        int current_node_id = next_node_id + i;
        graph->SetDoubleAttribute("alpha", current_node_id, alpha_arr[i]);
    }
}

void ABM::UpdateGraphAttributesOutDegrees(Graph* graph, int next_node_id, int* out_degree_arr, int len) {
    for(int i = 0; i < len; i ++) {
        int current_node_id = next_node_id + i;
        graph->SetIntAttribute("assigned_out_degree", current_node_id, out_degree_arr[i]);
    }
}

std::vector<int> ABM::GetGraphAttributesGeneratorNodes(Graph* graph, int new_node) const {
    std::vector<int> generator_nodes;
    std::string generator_node_string = graph->GetStringAttribute("generator_node_string", new_node);
    std::stringstream ss(generator_node_string);
    std::string current_value;
    while(std::getline(ss, current_value, ';')) {
        generator_nodes.push_back(std::stoi(current_value));
    }
    return generator_nodes;
}

void ABM::UpdateGraphAttributesAuthors(Graph* graph, int new_node, int author_id) {
        graph->SetIntAttribute("author_id", new_node, author_id);
        // graph->SetIntAttribute("num_authors", new_node, num_authors);
        graph->UpdateAuthorPublicationMap(author_id, new_node);
}

void ABM::UpdateGraphAttributesGeneratorNodes(Graph* graph, int new_node, const std::vector<int>& generator_nodes) {
    std::string generator_node_string;
    generator_node_string += std::to_string(generator_nodes.at(0));
    for(size_t i = 1; i < generator_nodes.size(); i ++) {
        generator_node_string += ";";
        generator_node_string += std::to_string(generator_nodes.at(i));
    }
    graph->SetStringAttribute("generator_node_string", new_node, generator_node_string);
}

void ABM::CalculateTanhScores(std::unordered_map<int, double>& cached_results, int* src_arr, double* dst_arr, int len) {
    double sum = 0;
    #pragma omp parallel for reduction(+:sum)
    for(int i = 0; i < len; i ++) {
        double current_dst = -1;
        if (src_arr[i] < 10000) {
            current_dst = cached_results[src_arr[i]];
        } else {
            current_dst = this->peak_constant * std::tanh((pow(src_arr[i], 3)/this->delay_constant)*(1/this->peak_constant));
            /* current_dst = (src_arr[i] * this->gamma) + 1; */
        }
        dst_arr[i] = current_dst;
        sum += current_dst;
    }
    #pragma omp parallel for
    for(int i = 0; i < len; i ++) {
        dst_arr[i] /= sum;
    }
}

void ABM::CalculateExpScores(std::unordered_map<int, double>& cached_results, int* src_arr, double* dst_arr, int len) {
    double sum = 0;
    #pragma omp parallel for reduction(+:sum)
    for(int i = 0; i < len; i ++) {
        double current_dst = std::max(pow(src_arr[i], this->gamma), 1.0) + 1;
        dst_arr[i] = current_dst;
        sum += current_dst;
    }
    #pragma omp parallel for
    for(int i = 0; i < len; i ++) {
        dst_arr[i] /= sum;
    }
}

void ABM::FillSameYearSourceNodes(std::set<int>& same_year_source_nodes, int current_year_new_nodes) {
    size_t num_same_year_source_nodes = (size_t)std::floor(current_year_new_nodes * this->same_year_citations);
    pcg_extras::seed_seq_from<std::random_device> rand_dev;
    pcg32 generator(rand_dev);
    std::uniform_int_distribution<int> int_uniform_distribution(0, current_year_new_nodes - 1);
    while(same_year_source_nodes.size() != num_same_year_source_nodes) {
        int current_source = int_uniform_distribution(generator);
        if (same_year_source_nodes.count(current_source) == 0) {
            same_year_source_nodes.insert(current_source);
        }
    }
}

int ABM::MakeSameYearCitations(const std::set<int>& same_year_source_nodes, int num_new_nodes, const std::unordered_map<int, int>& reverse_continuous_node_mapping, int* citations, int current_graph_size) {
    pcg_extras::seed_seq_from<std::random_device> rand_dev;
    pcg32 generator(rand_dev);
    std::uniform_int_distribution<int> int_uniform_distribution(0, num_new_nodes - 1);
    int current_citation = int_uniform_distribution(generator);
    while(same_year_source_nodes.contains(current_citation)) {
        current_citation = int_uniform_distribution(generator);
    }
    citations[0] = reverse_continuous_node_mapping.at(current_graph_size + current_citation);
    return 1;
}

int ABM::MakeUniformRandomCitationsFromGraph(Graph* graph, const std::unordered_map<int, int>& reverse_continuous_node_mapping, std::vector<int>& generator_nodes, int* citations, int num_cited_so_far, int num_citations) {
    if (num_citations <= 0) {
        return 0;
    }
    /* std::cerr << "uniformm random citaitons to graph: " << std::to_string(num_cited_so_far) << " cited so far, " << std::to_string(num_citations) << " citations to graph requested" << std::endl; */
    int actual_num_cited = num_citations;
    std::set<int> selected;
    for(int i = 0; i < num_cited_so_far; i ++) {
        selected.insert(citations[i]);
    }
    for(size_t i = 0; i < generator_nodes.size(); i ++) {
        selected.insert(generator_nodes.at(i));
    }
    if ((int)graph->GetNodeSet().size() - (int)selected.size() <= num_citations) {
        actual_num_cited = (int)graph->GetNodeSet().size() - (int)selected.size();
        /* for(int i = 0; i < actual_num_cited; i ++) { */
        int current_citation_index = 0;
        for(auto const& node_id : graph->GetNodeSet()) {
            /* int current_cited_node = reverse_continuous_node_mapping.at(i); */
            if (!selected.contains(node_id)) {
                citations[num_cited_so_far + current_citation_index] = node_id;
                selected.insert(node_id);
                current_citation_index ++;
            }
        }
    } else {
        pcg_extras::seed_seq_from<std::random_device> rand_dev;
        pcg32 generator(rand_dev);
        std::uniform_int_distribution<int> int_uniform_distribution(0, (int)(graph->GetNodeSet().size() - 1));
        int current_citation_index = 0;
        /* while(selected.size() != num_cited_so_far + generator_nodes.size() + actual_num_cited) { */
        while(current_citation_index < actual_num_cited) {
            int current_citation = int_uniform_distribution(generator);
            int current_cited_node = reverse_continuous_node_mapping.at(current_citation);
            if (current_cited_node < 0) {
                std::cerr << "randomly selected negative node: " << std::to_string(current_cited_node) << " from continous index: " << std::to_string(current_citation) << std::endl;
            }
            if (!selected.contains(current_cited_node)) {
                citations[num_cited_so_far + current_citation_index] = current_cited_node;
                selected.insert(current_cited_node);
                current_citation_index ++;
            }
        }
    }
    /* std::cerr << std::to_string(actual_num_cited) << " nodes cited" << std::endl; */
    return actual_num_cited;
}

int ABM::MakeUniformRandomCitations(Graph* graph, const std::unordered_map<int, int>& continuous_node_mapping, int current_year, const std::vector<int>& candidate_nodes, int* citations, int current_graph_size, int num_citations) {
    if (num_citations <= 0) {
        return 0;
    }
    if (candidate_nodes.size() <= 0) {
        return 0;
    }

    int actual_num_cited = num_citations;
    if (candidate_nodes.size() <= (size_t)num_citations) {
        actual_num_cited = candidate_nodes.size();
        for(int i = 0; i < actual_num_cited; i ++) {
            citations[i] = candidate_nodes.at(i);
        }
    } else {
        pcg_extras::seed_seq_from<std::random_device> rand_dev;
        pcg32 generator(rand_dev);
        std::uniform_int_distribution<int> int_uniform_distribution(0, (int)(candidate_nodes.size() - 1));
        std::set<int> selected;
        int current_citation_index = 0;
        while((int)selected.size() != actual_num_cited) {
            int current_selected_index = int_uniform_distribution(generator);
            int current_node = candidate_nodes.at(current_selected_index);
            if (!selected.contains(current_node)) {
                citations[current_citation_index] = current_node;
                selected.insert(current_node);
                current_citation_index ++;
            }
        }
    }
    return actual_num_cited;
}

int ABM::MakeCitations(Graph* graph, const std::unordered_map<int, int>& continuous_node_mapping, int current_year, const std::vector<int>& candidate_nodes, int* citations, double* pa_arr, double* fit_arr, double* na_arr, double* ar_arr, double pa_weight, double fit_weight, double num_authors_weight, double author_reputation_weight, int current_graph_size, int num_citations) {
    if (num_citations <= 0) {
        return 0;
    }
    if (candidate_nodes.size() <= 0) {
        return 0;
    }
    int actual_num_cited = num_citations;
    if (candidate_nodes.size() < (size_t)num_citations) {
        actual_num_cited = candidate_nodes.size();
    }
    std::vector<std::pair<double, int>> element_index_vec;
    pcg_extras::seed_seq_from<std::random_device> rand_dev;
    pcg32 generator(rand_dev);

    /* begin weighted random sampling results */
    Eigen::MatrixXd current_scores(candidate_nodes.size(), 4);
    Eigen::Vector4d current_weights(pa_weight, fit_weight, num_authors_weight, author_reputation_weight);
    double pa_sum = 0.0;
    double fit_sum = 0.0;
    double na_sum = 0.0;
    double ar_sum = 0.0;
    std::vector<double> raw_pa_arr;
    std::vector<double> raw_fit_arr;
    std::vector<double> raw_na_arr;
    std::vector<double> raw_ar_arr;
    raw_pa_arr.reserve(candidate_nodes.size());
    raw_fit_arr.reserve(candidate_nodes.size());
    raw_na_arr.reserve(candidate_nodes.size());
    raw_ar_arr.reserve(candidate_nodes.size());
    for(size_t i = 0; i < candidate_nodes.size(); i ++) {
        int continuous_node_id = continuous_node_mapping.at(candidate_nodes.at(i));
        double current_pa = pa_arr[continuous_node_id];
        double current_fit = fit_arr[continuous_node_id];
        double current_na = na_arr[continuous_node_id];
        double current_ar = ar_arr[continuous_node_id];
        pa_sum += current_pa;
        fit_sum += current_fit;
        na_sum += current_na;
        ar_sum += current_ar;
        raw_pa_arr.push_back(current_pa);
        raw_fit_arr.push_back(current_fit);
        raw_na_arr.push_back(current_na);
        raw_ar_arr.push_back(current_ar);
    }
    for(size_t i = 0; i < candidate_nodes.size(); i ++) {
        current_scores(i, 0) = raw_pa_arr[i] / pa_sum;
        current_scores(i, 1) = raw_fit_arr[i] / fit_sum;
        current_scores(i, 2) = raw_na_arr[i] / na_sum;
        current_scores(i, 3) = raw_ar_arr[i] / ar_sum;
    }
    Eigen::MatrixXd current_weighted_scores = current_scores * current_weights;
    // double u = log(current_pa) + log(pa_weight);
    // double v = log(current_rec) + log(rec_weight);
    // double w = log(current_fit) + log(fit_weight);
    // double max_value = std::max(u, std::max(v, w));
    // current_scores[i] = max_value + log(exp(u - max_value) + exp(v - max_value) + exp(w - max_value));
    // Eigen::ArrayXd current_weighted_scores(candidate_nodes.size());
    // current_scores = current_scores.array().log();
    // current_weights = current_weights.array().log();
    // for(size_t i = 0; i < candidate_nodes.size(); i ++) {
        // double u = log(current_scores(i, 0)) + log(current_weights(0));
        // double v = log(current_scores(i, 1)) + log(current_weights(1));
        // double u = current_scores(i, 0) + current_weights(0);
        // double v = current_scores(i, 1) + current_weights(1);
        // double max_value = std::max(u, v);
        // current_weighted_scores(i) = max_value + log(exp(u - max_value) + exp(v - max_value));
    // }


    auto current_wrs_uniform = [&] () {return wrs_uniform_distribution(generator);};
    Eigen::ArrayXd current_bases = Eigen::ArrayXd::NullaryExpr(candidate_nodes.size(), current_wrs_uniform);
    // Eigen::ArrayXd weighted_random_sampling_results = current_bases.pow(1.0 / current_weighted_scores.array());
    Eigen::ArrayXd weighted_random_sampling_results = current_bases.log() / current_weighted_scores.array();
    /* end weighted random sampling results */


    for(size_t i = 0; i < candidate_nodes.size(); i ++) {
        element_index_vec.push_back({weighted_random_sampling_results(i), candidate_nodes.at(i)});
    }
    std::ranges::shuffle(element_index_vec, generator);
    /* std::sort(element_index_vec.begin(), element_index_vec.end(), [](auto& left, auto& right){ */
    std::partial_sort(element_index_vec.begin(), element_index_vec.begin() + actual_num_cited, element_index_vec.end(), [](auto& left, auto& right){
        return left.first > right.first; // read
    });
    for (int i = 0; i < actual_num_cited; i ++) {
        citations[i] = element_index_vec[i].second;
    }
    //*/

    return actual_num_cited;
}

std::vector<int> ABM::GetEligibleGeneratorNodes(Graph* graph, int graph_size, const std::unordered_map<int, int>& reverse_continuous_node_mapping, int* in_degree_arr, int* fitness_arr, int in_degree_threshold, int fitness_threshold, int start_year, int current_year, int recency_threshold) {
    std::vector<std::pair<double, int>> in_degree_eligible_generator_nodes;
    std::vector<std::pair<double, int>> fitness_eligible_generator_nodes;
    std::vector<int> eligible_generator_nodes;
    // std::cerr << "current year is " << current_year << " for a simulation where first year agents have year " << start_year << std::endl;
    if (current_year - start_year <= recency_threshold) {
        // std::cerr << "starting eligible check by scanning the whole array" << std::endl;
        for(int i = graph_size - 1; i >= 0; i --) {
            int current_node_id = reverse_continuous_node_mapping.at(i);
            if ((current_year - graph->GetIntAttribute("year", current_node_id)) > recency_threshold) {
                // std::cerr << "node too old, born in year " << graph->GetIntAttribute("year", current_node_id) << std::endl;
            } else {
                in_degree_eligible_generator_nodes.push_back({in_degree_arr[i], current_node_id});
                if (graph->GetStringAttribute("type", current_node_id) == "seed") {
                    fitness_eligible_generator_nodes.push_back({this->fitness_value_max, current_node_id});
                } else {
                    fitness_eligible_generator_nodes.push_back({fitness_arr[i], current_node_id});
                }
                // std::cerr << "adding node" << std::endl;
            }
        }
    } else {
        // std::cerr << "starting eligible check by only scanning the past " << recency_threshold << " years from the back" << std::endl;
        for(int i = graph_size - 1; i >= 0; i --) {
            int current_node_id = reverse_continuous_node_mapping.at(i);
            if ((current_year - graph->GetIntAttribute("year", current_node_id)) > recency_threshold) {
                // std::cerr << "these nodes are too old since they were born in year " << graph->GetIntAttribute("year", current_node_id) << ". breaking." << std::endl;
                break;
            }
            in_degree_eligible_generator_nodes.push_back({in_degree_arr[i], current_node_id});
            fitness_eligible_generator_nodes.push_back({fitness_arr[i], current_node_id});
        }
    }
    int in_degree_top_n_nodes_index = (int)((in_degree_eligible_generator_nodes.size() - 1) * ((double)in_degree_threshold / 100));
    int fitness_top_n_nodes_index = (int)((fitness_eligible_generator_nodes.size() - 1) * ((double)fitness_threshold / 100));

    // std::cerr << "looking for top " << in_degree_threshold << " percentile in-degree and top " << fitness_threshold << " percentile fitness nodes" << " in the past " << recency_threshold << " years" << std::endl;
    // std::cerr << "there are " << in_degree_eligible_generator_nodes.size() << " nodes that fit by in-degree and we look for the top " << in_degree_top_n_nodes_index << " nodes" << std::endl;
    // std::cerr << "there are " << fitness_eligible_generator_nodes.size() << " nodes that fit by fitness and we look for the top " << fitness_top_n_nodes_index << " nodes" << std::endl;

    /* std::partial_sort(in_degree_eligible_generator_nodes.begin(), in_degree_eligible_generator_nodes.begin() + in_degree_top_n_nodes_index, in_degree_eligible_generator_nodes.end(), [](auto& left, auto& right){ */
    /*     return left.first > right.first; // read */
    /* }); */
    /* std::cerr << "in-degree sorted" << std::endl; */
    /* std::partial_sort(fitness_eligible_generator_nodes.begin(), fitness_eligible_generator_nodes.begin() + fitness_top_n_nodes_index, fitness_eligible_generator_nodes.end(), [](auto& left, auto& right){ */
    /*     return left.first > right.first; // read */
    /* }); */
    /* std::cerr << "fitness sorted" << std::endl; */
    std::nth_element(in_degree_eligible_generator_nodes.begin(), in_degree_eligible_generator_nodes.begin() + in_degree_top_n_nodes_index, in_degree_eligible_generator_nodes.end(), [](auto& left, auto& right){
        return left.first > right.first; // read
    });
    // std::cerr << "in-degree sorted" << std::endl;
    std::nth_element(fitness_eligible_generator_nodes.begin(), fitness_eligible_generator_nodes.begin() + fitness_top_n_nodes_index, fitness_eligible_generator_nodes.end(), [](auto& left, auto& right){
        return left.first > right.first; // read
    });
    // std::cerr << "fitness sorted" << std::endl;

    /* if (in_degree_top_n_nodes_index < fitness_top_n_nodes_index) { */
    /*     std::set<int> eligible_fitness_node_ids; */
    /*     for (size_t i = 0; i < fitness_top_n_nodes_index; i ++) { */
    /*         eligible_fitness_node_ids.insert(fitness_eligible_generator_nodes.at(i).second); */
    /*     } */
    /*     for (size_t i = 0; i < in_degree_top_n_nodes_index; i ++) { */
    /*         int current_node_id = in_degree_eligible_generator_nodes.at(i).second; */
    /*         if (eligible_fitness_node_ids.contains(current_node_id)) { */
    /*             eligible_generator_nodes.push_back(current_node_id); */
    /*         } */
    /*     } */
    /* } */
    /* } else { */
    std::set<int> eligible_in_degree_node_ids;
    std::set<int> eligible_fitness_node_ids;
    for (size_t i = 0; i < in_degree_eligible_generator_nodes.size(); i ++) {
        if (in_degree_eligible_generator_nodes.at(i).first >= in_degree_eligible_generator_nodes.at(in_degree_top_n_nodes_index).first) {
            eligible_in_degree_node_ids.insert(in_degree_eligible_generator_nodes.at(i).second);
        }
        if (fitness_eligible_generator_nodes.at(i).first >= fitness_eligible_generator_nodes.at(fitness_top_n_nodes_index).first) {
            eligible_fitness_node_ids.insert(fitness_eligible_generator_nodes.at(i).second);
        }
    }
    /* for (size_t i = 0; i < in_degree_top_n_nodes_index; i ++) { */
    /*     eligible_in_degree_node_ids.insert(in_degree_eligible_generator_nodes.at(i).second); */
    /* } */
    /* for (size_t i = 0; i < fitness_top_n_nodes_index; i ++) { */
    /*     eligible_fitness_node_ids.insert(fitness_eligible_generator_nodes.at(i).second); */
    /* } */
    // std::cerr << "We lookd for them and got " << eligible_in_degree_node_ids.size() << " in-degreee eligible node ids and " << eligible_fitness_node_ids.size() << " fitness eligible node ids" << std::endl;
    std::set_intersection(eligible_in_degree_node_ids.begin(), eligible_in_degree_node_ids.end(), eligible_fitness_node_ids.begin(), eligible_fitness_node_ids.end(), std::back_inserter(eligible_generator_nodes));
    // std::cerr << "there are " << eligible_generator_nodes.size() << " nodes in the intersection" << std::endl;
    // only use in-degree if intersection 0?
    return eligible_generator_nodes;
}

std::vector<int> ABM::GetGeneratorNodesFromSet(std::vector<int>& eligible_generator_nodes) {
    std::vector<int> generator_nodes;
    std::uniform_int_distribution<int> generator_uniform_distribution{0, (int)(eligible_generator_nodes.size() - 1)};
    int num_generator_nodes = 1;
    pcg_extras::seed_seq_from<std::random_device> rand_dev;
    pcg32 generator(rand_dev);
    for(int i = 0; i < num_generator_nodes; i ++) {
        int continuous_generator_node = generator_uniform_distribution(generator);
        int generator_node = eligible_generator_nodes.at(continuous_generator_node);
        generator_nodes.push_back(generator_node);
    }
    return generator_nodes;
}

std::vector<int> ABM::GetGeneratorNodes(Graph* graph, const std::unordered_map<int, int>& reverse_continuous_node_mapping) {
    std::vector<int> generator_nodes;
    std::uniform_int_distribution<int> generator_uniform_distribution{0, (int)(graph->GetNodeSet().size() - 1)};
    int num_generator_nodes = 1;
    pcg_extras::seed_seq_from<std::random_device> rand_dev;
    pcg32 generator(rand_dev);
    for(int i = 0; i < num_generator_nodes; i ++) {
        int continuous_generator_node = generator_uniform_distribution(generator);
        int generator_node = reverse_continuous_node_mapping.at(continuous_generator_node);
        generator_nodes.push_back(generator_node);
    }
    return generator_nodes;
}

std::unordered_map<int, int> ABM::BinOutdegrees(const std::unordered_map<int, std::vector<int>>& binned_neighborhood, int total_outdegree, std::unordered_map<int, double> binned_recency_probabilities) {
    // initially let's say total_outdegree (requested) = 100
    // [0.5, 0.2, 0.2, 0.1] recency probability that's the same for all agents
    // split 100 into proportions
    // [50, 20, 20, 10]
    // first check sum(E) == 100
    // [(6,7,9), (10,9,2), (19,1), (5 * )] actual neighborhood with node ids
    // [50 - 3, 20 - 3, 20 - 3, 10 - 1] -> "unfulfilled quota"
    // [47, 17, 17, 9] -> "unfulfilled quota"
    // look left and right to see if any bins are available
    // end up citing [3, 3, 2, x] things from each bin
    std::unordered_map<int, int> target_outdegree_per_bin_map;
    int remaining_outdegree = total_outdegree;
    for(int bin_index = 0; bin_index < this->num_bins; bin_index ++) {
        if (remaining_outdegree == 0) {
            break;
        }
        double bin_probability = binned_recency_probabilities[bin_index];
        int current_bin_outdegree = std::round(total_outdegree * bin_probability);
        current_bin_outdegree = std::min(current_bin_outdegree, remaining_outdegree);
        target_outdegree_per_bin_map[bin_index] = current_bin_outdegree;
        remaining_outdegree -= current_bin_outdegree;
    }
    std::uniform_int_distribution<int> year_distribution{1, 5};
    pcg_extras::seed_seq_from<std::random_device> rand_dev;
    pcg32 generator(rand_dev);
    while(remaining_outdegree > 0) {
        int chosen_year = year_distribution(generator);
        int current_bin_index = this->GetBinIndex(chosen_year);
        target_outdegree_per_bin_map[current_bin_index] += 1;
        remaining_outdegree --;
    }
    for(int bin_index = 0; bin_index < this->num_bins; bin_index ++) {
        int current_uncited_num_nodes = target_outdegree_per_bin_map[bin_index] - binned_neighborhood.at(bin_index).size();
        if (current_uncited_num_nodes > 0) {
            for(int sweep_index = bin_index - 1; sweep_index >= 0 && current_uncited_num_nodes > 0; sweep_index --) {
                if (target_outdegree_per_bin_map[sweep_index] < (int)binned_neighborhood.at(sweep_index).size()) {
                    int current_citable = std::min(current_uncited_num_nodes, (int)binned_neighborhood.at(sweep_index).size() - target_outdegree_per_bin_map[sweep_index]);
                    current_uncited_num_nodes -= current_citable;
                    target_outdegree_per_bin_map[sweep_index] += current_citable;
                    target_outdegree_per_bin_map[bin_index] -= current_citable;
                }
            }
            for(int sweep_index = bin_index + 1; sweep_index < this->num_bins && current_uncited_num_nodes > 0; sweep_index ++) {
                if (target_outdegree_per_bin_map[sweep_index] < (int)binned_neighborhood.at(sweep_index).size()) {
                    int current_citable = std::min(current_uncited_num_nodes, (int)binned_neighborhood.at(sweep_index).size() - target_outdegree_per_bin_map[sweep_index]);
                    current_uncited_num_nodes -= current_citable;
                    target_outdegree_per_bin_map[sweep_index] += current_citable;
                    target_outdegree_per_bin_map[bin_index] -= current_citable;
                }
            }
        }
    }
    return target_outdegree_per_bin_map;
}

int ABM::GetBinIndex(int year_diff) {
    /* 5,10,25 with explicit but ignored 1*/
    /* [1, 5) */
    /* [5, 10) */
    /* [10, 25) */
    /* [25, inf) */
    for (int i = 0; i < this->num_bins - 1; i ++) {
        if (this->bin_boundaries.at(i) <= year_diff && year_diff < this->bin_boundaries.at(i + 1)) {
            return i;
        }
    }
    return this->bin_boundaries.size() - 1;
}

int ABM::GetBinIndex(Graph* graph, int current_node, int current_year) {
    int current_diff = current_year - graph->GetIntAttribute("year", current_node);
    return this->GetBinIndex(current_diff);
}

std::unordered_map<int, std::vector<int>> ABM::BinNeighborhood(Graph* graph, int current_year, std::vector<int> n_hop_list) {
    std::unordered_map<int, std::vector<int>> binned_neighborhood;
    for(int i = 0; i < this->num_bins; i ++) {
        binned_neighborhood[i] = {};
    }
    for(size_t i = 0; i < n_hop_list.size(); i ++) {
        int current_node = n_hop_list[i];
        int current_bin = GetBinIndex(graph, current_node, current_year);
        binned_neighborhood[current_bin].push_back(current_node);
    }
    return binned_neighborhood;
}
std::unordered_map<int, std::vector<int>> ABM::GetNeighborhoodMap(Graph* graph, int current_year, const std::vector<int>& generator_nodes, int num_hops) {
    if (this->use_alpha) {
        // create distance 1 and distance 2 neighborhoods
        return this->GetOneAndTwoDistanceNeighborhoods(graph, current_year, generator_nodes, num_hops);
    } else {
        return this->GetNHopNeighborhood(graph, current_year, generator_nodes, num_hops);
    }
}

std::unordered_map<int, std::vector<int>> ABM::GetOneAndTwoDistanceNeighborhoods(Graph* graph, int current_year, const std::vector<int>& generator_nodes, int num_hops) {
    std::unordered_map<int, std::vector<int>> n_hop_map;
    if (this->neighborhood_sample == -1) {
        std::set<int> visited;
        for(size_t i = 0; i < generator_nodes.size(); i ++) {
            int generator_node = generator_nodes.at(i);
            std::queue<std::pair<int, int>> to_visit;
            to_visit.push({generator_node, 0});
            visited.insert(generator_node);
            while(!to_visit.empty()) {
                std::pair<int, int> current_pair = to_visit.front();
                to_visit.pop();
                int current_node = current_pair.first;
                int current_distance = current_pair.second;
                if (current_distance > 0) {
                    n_hop_map[current_distance].push_back(current_node);
                }
                if (current_distance < num_hops) {
                    if(graph->GetOutDegree(current_node) > 0) {
                        for(auto const& outgoing_neighbor : graph->GetForwardAdjMap().at(current_node)) {
                            if(!visited.contains(outgoing_neighbor)) {
                                visited.insert(outgoing_neighbor);
                                to_visit.push({outgoing_neighbor, current_distance + 1});
                            }
                        }
                    }
                    if(graph->GetInDegree(current_node) > 0) {
                        for(auto const& incoming_neighbor : graph->GetBackwardAdjMap().at(current_node)) {
                            if(!visited.contains(incoming_neighbor)) {
                                visited.insert(incoming_neighbor);
                                to_visit.push({incoming_neighbor, current_distance + 1});
                            }
                        }
                    }
                }
            }
        }
    } else {
        // NOTE: only supports randomly sampling from up to 2-hop
        n_hop_map[1].reserve(this->neighborhood_sample);
        n_hop_map[2].reserve(this->neighborhood_sample);
        size_t max_neighborhood_size = this->neighborhood_sample;
        std::set<int> visited;
        pcg_extras::seed_seq_from<std::random_device> rand_dev;
        pcg32 generator(rand_dev);
        for(size_t i = 0; i < generator_nodes.size(); i ++) {
            // get the 1-hop first
            int generator_node = generator_nodes.at(i);
            visited.insert(generator_node);
            std::vector<int> current_one_hop_neighborhood;
            if(graph->GetOutDegree(generator_node) > 0) {
                for(auto const& outgoing_neighbor : graph->GetForwardAdjMap().at(generator_node)) {
                    if (!visited.contains(outgoing_neighbor)) {
                        current_one_hop_neighborhood.push_back(outgoing_neighbor);
                        visited.insert(outgoing_neighbor);
                    }
                }
            }
            if(graph->GetInDegree(generator_node) > 0) {
                for(auto const& incoming_neighbor : graph->GetBackwardAdjMap().at(generator_node)) {
                    if (!visited.contains(incoming_neighbor)) {
                        current_one_hop_neighborhood.push_back(incoming_neighbor);
                        visited.insert(incoming_neighbor);
                    }
                }
            }
            // pick random nodes to get to 2-hop
            if (current_one_hop_neighborhood.size() > max_neighborhood_size) {
                std::vector<int> sampled_one_hop_neighborhood;
                std::sample(current_one_hop_neighborhood.begin(), current_one_hop_neighborhood.end(), std::back_inserter(sampled_one_hop_neighborhood), max_neighborhood_size, generator);
                current_one_hop_neighborhood = sampled_one_hop_neighborhood;
            }
            // until here should be fast
            std::copy(current_one_hop_neighborhood.begin(), current_one_hop_neighborhood.end(), std::back_inserter(n_hop_map[1]));
            std::shuffle(current_one_hop_neighborhood.begin(), current_one_hop_neighborhood.end(), generator);
            for(size_t j = 0; j < current_one_hop_neighborhood.size(); j ++) {
                int current_two_hop_size = 0;
                if (graph->GetOutDegree(current_one_hop_neighborhood[j]) > 0) {
                    current_two_hop_size += graph->GetForwardAdjMap().at(current_one_hop_neighborhood[j]).size();
                }
                if (graph->GetInDegree(current_one_hop_neighborhood[j]) > 0) {
                    current_two_hop_size += graph->GetBackwardAdjMap().at(current_one_hop_neighborhood[j]).size();
                }
                // worst case for one node we might only grab the outgoing edges
                if (n_hop_map[2].size() + current_two_hop_size < max_neighborhood_size) {
                    if (graph->GetOutDegree(current_one_hop_neighborhood[j]) > 0) {
                        for(auto const& outgoing_neighbor : graph->GetForwardAdjMap().at(current_one_hop_neighborhood[j])) {
                            if (!visited.contains(outgoing_neighbor)) {
                                visited.insert(outgoing_neighbor);
                                n_hop_map[2].push_back(outgoing_neighbor);
                            }
                        }
                    }
                    if (graph->GetInDegree(current_one_hop_neighborhood[j]) > 0) {
                        for(auto const& incoming_neighbor : graph->GetBackwardAdjMap().at(current_one_hop_neighborhood[j])) {
                            if (!visited.contains(incoming_neighbor)) {
                                visited.insert(incoming_neighbor);
                                n_hop_map[2].push_back(incoming_neighbor);
                            }
                        }
                    }
                } else {
                    std::vector<int> to_be_sampled_neighborhood;
                    if (graph->GetOutDegree(current_one_hop_neighborhood[j]) > 0) {
                        for(auto const& outgoing_neighbor : graph->GetForwardAdjMap().at(current_one_hop_neighborhood[j])) {
                            if (!visited.contains(outgoing_neighbor)) {
                                to_be_sampled_neighborhood.push_back(outgoing_neighbor);
                            }
                        }
                    }
                    if (graph->GetInDegree(current_one_hop_neighborhood[j]) > 0) {
                        for(auto const& incoming_neighbor : graph->GetBackwardAdjMap().at(current_one_hop_neighborhood[j])) {
                            if (!visited.contains(incoming_neighbor)) {
                                to_be_sampled_neighborhood.push_back(incoming_neighbor);
                            }
                        }
                    }
                    std::sample(to_be_sampled_neighborhood.begin(), to_be_sampled_neighborhood.end(), std::back_inserter(n_hop_map[2]), max_neighborhood_size - n_hop_map[2].size(), generator);
                    if (n_hop_map[2].size() == max_neighborhood_size) {
                        return n_hop_map;
                    }
                }
            }
        }
    }
    return n_hop_map;
}

std::unordered_map<int, std::vector<int>> ABM::GetNHopNeighborhood(Graph* graph, int current_year, const std::vector<int>& generator_nodes, int num_hops) {
    std::unordered_map<int, std::vector<int>> n_hop_map;
    std::vector<int> n_hop_neighborhood;
    if (this->neighborhood_sample == -1) {
        std::set<int> visited;
        for(size_t i = 0; i < generator_nodes.size(); i ++) {
            int generator_node = generator_nodes.at(i);
            std::queue<std::pair<int, int>> to_visit;
            to_visit.push({generator_node, 0});
            visited.insert(generator_node);
            while(!to_visit.empty()) {
                std::pair<int, int> current_pair = to_visit.front();
                to_visit.pop();
                int current_node = current_pair.first;
                int current_distance = current_pair.second;
                if (current_distance > 0) {
                    n_hop_neighborhood.push_back(current_node);
                }
                if (current_distance < num_hops) {
                    if(graph->GetOutDegree(current_node) > 0) {
                        for(auto const& outgoing_neighbor : graph->GetForwardAdjMap().at(current_node)) {
                            if(!visited.contains(outgoing_neighbor)) {
                                visited.insert(outgoing_neighbor);
                                to_visit.push({outgoing_neighbor, current_distance + 1});
                            }
                        }
                    }
                    if(graph->GetInDegree(current_node) > 0) {
                        for(auto const& incoming_neighbor : graph->GetBackwardAdjMap().at(current_node)) {
                            if(!visited.contains(incoming_neighbor)) {
                                visited.insert(incoming_neighbor);
                                to_visit.push({incoming_neighbor, current_distance + 1});
                            }
                        }
                    }
                }
            }
        }
    } else {
        // NOTE: only supports randomly sampling from up to 2-hop
        n_hop_neighborhood.reserve(this->neighborhood_sample);
        size_t max_neighborhood_size = this->neighborhood_sample;
        std::set<int> visited;
        pcg_extras::seed_seq_from<std::random_device> rand_dev;
        pcg32 generator(rand_dev);
        for(size_t i = 0; i < generator_nodes.size(); i ++) {
            // get the 1-hop first
            int generator_node = generator_nodes.at(i);
            visited.insert(generator_node);
            std::vector<int> current_one_hop_neighborhood;
            if(graph->GetOutDegree(generator_node) > 0) {
                for(auto const& outgoing_neighbor : graph->GetForwardAdjMap().at(generator_node)) {
                    if (!visited.contains(outgoing_neighbor)) {
                        current_one_hop_neighborhood.push_back(outgoing_neighbor);
                        visited.insert(outgoing_neighbor);
                    }
                }
            }
            if(graph->GetInDegree(generator_node) > 0) {
                for(auto const& incoming_neighbor : graph->GetBackwardAdjMap().at(generator_node)) {
                    if (!visited.contains(incoming_neighbor)) {
                        current_one_hop_neighborhood.push_back(incoming_neighbor);
                        visited.insert(incoming_neighbor);
                    }
                }
            }
            // pick random nodes to get to 2-hop
            if (current_one_hop_neighborhood.size() > max_neighborhood_size) {
                std::vector<int> sampled_one_hop_neighborhood;
                std::sample(current_one_hop_neighborhood.begin(), current_one_hop_neighborhood.end(), std::back_inserter(sampled_one_hop_neighborhood), max_neighborhood_size, generator);
                current_one_hop_neighborhood = sampled_one_hop_neighborhood;
            }
            // until here should be fast
            std::copy(current_one_hop_neighborhood.begin(), current_one_hop_neighborhood.end(), std::back_inserter(n_hop_neighborhood));
            std::shuffle(current_one_hop_neighborhood.begin(), current_one_hop_neighborhood.end(), generator);
            for(size_t j = 0; j < current_one_hop_neighborhood.size(); j ++) {
                int current_two_hop_size = 0;
                if (graph->GetOutDegree(current_one_hop_neighborhood[j]) > 0) {
                    current_two_hop_size += graph->GetForwardAdjMap().at(current_one_hop_neighborhood[j]).size();
                }
                if (graph->GetInDegree(current_one_hop_neighborhood[j]) > 0) {
                    current_two_hop_size += graph->GetBackwardAdjMap().at(current_one_hop_neighborhood[j]).size();
                }
                // worst case for one node we might only grab the outgoing edges
                if (n_hop_neighborhood.size() + current_two_hop_size < max_neighborhood_size) {
                    if (graph->GetOutDegree(current_one_hop_neighborhood[j]) > 0) {
                        for(auto const& outgoing_neighbor : graph->GetForwardAdjMap().at(current_one_hop_neighborhood[j])) {
                            if (!visited.contains(outgoing_neighbor)) {
                                visited.insert(outgoing_neighbor);
                                n_hop_neighborhood.push_back(outgoing_neighbor);
                            }
                        }
                    }
                    if (graph->GetInDegree(current_one_hop_neighborhood[j]) > 0) {
                        for(auto const& incoming_neighbor : graph->GetBackwardAdjMap().at(current_one_hop_neighborhood[j])) {
                            if (!visited.contains(incoming_neighbor)) {
                                visited.insert(incoming_neighbor);
                                n_hop_neighborhood.push_back(incoming_neighbor);
                            }
                        }
                    }
                } else {
                    std::vector<int> to_be_sampled_neighborhood;
                    if (graph->GetOutDegree(current_one_hop_neighborhood[j]) > 0) {
                        for(auto const& outgoing_neighbor : graph->GetForwardAdjMap().at(current_one_hop_neighborhood[j])) {
                            if (!visited.contains(outgoing_neighbor)) {
                                to_be_sampled_neighborhood.push_back(outgoing_neighbor);
                            }
                        }
                    }
                    if (graph->GetInDegree(current_one_hop_neighborhood[j]) > 0) {
                        for(auto const& incoming_neighbor : graph->GetBackwardAdjMap().at(current_one_hop_neighborhood[j])) {
                            if (!visited.contains(incoming_neighbor)) {
                                to_be_sampled_neighborhood.push_back(incoming_neighbor);
                            }
                        }
                    }
                    std::sample(to_be_sampled_neighborhood.begin(), to_be_sampled_neighborhood.end(), std::back_inserter(n_hop_neighborhood), max_neighborhood_size - n_hop_neighborhood.size(), generator);
                    if (n_hop_neighborhood.size() == max_neighborhood_size) {
                        n_hop_map[1] = n_hop_neighborhood;
                        return n_hop_map;
                    }
                }
            }
        }
    }
    n_hop_map[1] = n_hop_neighborhood;
    return n_hop_map;
}

void ABM::LogTime(int current_year, std::string label, int time_elapsed) {
    this->timing_file_handle << std::to_string(current_year) << "," << label << "," << std::to_string(time_elapsed) << "\n";
    std::flush(this->timing_file_handle);
}

void ABM::LogTime(int current_year, std::string label) {
    std::chrono::time_point<std::chrono::steady_clock> current_time = std::chrono::steady_clock::now();
    auto milliseconds_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - this->prev_time);
    this->timing_map[current_year].push_back({label, milliseconds_elapsed.count()});
    this->prev_time = current_time;
    this->timing_file_handle << std::to_string(current_year) << "," << label << "," << std::to_string(milliseconds_elapsed.count()) << "\n";
    std::flush(this->timing_file_handle);
}


std::chrono::time_point<std::chrono::steady_clock> ABM::LocalLogTime(std::vector<std::pair<std::string, int>>& local_parallel_stage_time_vec, std::chrono::time_point<std::chrono::steady_clock> local_prev_time, std::string label) {
    std::chrono::time_point<std::chrono::steady_clock> local_current_time = std::chrono::steady_clock::now();
    auto milliseconds_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(local_current_time - local_prev_time);
    local_parallel_stage_time_vec.push_back({label, milliseconds_elapsed.count()});
    return local_current_time;
}

void ABM::WriteTimingFile(int start_year, int end_year) {
    for(int i = start_year; i < end_year; i ++) {
        for(size_t j = 0; j < this->timing_map[i].size(); j ++) {
            this->timing_file_handle << std::to_string(i) << "," << (this->timing_map[i][j]).first << "," << std::to_string((this->timing_map[i][j]).second) << "\n";
        }
    }
}

std::unordered_map<int, int> ABM::GetNumCitationsPerNeighborhood(double alpha, int total_num_citations_neighborhood, const std::unordered_map<int, std::vector<int>>& n_hop_map) {
    std::unordered_map<int, int> num_citations_per_neighborhood;
    if (this->use_alpha) {
        num_citations_per_neighborhood[1] = std::min((int)(total_num_citations_neighborhood * alpha), (int)n_hop_map.at(1).size());
        num_citations_per_neighborhood[2] = std::min(total_num_citations_neighborhood - num_citations_per_neighborhood[1], (int)n_hop_map.at(2).size());
    } else {
        num_citations_per_neighborhood[1] = std::min(total_num_citations_neighborhood, (int)n_hop_map.at(1).size());
    }
    return num_citations_per_neighborhood;
}


void ABM::InitializeBinBoundaries() {
    std::string bin_boundary_string = this->recency_bins;
    std::stringstream ss(bin_boundary_string);
    std::string current_value;
    int element_no = 0;
    while(std::getline(ss, current_value, ',')) {
        /* if (element_no > 0) { */
        int current_bin_value = std::stoi(current_value);
        if (current_bin_value != -1) {
            this->bin_boundaries.push_back(current_bin_value);
        }
        /* } */
        element_no ++;
    }
    this->num_bins = element_no;
}

bool ABM::ValidateBinBoundaries() {
    this->WriteToLogFile(std::to_string(this->bin_boundaries.size()) + " bins have been created", Log::info);
    if (this->bin_boundaries.size() == 0) {
        this->WriteToLogFile("At least one bin is required", Log::error);
        return false;
    }
    if (this->bin_boundaries.at(0) != 1) {
        this->WriteToLogFile("The first bin must start with year 1", Log::error);
        return false;
    }
    std::string recency_bin_string;
    for(size_t i = 0; i < this->bin_boundaries.size() - 1; i ++) {
        recency_bin_string += ("[" + std::to_string(bin_boundaries.at(i)) + "," + std::to_string(this->bin_boundaries.at(i + 1)) + "), ");
    }
    recency_bin_string += ("[" + std::to_string(this->bin_boundaries.at(this->bin_boundaries.size() - 1)) + ",infinity)");
    this->WriteToLogFile("Here are the bins: " + recency_bin_string, Log::info);
    return true;
}

bool ABM::ValidateArguments() {
    if (!this->ValidateArgument("Environment", "edgelist", this->edgelist, "")) {
        return false;
    }
    if (!this->ValidateArgument("Environment", "nodelist", this->nodelist, "")) {
        return false;
    }
    if (!this->ValidateArgument("Environment", "growth_rate", this->growth_rate, -42)) {
        return false;
    }
    if (!this->ValidateArgument("Environment", "num_cycles", this->num_cycles, -42)) {
        return false;
    }
    if (!this->ValidateArgument("Environment", "out_degree_bag", this->out_degree_bag, "")) {
        return false;
    }
    if (!this->ValidateArgument("Environment", "recency_table", this->recency_table, "")) {
        return false;
    }
    if (this->planted_nodes == "") {
        this->WriteToLogFile("No agents will be planted", Log::info);
    } else {
        this->WriteToLogFile("planted_nodes: " + this->planted_nodes, Log::info);
    }
    if (!this->ValidateArgument("Agent", "fully_random_citations", this->fully_random_citations, -42)) {
        return false;
    }
    if (this->preferential_weight == -42) {
        this->WriteToLogFile("Required parameter 'preferential_weight' was not found in the 'Agent' section", Log::error);
        return false;
    } else if (this->preferential_weight == -1) {
        this->WriteToLogFile("preferential_weight: randomized", Log::info);
    } else {
        this->WriteToLogFile("preferential_weight: " + std::to_string(this->preferential_weight), Log::info);
    }
    if (this->fitness_weight == -42) {
        this->WriteToLogFile("Required parameter 'fitness_weight' was not found in the 'Agent' section", Log::error);
        return false;
    } else if (this->fitness_weight == -1) {
        this->WriteToLogFile("fitness_weight: randomized", Log::info);
    } else {
        this->WriteToLogFile("fitness_weight: " + std::to_string(this->fitness_weight), Log::info);
    }
    if (this->num_authors_weight == -42) {
        this->WriteToLogFile("Required parameter 'num_authors_weight' was not found in the 'Agent' section", Log::error);
        return false;
    } else if (this->num_authors_weight == -1) {
        this->WriteToLogFile("num_authors_weight: randomized", Log::info);
    } else {
        this->WriteToLogFile("num_authors_weight: " + std::to_string(this->num_authors_weight), Log::info);
    }
    if (this->author_reputation_weight == -42) {
        this->WriteToLogFile("Required parameter 'author_reputation_weight' was not found in the 'Agent' section", Log::error);
        return false;
    } else if (this->author_reputation_weight == -1) {
        this->WriteToLogFile("author_reputation_weight: randomized", Log::info);
    } else {
        this->WriteToLogFile("author_reputation_weight: " + std::to_string(this->author_reputation_weight), Log::info);
    }
    if (!this->ValidateArgument("Agent", "fitness_value_min", this->fitness_value_min, -42)) {
        return false;
    }
    if (!this->ValidateArgument("Agent", "fitness_value_max", this->fitness_value_max, -42)) {
        return false;
    }
    if (!this->ValidateArgument("Agent", "fitness_lag_duration_min", this->fitness_lag_duration_min, -42)) {
        return false;
    }
    if (!this->ValidateArgument("Agent", "fitness_lag_duration_max", this->fitness_lag_duration_max, -42)) {
        return false;
    }
    if (!this->ValidateArgument("Agent", "fitness_peak_duration_min", this->fitness_peak_duration_min, -42)) {
        return false;
    }
    if (!this->ValidateArgument("Agent", "fitness_peak_duration_max", this->fitness_peak_duration_max, -42)) {
        return false;
    }
    if (!this->ValidateArgument("Agent", "same_year_citations", this->same_year_citations, -42)) {
        return false;
    }
    if (!this->ValidateArgument("Agent", "neighborhood_sample", this->neighborhood_sample, -42)) {
        return false;
    }
    if (!this->ValidateArgument("Agent", "num_authors_bag", this->num_authors_bag, "")) {
        return false;
    }
    if (!this->ValidateArgument("Agent", "author_max_lifetime", this->author_max_lifetime, -42)) {
        return false;
    }
    if (!this->ValidateArgument("Agent", "in_degree_threshold", this->in_degree_threshold, -42)) {
        return false;
    }
    if (!this->ValidateArgument("Agent", "fitness_threshold", this->fitness_threshold, -42)) {
        return false;
    }
    if (!this->ValidateArgument("Agent", "recency_threshold", this->recency_threshold, -42)) {
        return false;
    }
    if (!this->ValidateArgument("Agent", "non_random_generator_probability", this->non_random_generator_probability, -42)) {
        return false;
    }
    if (this->use_alpha) {
        if (this->alpha == -42) {
            this->WriteToLogFile("Required parameter 'alpha' was not found in the 'Agent' section while 'use_alpha' was true", Log::error);
            return false;
        } else if (this->alpha == -1) {
            this->WriteToLogFile("alpha: randomized", Log::info);
        } else {
            this->WriteToLogFile("alpha: " + std::to_string(this->alpha), Log::info);
        }
    } else {
        if (this->alpha == -42) {
            this->WriteToLogFile("Alpha ignored. Agents will not split the neighborhood into 1 and 2 distance neighborhoods", Log::info);
        } else {
            this->WriteToLogFile("'use_alpha' is false but a value of" + std::to_string(this->alpha) + " for 'alpha' was provided. Leave the alpha line as 'alpha=' with an empty string as the alpha value if alpha should be ignored.", Log::error);
            return false;
        }
    }
    if (this->start_from_checkpoint) {
        this->WriteToLogFile("Starting from a checkpoint. Make sure the edgelist and nodelist provided are the result of a previous simulation", Log::info);
    } else {
        this->WriteToLogFile("Not using checkpointing. Starting simulation from the first year.", Log::info);
    }
    if (!this->ValidateArgument("General", "output_file", this->output_file, "")) {
        return false;
    }
    if (!this->ValidateArgument("General", "recency_bins", this->recency_bins, "")) {
        return false;
    }
    if (!this->ValidateArgument("General", "auxiliary_information_file", this->auxiliary_information_file, "")) {
        return false;
    }
    if (!this->ValidateArgument("General", "log_file", this->log_file, "")) {
        return false;
    }
    if (!this->ValidateArgument("General", "num_processors", this->num_processors, -42)) {
        return false;
    }
    if (!this->ValidateArgument("General", "log_level", this->log_level, -42)) {
        return false;
    }
    return true;
}


int ABM::main() {
    if (!this->ValidateBinBoundaries()) {
        return 1;
    }
    Graph* graph = new Graph(this->edgelist, this->nodelist, this->start_from_checkpoint, this->num_authors_bag, this->author_max_lifetime);
    this->InitializeSeedFitness(graph);
    this->WriteToLogFile("loaded graph", Log::info);
    /* node ids to continous integer from 0 */
    std::unordered_map<int, int> continuous_node_mapping = this->BuildContinuousNodeMapping(graph);

    /* continous integer from 0 to node ids*/
    std::unordered_map<int, int> reverse_continuous_node_mapping = this->ReverseMapping(continuous_node_mapping);

    int start_year = this->GetMaxYear(graph) + 1;
    int next_node_id = this->GetMaxNode(graph) + 1;
    int initial_next_node_id = next_node_id;

    /* get input to score arrays based on continuous_node_mapping */
    int initial_graph_size = graph->GetNodeSet().size();
    int final_graph_size = this->GetFinalGraphSize(graph);
    this->WriteToLogFile("final graph size is " + std::to_string(final_graph_size), Log::info);
    int* in_degree_arr = new int[final_graph_size]; // live updated in-degree array (changes every year)
    int* fitness_arr = new int[final_graph_size]; // live updated fitness array (changes every year)
    int* num_authors_arr = new int[final_graph_size];
    int* author_reputation_arr = new int[final_graph_size];
    double* pa_arr = new double[final_graph_size]; // exp of in-degree array
    double* fit_arr = new double[final_graph_size]; // exp of fitness array
    double* na_arr = new double[final_graph_size]; // exp of num_authors_arr
    double* ar_arr = new double[final_graph_size]; // exp of author_reputation_arr
    double* random_weight_arr = new double[final_graph_size];
    double* current_score_arr = new double[final_graph_size];

    // the first new agent node has index 0 but is actually index initial_graph_size in the continuous mapping
    double* pa_weight_arr = new double[final_graph_size - initial_graph_size];
    double* fit_weight_arr = new double[final_graph_size - initial_graph_size];
    double* num_authors_weight_arr = new double[final_graph_size - initial_graph_size];
    double* author_reputation_weight_arr = new double[final_graph_size - initial_graph_size];
    int* out_degree_arr = new int[final_graph_size - initial_graph_size];
    double* alpha_arr = new double[final_graph_size - initial_graph_size];
    int* fitness_lag_duration_arr = new int[final_graph_size - initial_graph_size];
    int* fitness_peak_value_arr = new int[final_graph_size - initial_graph_size];
    int* fitness_peak_duration_arr = new int[final_graph_size - initial_graph_size];

    this->PopulateWeightArrs(pa_weight_arr, fit_weight_arr, num_authors_weight_arr, author_reputation_weight_arr, final_graph_size - initial_graph_size);
    this->PopulateAlphaArr(alpha_arr, final_graph_size - initial_graph_size);
    this->PopulateOutDegreeArr(out_degree_arr, final_graph_size - initial_graph_size);
    this->PopulateFitnessArrs(fitness_lag_duration_arr, fitness_peak_value_arr, fitness_peak_duration_arr, final_graph_size - initial_graph_size);
    this->PopulateNumAuthorsArr(graph, num_authors_arr, final_graph_size);
    std::unordered_map<int, int> planted_nodes_line_number_map = this->PlantNodes(graph, pa_weight_arr, fit_weight_arr, num_authors_weight_arr, author_reputation_weight_arr, out_degree_arr, alpha_arr, fitness_lag_duration_arr, fitness_peak_value_arr, fitness_peak_duration_arr, num_authors_arr);


    std::vector<int> new_nodes_vec;
    std::vector<std::pair<int, int>> new_edges_vec;
    std::set<int> same_year_source_nodes;
    std::unordered_map<int, double> exp_cached_results;
    for(int i = 0; i < 1000; i ++) {
        exp_cached_results[i] = std::max(pow(i, this->gamma), 1.0) + 1;
    }
    std::unordered_map<int, double> binned_recency_probabilities = GetBinnedRecencyProbabilities();
    Eigen::setNbThreads(1);
    pcg_extras::seed_seq_from<std::random_device> rand_dev;
    pcg32 generator(rand_dev);
    std::uniform_real_distribution<double> non_random_generator_uniform_distribution{0, 1};
    for (int current_year = start_year; current_year < start_year + this->num_cycles; current_year ++) {
        this->prev_time = std::chrono::steady_clock::now();
        int current_graph_size = graph->GetNodeSet().size();
        this->WriteToLogFile("current year is: " + std::to_string(current_year) + " and the graph is " + std::to_string(current_graph_size) + " nodes large", Log::info);
        this->FillInDegreeArr(graph, continuous_node_mapping, in_degree_arr);
        this->LogTime(current_year, "Fill in-degree array");
        this->FillFitnessArr(graph, continuous_node_mapping, current_year, fitness_arr);
        this->LogTime(current_year, "Fill fitness array");
        // this->FillNumAuthorsArr(graph, continuous_node_mapping, num_authors_arr);
        // this->LogTime(current_year, "Fill num authors array");
        this->CalculateExpScores(exp_cached_results, in_degree_arr, pa_arr, current_graph_size);
        this->LogTime(current_year, "Process in-degree array");
        this->CalculateExpScores(exp_cached_results, fitness_arr, fit_arr, current_graph_size);
        this->LogTime(current_year, "Process fitness array");
        this->CalculateExpScores(exp_cached_results, num_authors_arr, na_arr, current_graph_size);
        this->LogTime(current_year, "Process num authors array");

        /* initialize new nodes */
        int num_new_nodes = std::ceil(current_graph_size * this->growth_rate);
        this->WriteToLogFile("making " + std::to_string(num_new_nodes) + " nodes this year", Log::info);
        for(int i = 0; i < num_new_nodes; i ++) {
            continuous_node_mapping[next_node_id] = current_graph_size + i;
            reverse_continuous_node_mapping[current_graph_size + i] = next_node_id;
            new_nodes_vec.push_back(next_node_id);
            graph->SetIntAttribute("year", next_node_id, current_year);
            graph->SetStringAttribute("type", next_node_id, "agent");
            next_node_id ++;
        }
        this->LogTime(current_year, "Create new node ids");
        this->FillSameYearSourceNodes(same_year_source_nodes, new_nodes_vec.size());
        this->LogTime(current_year, "Pick same year nodes");


        std::vector<int> eligible_generator_nodes = this->GetEligibleGeneratorNodes(graph, current_graph_size, reverse_continuous_node_mapping, in_degree_arr, fitness_arr, this->in_degree_threshold, this->fitness_threshold, start_year, current_year, this->recency_threshold);
        for(size_t i = 0; i < new_nodes_vec.size(); i ++) {
            int new_node = new_nodes_vec[i];
            double new_node_non_random_draw = non_random_generator_uniform_distribution(generator);
            if (new_node_non_random_draw < this->non_random_generator_probability) {
                std::vector<int> generator_nodes = this->GetGeneratorNodesFromSet(eligible_generator_nodes);
                this->UpdateGraphAttributesGeneratorNodes(graph, new_node, generator_nodes);
            } else {
                std::vector<int> generator_nodes = this->GetGeneratorNodes(graph, reverse_continuous_node_mapping);
                this->UpdateGraphAttributesGeneratorNodes(graph, new_node, generator_nodes);
            }
            int author_id = graph->GetNextAuthor(current_year);
            // int num_authors = graph->GetNextNumAuthors();
            this->UpdateGraphAttributesAuthors(graph, new_node, author_id);
        }
        this->LogTime(current_year, "Pick generator nodes");

        this->FillAuthorReputationArr(graph, continuous_node_mapping, author_reputation_arr);
        this->LogTime(current_year, "Fill author reputation array");
        this->CalculateExpScores(exp_cached_results, author_reputation_arr, ar_arr, current_graph_size);
        this->LogTime(current_year, "Process author reputation array");

        std::vector<int> sampled_neighborhood_sizes_map(new_nodes_vec.size());
        std::vector<int> fully_random_citations_map(new_nodes_vec.size());
        std::vector<std::vector<int>> sampled_binned_neighborhood_sizes_map(new_nodes_vec.size());

        std::vector<std::pair<std::string, int>> parallel_stage_time_vec;
        #pragma omp parallel for reduction(custom_merge_vec_int: new_edges_vec) reduction(merge_str_int_pair_vecs: parallel_stage_time_vec)
        for(size_t i = 0; i < new_nodes_vec.size(); i ++) {
            std::chrono::time_point<std::chrono::steady_clock> local_prev_time = std::chrono::steady_clock::now();
            std::vector<std::pair<int, int>> local_new_edges_vec;
            std::vector<std::pair<std::string, int>> local_parallel_stage_time_vec;


            int citations[this->max_out_degree + 1]; // out-degree assumed to be max 249 for now
            int new_node = new_nodes_vec[i];
            // continuous_node_mapping = node id -> 0..n but guaranteed 0 .. initial graph size are seed nodes
            // initial graphsize .. n are agent nodes
            int weight_arr_index = continuous_node_mapping[new_node] - initial_graph_size;
            double pa_weight = pa_weight_arr[weight_arr_index];
            double fit_weight = fit_weight_arr[weight_arr_index];
            double num_authors_weight = num_authors_weight_arr[weight_arr_index];
            double author_reputation_weight = author_reputation_weight_arr[weight_arr_index];
            double alpha = alpha_arr[weight_arr_index];
            std::vector<int> generator_nodes = this->GetGraphAttributesGeneratorNodes(graph, new_node);
            int num_hops = 2;
            // if use alpha then map has keys 1 and 2
            // if use alpha false then map has only key 1
            std::unordered_map<int, std::vector<int>> n_hop_map = this->GetNeighborhoodMap(graph, current_year, generator_nodes, num_hops);
            // out-degree assigned is D
            // G=1 for generator node
            // S=0 or 1 sometimes if i'm a same year source
            // 5% or 0.05 is default proprotion of random so R = (D * 0.05)
            // citing from neighborhood based on scores is N = D - G - S - R
            // if use alpha true then there's 2 neighborhoods so N * alpha for 1-hop N * (1- alpha) for 2-hop
            // if use alpha false then there's 1 neighborhood so cite N things from there

            int num_generator_node_citation = generator_nodes.size(); // should be 1 for now
            int same_year_citation = same_year_source_nodes.count(i); // could be 0 or 1
            int num_fully_random_cited_reserved = std::floor(this->fully_random_citations * out_degree_arr[weight_arr_index]); // e.g., 5% of out-degree. some small number
            int total_num_citations_neighborhood = out_degree_arr[weight_arr_index] - num_generator_node_citation - same_year_citation - num_fully_random_cited_reserved;
            std::unordered_map<int, int> num_citations_per_neighborhood = this->GetNumCitationsPerNeighborhood(alpha, total_num_citations_neighborhood, n_hop_map);

            int num_actually_cited = 0;
            if (same_year_citation) {
                num_actually_cited += this->MakeSameYearCitations(same_year_source_nodes, new_nodes_vec.size(), reverse_continuous_node_mapping, citations, current_graph_size);
            }
            local_prev_time = this->LocalLogTime(local_parallel_stage_time_vec, local_prev_time, "make same year citations");

            for(size_t current_neighborhood_index = 1; current_neighborhood_index < n_hop_map.size() + 1; current_neighborhood_index ++) { // 2 iter if use alpha true
                sampled_neighborhood_sizes_map[i] += n_hop_map.at(current_neighborhood_index).size();
                std::unordered_map<int, std::vector<int>> binned_neighborhood = this->BinNeighborhood(graph, current_year, n_hop_map.at(current_neighborhood_index));
                local_prev_time = this->LocalLogTime(local_parallel_stage_time_vec, local_prev_time, "bin neighborhood");

                std::unordered_map<int, int> outdegree_per_bin_map = this->BinOutdegrees(binned_neighborhood, num_citations_per_neighborhood.at(current_neighborhood_index), binned_recency_probabilities);
                for(int bin_index = 0; bin_index < this->num_bins - 1; bin_index ++) { // if there's only 1 bin then this is always false
                    num_actually_cited += this->MakeCitations(graph, continuous_node_mapping, current_year, binned_neighborhood[bin_index], citations + num_actually_cited, pa_arr, fit_arr, na_arr, ar_arr, pa_weight, fit_weight, num_authors_weight, author_reputation_weight, current_graph_size, outdegree_per_bin_map[bin_index]);
                }
                num_actually_cited += this->MakeUniformRandomCitations(graph, continuous_node_mapping, current_year, binned_neighborhood[this->num_bins - 1], citations + num_actually_cited, current_graph_size, outdegree_per_bin_map[this->num_bins - 1]);
            }
            for(int j = 0; j < num_actually_cited; j ++) {
                if (citations[j] < 0) {
                    std::cerr << "[checking before uniform random to graph] regular citation negative: " << std::to_string(citations[j]) << std::endl;
                }
            }

            local_prev_time = this->LocalLogTime(local_parallel_stage_time_vec, local_prev_time, "make neighborhood citations");

            int num_fully_random_cited = out_degree_arr[weight_arr_index] - num_actually_cited - generator_nodes.size(); // finalized later
            fully_random_citations_map[i] = num_fully_random_cited;
            num_actually_cited += this->MakeUniformRandomCitationsFromGraph(graph, reverse_continuous_node_mapping, generator_nodes, citations, num_actually_cited, num_fully_random_cited);
            local_prev_time = this->LocalLogTime(local_parallel_stage_time_vec, local_prev_time, "make random citations");

            for(size_t j = 0; j < generator_nodes.size(); j ++) {
                if (generator_nodes[j] < 0) {
                    std::cerr << "generator node negative: " << std::to_string(generator_nodes[j]) << std::endl;
                }
                local_new_edges_vec.push_back({new_node, generator_nodes[j]});
            }
            for(int j = 0; j < num_actually_cited; j ++) {
                if (citations[j] < 0) {
                    std::cerr << "regular citation negative: " << std::to_string(citations[j])  << " at index " << std::to_string(j) << std::endl;
                    std::cerr << "num_actually_cited: " << std::to_string(num_actually_cited) << std::endl;
                }
                local_new_edges_vec.push_back({new_node, citations[j]});
            }
            new_edges_vec.insert(new_edges_vec.end(), local_new_edges_vec.begin(), local_new_edges_vec.end());
            local_prev_time = this->LocalLogTime(local_parallel_stage_time_vec, local_prev_time, "record edges");
            parallel_stage_time_vec.insert(parallel_stage_time_vec.end(), local_parallel_stage_time_vec.begin(), local_parallel_stage_time_vec.end());
        } // end of omp parallel loop
        std::map<std::string, int> per_stage_time_map;
        for(size_t i = 0; i < parallel_stage_time_vec.size(); i ++) {
            per_stage_time_map[parallel_stage_time_vec[i].first] +=  parallel_stage_time_vec[i].second;
        }
        this->LogTime(current_year, "find neighborhood", per_stage_time_map["retrieve neighborhood"]);
        this->LogTime(current_year, "bin neighborhood", per_stage_time_map["bin neighborhood"]);
        this->LogTime(current_year, "make same year citations", per_stage_time_map["make same year citations"]);
        this->LogTime(current_year, "make neighborhood citations", per_stage_time_map["make neighborhood citations"]);
        this->LogTime(current_year, "make random citations", per_stage_time_map["make random citations"]);
        this->LogTime(current_year, "record edges", per_stage_time_map["record edges"]);
        for(size_t i = 0; i < new_edges_vec.size(); i ++) {
            int new_node = new_edges_vec[i].first;
            int destination_id = new_edges_vec[i].second;
            graph->AddEdge({new_node, destination_id});
            if (new_node < 0 || destination_id < 0) {
                std::cerr << "making edge" << std::to_string(new_node) << " -> " << std::to_string(destination_id) << std::endl;
            }
        }
        this->LogTime(current_year, "Add edges to graph");

        for(size_t i = 0; i < new_nodes_vec.size(); i ++) {
            int new_node = new_nodes_vec[i];
            graph->SetIntAttribute("sampled_neighborhood_size", new_node, sampled_neighborhood_sizes_map[i]);
            graph->SetIntAttribute("fully_random_citations", new_node, fully_random_citations_map[i]);
        }

        this->LogTime(current_year, "Update graph attributes (neighborhood sizes)");
        this->UpdateGraphAttributesFitnesses(graph, new_nodes_vec, continuous_node_mapping, fitness_lag_duration_arr, fitness_peak_value_arr, fitness_peak_duration_arr, initial_graph_size);
        this->LogTime(current_year, "Assign fitness values to new nodes");
        graph->ComputeAuthorReputations();
        this->UpdateGraphAttributesInitialAuthorReputations(graph, new_nodes_vec);
        new_nodes_vec.clear();
        new_edges_vec.clear();
        same_year_source_nodes.clear();
    }

    this->WriteToLogFile("finished sim", Log::info);
    graph->WriteGraph(this->output_file);

    this->UpdateGraphAttributesWeights(graph, initial_next_node_id, pa_weight_arr, fit_weight_arr, num_authors_weight_arr, author_reputation_weight_arr, final_graph_size - initial_graph_size);
    this->UpdateGraphAttributesOutDegrees(graph, initial_next_node_id, out_degree_arr, final_graph_size - initial_graph_size);
    this->UpdateGraphAttributesAlphas(graph, initial_next_node_id, alpha_arr, final_graph_size - initial_graph_size);
    this->UpdateGraphAttributesNumAuthors(graph, continuous_node_mapping, num_authors_arr);
    this->UpdateGraphAttributesPlantedNodesLineNumbers(graph, initial_next_node_id, planted_nodes_line_number_map);

    for(auto const& node_id : graph->GetNodeSet()) {
        graph->SetIntAttribute("in_degree", node_id, graph->GetInDegree(node_id));
        graph->SetIntAttribute("out_degree", node_id, graph->GetOutDegree(node_id));
    }
    graph->ComputeAuthorReputations();
    graph->WriteAttributes(this->auxiliary_information_file);
    this->WriteToLogFile("wrote graph", Log::info);
    delete[] in_degree_arr;
    delete[] fitness_arr;
    delete[] num_authors_arr;
    delete[] author_reputation_arr;
    delete[] pa_arr;
    delete[] fit_arr;
    delete[] na_arr;
    delete[] ar_arr;
    delete[] pa_weight_arr;
    delete[] fit_weight_arr;
    delete[] num_authors_weight_arr;
    delete[] author_reputation_weight_arr;
    delete[] out_degree_arr;
    delete[] random_weight_arr;
    delete[] current_score_arr;
    delete[] alpha_arr;
    delete[] fitness_lag_duration_arr;
    delete[] fitness_peak_value_arr;
    delete[] fitness_peak_duration_arr;

    delete graph;
    return 0;
}
