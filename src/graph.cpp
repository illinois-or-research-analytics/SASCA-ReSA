#include "graph.h"

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>

Graph::Graph(std::string edgelist, std::string nodelist, bool start_from_checkpoint, std::string num_authors_bag, int author_max_lifetime): edgelist(edgelist), nodelist(nodelist), start_from_checkpoint(start_from_checkpoint), num_authors_bag(num_authors_bag), author_max_lifetime(author_max_lifetime) {
    this->ReadNumAuthorsBag();
    this->ParseEdgelist();
    this->ParseNodelist();
}

bool EndsWithParquet(const std::string& path) {
    return path.size() >= 8 &&
           path.substr(path.size() - 8) == ".parquet";
}

std::shared_ptr<arrow::Table> ReadParquetTable(const std::string& path) {
    auto input_result = arrow::io::ReadableFile::Open(path);

    if (!input_result.ok()) {
        throw std::runtime_error("Could not open parquet file: " + path);
    }

    auto reader_result = parquet::arrow::OpenFile(
        input_result.ValueOrDie(),
        arrow::default_memory_pool()
    );

    if (!reader_result.ok()) {
        throw std::runtime_error("Could not open parquet reader: " + path);
    }

    auto table_result = reader_result.ValueOrDie()->ReadTable();

    if (!table_result.ok()) {
        throw std::runtime_error("Could not read parquet table: " + path);
    }

    return table_result.ValueOrDie();
}


