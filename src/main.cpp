#include <iostream>

#include "argparse.hpp"
#include "INIReader.h"
#include "abm.h"


int main(int argc, char* argv[]) {
    argparse::ArgumentParser main_program("abm");

    main_program.add_description("Agent Based Modelling");
    main_program.add_argument("--config")
        .required()
        .help(".ini Configuration fileuration file. Example provided in docs/");

    try {
        main_program.parse_args(argc, argv);
    } catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << main_program;
        return 1;
    }
    std::string config_file = main_program.get<std::string>("--config");
    INIReader reader(config_file);
    int error_code = reader.ParseError();
    if (error_code != 0) {
        std::cout << reader.ParseErrorMessage() << std::endl;;
        return 1;
    }
    const std::unordered_map<std::string, std::vector<std::string>> required_params = {
        {"Environment", {"edgelist", "nodelist", "out_degree_bag", "recency_table", "planted_nodes", "growth_rate", "num_cycles", "recency_bins", "start_from_checkpoint"}},
        {"Agent", {"fully_random_citations", "preferential_weight", "fitness_weight", "num_authors_weight", "author_reputation_weight", "fitness_value_min", "fitness_value_max", "fitness_lag_duration_min", "fitness_lag_duration_max", "fitness_peak_duration_min", "fitness_peak_duration_max", "same_year_citations", "neighborhood_sample", "num_authors_bag", "author_max_lifetime", "cartel_outdegree_proportion", "null_cartel", "clonal_cartel_agent_file", "alpha", "use_alpha", "in_degree_threshold", "fitness_threshold", "recency_threshold", "non_random_generator_probability"}},
        {"General", {"output_file", "auxiliary_information_file", "log_file", "num_processors", "log_level"}}
    };
    for(auto const& [section_name, section_expected_variables] : required_params) {
        if(!reader.HasSection(section_name)) {
            std::cerr << "Required section: " + section_name + " is missing" << std::endl;
            std::cerr << "The list of required sections is: ";
            bool first = true;
            for(auto const& [diagnostic_section_name, diagnostic_section_expected_variables] : required_params) {
                if (first) {
                    first = false;
                } else {
                    std::cerr << ", ";
                }
                std::cerr << diagnostic_section_name;
            }
            std::cerr << std::endl;
            exit(1);
        }
        for(auto const& section_expected_variable : section_expected_variables) {
            if(!reader.HasValue(section_name, section_expected_variable)) {
                std::cerr << "Required value: " + section_expected_variable + " in section: " + section_name + " is missing" << std::endl;
                std::cerr << "The list of required variables for section " + section_name + " is: ";
                bool first = true;
                for(auto const& diagnostic_section_expected_variable : section_expected_variables) {
                    if (first) {
                        first = false;
                    } else {
                        std::cerr << ", ";
                    }
                    std::cerr << diagnostic_section_expected_variable;
                }
                std::cerr << std::endl;
                exit(1);
            }
        }
    }


    std::string edgelist = reader.Get("Environment", "edgelist", "");
    std::string nodelist = reader.Get("Environment", "nodelist", "");
    std::string out_degree_bag = reader.Get("Environment", "out_degree_bag", "");
    std::string recency_table = reader.Get("Environment", "recency_table", "");
    std::string planted_nodes = reader.Get("Environment", "planted_nodes", "");
    double growth_rate = reader.GetReal("Environment", "growth_rate", -42);
    int num_cycles = reader.GetInteger("Environment", "num_cycles", -42);
    double fully_random_citations = reader.GetReal("Agent", "fully_random_citations", -42);
    double preferential_weight = reader.GetReal("Agent", "preferential_weight", -42);
    double fitness_weight = reader.GetReal("Agent", "fitness_weight", -42);
    double num_authors_weight = reader.GetReal("Agent", "num_authors_weight", -42);
    double author_reputation_weight = reader.GetReal("Agent", "author_reputation_weight", -42);
    int fitness_value_min = reader.GetInteger("Agent", "fitness_value_min", -42);
    int fitness_value_max = reader.GetInteger("Agent", "fitness_value_max", -42);
    int fitness_lag_duration_min = reader.GetInteger("Agent", "fitness_lag_duration_min", -42);
    int fitness_lag_duration_max = reader.GetInteger("Agent", "fitness_lag_duration_max", -42);
    int fitness_peak_duration_min = reader.GetInteger("Agent", "fitness_peak_duration_min", -42);
    int fitness_peak_duration_max = reader.GetInteger("Agent", "fitness_peak_duration_max", -42);
    double minimum_preferential_weight = reader.GetReal("Agent", "minimum_preferential_weight", -42); // unused
    double minimum_fitness_weight = reader.GetReal("Agent", "minimum_fitness_weight", -42); // unused
    double same_year_citations = reader.GetReal("Agent", "same_year_citations", -42);
    int neighborhood_sample = reader.GetInteger("Agent", "neighborhood_sample", -42);
    std::string num_authors_bag = reader.Get("Agent", "num_authors_bag", "");
    int author_max_lifetime = reader.GetInteger("Agent", "author_max_lifetime", -42);
    double cartel_outdegree_proportion = reader.GetReal("Agent", "cartel_outdegree_proportion", -42);
    std::string null_cartel_string = reader.Get("Agent", "null_cartel", "");
    bool null_cartel = false;
    if (null_cartel_string == "true") {
        null_cartel = true;
    } else if (null_cartel_string == "false") {
        null_cartel = false;
    } else {
        std::cerr << "Valid values for required flag null_cartel are 'true' or 'false'" << std::endl;
        std::cerr << null_cartel_string + " is invalid" << std::endl;
        return 1;
    }
    double alpha = reader.GetReal("Agent", "alpha", -42);
    double minimum_alpha = reader.GetReal("Agent", "minimum_alpha", -42); // unused
    std::string use_alpha_string = reader.Get("Agent", "use_alpha", "");
    bool use_alpha = false;
    if (use_alpha_string == "true") {
        use_alpha = true;
    } else if (use_alpha_string == "false") {
        use_alpha = false;
    } else {
        std::cerr << "Valid values for required flag use_alpha are 'true' or 'false'" << std::endl;
        return 1;
    }
    int in_degree_threshold = reader.GetInteger("Agent", "in_degree_threshold", -42);
    int fitness_threshold = reader.GetInteger("Agent", "fitness_threshold", -42);
    int recency_threshold = reader.GetInteger("Agent", "recency_threshold", -42);
    double non_random_generator_probability = reader.GetReal("Agent", "non_random_generator_probability", -42);
    std::string start_from_checkpoint_string = reader.Get("Environment", "start_from_checkpoint", "");
    bool start_from_checkpoint = false;
    if (start_from_checkpoint_string == "true") {
        start_from_checkpoint = true;
    } else if (start_from_checkpoint_string == "false") {
        start_from_checkpoint = false;
    } else {
        std::cerr << "Valid values for required flag start_from_checkpoint are 'true' or 'false'" << std::endl;
        std::cerr << start_from_checkpoint_string + " is invalid" << std::endl;
        return 1;
    }
    std::string recency_bins = reader.Get("Environment", "recency_bins", "");
    std::string output_file = reader.Get("General", "output_file", "");
    std::string clonal_cartel_agent_file = reader.Get("Agent", "clonal_cartel_agent_file", "");
    std::string auxiliary_information_file = reader.Get("General", "auxiliary_information_file", "");
    std::string log_file = reader.Get("General", "log_file", "");
    int num_processors = reader.GetInteger("General", "num_processors", -42);
    int log_level = reader.GetInteger("General", "log_level", -41) - 1;
    ABM* abm = new ABM(edgelist, nodelist, out_degree_bag, recency_table, recency_bins, alpha, minimum_alpha, use_alpha, start_from_checkpoint, planted_nodes, fully_random_citations, preferential_weight, fitness_weight, num_authors_weight, author_reputation_weight, fitness_value_min, fitness_value_max,  fitness_lag_duration_min, fitness_lag_duration_max, fitness_peak_duration_min, fitness_peak_duration_max, minimum_preferential_weight, minimum_fitness_weight, in_degree_threshold, fitness_threshold, recency_threshold, non_random_generator_probability, growth_rate, num_cycles, same_year_citations, neighborhood_sample, num_authors_bag, author_max_lifetime, cartel_outdegree_proportion, null_cartel, output_file, clonal_cartel_agent_file, auxiliary_information_file, log_file, num_processors, log_level);
    abm->main();
    delete abm;
}
