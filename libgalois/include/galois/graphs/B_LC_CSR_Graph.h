/** Bidirectional LC CSR Graph -*- C++ -*-
 * @file
 * @section License
 *
 * This file is part of Galois.  Galois is a framework to exploit
 * amorphous data-parallelism in irregular programs.
 *
 * Galois is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 2.1 of the
 * License.
 *
 * Galois is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Galois.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * @section Copyright
 *
 * Copyright (C) 2018, The University of Texas at Austin. All rights
 * reserved.
 *
 * @section Description
 *
 * @author Loc Hoang <l_hoang@utexas.edu>
 */

#ifndef __B_LC_CSR_GRAPH__
#define __B_LC_CSR_GRAPH__

#include "galois/graphs/LC_CSR_Graph.h"

namespace galois {
namespace graphs {

/**
 * An extended LC_CSR_Graph that allows the construction of in-edges from its
 * outedges.
 *
 * @tparam NodeTy type of the node data
 * @tparam EdgeTy type of the edge data
 * @tparam EdgeDataByValue If set to true, the in-edges will have their own
 * copy of the edge data. Otherwise, the in-edge edge data will be shared with
 * its corresponding out-edge.
 * @tparam HasNoLockable If set to true, then node accesses will cannot acquire
 * an abstract lock. Otherwise, accessing nodes can get a lock.
 * @tparam UseNumaAlloc If set to true, allocate data in a possibly more NUMA 
 * friendly way.
 * @tparam HasOutOfLineLockable
 * @tparam FileEdgeTy
 */
template<typename NodeTy, typename EdgeTy, bool EdgeDataByValue=false,
         bool HasNoLockable=false, bool UseNumaAlloc=false,
         bool HasOutOfLineLockable=false, typename FileEdgeTy=EdgeTy>
class B_LC_CSR_Graph 
    : public LC_CSR_Graph<NodeTy, EdgeTy, HasNoLockable, UseNumaAlloc, 
                          HasOutOfLineLockable, FileEdgeTy> {
  // typedef to make it easier to read
  using BaseGraph = LC_CSR_Graph<NodeTy, EdgeTy, HasNoLockable, UseNumaAlloc, 
                                 HasOutOfLineLockable, FileEdgeTy>;
  using ThisGraph = B_LC_CSR_Graph<NodeTy, EdgeTy, EdgeDataByValue, 
                                   HasNoLockable, UseNumaAlloc, 
                                   HasOutOfLineLockable, FileEdgeTy>;
 protected:
  // retypedefs of base class
  using EdgeData = LargeArray<EdgeTy>;
  using EdgeDst = LargeArray<uint32_t>;
  using EdgeIndData = LargeArray<uint64_t>;

  EdgeIndData inEdgeIndData;
  EdgeDst inEdgeDst;
  // Edge data of inedges can be a value copy of the outedges (i.e. in and
  // out edges have separate edge values) or inedges can refer to the same
  // data as its corresponding outedge
  using EdgeDataRep = typename std::conditional<EdgeDataByValue, EdgeData, 
                                                EdgeIndData>::type;
  EdgeDataRep inEdgeData; 

  /**
   * Copy the data of outedge by value to inedge.
   *
   * @param e_new position of out-edge to copy as an in-edge
   * @param e position of in-edge
   */
  template <bool A = EdgeDataByValue, 
            typename std::enable_if<A>::type* = nullptr>
  void createEdgeData(const uint64_t e_new, const uint64_t e) {
    BaseGraph::edgeDataCopy(inEdgeData, BaseGraph::edgeData, e_new, e);
  }

  /**
   * Save a pointer to an outedge (i.e. map an in-edge to an out-edge). Done
   * to share edge data.
   *
   * @param e_new position of out-edge to save
   * @param e position of in-edge
   */
  template <bool A = EdgeDataByValue, 
            typename std::enable_if<!A>::type* = nullptr>
  void createEdgeData(const uint64_t e_new, const uint64_t e) {
    if (!std::is_void<EdgeTy>::value) {
      inEdgeData[e_new] = e;
    }
  }

  /**
   * Determine the in-edge indices for every node by accumulating how many
   * in-edges each node has, getting a prefix sum, and saving it to the
   * in edge index data array.
   *
   * @param dataBuffer temporary buffer that is used to accumulate in-edge 
   * counts; at the end of this function, it will contain a prefix sum of 
   * in-edges 
   */
  void determineInEdgeIndices(EdgeIndData& dataBuffer) {
    // counting outgoing edges in the tranpose graph by
    // counting incoming edges in the original graph
    galois::do_all(
      galois::iterate(0ul, BaseGraph::numEdges), 
      [&](uint64_t e) {
        auto dst = BaseGraph::edgeDst[e];
        __sync_add_and_fetch(&(dataBuffer[dst]), 1);
      }
    );

    // prefix sum calculation of the edge index array
    for (uint32_t n = 1; n < BaseGraph::numNodes; ++n) {
      dataBuffer[n] += dataBuffer[n - 1];
    }

    // copy over the new tranposed edge index data
    inEdgeIndData.allocateInterleaved(BaseGraph::numNodes);
    galois::do_all(
      galois::iterate(0ul, BaseGraph::numNodes), 
      [&](uint32_t n) {
        inEdgeIndData[n] = dataBuffer[n];
      }
    );
  }

  /**
   * Determine the destination of each in-edge and copy the data associated
   * with an edge (or point to it).
   *
   * @param dataBuffer A prefix sum of in-edges
   */
  void determineInEdgeDestAndData(EdgeIndData& dataBuffer) {
    // after this block dataBuffer[i] will now hold number of edges that all 
    // nodes before the ith node have; used to determine where to start
    // saving an edge for a node
    if (BaseGraph::numNodes >= 1) {
      dataBuffer[0] = 0;
      galois::do_all(
        galois::iterate(1ul, BaseGraph::numNodes), 
        [&](uint32_t n) {
          dataBuffer[n] = inEdgeIndData[n - 1];
        }
      );
    }

    // allocate edge dests and data
    inEdgeDst.allocateInterleaved(BaseGraph::numEdges);

    if (!std::is_void<EdgeTy>::value) {
      inEdgeData.allocateInterleaved(BaseGraph::numEdges);
    }

    galois::do_all(
      galois::iterate(0ul, BaseGraph::numNodes), 
      [&](uint32_t src) {
        // e = start index into edge array for a particular node
        uint64_t e = (src == 0) ? 0 : BaseGraph::edgeIndData[src - 1];

        // get all outgoing edges of a particular node in the non-transpose and
        // convert to incoming
        while (e < BaseGraph::edgeIndData[src]) {
          // destination nodde
          auto dst = BaseGraph::edgeDst[e];
          // location to save edge
          auto e_new = __sync_fetch_and_add(&(dataBuffer[dst]), 1);
          // save src as destination
          inEdgeDst[e_new] = src;
          // edge data to "new" array
          createEdgeData(e_new, e);
          e++;
        }
      }
    );
  }

 public:
  using GraphNode = uint32_t;
  using edge_iterator = 
    boost::counting_iterator<typename EdgeIndData::value_type>;
  using edge_data_reference = typename EdgeData::reference;

  B_LC_CSR_Graph() = default;
  B_LC_CSR_Graph(B_LC_CSR_Graph&& rhs) = default;
  B_LC_CSR_Graph& operator=(B_LC_CSR_Graph&&) = default;

  /*****************************************************************************
   * Construction functions
   ****************************************************************************/

  /**
   * Call only after the LC_CSR_Graph part of this class is fully constructed.
   * Creates the in edge data by reading from the out edge data.
   */
  void constructIncomingEdges() {
    galois::StatTimer incomingEdgeConstructTimer("IncomingEdgeConstruct");
    incomingEdgeConstructTimer.start();

    // initialize the temp array
    EdgeIndData dataBuffer;
    dataBuffer.allocateInterleaved(BaseGraph::numNodes);
    galois::do_all(
      galois::iterate(0ul, BaseGraph::numNodes), 
      [&](uint32_t n) {
        dataBuffer[n] = 0;
      }
    );

    determineInEdgeIndices(dataBuffer);
    determineInEdgeDestAndData(dataBuffer);

    incomingEdgeConstructTimer.stop();
  }

  /*****************************************************************************
   * Access functions
   ****************************************************************************/

  /**
   * Grabs in edge beginning without lock/safety.
   *
   * @param N node to get edge beginning of
   * @returns Iterator to first in edge of node N
   */
  edge_iterator in_raw_begin(GraphNode N) const {
    return edge_iterator((N == 0) ? 0 : inEdgeIndData[N - 1]);
  }

  /**
   * Grabs in edge end without lock/safety.
   *
   * @param N node to get edge end of
   * @returns Iterator to end of in edges of node N (i.e. first edge of 
   * node N+1)
   */
  edge_iterator in_raw_end(GraphNode N) const {
    return edge_iterator(inEdgeIndData[N]);
  }

  /**
   * Wrapper to get the in edge end of a node; lock if necessary.
   *
   * @param N node to get edge beginning of
   * @param mflag how safe the acquire should be
   * @returns Iterator to first in edge of node N
   */
  edge_iterator in_edge_begin(GraphNode N, 
                              MethodFlag mflag = MethodFlag::WRITE) {
    BaseGraph::acquireNode(N, mflag);
    if (galois::runtime::shouldLock(mflag)) {
      for (edge_iterator ii = in_raw_begin(N), ee = in_raw_end(N); 
           ii != ee; 
           ++ii) {
        BaseGraph::acquireNode(inEdgeDst[*ii], mflag);
      }
    }
    return in_raw_begin(N);
  }

  /**
   * Wrapper to get the in edge end of a node; lock if necessary.
   *
   * @param N node to get in edge end of
   * @param mflag how safe the acquire should be
   * @returns Iterator to end of in edges of node N (i.e. first in edge of N+1)
   */
  edge_iterator in_edge_end(GraphNode N, MethodFlag mflag = MethodFlag::WRITE) {
    BaseGraph::acquireNode(N, mflag);
    return in_raw_end(N);
  }

  /**
   * @param N node to get in edges for
   * @param mflag how safe the acquire should be
   * @returns Range to in edges of node N
   */
  runtime::iterable<NoDerefIterator<edge_iterator>> in_edges(GraphNode N,
      MethodFlag mflag = MethodFlag::WRITE) {
    return internal::make_no_deref_range(in_edge_begin(N, mflag), in_edge_end(N, mflag));
  }

  /**
   * Given an edge id for in edges, get the destination of the edge
   *
   * @param ni edge id
   * @returns destination for that in edge
   */
  GraphNode getInEdgeDst(edge_iterator ni) const {
    return inEdgeDst[*ni];
  }

  /**
   * Given an edge id for in edge, get the data associated with that edge.
   * Returns a constant reference.
   *
   * In-edge has own copy of edge-data version.
   *
   * @param ni in-edge id
   * @returns data of the edge
   */
  template <bool A = EdgeDataByValue, 
            typename std::enable_if<A>::type* = nullptr>
  edge_data_reference getInEdgeData(edge_iterator ni, 
      MethodFlag = MethodFlag::UNPROTECTED) const {
    return inEdgeData[*ni];
  }

  /**
   * Given an edge id for in edge, get the data associated with that edge.
   * Returns a non-constant reference.
   *
   * In-edge has own copy of edge-data version.
   *
   * @param ni in-edge id
   * @returns data of the edge
   */
  template <bool A = EdgeDataByValue, 
            typename std::enable_if<A>::type* = nullptr>
  edge_data_reference getInEdgeData(edge_iterator ni, 
                                    MethodFlag = MethodFlag::UNPROTECTED) {
    return inEdgeData[*ni];
  }


  /**
   * Given an edge id for in edge, get the data associated with that edge.
   * Returns a constant reference.
   *
   * In-edge and out-edge share edge data version.
   *
   * @param ni in-edge id
   * @returns data of the edge
   */
  template <bool A = EdgeDataByValue, 
            typename std::enable_if<!A>::type* = nullptr>
  edge_data_reference getInEdgeData(edge_iterator ni, 
      MethodFlag = MethodFlag::UNPROTECTED) const {
    return BaseGraph::edgeData[inEdgeData[*ni]];
  }

  /**
   * Given an edge id for in edge, get the data associated with that edge.
   * Returns a non-constant reference.
   *
   * In-edge and out-edge share edge data version.
   *
   * @param ni in-edge id
   * @returns data of the edge
   */
  template <bool A = EdgeDataByValue, 
            typename std::enable_if<!A>::type* = nullptr>
  edge_data_reference getInEdgeData(edge_iterator ni, 
                                    MethodFlag = MethodFlag::UNPROTECTED) {
    return BaseGraph::edgeData[inEdgeData[*ni]];
  }
};

} // end graphs namespace
} // end galois namespace
#endif