void Graph::ParseEdgelist() {

    if (EndsWithParquet(this->edgelist)) {

        auto table = ReadParquetTable(this->edgelist);

        auto combined_result = table->CombineChunks();

        if (!combined_result.ok()) {
            throw std::runtime_error("Could not combine parquet chunks");
        }

        table = combined_result.ValueOrDie();

        auto source_column = table->GetColumnByName("source");
        auto target_column = table->GetColumnByName("target");

        if (!source_column || !target_column) {
            throw std::runtime_error(
                "Parquet edgelist must have source and target columns"
            );
        }

        auto source_array =
            std::static_pointer_cast<arrow::Int64Array>(
                source_column->chunk(0)
            );

        auto target_array =
            std::static_pointer_cast<arrow::Int64Array>(
                target_column->chunk(0)
            );

        for (int64_t i = 0; i < table->num_rows(); i++) {

            int source =
                static_cast<int>(source_array->Value(i));

            int target =
                static_cast<int>(target_array->Value(i));

            this->AddEdge({source, target});
        }

        return;
    }

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

    if (EndsWithParquet(this->nodelist)) {

        if (this->start_from_checkpoint) {
            throw std::runtime_error(
                "Parquet checkpoint input is not supported yet"
            );
        }

        auto table = ReadParquetTable(this->nodelist);

        auto combined_result = table->CombineChunks();

        if (!combined_result.ok()) {
            throw std::runtime_error(
                "Could not combine parquet chunks"
            );
        }

        table = combined_result.ValueOrDie();

        auto node_id_column =
            table->GetColumnByName("node_id");

        auto year_column =
            table->GetColumnByName("year");

        if (!node_id_column || !year_column) {
            throw std::runtime_error(
                "Parquet nodelist must have node_id and year columns"
            );
        }

        auto node_id_array =
            std::static_pointer_cast<arrow::Int64Array>(
                node_id_column->chunk(0)
            );

        auto year_array =
            std::static_pointer_cast<arrow::Int64Array>(
                year_column->chunk(0)
            );

        std::vector<std::pair<int, int>> node_year_vec;

        for (int64_t i = 0; i < table->num_rows(); i++) {

            int integer_node =
                static_cast<int>(
                    node_id_array->Value(i)
                );

            int integer_year =
                static_cast<int>(
                    year_array->Value(i)
                );

            this->SetIntAttribute(
                "year",
                integer_node,
                integer_year
            );

            node_year_vec.push_back(
                {integer_node, integer_year}
            );

            int fitness_lag_uniform = 0;
            int fitness_peak_uniform = 1000;
            int fitness_power = 1;

            this->SetStringAttribute(
                "type",
                integer_node,
                "seed"
            );

            this->SetIntAttribute(
                "fitness_lag_duration",
                integer_node,
                fitness_lag_uniform
            );

            this->SetIntAttribute(
                "fitness_peak_duration",
                integer_node,
                fitness_peak_uniform
            );

            this->SetIntAttribute(
                "fitness_peak_value",
                integer_node,
                fitness_power
            );
        }

        std::sort(
            node_year_vec.begin(),
            node_year_vec.end(),
            [](const std::pair<int, int>& left,
               const std::pair<int, int>& right) {
                return left.second < right.second;
            }
        );

        size_t previous_index = 0;
        int previous_year =
            node_year_vec.at(previous_index).second;

        for (size_t i = 0;
             i < node_year_vec.size();
             i++) {

            int current_node_id =
                node_year_vec[i].first;

            int current_year =
                node_year_vec[i].second;

            int author_id =
                this->GetNextAuthor(
                    current_year,
                    std::set<int>()
                );

            this->SetIntAttribute(
                "author_id",
                current_node_id,
                author_id
            );

            this->UpdateAuthorPublicationMap(
                author_id,
                current_node_id
            );

            if (previous_year != current_year) {

                this->ComputeAuthorReputations();

                for (size_t j = previous_index;
                     j < i;
                     j++) {

                    int node_id =
                        node_year_vec.at(j).first;

                    int author_id =
                        this->GetIntAttribute(
                            "author_id",
                            node_id
                        );

                    this->SetIntAttribute(
                        "initial_author_reputation",
                        node_id,
                        this->author_reputation_map.at(
                            author_id
                        )
                    );
                }

                previous_index = i;

                previous_year =
                    node_year_vec.at(
                        previous_index
                    ).second;
            }
        }

        if (previous_index <
            node_year_vec.size()) {

            this->ComputeAuthorReputations();

            for (size_t j = previous_index;
                 j < node_year_vec.size();
                 j++) {

                int node_id =
                    node_year_vec.at(j).first;

                int author_id =
                    this->GetIntAttribute(
                        "author_id",
                        node_id
                    );

                this->SetIntAttribute(
                    "initial_author_reputation",
                    node_id,
                    this->author_reputation_map.at(
                        author_id
                    )
                );
            }
        }

        return;
    }

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
                int cartel_id = std::stoi(current_line[header_to_index_map["cartel_id"]]);
                if (cartel_id != -1) {
                    this->SetIntAttribute("cartel_id", integer_node, cartel_id);
                    this->cartel_set.insert(cartel_id);
                    this->SetCartelID(author, cartel_id);
                }
                if(this->author_birth_year_map.contains(author)) {
                    this->author_birth_year_map[author] = std::min(this->author_birth_year_map[author], integer_year);
                } else {
                    this->author_birth_year_map[author] = integer_year;
                }
                this->UpdateAuthorPublicationMap(author, integer_node);
                this->next_author_id = std::max(this->next_author_id, author);
            } else {
                int fitness_lag_uniform = 0; // MARK: hard coded to be static fitness
                int fitness_peak_uniform = 1000; // MARK: hard coded to be static fitness
                int fitness_power = 1;
                this->SetStringAttribute("type", integer_node, "seed");
                this->SetIntAttribute("fitness_lag_duration", integer_node, fitness_lag_uniform);
                this->SetIntAttribute("fitness_peak_duration", integer_node, fitness_peak_uniform);
                this->SetIntAttribute("fitness_peak_value", integer_node, fitness_power);
            }
        }
        line_no ++;
    }
    if (this->start_from_checkpoint) {
        this->next_author_id ++;
        for(const auto& [author_id, birth_year]: author_birth_year_map) {
            int author_id_publication_count = this->author_publication_map.at(author_id).size();
            this->publication_count_to_author_map[author_id_publication_count].push_back(author_id);
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
            int author_id = this->GetNextAuthor(current_year, std::set<int>());
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
        if (previous_index < node_year_vec.size()) {
            this->ComputeAuthorReputations();
            for(size_t j = previous_index; j < node_year_vec.size(); j ++) {
                int node_id = node_year_vec.at(j).first;
                int author_id = this->GetIntAttribute("author_id", node_id);
                this->SetIntAttribute("initial_author_reputation", node_id, this->author_reputation_map.at(author_id));
            }
        }
    }
}

