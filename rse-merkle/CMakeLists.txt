cmake_minimum_required(VERSION 3.22)
project(rse_merkel)

set(CMAKE_CXX_STANDARD 14)
set(LEOPARDLIB
        leopard/leopard.cpp
        leopard/leopard.h
        leopard/LeopardCommon.cpp
        leopard/LeopardCommon.h
        leopard/LeopardFF16.cpp
        leopard/LeopardFF16.h
        leopard/LeopardFF8.cpp
        leopard/LeopardFF8.h)

include_directories(leopard)
include_directories(leopard/sse2neon)
include_directories(leopard/sse2neon/tests)

include_directories(PicoSHA2)


# add_executable(test
#         ${LEOPARDLIB}
#         test.cpp)

# add_executable(input_generator input_generator.cpp)

# add_executable(test-merkletree
#         MerkleTree.cpp
#         MerkleTree.h
#         test-merkletree.cpp)

# add_executable(test-rse
#         RSE.cpp
#         RSE.h
#         ${LEOPARDLIB}
#         test-rse.cpp)

# add_executable(test-leopard
#         ${LEOPARDLIB}
#         test-leopard.cpp)

# add_executable(test-l2
#         ${LEOPARDLIB}
#         test-l2.cpp)

# add_executable(rse-benchmark
#         RSE.cpp
#         RSE.h
#         ${LEOPARDLIB}
#         rse-benchmark.cpp)

add_executable(benchmark
        RSE.cpp
        RSE.h
        MerkleTree.cpp
        MerkleTree.h
        ShardsContainer.cpp
        ShardsContainer.h
        ${LEOPARDLIB}
        benchmark.cpp)        