//
// Created by alang on 2022/10/24.
//

#include "MerkleTree.h"

string hash_2_leaf(string left, string right) {
    left.append(right);
    return picosha2::hash256_hex_string(left);
}

MerkleTree::MerkleTree(vector<vector<uint8_t>> shards) {
    m_data = shards;
    m_nshards = m_data.size();
    vector<string> leafs;
    for(int i=0; i<m_data.size(); i++) {
        leafs.push_back(picosha2::hash256_hex_string(m_data[i]));
    }
    m_levels.push_back(leafs);
    int cur_level = 0;
    while(m_levels[cur_level].size()!=1) {
        if(m_levels[cur_level].size()%2 == 1){
            m_levels[cur_level].push_back(EMPTY_HASH);
        }
        int j=0;
        vector<string> this_level;
        while(j+1 < m_levels[cur_level].size()) {
            this_level.push_back(hash_2_leaf(m_levels[cur_level][j],m_levels[cur_level][j+1]));
            j+=2;
        }
        m_levels.push_back(this_level);
        cur_level+=1;
    }
    m_root_hash = m_levels[cur_level][0];
}

void MerkleTree::print_tree() {
    cout << "The merkle tree of root_hash " << m_root_hash.substr(0, 4) << endl;
    for(int i=0; i<m_levels.size(); i++){
        print_level(i);
    }
}

void MerkleTree::print_level(int cur_level) {
    for(int i=0; i<m_levels[cur_level].size(); i++) {
        cout<< m_levels[cur_level][i].substr(0, 4) << " ";
    }
    cout << endl;
}

MerkleProof MerkleTree::proof_i(int index) {
    int cur_level = 0;
    int this_index = index;
    vector<string> branch;
    while(cur_level<m_levels.size()-1) {
        if(this_index%2 == 0) {
            branch.push_back(m_levels[cur_level][this_index+1]);
        }
        else {
            branch.push_back(m_levels[cur_level][this_index-1]);
        }
        cur_level+=1;
        this_index/=2;
    }
    return MerkleProof(m_data[index], index, m_root_hash, branch);
}

vector<MerkleProof> MerkleTree::proofs() {
    vector<MerkleProof> proofs;
    for(int i=0; i<m_nshards; i++) {
        proofs.push_back(proof_i(i));
    }
    return proofs;
}

const string MerkleTree::root_hash(){
    return m_root_hash;
}

MerkleProof::MerkleProof(vector<uint8_t> data, int index, string root_hash, vector<string> branch)
: m_data(data), m_index(index), m_root_hash(root_hash), m_branch(branch) {}

void MerkleProof::print_proof() {
    cout << "Merkle proof for" << endl;
    cout << "index " << m_index << endl;
    cout << "root_hash " << m_root_hash.substr(0, 4) << endl;
    cout << "branch" << endl;
    for(int i=0; i<m_branch.size(); i++) {
        cout << m_branch[i].substr(0,4) << " ";
    }
    cout << endl;
}

bool MerkleProof::validate() const {
    string cur_hash = picosha2::hash256_hex_string(m_data);
    int cur_index = m_index;
    for(int i=0; i<m_branch.size(); i++) {
        if(cur_index%2 == 0) {
            cur_hash = hash_2_leaf(cur_hash, m_branch[i]);
        }
        else {
            cur_hash = hash_2_leaf(m_branch[i], cur_hash);
        }
        cur_index /= 2;
    }
    return cur_hash==m_root_hash;
}

const vector<uint8_t> MerkleProof::data() {
    return m_data;
}

int MerkleProof::index() {
    return m_index;
}

const string& MerkleProof::root_hash() {
    return m_root_hash;
}

const vector<string>& MerkleProof::branch() {
    return m_branch;
}

