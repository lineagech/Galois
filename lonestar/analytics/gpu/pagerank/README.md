## DESCRIPTION

 PageRank is a key technique in web mining to rank the importance of web pages. In PageRank, each web page is assigned a numerical weight to begin with, and the algorithm tries to estimate the importance of the web page relative to other web pages in the hyperlinked set of pages. The key assumption is that more important web pages are likely to receive more links from other websites. More details about the problem and different solutions can be found in [1, 2].

[1] https://en.wikipedia.org/wiki/PageRank

[2] Whang et al. Scalable Data-driven PageRank: Algorithms, System Issues, and Lessons Learned. European Conference on Parallel Processing, 2015. 

 This benchmark computes the PageRank of the nodes for a given input graph using  using a push-style  residual-based algorithm. The algorithm takes input as a graph, and some constant parameters that are used in the computation. The algorithmic parameters are the following:

* ALPHA: ALPHA represents the damping factor, which is the probability that a web surfer will continue browsing by clicking on the linked pages. The damping factor is generally set to 0.85 in the literature.
* TOLERANCE: It represents a bound on the error in the computation.
* MAX_ITER: The number of iterations to repeat the PageRank computation.


## BUILD

Assuming CMake is performed in the ${GALOIS\_ROOT}/build, compile the provided the application by executing the
following command in the ${GALOIS\_ROOT}/build/lonestar/analytics/gpu/pagerank directory.

`make -j`

## RUN

Execute as: ./pagerank [-o output-file] [-t top_ranks] [-x max_iterations] graph-file 

e.g., ./pagerank -o outfile.txt -x 1000000 road-USA.gr

