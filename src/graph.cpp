#include "graph.h"


Graph::Graph(std::string edgelist, std::string nodelist, bool start_from_checkpoint, std::string num_authors_bag, int author_max_lifetime): edgelist(edgelist), nodelist(nodelist), start_from_checkpoint(start_from_checkpoint), num_authors_bag(num_authors_bag), author_max_lifetime(author_max_lifetime) {
    this->ReadNumAuthorsBag();
    this->ParseEdgelist();
    this->ParseNodelist();
    // this->ComputeAuthorReputations();
    // this->SaveInitialAuthorReputations();
}


void Graph::ParseEdgelist() {
    char delimiter = Graph::GetDelimiter(this->edgelist);
    std::ifstream input_edgelist(this->edgelist);
    std::string line;
    int line_no = 0;
    while(std::getline(input_edgelist, line)) {
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
            int integer_citing = std::stoi(current_line[0]);
            int integer_cited = std::stoi(current_line[1]);
            this->AddEdge({integer_citing, integer_cited});
        }
        line_no ++;
    }
}

void Graph::ParseNodelist() {
    char delimiter = Graph::GetDelimiter(this->nodelist);
    std::unordered_map<std::string, int> header_to_index_map = Graph::GetHeaderToIndexMap(delimiter, this->nodelist);
    std::ifstream input_nodelist(this->nodelist);
    std::string line;
    int line_no = 0;
    std::vector<std::pair<int, int>> node_year_vec;
    while(std::getline(input_nodelist, line)) {
        std::stringstream ss(line);
        std::string current_value;
        std::vector<std::string> current_line;
        while(std::getline(ss, current_value, delimiter)) {
            current_line.push_back(current_value);
        }
        if(current_line.size() == 0) {
            break;
        }
        if (line_no != 0) {
            int integer_node = std::stoi(current_line[header_to_index_map["node_id"]]);
            int integer_year = std::stoi(current_line[header_to_index_map["year"]]);
            this->SetIntAttribute("year", integer_node, integer_year);
            node_year_vec.push_back({integer_node, integer_year});
            if (this->start_from_checkpoint) {
                std::string type_string = current_line[header_to_index_map["type"]];
                this->SetStringAttribute("type", integer_node, type_string);
                double alpha = std::stod(current_line[header_to_index_map["alpha"]]);
                this->SetDoubleAttribute("alpha", integer_node, alpha);
                double pa_weight = std::stod(current_line[header_to_index_map["pa_weight"]]);
                this->SetDoubleAttribute("pa_weight", integer_node, pa_weight);
                double fit_weight = std::stod(current_line[header_to_index_map["fit_weight"]]);
                this->SetDoubleAttribute("fit_weight", integer_node, fit_weight);
                int fit_lag_duration = std::stoi(current_line[header_to_index_map["fit_lag_duration"]]);
                this->SetIntAttribute("fitness_lag_duration", integer_node, fit_lag_duration);
                int fit_peak_value = std::stoi(current_line[header_to_index_map["fit_peak_value"]]);
                this->SetIntAttribute("fitness_peak_value", integer_node, fit_peak_value);
                int fit_peak_duration = std::stoi(current_line[header_to_index_map["fit_peak_duration"]]);
                this->SetIntAttribute("fitness_peak_duration", integer_node, fit_peak_duration);
                int assigned_out_degree = std::stoi(current_line[header_to_index_map["assigned_out_degree"]]);
                this->SetIntAttribute("assigned_out_degree", integer_node, assigned_out_degree);
                int planted_nodes_line_number = std::stoi(current_line[header_to_index_map["planted_nodes_line_number"]]);
                this->SetIntAttribute("planted_nodes_line_number", integer_node, planted_nodes_line_number);
                int sampled_neighborhood_size = std::stoi(current_line[header_to_index_map["sampled_neighborhood_size"]]);
                this->SetIntAttribute("sampled_neighborhood_size", integer_node, sampled_neighborhood_size);
                std::string generator_node_string = current_line[header_to_index_map["generator_node_string"]];
                this->SetStringAttribute("generator_node_string", integer_node, generator_node_string);
                int fully_random_citations = std::stoi(current_line[header_to_index_map["fully_random_citations"]]);
                this->SetIntAttribute("fully_random_citations", integer_node, fully_random_citations);
                int author = std::stoi(current_line[header_to_index_map["author_id"]]);
                this->SetIntAttribute("author_id", integer_node, author);
                int initial_author_reputation = std::stoi(current_line[header_to_index_map["initial_author_reputation"]]);
                this->SetIntAttribute("initial_author_reputation", integer_node, initial_author_reputation);
                int num_authors = std::stoi(current_line[header_to_index_map["num_authors"]]);
                this->SetIntAttribute("num_authors", integer_node, num_authors);
                this->author_birth_year_map[author] = integer_year;
                this->UpdateAuthorPublicationMap(author, integer_node);
            } else {
                int fitness_lag_uniform = 0; // MARK: hard coded to be static fitness
                int fitness_peak_uniform = 1000; // MARK: hard coded to be static fitness
                int fitness_power = 1;
                // int author_id = this->GetNextAuthor();
                // int num_authors = this->GetNextNumAuthors();
                // std::cerr << "author id  " << author_id << " with count " << num_authors << std::endl;
                // int num_authors = this->GetNextNumAuthors();
                this->SetStringAttribute("type", integer_node, "seed");
                this->SetIntAttribute("fitness_lag_duration", integer_node, fitness_lag_uniform);
                this->SetIntAttribute("fitness_peak_duration", integer_node, fitness_peak_uniform);
                this->SetIntAttribute("fitness_peak_value", integer_node, fitness_power);
                // this->SetIntAttribute("num_authors", integer_node, num_authors);
                // this->SetIntAttribute("author", integer_node, author_id);
                // this->SetIntAttribute("num_authors", integer_node, num_authors);
            }
        }
        line_no ++;
    }
    if (this->start_from_checkpoint) {
        // for(size_t i = 0; i < this->author_birth_year_map.size(); i ++) {
        //     int current_author = this->author_birth_year_map[i];
        //     // this->author_reputation_map[current_author] ++;
        // }
        for(size_t i = 0; i < this->author_birth_year_map.size(); i ++) {
            int current_author = this->author_birth_year_map[i];
            int current_author_publication_count = this->author_publication_map.at(current_author).size();
            this->publication_count_to_author_map[current_author_publication_count].push_back(current_author);
        }
    } else {
        std::sort(node_year_vec.begin(), node_year_vec.end(), [](const std::pair<int, int>& left, const std::pair<int, int>& right) {
            return left.second < right.second;
        });
        size_t previous_index = 0;
        int previous_year = node_year_vec.at(previous_index).second;
        for(size_t i = 0; i < node_year_vec.size(); i ++) {
            int current_node_id = node_year_vec[i].first;
            int current_year = node_year_vec[i].second;
            int author_id = this->GetNextAuthor(current_year);
            this->SetIntAttribute("author_id", current_node_id, author_id);
            this->UpdateAuthorPublicationMap(author_id, current_node_id);
            if (previous_year != current_year) {
                this->ComputeAuthorReputations();
                for(size_t j = previous_index; j < i; j ++) {
                    int node_id = node_year_vec.at(j).first;
                    int author_id = this->GetIntAttribute("author_id", node_id);
                    this->SetIntAttribute("initial_author_reputation", node_id, this->author_reputation_map.at(author_id));
                }
                previous_index = i;
                previous_year = node_year_vec.at(previous_index).second;
            }
        }
        if (previous_index != node_year_vec.size() - 1) {
            this->ComputeAuthorReputations();
            for(size_t j = previous_index; j < node_year_vec.size(); j ++) {
                int node_id = node_year_vec.at(j).first;
                int author_id = this->GetIntAttribute("author_id", node_id);
                this->SetIntAttribute("initial_author_reputation", node_id, this->author_reputation_map.at(author_id));
            }
        }
    }
}

