# Copyright (c) 2019 Ted Yin
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
# of the Software, and to permit persons to whom the Software is furnished to do
# so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

set(RSELIB
../rse-merkle/leopard/leopard.cpp
../rse-merkle/leopard/leopard.h
../rse-merkle/leopard/LeopardCommon.cpp
../rse-merkle/leopard/LeopardCommon.h
../rse-merkle/leopard/LeopardFF16.cpp
../rse-merkle/leopard/LeopardFF16.h
../rse-merkle/leopard/LeopardFF8.cpp
../rse-merkle/leopard/LeopardFF8.h
../rse-merkle/RSE.cpp
../rse-merkle/RSE.h
../rse-merkle/MerkleTree.cpp
../rse-merkle/MerkleTree.h
../rse-merkle/ShardsContainer.cpp
../rse-merkle/ShardsContainer.h
)

add_executable(hotstuff-app ${RSELIB} hotstuff_app.cpp)
target_link_libraries(hotstuff-app hotstuff_static)

add_executable(hotstuff-client ${RSELIB} hotstuff_client.cpp)
target_link_libraries(hotstuff-client hotstuff_static)
