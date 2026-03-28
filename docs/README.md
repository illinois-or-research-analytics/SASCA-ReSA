# Input data overview
Files contained in [inputs.tar.gz](inputs.tar.gz)
- num\_authors\_distribution.csv
- outdegree\_distribution.csv
- recency\_table.csv
- seed\_edgelist.csv
- seed\_nodelist.csv


### num\_authors\_distribution.csv
To be used as input data for `num_authors_bag` configuration parameter. csv file with two columns: id, num\_authors. First column is ignored. Values in the second column are used as the distribution for the number of authors. Obtained from the pubmed query "journal article[Publication Type] AND 2025/1/1[Date - Publication] : 2025/12/31[Date - Publication] AND english[Language]".

### outdegree\_distribution.csv
To be used as input data for `out_degree_bag` configuration parameter csv file with two columns: id, out\_degree. First column is ignored. Values in the second column are used as the distribution for the out-degree. Protocol is described in Section 3 of the [Supplementary Materials for SASCA-ReS](https://github.com/illinois-or-research-analytics/SASCA-ReS/blob/main/docs/suppl.pdf).

### recency\_table.csv
To be used as input data for `recency_table` configuration parameter. csv file with two columns: id, count. First column is ignored. Values in the second column are used to create a recency distribution, the protocol for which is described in Section 1 of the [Supplementary Materials for SASCA-ReS](https://github.com/illinois-or-research-analytics/SASCA-ReS/blob/main/docs/suppl.pdf).


### seed\_nodelist.csv
To be used as input data for `nodelist` configuration parameter. csv file with two columns: id, year. First column represents the node id, second column represents the publication year of the node. Originally described in [1].

### seed\_edgelist.csv
To be used as input data for `edgelist` configuration parameter. csv file with two columns: source, target. First column represents the source node id of the edge, second column represents the target node id of the edge. Originally described in [1].

---
# References
[1] Chacko, George, Minhyuk Park, Vikram Ramavarapu, Ananth Grama, Pablo Robles-Granda, and Tandy Warnow. "An agent-based model of citation behavior." Applied Network Science 11, no. 1 (2026): 10.
