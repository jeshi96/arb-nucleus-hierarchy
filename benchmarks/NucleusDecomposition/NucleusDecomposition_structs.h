#pragma once

#include <math.h>
#include <limits>

// Library dependencies
#include "gbbs/bucket.h"
#include "gbbs/edge_map_reduce.h"
#include "gbbs/gbbs.h"
#include "gbbs/helpers/dyn_arr.h"
#include "gbbs/helpers/sparse_table.h"
#include "gbbs/helpers/sparse_additive_map.h"

// Ordering files
#include "benchmarks/DegeneracyOrder/BarenboimElkin08/DegeneracyOrder.h"
#include "benchmarks/DegeneracyOrder/GoodrichPszona11/DegeneracyOrder.h"
#include "benchmarks/KCore/JulienneDBS17/KCore.h"
#include "benchmarks/TriangleCounting/ShunTangwongsan15/Triangle.h"
#include "benchmarks/CliqueCounting/Clique.h"

// Clique files
#include "benchmarks/CliqueCounting/intersect.h"
#include "benchmarks/CliqueCounting/induced_intersection.h"
#include "benchmarks/CliqueCounting/induced_neighborhood.h"
#include "benchmarks/CliqueCounting/induced_hybrid.h"
#include "benchmarks/CliqueCounting/induced_split.h"
#include "benchmarks/CliqueCounting/relabel.h"

#include "twotable.h"
#include "twotable_nosearch.h"
#include "onetable.h"
#include "commontable.h"
#include "list_buffer.h"

#include "benchmarks/Connectivity/SimpleUnionAsync/Connectivity.h"

namespace gbbs {

  struct IntersectSpace {
  // Label each vertex for fast intersect for first recursive level
  uintE* labels = nullptr;

  IntersectSpace () {}

  void alloc(size_t n) {
    if (labels == nullptr) {
      labels = (uintE*) calloc(n, sizeof(uintE));
      assert(labels[0] == 0);
    }
  }
  };

  int BinomialCoefficient(const int n, const int k) {
  std::vector<int> aSolutions(k);
  aSolutions[0] = n - k + 1;

  for (int i = 1; i < k; ++i) {
    aSolutions[i] = aSolutions[i - 1] * (n - k + 1 + i) / (i + 1);
  }

  return aSolutions[k - 1];
}


template <class Graph>
bool is_edge(Graph& DG, uintE v, uintE u) {
  using W = typename Graph::weight_type;
  bool is = false;
  auto map_f = [&] (const uintE& src, const uintE& vv, const W& wgh) {
    if (vv == u) is = true;
    };
    DG.get_vertex(v).out_neighbors().map(map_f, false);
    return is;
}

class EfficientConnectWhilePeeling {
  public:
    EfficientConnectWhilePeeling() {}
    EfficientConnectWhilePeeling(size_t _n) {
      n = _n;
      uf = gbbs::simple_union_find::SimpleUnionAsyncStruct(n);
      links = sequence<uintE>::from_function(n, [&](size_t s) { return UINT_E_MAX; });
    }
    template<class X, class F>
    void link(X a, X b, F& cores);

    template<class bucket_t>
    void init(bucket_t cur_bkt);

    gbbs::simple_union_find::SimpleUnionAsyncStruct uf =  gbbs::simple_union_find::SimpleUnionAsyncStruct(0);
    sequence<uintE> links;
    size_t n; // table size
};

template<class X, class F>
void EfficientConnectWhilePeeling::link(X a, X b, F& cores) {
  if (cores(a) == cores(b)) {
    this->uf.unite(a, b);
  }
  else if (cores(a) < cores(b)) {
    if (!gbbs::atomic_compare_and_swap<uintE>(&(links[b]), UINT_E_MAX, a)) {
      while (true) {
        gbbs::uintE c = links[b];
        if (cores(c) < cores(a)) {
          if (gbbs::atomic_compare_and_swap<uintE>(&(links[b]), c, a)) {
            this->link(a, c, cores);
            break;
          }
        } else {
          this->link(a, c, cores);
          break;
        }
      }
    }
  }
  else {
    this->link(b, a, cores);
  }
}

template <class bucket_t>
void EfficientConnectWhilePeeling::init(bucket_t cur_bkt) {
}


class ConnectWhilePeeling {
  public:
    ConnectWhilePeeling() {}
    ConnectWhilePeeling(size_t _n){
      n = _n;
      //uf = gbbs::simple_union_find::SimpleUnionAsyncStruct(n);
      //links = sequence<uintE>::from_function(n, [&](size_t s) { return UINT_E_MAX; });
      //uf_links = gbbs::simple_union_find::SimpleUnionAsyncStruct(n);
    }
    template<class X, class F>
    void link(X x, X index, F& cores);

