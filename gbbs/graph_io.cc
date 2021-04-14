#include "graph_io.h"

#include <sys/mman.h>

namespace gbbs {
namespace gbbs_io {

typedef std::pair<uintE, uintE> intPair;

namespace internal {

// Starting from the current position, skips all consecutive lines in the stream
// that start with '#' or are empty.
//
// The intent here is that lines starting with '#' are interpreted to be
// comments that should be ignored.
void skip_ifstream_comments(std::ifstream* stream) {
  std::string line;
  while (*stream) {
    std::streampos current_position = stream->tellg();
    std::getline(*stream, line);
    if (!(line.empty() || line[0] == '#')) {
      stream->seekg(current_position);
      return;
    }
  }
}

}  // namespace internal

template <>
Edge<gbbs::empty>::Edge(const uintE _from, const uintE _to)
  : from(_from)
  , to(_to) {}

std::tuple<size_t, size_t, uintT*, uintE*> parse_unweighted_graph(
    const char* fname,
    bool mmap,
    bool binary,
    char* bytes,
    size_t bytes_size) {

  uintT* offsets;
  uintE* edges;
  uint64_t n, m;

  if (!binary) {
    sequence<char*> tokens;
    sequence<char> S;

    if (bytes == nullptr) {
      if (mmap) {
        std::pair<char*, size_t> MM = mmapStringFromFile(fname);
        S = sequence<char>(MM.second);
        // Cannot mutate the graph unless we copy.
        par_for(0, S.size(), kDefaultGranularity, [&] (size_t i)
                        { S[i] = MM.first[i]; });
        if (munmap(MM.first, MM.second) == -1) {
          perror("munmap");
          exit(-1);
        }
      } else {
        S = readStringFromFile(fname);
      }
    }
    sequence<slice<char*, char*>> tokens = parlay::map_tokens(parlay::make_slice(S),
        [] (auto x) { return parlay::make_slice(x); });

    assert(tokens[0] == internal::kUnweightedAdjGraphHeader);

    uint64_t n = parlay::internal::chars_to_int_t<unsigned long>(tokens[1]);
    uint64_t m = parlay::internal::chars_to_int_t<unsigned long>(tokens[2]);

    debug(std::cout << "# n = " << n << " m = " << m << " len = " << (tokens.size() - 1) << "\n";
    uint64_t len = tokens.size() - 1;
    assert(len == n + m + 2););

    offsets = pbbslib::new_array_no_init<uintT>(n+1);
    edges = pbbslib::new_array_no_init<uintE>(m);

    par_for(0, n, gbbs::kSequentialForThreshold, [&] (size_t i)
                    { offsets[i] = parlay::internal::chars_to_int_t<unsigned long>(tokens[i + 3]); });
    offsets[n] = m; /* make sure to set the last offset */
    par_for(0, m, gbbs::kSequentialForThreshold, [&] (size_t i)
                    { edges[i] = parlay::internal::chars_to_int_t<unsigned long>(tokens[i + n + 3]); });

    S.clear();
    tokens.clear();
  } else {
    std::pair<char*, size_t> MM = mmapStringFromFile(fname);

    auto mmap_file = MM.first;

    long* sizes = (long*)mmap_file;
    n = sizes[0], m = sizes[1];

    offsets = (uintT*)(mmap_file + 3 * sizeof(long));
    uint64_t skip = 3 * sizeof(long) + (n + 1) * sizeof(intT);
    edges = (uintE*)(mmap_file + skip);
  }

  return std::make_tuple(n, m, offsets, edges);
}

symmetric_graph<symmetric_vertex, gbbs::empty> read_unweighted_symmetric_graph(
    const char* fname,
    bool mmap,
    bool binary,
    char* bytes,
    size_t bytes_size) {
  size_t n, m;
  uintT* offsets;
  uintE* edges;
  std::tie(n, m, offsets, edges) = parse_unweighted_graph(fname, mmap, binary, bytes, bytes_size);

  auto v_data = pbbslib::new_array_no_init<vertex_data>(n);
  parallel_for(0, n, [&] (size_t i) {
    v_data[i].offset = offsets[i];
    v_data[i].degree = offsets[i+1]-v_data[i].offset;
  });
  if (!binary) {
    gbbs::free_array(offsets, n+1);
  }

  return symmetric_graph<symmetric_vertex, gbbs::empty>(
      v_data, n, m,
      [=](){
        gbbs::free_array(v_data, n);
        if (!binary) gbbs::free_array(edges, m); },
      (std::tuple<uintE, gbbs::empty>*)edges);
}

asymmetric_graph<asymmetric_vertex, gbbs::empty> read_unweighted_asymmetric_graph(
    const char* fname,
    bool mmap,
    bool binary,
    char* bytes,
    size_t bytes_size) {
  size_t n, m;
  uintT* offsets;
  uintE* edges;
  if (binary) {
    std::cout << "Todo: implement binary support for asymmetric graphs" << std::endl;
    exit(-1);
  }
  std::tie(n, m, offsets, edges) = parse_unweighted_graph(fname, mmap, binary, bytes, bytes_size);

  auto v_data = pbbslib::new_array_no_init<vertex_data>(n);
  parallel_for(0, n, [&] (size_t i) {
    v_data[i].offset = offsets[i];
    v_data[i].degree = offsets[i+1]-v_data[i].offset;
  });
  if (!binary) {
    pbbslib::free_array(offsets, n+1);
  }

  /* construct transpose of the graph */
  uintT* tOffsets = pbbslib::new_array_no_init<uintT>(n);
  par_for(0, n, kDefaultGranularity, [&] (size_t i)
                  { tOffsets[i] = INT_T_MAX; });
  intPair* temp = pbbslib::new_array_no_init<intPair>(m);
  par_for(0, n, kDefaultGranularity, [&] (size_t i) {
    uintT o = v_data[i].offset;
    uintT deg = v_data[i].degree;
    for (uintT j = 0; j < deg; j++) {
      temp[o + j ] = std::make_pair((edges + o)[j], i);
    }
  });

  auto temp_seq = pbbslib::make_range(temp, m);
  pbbslib::integer_sort_inplace(temp_seq.slice(), [&] (const intPair& p) { return p.first; }, pbbslib::log2_up(n));

  tOffsets[temp[0].first] = 0;
  uintE* inEdges = pbbslib::new_array_no_init<uintE>(m);
  inEdges[0] = temp[0].second;
  par_for(1, m, kDefaultGranularity, [&] (size_t i) {
    inEdges[i] = temp[i].second;
    if (temp[i].first != temp[i - 1].first) {
      tOffsets[temp[i].first] = i;
    }
  });

  pbbslib::free_array(temp, m);

  // fill in offsets of degree 0 vertices by taking closest non-zero
  // offset to the right
  auto forward_seq = pbbslib::make_delayed(tOffsets, n);
  auto t_seq = make_slice(forward_seq.rbegin(), forward_seq.rend());

  auto M = pbbslib::minm<uintT>();
  M.identity = m;
  pbbslib::scan_inplace(t_seq, M, pbbslib::fl_scan_inclusive);

  auto v_in_data = pbbslib::new_array_no_init<vertex_data>(n);
  parallel_for(0, n, [&] (size_t i) {
    v_in_data[i].offset = tOffsets[i];
    v_in_data[i].degree = tOffsets[i+1]-v_in_data[i].offset;
  });
  pbbslib::free_array(tOffsets, n);

  return asymmetric_graph<asymmetric_vertex, gbbs::empty>(
      v_data, v_in_data, n, m,
      [=]() {
        gbbs::free_array(v_data, n);
        gbbs::free_array(v_in_data, n);
        gbbs::free_array(inEdges, m);
        gbbs::free_array(edges, m);
      },
      (std::tuple<uintE, gbbs::empty>*)edges, (std::tuple<uintE, gbbs::empty>*)inEdges);
}

std::tuple<char*, size_t> parse_compressed_graph(
    const char* fname, bool mmap, bool mmapcopy) {
  char* bytes;
  size_t bytes_size;

  if (mmap) {
    std::tie(bytes, bytes_size) = mmapStringFromFile(fname);
    if (mmapcopy) {
      debug(std::cout << "# Copying compressed graph due to mmapcopy being set."
                << "\n";);
      char* next_bytes = pbbslib::new_array_no_init<char>(bytes_size);
      par_for(0, bytes_size, kDefaultGranularity, [&] (size_t i)
                      { next_bytes[i] = bytes[i]; });
      if (munmap(bytes, bytes_size) == -1) {
        perror("munmap");
        exit(-1);
      }
      bytes = next_bytes;
    }
  } else {
    std::tie(bytes, bytes_size) = read_o_direct(fname);
  }
  return std::make_tuple(bytes, bytes_size);
}

std::vector<Edge<gbbs::empty>>
read_unweighted_edge_list(const char* filename) {
  std::ifstream file{filename};
  if (!file.is_open()) {
    std::cout << "ERROR: Unable to open file: " << filename << '\n';
    std::terminate();
  }
  internal::skip_ifstream_comments(&file);

  std::vector<Edge<gbbs::empty>> edge_list;
  uintE from;
  uintE to;
  while (file >> from >> to) {
    edge_list.emplace_back(from, to);
  }
  return edge_list;
}

}  // namespace gbbs_io
}  // namespace gbbs