std::set<int> Graph::GetCartelSet() const {
    return this->cartel_set;
}

void Graph::SetCartelID(int author, int cartel_id) {
    this->author_cartel_map[author] = cartel_id;
    this->cartel_author_map[cartel_id].insert(author);
}

std::set<int> Graph::GetCartelAuthors(int cartel_id) const {
    if (cartel_id == -1) {
        return std::set<int>();
    }
    return this->cartel_author_map.at(cartel_id);
}

std::vector<int> Graph::GetAuthorPublications(int author_id) const {
    return this->author_publication_map.at(author_id);
}

int Graph::GetCartelID(int author) const {
    if (this->author_cartel_map.contains(author)) {
        return this->author_cartel_map.at(author);
    }
    return -1;
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

void Graph::UpdateAuthorManual(int author_id) {
    int num_publications_by_author = this->author_publication_map[author_id].size();
    std::erase(this->publication_count_to_author_map[num_publications_by_author], author_id);
    this->publication_count_to_author_map[num_publications_by_author + 1].push_back(author_id);
}


int Graph::GetNextAuthor(int current_year, const std::set<int>& exclusion_set) {
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
            if (current_year - this->author_birth_year_map[current_author] < this->author_max_lifetime && !exclusion_set.contains(current_author)) {
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
            std::cout << u << "-" << v << "\n";
            /* } */
        }
    }
}

void Graph::WriteGraph(std::string output_file) const {
    if (EndsWithParquet(output_file)) {
        arrow::Int64Builder source_builder;
        arrow::Int64Builder target_builder;
        for (auto const& [u, u_neighbors] : this->GetForwardAdjMap()) {
            for (const int& v : u_neighbors) {
                if (!source_builder.Append(u).ok()) {
                    throw std::runtime_error("Could not append source");
                }
                if (!target_builder.Append(v).ok()) {
                    throw std::runtime_error("Could not append target");
                }
            }
        }
        std::shared_ptr<arrow::Array> source_array;
        std::shared_ptr<arrow::Array> target_array;

        auto source_status = source_builder.Finish(&source_array);
        auto target_status = target_builder.Finish(&target_array);

        if (!source_status.ok() || !target_status.ok()) {
            throw std::runtime_error("Could not build parquet edge arrays");
        }

        auto schema = arrow::schema({
            arrow::field("source", arrow::int64()),
            arrow::field("target", arrow::int64())
        });

        auto table = arrow::Table::Make(
            schema,
            {source_array, target_array}
        );

        auto output_result =
            arrow::io::FileOutputStream::Open(output_file);
        if (!output_result.ok()) {
            throw std::runtime_error(
                "Could not open parquet output file: " + output_file
            );
        }

        auto status = parquet::arrow::WriteTable(
            *table,
            arrow::default_memory_pool(),
            output_result.ValueOrDie(),
            1024
        );

        if (!status.ok()) {
            throw std::runtime_error(
                "Could not write parquet file: " + output_file
            );
        }
        return;
    }
    std::ofstream output_filehandle(output_file);
    output_filehandle << "source,target\n";
    for(auto const& [u,u_neighbors] : this->GetForwardAdjMap()) {
        for(const int& v : u_neighbors) {
            /* if (u < v) { */
            output_filehandle << u << "," << v << "\n";
            /* } */
        }
    }
    output_filehandle.close();
}