void Graph::UpdateAuthorPublicationMap(int author, int node) {
    this->author_publication_map[author].push_back(node);
}

void Graph::ComputeAuthorReputations() {
    for (const auto& [author_id, birth_year] : this->author_birth_year_map) {
        const std::vector<int>& publication_vec = this->author_publication_map.at(author_id);
        int h_index = 0;
        if (!publication_vec.empty()) {
            std::unordered_map<int, int> freq_map;
            for(size_t i = 0; i < publication_vec.size(); i ++) {
                int current_publication = publication_vec.at(i);
                size_t current_publication_in_degree = this->GetInDegree(current_publication);
                freq_map[std::min(publication_vec.size(), current_publication_in_degree)] ++;
            }
            h_index = publication_vec.size();
            int num_candidate_papers = freq_map[h_index];
            while(h_index > num_candidate_papers) {
                h_index --;
                num_candidate_papers += freq_map[h_index];
            }
        }
        this->author_reputation_map[author_id] = h_index;
    }
}

void Graph::SaveInitialAuthorReputations() {
    for(const auto& node_id : this->GetNodeSet()) {
        int author_id = this->GetIntAttribute("author_id", node_id);
        this->SetIntAttribute("initial_author_reputation", node_id, this->author_reputation_map.at(author_id));
    }
}

