//
// Created by alang on 2022/10/24.
//

#ifndef RSE_MERKEL_MERKLETREE_H
#define RSE_MERKEL_MERKLETREE_H

#include <string>
#include <vector>
#include "picosha2.h"
#include <iostream>

using namespace std;

#define EMPTY_HASH "0000000000000000000000000000000000000000000000000000000000000000"

string hash_2_leaf(string left, string right);

class MerkleProof {
public:
    vector<uint8_t> m_data;
    int m_index;
    string m_root_hash;
    vector<string> m_branch;

    MerkleProof() {
        m_index = 0;
        m_branch = std::vector<string>();
        m_data = std::vector<uint8_t>();
        m_root_hash = EMPTY_HASH;
    }
    
    MerkleProof(vector<uint8_t> data, int index, string root, vector<string> branch);
    void print_proof();
    bool validate();
    const vector<uint8_t> data();
    int index();
    const string& root_hash();
    const vector<string>& branch();
};

class MerkleTree {
private:
    vector<vector<uint8_t>> m_data;
    string m_root_hash;
    vector<vector<string>> m_levels;
    unsigned m_nshards;
public:
    MerkleTree(vector<vector<uint8_t>> shards);
    void print_tree();
    void print_level(int cur_level);
    MerkleProof proof_i(int index);
    vector<MerkleProof> proofs();
    const string root_hash();
};


#endif //RSE_MERKEL_MERKLETREE_H
