# SASCA(-ReSA): Scalable Agent-based Simulator for Citation Analysis (with Recency-emphasized Sampling and Author layer)
SASCA, or Scalable Agent-based Simulator for Citation Analysis, as the name suggests, is a scalable agent-based modeling simulator that can begin with a small seed network and simulate an exponential network growth to reach sizes of 100 million nodes and more. Currently, SASCA is implemented in modern C++ and can be run across hundreds of cores.

## Recent changelogs
6.2.2: Fixes a bug with number of authors and planted nodes feature where number of authors was not having an effect
6.2.1: functionality did not change but the output nodelist now has initial author reputation and final author reputation
6.2.0: introduces author layer with h-index as author reputation and updated planted nodes structure
6.1.0: introduces author layer and log-based score calculation

## Dependencies
- C++ >= 20
- OpenMP >= 4.0
- cmake >= 3.23
- Eigen3 (can be locally installed via [setup.sh](setup.sh))
- PCG (can be locally installed via [setup.sh](setup.sh))
- inih (can be locally installed via [setup.sh](setup.sh))
- argparse (can be locally installed via [setup.sh](setup.sh))

## One time setup
Run [setup.sh](setup.sh) to locally install [Eigen](https://eigen.tuxfamily.org/index.php?title=Main_Page), [PCG](https://www.pcg-random.org/), [inih](https://github.com/jtilly/inih.git), and [argparse](https://github.com/p-ranav/argparse.git). Alternatively, just ensure that these libraries are discoverable by cmake and adjust the build accordingly.

## How to build
SASCA is a standard cmake project. [easy_build_and_compile.sh](easy_build_and_compile.sh) is provided for user convenience.

### Running in a cluster environment
Below is the recommended way of building and running a standard cmake project on a cluster environment such as the Illinois Campus Cluster. This will ensure that the code is compiled with the right optimizations for the specific compute node it is assigned.
```console
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release <SASCA git root>
make
cd ..
./build/bin/abm --config <configuration file>
rm -r ./build
```

### Release mode node on the Illinois Campus Cluster
Building the project under the Release mode requires no modules to be loaded.

### Debug mode node on the Illinois Campus Cluster
Building the project under the Debug mode requires the ASAN libray. This library is not loaded properly on the campus cluster by default. For those using the Illinois Campus Cluster, load any `gcc` module version greater than 11 e.g., `module load gcc/12.4.0`.

## How to run
The general command is given below.
```console
abm --config <Configuration file>
```

An example configuration file, shown below, is also provided in the docs folder [Example configuration file](docs/example.ini)
Brief comments for each flag is listed inside the example configuration file. Detailed explanations for each of the flags are at the end of this section. All flags are required to be present in the configuration file. Unused flags can simply be left empty but the flag itself must still be present.


```
[Environment]
nodelist=<FILE> ; csv with (node_id, publication_year) on each line
edgelist=<FILE> ; csv with (source,target) on each line
recency_table=<FILE> ; csv derived from a real-world network
out_degree_bag=<FILE> ; csv derived from a real-world network
growth_rate=<DOUBLE> ; floating point value e.g., 0.03 for 3%
num_cycles=<INT> ; integer value e.g., 30 for a 30-year simulation
recency_bins=<STRING> ; string with comma separated values for each bin
planted_agents=<(optional) FILE> ; csv with planted agent with a planted agent per line.
start_from_checkpoint=<BOOL> ; boolean value e.g., true or false for whether to start from a checkpoint or not

[Agent]
alpha=<FLOAT> ; floating point value specifying the alpha for neighborhood
use_alpha=<BOOL> ; boolean value e.g., true or false for whether to use alpha or not
same_year_citations=<DOUBLE> ; floating point value e.g., 0.12 for 12%
fully_random_citations=<DOUBLE> ; floating point value e.g., 0.05 for 5%
preferential_weight=<DOUBLE> ; floating point value e.g., 0.33
fitness_weight=<DOUBLE> ; floating point value e.g., 0.33
fitness_value_min=<INT> ; minimum fitness value (inclusive)
fitness_value_max=<INT> ; maximum fitness value (inclusive)
neighborhood_sample=<INT> ; maximum number of nodes to sample from each neighborhood
in_degree_threshold=<INT> ; used for non-random generator selection. Selects the top <INT> percentile nodes by in-degree value
fitness_threshold=<INT> ; used for non-random generator selection. Selects the top <INT> percentile nodes by fitness value
recency_threshold=<INT> ; used for non-random generator selection. Selects only from the past <INT> years
non_random_generator_probability=<DOUBLE> ; used for non-random generator selection. 0.9 for 90% of the nodes picking non-random generators
author_max_lifetime=<INT> ; used for the maximum amount of years that an author is allowed to publish
num_authors_bag=<FILE> ; used for sampling the number of authors per paper


[General]
output_file=<FILE> ; output csv edgelist
auxiliary_information_file=<FILE> ; output auxiliary information file
log_file=<FILE> ; output log file
num_processors=<INT> ; integer valued maximum parallelism allowed
log_level=<INT> ; 0, 1, and 2 for silent, info, and verbose
```

For example, a simulation with no planted agents can be done using a configuration file structured as below.
```
...
recency_bins=1,5,10
planted_agents=
start_from_checkpoint=false
...
```

In some cases, such as when `use_alpha` is false, the value for `alpha` must be empty.

```
...
[Agent]
alpha=
use_alpha=false
...
```
### Null model
In order to do a "null model" run, in which agents cite fully randomly within the neighborhood, set the `recency_bins` string to be "1" and nothing else. This leads to a binning where all nodes with age at least 1 get binned into the same single bin. Moreover, since this single bin is also the last bin, everything in this bin will be cited in a fully random manner.

### Single-bin model
In order to do a "single-bin model" run, in which agents cite based on preferental attachment and fitness within the neighborhood without regards to recency, set the `recency_bins` string to be "1,`some large number`". This leads to a binning where all nodes with age at least 1 and less than `some large number` get binned into the same bin and then any node from `some large number` to infinity gets binned in another bin. This ensures that the [1, `some large number`) bin is not the last bin in the simulation. Therefore, it will cite essentially all nodes according to preferential attachment and fitness only.

### Individual flags
#### Environment flags
- `nodelist`: csv file with (node\_id, publication\_year) on each line. Both headers and their values are required. This is the starting seed network nodelist. When starting from a checkpoint, so when `start_from_checkpoint` is true, then `nodelist` supplied should be the auxiliary information file from the previous run with no modifications.
- `edgelist`: csv file with (source,target) on each line. This is the starting seed network edgelist. When starting from a checkpoint, so when `start_from_checkpoint` is true, then `edgelist` supplied should be the output file from the previous run with no modifications. Note that the header line for this CSV does not actually matter as long as the first column represents the source node and the second columns repreresns the target node. For example, the header line could be source,target or citing,cited, or u,v or any valid csv row.
- `recency_table` csv file derived from a real-world network. This file is usually created by taking a real-world citation network and getting the count Y which equals the number of edges that represent a citation across x years. This is encoded as a row in the csv such as x,y for each x. Note that for any simulation, a row for x needs to exist for all x from 1 to last year in the simulation - oldest publication year in the seed node set. Note that the header line for this CSV is required but the string values of the header line is not enforced. The only requirement is that starting from the second line of the csv that there are two comma separated integer values for each line.
- `out_degree_bag`: csv file derived from a real-world network. This file is usually created by taking real-world citation network and getting the out-degree of each node in the network. This is encoded as a row in the csv such as node\_id,out\_degree for each node in the network. Note that the node\_id column is unused but is useful for keeping track of which real-world publication was used. The headers in this file are not enforced except that two columns are required and that the second column must be integers.
- `growth_rate`: floating point value, e.g., 0.03 for 3%, which serves as the exponent for the exponential growth formula used in determining how many new agents should spawn per cycle of simulation.
- `num_cycles`: integer value e.g., 30 for a 30-year simulation
- `recency_bins`: This string is a user supplied comma separated string such that each comma separated value in the string is the start of the bin boundary. For example, a string "1,2,5,10,20" represents a binning for recency such that the first bin contains all publications that are less than 2 year old, meaning 1 <= current year - publication year < 2. It follows that the second bin now are publications that are at least 2 years old but at most 4 years old (2 <= current year - publication year < 5). The last bin is implied to go on until infinity so in this example, the last bin contains all publications that are at least 20 years old (20 <= current\_year - publication year < infinity).
- `planted_agents`: an optional csv file with planted agents. The only required column is the "year" column. The year column specifies the relative year, from the start of the simulation, when the node should be planted. 1 is the earliest relative year an agent can be planted. Any column header here must match the exact name in the output nodelist.
- `start_from_checkpoint` boolean value e.g., true or false for whether to start from a checkpoint or not. Check notes about `nodelist` and `edgelist` if this flag is set to true.

#### Agent flags
- `alpha` floating point value specifying the alpha for neighborhood. This value for alpha determines the proportion of citations that are made to the 1-hop nodes of the generator node relative to the total number of citations that the agent will make to the generator node's neighborhood. It can be left to be -1 for a random model or a constant value such as 0.5.
- `use_alpha` boolean value e.g., true or false for whether to use alpha or not. When alpha i
- `same_year_citations`: floating point value e.g., 0.12 for 12%. This value determines the proportion of new agents in a given year that cite another agent from the same year.
- `fully_random_citations`: floating point value e.g., 0.05 for 5%. This value determines the proportion of assigned out-degree that goes towards citing any node from the graph in a fully random manner.
- `preferential_weight`: floating point value e.g., 0.33. This value can be left -1 for agents with random weights or a constant value for static agents.
- `fitness_weight`: floating point value e.g., 0.33. This value can be left -1 for agents with random weights or a constant value for static agents.
- `fitness_value_min`: minimum possible integer fitness value for each agent.
- `fitness_value_max`: maximum possible integer fitness value for each agent.
- `neighborhood_sample`: maximum number of nodes to sample from each neighborhood. The neighborhoods of the genartor will be sampled such that the final list of nodes is at most this number.
- `in_degree_threshold`: used for non-random generator selection. Selects the top percentile nodes by in-degree value, meaning that the top nth percentile node is found by in-degree, afterwhich all nodes with in-degree value at least that value is selected.
- `fitness_threshold`: used for non-random generator selection. Selects the top percentile nodes by fitness value, meaning that the top nth percentile node is found by fitness, afterwhich all nodes with fitness value at least that value is selected.
- `recency_threshold`: used for non-random generator selection. Selects only from the past n years e.g., if `recency_threshold=5` and current year is 1988, then 5 years of publications will be considered so the nodes with publication year at least 1983 and at most 1987.
- `non_random_generator_probability`: Probability of each node to select a generator node in a non-random manner according to the different thresholds. Set to 0 for fully random and 1 for all non-random.
- `author_max_lifetime`: used for the maximum amount of years that an author is allowed to publish. When this value is `k`, the difference between the publication year of the last paper and the publication year of the first paper of any given author cannot exceed `k`.
- `num_authors_bag` ; used for sampling the number of authors per paper. This is a CSV wherelthe header line is ignored and each subsequent line is structed as (index,number of authors). The index column is not used, and the number of authors column is used to sample a number for the number of authors for each paper.


#### General flags
- `output_file`: output csv edgelist file.
- `auxiliary_information_file`: output auxiliary information file a.k.a. the nodelist. This file contains the information for each node such as assigned out-degree, assigned alpha values, assigned fitness values, final in-degree, and so on.
- `log_file`: output log file. This log file contains information about the run itself such as the runtime. The configuration file flags will be parsed and the parsed results will be written to this file before the simulation begins. Additionally, an extra file is created in the same directory as the log file containing the wall-time of individual stages inside the simulator.
- `num_processors`: integer valued maximum parallelism allowed. Used for setting the maximum number of workers.
- `log_level`: enum flag where 0, 1, and 2 correspond to silent, info, and verbose, respectively.