int Graph::GetAuthorReputationForNode(int node) const {
    int author_id = this->GetIntAttribute("author_id", node);
    return this->author_reputation_map.at(author_id);
    // const std::vector<int>& publication_vec = this->author_publication_map.at(author_id);
    // if (publication_vec.empty()) {
    //     return 0;
    // }
    // std::unordered_map<int, int> freq_map;
    // for(size_t i = 0; i < publication_vec.size(); i ++) {
    //     int current_publication = publication_vec.at(i);
    //     size_t current_publication_in_degree = this->GetInDegree(current_publication);
    //     freq_map[std::min(publication_vec.size(), current_publication_in_degree)] ++;
    // }
    // int h_index = publication_vec.size();
    // int num_candidate_papers = freq_map[h_index];
    // while(h_index > num_candidate_papers) {
    //     h_index --;
    //     num_candidate_papers += freq_map[h_index];
    // }
    // return h_index;
}

int Graph::GetNextNumAuthors() {
    std::uniform_int_distribution<int> num_authors_uniform_distribution{0, (int)(this->num_authors_bag_vec.size() - 1)};
    pcg_extras::seed_seq_from<std::random_device> rand_dev;
    pcg32 generator(rand_dev);
    int index_uniform = num_authors_uniform_distribution(generator);
    return this->num_authors_bag_vec[index_uniform];
}

void Graph::ReadNumAuthorsBag() {
    char delimiter = ',';
    std::ifstream out_degree_bag_stream(this->num_authors_bag);
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
            this->num_authors_bag_vec.push_back(std::stoi(current_line[1]));
        }
        line_no ++;
    }
}

