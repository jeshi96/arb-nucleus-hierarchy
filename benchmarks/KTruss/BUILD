licenses(["notice"])

cc_library(
    name = "KTruss",
    hdrs = [
        "KTruss.h",
        "truss_utils.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//gbbs",
        "//gbbs:bridge",
        "//gbbs:bucket",
        "//gbbs:edge_map_reduce",
        "//gbbs/helpers:dyn_arr",
        "//gbbs/helpers:sparse_table",
        "//benchmarks/Connectivity/SimpleUnionAsync:Connectivity",
    ],
)

cc_binary(
    name = "KTruss_main",
    srcs = ["KTruss.cc"],
    deps = [":KTruss"],
)