void Graph::WriteAttributes(std::string auxiliary_information_file) const {

    if (EndsWithParquet(auxiliary_information_file)) {

        arrow::Int64Builder node_id_builder;
        arrow::StringBuilder type_builder;
        arrow::Int64Builder year_builder;
        arrow::DoubleBuilder alpha_builder;
        arrow::DoubleBuilder pa_weight_builder;
        arrow::DoubleBuilder fit_weight_builder;
        arrow::DoubleBuilder num_authors_weight_builder;
        arrow::DoubleBuilder author_reputation_weight_builder;
        arrow::Int64Builder fit_lag_duration_builder;
        arrow::Int64Builder fit_peak_value_builder;
        arrow::Int64Builder fit_peak_duration_builder;
        arrow::Int64Builder in_degree_builder;
        arrow::Int64Builder out_degree_builder;
        arrow::Int64Builder assigned_out_degree_builder;
        arrow::Int64Builder planted_nodes_line_number_builder;
        arrow::StringBuilder generator_node_string_builder;
        arrow::Int64Builder sampled_neighborhood_size_builder;
        arrow::Int64Builder fully_random_citations_builder;
        arrow::Int64Builder author_id_builder;
        arrow::Int64Builder num_authors_builder;
        arrow::Int64Builder initial_author_reputation_builder;
        arrow::Int64Builder final_author_reputation_builder;
        arrow::Int64Builder cartel_id_builder;

        for (const auto& node_id : this->GetNodeSet()) {

            std::string node_type =
                this->GetStringAttribute("type", node_id);

            int year =
                this->GetIntAttribute("year", node_id);

            double pa_weight = -1;
            double fit_weight = -1;
            double num_authors_weight = -1;
            double author_reputation_weight = -1;
            double alpha = -1;

            int fit_lag_duration =
                this->GetIntAttribute("fitness_lag_duration", node_id);

            int fit_peak_value =
                this->GetIntAttribute("fitness_peak_value", node_id);

            int fit_peak_duration =
                this->GetIntAttribute("fitness_peak_duration", node_id);

            int out_degree =
                this->GetIntAttribute("out_degree", node_id);

            int assigned_out_degree = -1;

            int in_degree =
                this->GetIntAttribute("in_degree", node_id);

            int author =
                this->GetIntAttribute("author_id", node_id);

            int num_authors =
                this->GetIntAttribute("num_authors", node_id);

            int planted_nodes_line_number = -1;
            std::string generator_node_string = "no_generators";
            int neighborhood_size = -1;
            int fully_random_citations = -1;

            int initial_author_reputation =
                this->GetIntAttribute(
                    "initial_author_reputation",
                    node_id
                );

            int final_author_reputation =
                this->GetAuthorReputationForNode(node_id);

            int cartel_id =
                this->GetCartelID(author);

            if (node_type != "seed") {

                alpha =
                    this->GetDoubleAttribute("alpha", node_id);

                pa_weight =
                    this->GetDoubleAttribute("pa_weight", node_id);

                fit_weight =
                    this->GetDoubleAttribute("fit_weight", node_id);

                num_authors_weight =
                    this->GetDoubleAttribute(
                        "num_authors_weight",
                        node_id
                    );

                author_reputation_weight =
                    this->GetDoubleAttribute(
                        "author_reputation_weight",
                        node_id
                    );

                assigned_out_degree =
                    this->GetIntAttribute(
                        "assigned_out_degree",
                        node_id
                    );

                generator_node_string =
                    this->GetStringAttribute(
                        "generator_node_string",
                        node_id
                    );

                if (this->HasIntAttribute(
                    "planted_nodes_line_number",
                    node_id
                )) {
                    planted_nodes_line_number =
                        this->GetIntAttribute(
                            "planted_nodes_line_number",
                            node_id
                        );
                }

                neighborhood_size =
                    this->GetIntAttribute(
                        "sampled_neighborhood_size",
                        node_id
                    );

                fully_random_citations =
                    this->GetIntAttribute(
                        "fully_random_citations",
                        node_id
                    );
            }

            node_id_builder.Append(node_id);
            type_builder.Append(node_type);
            year_builder.Append(year);
            alpha_builder.Append(alpha);
            pa_weight_builder.Append(pa_weight);
            fit_weight_builder.Append(fit_weight);
            num_authors_weight_builder.Append(num_authors_weight);
            author_reputation_weight_builder.Append(
                author_reputation_weight
            );
            fit_lag_duration_builder.Append(fit_lag_duration);
            fit_peak_value_builder.Append(fit_peak_value);
            fit_peak_duration_builder.Append(fit_peak_duration);
            in_degree_builder.Append(in_degree);
            out_degree_builder.Append(out_degree);
            assigned_out_degree_builder.Append(
                assigned_out_degree
            );
            planted_nodes_line_number_builder.Append(
                planted_nodes_line_number
            );
            generator_node_string_builder.Append(
                generator_node_string
            );
            sampled_neighborhood_size_builder.Append(
                neighborhood_size
            );
            fully_random_citations_builder.Append(
                fully_random_citations
            );
            author_id_builder.Append(author);
            num_authors_builder.Append(num_authors);
            initial_author_reputation_builder.Append(
                initial_author_reputation
            );
            final_author_reputation_builder.Append(
                final_author_reputation
            );
            cartel_id_builder.Append(cartel_id);
        }

        std::shared_ptr<arrow::Array> node_id_array;
        std::shared_ptr<arrow::Array> type_array;
        std::shared_ptr<arrow::Array> year_array;
        std::shared_ptr<arrow::Array> alpha_array;
        std::shared_ptr<arrow::Array> pa_weight_array;
        std::shared_ptr<arrow::Array> fit_weight_array;
        std::shared_ptr<arrow::Array> num_authors_weight_array;
        std::shared_ptr<arrow::Array> author_reputation_weight_array;
        std::shared_ptr<arrow::Array> fit_lag_duration_array;
        std::shared_ptr<arrow::Array> fit_peak_value_array;
        std::shared_ptr<arrow::Array> fit_peak_duration_array;
        std::shared_ptr<arrow::Array> in_degree_array;
        std::shared_ptr<arrow::Array> out_degree_array;
        std::shared_ptr<arrow::Array> assigned_out_degree_array;
        std::shared_ptr<arrow::Array> planted_nodes_line_number_array;
        std::shared_ptr<arrow::Array> generator_node_string_array;
        std::shared_ptr<arrow::Array> sampled_neighborhood_size_array;
        std::shared_ptr<arrow::Array> fully_random_citations_array;
        std::shared_ptr<arrow::Array> author_id_array;
        std::shared_ptr<arrow::Array> num_authors_array;
        std::shared_ptr<arrow::Array> initial_author_reputation_array;
        std::shared_ptr<arrow::Array> final_author_reputation_array;
        std::shared_ptr<arrow::Array> cartel_id_array;

        node_id_builder.Finish(&node_id_array);
        type_builder.Finish(&type_array);
        year_builder.Finish(&year_array);
        alpha_builder.Finish(&alpha_array);
        pa_weight_builder.Finish(&pa_weight_array);
        fit_weight_builder.Finish(&fit_weight_array);
        num_authors_weight_builder.Finish(
            &num_authors_weight_array
        );
        author_reputation_weight_builder.Finish(
            &author_reputation_weight_array
        );
        fit_lag_duration_builder.Finish(
            &fit_lag_duration_array
        );
        fit_peak_value_builder.Finish(
            &fit_peak_value_array
        );
        fit_peak_duration_builder.Finish(
            &fit_peak_duration_array
        );
        in_degree_builder.Finish(&in_degree_array);
        out_degree_builder.Finish(&out_degree_array);
        assigned_out_degree_builder.Finish(
            &assigned_out_degree_array
        );
        planted_nodes_line_number_builder.Finish(
            &planted_nodes_line_number_array
        );
        generator_node_string_builder.Finish(
            &generator_node_string_array
        );
        sampled_neighborhood_size_builder.Finish(
            &sampled_neighborhood_size_array
        );
        fully_random_citations_builder.Finish(
            &fully_random_citations_array
        );
        author_id_builder.Finish(&author_id_array);
        num_authors_builder.Finish(&num_authors_array);
        initial_author_reputation_builder.Finish(
            &initial_author_reputation_array
        );
        final_author_reputation_builder.Finish(
            &final_author_reputation_array
        );
        cartel_id_builder.Finish(&cartel_id_array);

        auto schema = arrow::schema({
            arrow::field("node_id", arrow::int64()),
            arrow::field("type", arrow::utf8()),
            arrow::field("year", arrow::int64()),
            arrow::field("alpha", arrow::float64()),
            arrow::field("pa_weight", arrow::float64()),
            arrow::field("fit_weight", arrow::float64()),
            arrow::field("num_authors_weight", arrow::float64()),
            arrow::field(
                "author_reputation_weight",
                arrow::float64()
            ),
            arrow::field(
                "fit_lag_duration",
                arrow::int64()
            ),
            arrow::field(
                "fit_peak_value",
                arrow::int64()
            ),
            arrow::field(
                "fit_peak_duration",
                arrow::int64()
            ),
            arrow::field("in_degree", arrow::int64()),
            arrow::field("out_degree", arrow::int64()),
            arrow::field(
                "assigned_out_degree",
                arrow::int64()
            ),
            arrow::field(
                "planted_nodes_line_number",
                arrow::int64()
            ),
            arrow::field(
                "generator_node_string",
                arrow::utf8()
            ),
            arrow::field(
                "sampled_neighborhood_size",
                arrow::int64()
            ),
            arrow::field(
                "fully_random_citations",
                arrow::int64()
            ),
            arrow::field("author_id", arrow::int64()),
            arrow::field("num_authors", arrow::int64()),
            arrow::field(
                "initial_author_reputation",
                arrow::int64()
            ),
            arrow::field(
                "final_author_reputation",
                arrow::int64()
            ),
            arrow::field("cartel_id", arrow::int64())
        });

        auto table = arrow::Table::Make(
            schema,
            {
                node_id_array,
                type_array,
                year_array,
                alpha_array,
                pa_weight_array,
                fit_weight_array,
                num_authors_weight_array,
                author_reputation_weight_array,
                fit_lag_duration_array,
                fit_peak_value_array,
                fit_peak_duration_array,
                in_degree_array,
                out_degree_array,
                assigned_out_degree_array,
                planted_nodes_line_number_array,
                generator_node_string_array,
                sampled_neighborhood_size_array,
                fully_random_citations_array,
                author_id_array,
                num_authors_array,
                initial_author_reputation_array,
                final_author_reputation_array,
                cartel_id_array
            }
        );

        auto output_result =
            arrow::io::FileOutputStream::Open(
                auxiliary_information_file
            );

        if (!output_result.ok()) {
            throw std::runtime_error(
                "Could not open parquet output file: " +
                auxiliary_information_file
            );
        }

        auto status = parquet::arrow::WriteTable(
            *table,
            arrow::default_memory_pool(),
            output_result.ValueOrDie(),
            1024
        );

        if (!status.ok()) {
            throw std::runtime_error(
                "Could not write parquet attributes file"
            );
        }

        return;
    }


    std::ofstream auxiliary_information_filehandle(auxiliary_information_file);
    auxiliary_information_filehandle << "node_id,type,year,alpha,pa_weight,fit_weight,num_authors_weight,author_reputation_weight,fit_lag_duration,fit_peak_value,fit_peak_duration,in_degree,out_degree,assigned_out_degree,planted_nodes_line_number,generator_node_string,sampled_neighborhood_size,fully_random_citations,author_id,num_authors,initial_author_reputation,final_author_reputation,cartel_id\n";
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
        int cartel_id = this->GetCartelID(author);
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
        auxiliary_information_filehandle << node_id << "," << node_type << "," << year << "," << alpha << "," << pa_weight << "," << fit_weight << "," << num_authors_weight << "," << author_reputation_weight << "," << fit_lag_duration << "," << fit_peak_value << "," << fit_peak_duration << "," << in_degree << "," << out_degree << "," << assigned_out_degree << "," << planted_nodes_line_number << "," << generator_node_string << "," << neighborhood_size << "," << fully_random_citations << "," << author << "," << num_authors << "," << initial_author_reputation << "," << final_author_reputation << "," << cartel_id << "\n";
    }
    auxiliary_information_filehandle.close();
}
