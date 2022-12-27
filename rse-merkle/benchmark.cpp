#include "RSE.h"
#include "MerkleTree.h"
#include "ShardsContainer.h"
#include <iostream>
#include <set>

bool bench_mark() {
    srand( (unsigned)time( NULL ) );
    unsigned node_num = rand() % 1000 + 4;
    unsigned input_size = rand() % (9*1024*1024) + 1024*1024;
    RSE rse(node_num);

    // Generate Ramdom Input Buffer

    vector<uint8_t> encode_input(input_size);
    for (unsigned i = 0; i < input_size; ++i) {
        encode_input[i] = (rand()%(126-32)+32);
    }
    cout << "[node_num=" << node_num << ", original_count=" << rse.get_original_count() << ", recovery_count=" << rse.get_recovery_count() << ", input_size=" << input_size << "]" << endl;
    
    // Encode

    vector<vector<uint8_t>> encode_output;
    if (rse.encode(encode_input, encode_output) != 0) {
        cout << "encode error " << endl;
        return false;
    }

    // Create MerkleTree

    MerkleTree mt(encode_output);
    string root_hash = mt.root_hash();
    vector<MerkleProof> proofs = mt.proofs();

    // Ramdom loss shards

    set<unsigned> loss;
    for (unsigned i = 0, count = rse.get_recovery_count(); i < count; ++i) {
        loss.insert(rand()%(rse.get_original_count() + rse.get_recovery_count()));
    }
    cout << "random loss " << loss.size() << " shards" << endl;

    // Shards Delivery

    ShardsContainer s_c(node_num);
    if(s_c.new_block(root_hash)!=0){
        return false;
    };

    for(unsigned i = 0; i < proofs.size(); ++i){
        if(loss.count(i) == 0) {
            if(!proofs[i].validate()){
                cout << "Invalide Proof " << i << endl;
                return false;
            }
            else{
                s_c.insert_shard(root_hash, proofs[i].index(), proofs[i].data());
            }
        }
    }

    // On commit

    vector<vector<uint8_t>> decode_input;
    if(s_c.get_block(root_hash, decode_input)!=0) {
        return false;
    }

    // Decode

    vector<uint8_t> decode_output;
    if (rse.decode(decode_input, decode_output) !=0 ) {
        cout << "decode error " << endl;
        return false;
    }

    // Check Correctness
    
    if (encode_input != decode_output) {
        cout << "wrong" << endl;
        return false;
    }

    cout << "correct" << endl << endl;

    return true;
}

int main(int argc, char* argv[]) {
    if (RSE::init() != 0) {
        cout << "leo_init failed." << endl;
        return -1;
    }
    for(;;) {
        if (!bench_mark()) {
            cout << "failed" << endl;
            break;
        }
    }
}