    template<class bucket_t>
    void init(bucket_t cur_bkt);
    /*template <class F, class Graph, class Graph2, class Table, class G, class H, class D>
    void update_cores(size_t active_core, F get_active, size_t active_size, Graph& GA, 
      Graph2& DG, size_t r, size_t k, Table& table, sequence<uintE>& rank, bool relabel,
      size_t max_deg, sequence<uintE>& inverse_rank, G& is_active, H& is_inactive, D& cores);*/
    //gbbs::simple_union_find::SimpleUnionAsyncStruct uf =  gbbs::simple_union_find::SimpleUnionAsyncStruct(0); // one uf to do per-level connectivity
    //sequence<uintE> links; // between-level links
    //gbbs::simple_union_find::SimpleUnionAsyncStruct uf_links =  gbbs::simple_union_find::SimpleUnionAsyncStruct(0);
    size_t n; // table size
    std::vector<gbbs::simple_union_find::SimpleUnionAsyncStruct> set_uf;
    std::vector<uintE> set_core;
};

template<class X, class F>
void ConnectWhilePeeling::link(X x, X index, F& cores) {
  parallel_for(0, set_uf.size(), [&](size_t idx){
    if (cores(index) >= set_core[idx]) set_uf[idx].unite(x, index);
  });
}

template <class bucket_t>
void ConnectWhilePeeling::init(bucket_t cur_bkt) {
  set_uf.push_back(gbbs::simple_union_find::SimpleUnionAsyncStruct(n));
  set_core.push_back(cur_bkt);
}


/*
template <class F, class Graph, class Graph2, class Table, class G, class H, class D>
void ConnectWhilePeeling::update_cores(size_t active_core, F get_active, size_t active_size, Graph& GA, 
      Graph2& DG, size_t r, size_t k, Table& table, sequence<uintE>& rank, bool relabel,
      size_t max_deg, sequence<uintE>& inverse_rank, G& is_active, H& is_inactive, D& cores){
  set_uf.push_back(gbbs::simple_union_find::SimpleUnionAsyncStruct(table.return_total()));
  set_core.push_back(active_core);
  // Run connectivity on vert in that core, store in uf
  auto init_induced = [&](HybridSpace_lw* induced) { induced->alloc(max_deg, k-r, GA.n, true, true, true); };
  auto finish_induced = [&](HybridSpace_lw* induced) { if (induced != nullptr) { delete induced; } };

      //auto is_active = [&](size_t index) {
      //  return cores[index] == active_core;
      //};
      //auto is_inactive = [&](size_t index) {
      //  return cores[index] < active_core;
      //};
      // except we should change is inactive to mean towards higher cores,
      // so if core of neighbor is > active_core (meaning it was never previously active)
    parallel_for(0, active_size, [&](size_t j){
      auto x = get_active(j);

      // Steps:
      // 1. Extract vertices given by index -- these will be the r-clique X
      // 2. Find all s-clique containing the r-clique X
      // 3. For each s-clique containing X, iterate over all combinations of
      // r vertices in that s-clique; these form r-clique X'
      // 4. Union X and X'
    static thread_local HybridSpace_lw* induced = nullptr;
    if (induced == nullptr) induced = new HybridSpace_lw();
    induced->alloc(max_deg, k-r, GA.n, true, true, true);
    init_induced(induced);

    auto update_d_bind = [&](uintE* base){
      table.extract_indices_conn(base, is_inactive, [&](std::size_t index){
        if (index != x) {
          for (size_t idx = 0; idx < set_uf.size(); idx++) {
            if (cores[index] < set_core[idx]) break;
            set_uf[idx].unite(x, index);
          }
          //gbbs::atomic_compare_and_swap<uintE>(&(links[x]), UINT_E_MAX, index);
          // To get rid of links, we should do CAS on uf if root, otherwise leave it
          //gbbs::simple_union_find::set_fake_parent(index, uf.parents, x);
        }
      }, r, k, x);
    };
    uintE base[10];
    table.extract_clique(x, base, GA, k);
    // Fill base[k] ... base[k-r+1] and base[0]
    if (relabel) {
      auto g_vert_map = [&](uintE vert){return rank[vert];};
      auto inverse_g_vert_map = [&](uintE vert){return inverse_rank[vert];};
      induced->setup_nucleus(GA, DG, k, base, r, g_vert_map, inverse_g_vert_map);
    } else {
      auto g_vert_map = [&](uintE vert){return vert;};
      induced->setup_nucleus(GA, DG, k, base, r, g_vert_map, g_vert_map);
    }
    NKCliqueDir_fast_hybrid_rec(DG, 1, k-r, induced, update_d_bind, base);

    }, 1, true); // granularity

    //parallel_for(0, n, [&](size_t l) { gbbs::simple_union_find::find_compress(l, uf.parents); });
}

*/

template <class bucket_t, class Graph, class Graph2, class Table>
inline std::vector<uintE> construct_nd_connectivity_from_connect(ConnectWhilePeeling& connect_with_peeling, 
sequence<bucket_t>& cores, Graph& GA, Graph2& DG,
size_t r, size_t k, Table& table, sequence<uintE>& rank, bool relabel){
  auto n = table.return_total();
  std::vector<uintE> connectivity_tree(n);
  sequence<uintE> prev_parent = sequence<uintE>::from_function(n, [&](size_t i){ return i; });
  uintE prev_max_parent = n;
  parallel_for(0, n, [&](std::size_t i){connectivity_tree[i] = UINT_E_MAX;});
  for (long idx = connect_with_peeling.set_uf.size() - 1; idx >= 0; idx--) {
    connectivity_tree.resize(prev_max_parent, UINT_E_MAX);
    parallel_for(0, n, [&](size_t l) { gbbs::simple_union_find::find_compress(l, connect_with_peeling.set_uf[idx].parents); });

    parallel_for(0, n, [&](size_t l){
      if (table.is_valid(l)) {
        connectivity_tree[prev_parent[l]] = prev_max_parent + connect_with_peeling.set_uf[idx].parents[l];
        // Update previous parent
        prev_parent[l] = connectivity_tree[prev_parent[l]];
      }
    });
    // Update previous max parent
    prev_max_parent += n;
  }
  return connectivity_tree;
  //std::cout << "Printing uf: " << std::endl;
  //gbbs::simple_union_find::print_uf(connect_with_peeling.uf.parents);
  //std::cout << "End printing uf: " << std::endl;
  /*

  // Sort vertices from highest core # to lowest core #
  auto get_core = [&](uintE p, uintE q){ return cores[p] > cores[q]; };
  auto sorted_vert = sequence<uintE>::from_function(n, [&](size_t i) { return i; });
  parlay::sample_sort_inplace(make_slice(sorted_vert), get_core);

  sequence<uintE> mark_keys(n + 1);
  parallel_for(0, n, [&](std::size_t i) {
    if (i != 0 && cores[sorted_vert[i]] == cores[sorted_vert[i-1]])
      mark_keys[i] = UINT_E_MAX;
    else
      mark_keys[i] = i;
  });
  mark_keys[n] = n;
  auto vert_buckets = 
      parlay::filter(mark_keys, [&](uintE x) -> bool { return x != UINT_E_MAX; });
  
  // TODO(jeshi): This isn't parallel, but I'd just like easy resizing atm
  std::vector<uintE> connectivity_tree(n);
  sequence<uintE> prev_parent = sequence<uintE>::from_function(n, [&](size_t i){ return i; });
  uintE prev_max_parent = n;
  uintE prev_offset = 0;
  parallel_for(0, n, [&](std::size_t i){connectivity_tree[i] = UINT_E_MAX;});

  std::vector<uintE> keep_clean_parent(n, UINT_E_MAX);
  parallel_for(0, n, [&](size_t l){keep_clean_parent[l] = l;});


  for (size_t i = 0; i < vert_buckets.size()-1; i++) {
    size_t start_index = vert_buckets[i];
    size_t end_index = vert_buckets[i + 1];

    auto first_x = sorted_vert[start_index];
    auto first_current_core = cores[first_x];

    auto is_active = [&](size_t index) {
      return cores[index] == first_current_core;
    };
    auto is_inactive = [&](size_t index) {
      return cores[index] < first_current_core;
    };
 
    if (first_current_core != 0) {
      parallel_for(start_index, end_index, [&](size_t j){
        // Vertices are those given by sorted_vert[j]
        auto x = sorted_vert[j];
        if (table.is_valid(x) && connect_with_peeling.links[x] != UINT_E_MAX) {
          auto current_core = cores[x];
          assert(current_core == first_current_core);
          connect_with_peeling.uf.unite(x, connect_with_peeling.links[x]);
        }
      });

    }

    connectivity_tree.resize(prev_max_parent, UINT_E_MAX);
    auto max_parent = n; //parlay::scan_inplace(make_slice(map_parents));
    
    parallel_for(0, n, [&](size_t l){
      // One check is that once one person changes this value, anyone else trying to change
      // this value should not succeed (they should be trying to change this to be the same val)
      if (table.is_valid(l)) {
        assert(uf.parents[l] < old_max_parent);
        assert(prev_parent[l] < prev_max_parent);
        if (first_current_core != 0 && is_active(l))
        connectivity_tree[prev_parent[l]] = prev_max_parent + connect_with_peeling.uf.parents[l];
        else connectivity_tree[prev_parent[l]] = prev_max_parent + l;
        //
        if (is_active(l)  && first_current_core != 0) {
          auto l_parent = connect_with_peeling.uf.parents[l];
          // If l is not a parent
          if (!gbbs::simple_union_find::is_parent(l, connect_with_peeling.uf.parents)) {
            if (!is_active(l_parent)) {
              std::cout << "This should be active: " << l_parent << std::endl;
            }
            // If lp's parent is itself, assign l to lp
            if (connect_with_peeling.uf.parents[l_parent] == l_parent) {
              connectivity_tree[prev_parent[l]] = prev_max_parent + l_parent;
              keep_clean_parent[l] = l_parent;
            }
            // If lp's parent is fake, assign l to kcp[real_parent of lp]
            else if (gbbs::simple_union_find::is_parent(l_parent,  connect_with_peeling.uf.parents)) {
              auto real_parent = gbbs::simple_union_find::extract_fake_parent(l_parent, connect_with_peeling.uf.parents);
              connectivity_tree[prev_parent[l]] = prev_max_parent + keep_clean_parent[real_parent];
              keep_clean_parent[l] = keep_clean_parent[real_parent];
            } else {
              // This could happen with chains of fake parents that then set themselves to have real parents...think about it
              std::cout << "This shouldn't happen? core l: " << cores[l] << ", core l parent: " << cores[l_parent] << " , core ll parent: " << cores[connect_with_peeling.uf.parents[l_parent]] << std::endl; fflush(stdout); exit(0);
              // This means that l has a real parent ostensibly on its core, whose parent is also real...but that should be compressed?
            }
          } else { // This means your parent is fake
            auto real_parent = gbbs::simple_union_find::extract_fake_parent(l, connect_with_peeling.uf.parents);
            connectivity_tree[prev_parent[l]] = prev_max_parent + keep_clean_parent[real_parent];
            keep_clean_parent[l] = keep_clean_parent[real_parent];
          }
        } else {
          connectivity_tree[prev_parent[l]] = prev_max_parent + keep_clean_parent[l];
        }*/
        // Update previous parent
        /*prev_parent[l] = connectivity_tree[prev_parent[l]];
      }
    });
    // Update previous max parent
    prev_offset = prev_max_parent;
    prev_max_parent += max_parent;
  }
  return connectivity_tree;*/
}



} // namespace gbbs