int Graph::GetNextAuthor(int current_year) {
    int num_authors_with_one_paper = this->publication_count_to_author_map[1].size();
    bool found_valid_place = false;
    int proposed_publication_count_for_author = 2;
    int return_author = this->next_author_id;
    while (std::round(num_authors_with_one_paper / pow(proposed_publication_count_for_author, this->lotka_exponent)) >= 1) {
        int expected_num_authors_with_proposed_publication_count = std::round(num_authors_with_one_paper / pow(proposed_publication_count_for_author, this->lotka_exponent));
        int actual_num_authors_with_proposed_publication_count = this->publication_count_to_author_map[proposed_publication_count_for_author].size();
        int deficit = expected_num_authors_with_proposed_publication_count - actual_num_authors_with_proposed_publication_count;
        if (deficit > 1) {
            found_valid_place = true;
            break;
        }
        proposed_publication_count_for_author ++;
    }
    if (found_valid_place) {
        std::vector<int> living_authors;
        for(size_t i = 0; i < this->publication_count_to_author_map[proposed_publication_count_for_author - 1].size(); i ++) {
            int current_author = this->publication_count_to_author_map[proposed_publication_count_for_author - 1][i];
            if (current_year - this->author_birth_year_map[current_author] < this->author_max_lifetime) {
                living_authors.push_back(current_author);
            }
        }
        if (living_authors.size() > 0) {
            pcg_extras::seed_seq_from<std::random_device> rand_dev;
            pcg32 generator(rand_dev);
            std::ranges::shuffle(living_authors, generator);
            int upgraded_author_id = living_authors.back();
            return_author = upgraded_author_id;
            std::erase(this->publication_count_to_author_map[proposed_publication_count_for_author - 1], upgraded_author_id);
            this->publication_count_to_author_map[proposed_publication_count_for_author].push_back(upgraded_author_id);
        } else {
            found_valid_place = false;
        }
    }
    if (!found_valid_place) {
        this->publication_count_to_author_map[1].push_back(this->next_author_id);
        this->author_birth_year_map[this->next_author_id] = current_year;
        this->next_author_id ++;
    }
    // this->author_publication_map[return_author].push_back();
    return return_author;

}

void Graph::SetIntAttribute(std::string attribute_key, int node, int attribute_value) {
    this->int_attribute_map[attribute_key][node] = attribute_value;
}

int Graph::GetIntAttribute(std::string attribute_key, int node) const {
    return this->int_attribute_map.at(attribute_key).at(node);
}

void Graph::SetStringAttribute(std::string attribute_key, int node, std::string attribute_value) {
    this->string_attribute_map[attribute_key][node] = attribute_value;
}

std::string Graph::GetStringAttribute(std::string attribute_key, int node) const {
    return this->string_attribute_map.at(attribute_key).at(node);
}

void Graph::SetDoubleAttribute(std::string attribute_key, int node, double attribute_value) {
    this->double_attribute_map[attribute_key][node] = attribute_value;
}

double Graph::GetDoubleAttribute(std::string attribute_key, int node) const {
    return this->double_attribute_map.at(attribute_key).at(node);
}

bool Graph::HasIntAttribute(std::string attribute_key, int node) const {
    return this->int_attribute_map.contains(attribute_key) && this->int_attribute_map.at(attribute_key).contains(node);
}

void Graph::AddEdge(std::pair<int, int> edge) {
    this->forward_adj_map[edge.first].push_back(edge.second);
    this->backward_adj_map[edge.second].push_back(edge.first);
    this->AddNode(edge.first);
    this->AddNode(edge.second);
}

int Graph::GetInDegree(int node) const {
    if (this->backward_adj_map.contains(node)) {
        return this->backward_adj_map.at(node).size();
    }
    return 0;
}

int Graph::GetOutDegree(int node) const {
    if (this->forward_adj_map.contains(node)) {
        return this->forward_adj_map.at(node).size();
    }
    return 0;
}


void Graph::AddNode(int u) {
    this->node_set.insert(u);
}

const std::set<int>& Graph::GetNodeSet() const {
    return this->node_set;
}
const std::unordered_map<int, std::vector<int>>& Graph::GetForwardAdjMap() const {
    return this->forward_adj_map;
}

const std::unordered_map<int, std::vector<int>>& Graph::GetBackwardAdjMap() const {
    return this->backward_adj_map;
}

void Graph::PrintGraph() const {
    for(auto const& [u,u_neighbors] : this->GetForwardAdjMap()) {
        for(const int& v : u_neighbors) {
            /* if (u < v) { */
            std::cout << u << "-" << v << std::endl;
            /* } */
        }
    }
}

void Graph::WriteGraph(std::string output_file) const {
    std::ofstream output_filehandle(output_file);
    output_filehandle << "source,target" << std::endl;
    for(auto const& [u,u_neighbors] : this->GetForwardAdjMap()) {
        for(const int& v : u_neighbors) {
            /* if (u < v) { */
            output_filehandle << u << "," << v << std::endl;
            /* } */
        }
    }
    output_filehandle.close();
}

void Graph::WriteAttributes(std::string auxiliary_information_file) const {
    std::ofstream auxiliary_information_filehandle(auxiliary_information_file);
    auxiliary_information_filehandle << "node_id,type,year,alpha,pa_weight,fit_weight,num_authors_weight,author_reputation_weight,fit_lag_duration,fit_peak_value,fit_peak_duration,in_degree,out_degree,assigned_out_degree,planted_nodes_line_number,generator_node_string,sampled_neighborhood_size,fully_random_citations,author_id,num_authors,initial_author_reputation,final_author_reputation\n";
    for(const auto& node_id : this->GetNodeSet()) {
        std::string node_type = this->GetStringAttribute("type", node_id);
        int year = this->GetIntAttribute("year", node_id);
        double pa_weight = -1;
        double fit_weight = -1;
        double num_authors_weight = -1;
        double author_reputation_weight = -1;
        double alpha = -1;
        int fit_lag_duration = this->GetIntAttribute("fitness_lag_duration", node_id);
        int fit_peak_value = this->GetIntAttribute("fitness_peak_value", node_id);
        int fit_peak_duration = this->GetIntAttribute("fitness_peak_duration", node_id);
        int out_degree = this->GetIntAttribute("out_degree", node_id);
        int assigned_out_degree  = -1;
        int in_degree = this->GetIntAttribute("in_degree", node_id);
        int author = this->GetIntAttribute("author_id", node_id);
        int num_authors = this->GetIntAttribute("num_authors", node_id);
        int planted_nodes_line_number = -1;
        std::string generator_node_string  = "no_generators";
        int neighborhood_size = -1;
        int fully_random_citations = -1;
        int initial_author_reputation = this->GetIntAttribute("initial_author_reputation", node_id);
        int final_author_reputation = this->GetAuthorReputationForNode(node_id);
        if(node_type != "seed") {
            alpha = this->GetDoubleAttribute("alpha", node_id);
            pa_weight = this->GetDoubleAttribute("pa_weight", node_id);
            fit_weight = this->GetDoubleAttribute("fit_weight", node_id);
            num_authors_weight = this->GetDoubleAttribute("num_authors_weight", node_id);
            author_reputation_weight = this->GetDoubleAttribute("author_reputation_weight", node_id);
            assigned_out_degree = this->GetIntAttribute("assigned_out_degree", node_id);
            generator_node_string = this->GetStringAttribute("generator_node_string", node_id);
            if(this->HasIntAttribute("planted_nodes_line_number", node_id)) {
                planted_nodes_line_number = this->GetIntAttribute("planted_nodes_line_number", node_id);
            }
            neighborhood_size = this->GetIntAttribute("sampled_neighborhood_size", node_id);
            fully_random_citations = this->GetIntAttribute("fully_random_citations", node_id);
        }
        auxiliary_information_filehandle << node_id << "," << node_type << "," << year << "," << alpha << "," << pa_weight << "," << fit_weight << "," << num_authors_weight << "," << author_reputation_weight << "," << fit_lag_duration << "," << fit_peak_value << "," << fit_peak_duration << "," << in_degree << "," << out_degree << "," << assigned_out_degree << "," << planted_nodes_line_number << "," << generator_node_string << "," << neighborhood_size << "," << fully_random_citations << "," << author << "," << num_authors << "," << initial_author_reputation << "," << final_author_reputation << "\n";
    }
    auxiliary_information_filehandle.close();
}
