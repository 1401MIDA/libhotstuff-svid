#ifndef RSE_MERKEL_SHARDSCONTAINER
#define RSE_MERKEL_SHARDSCONTAINER

#include <map>
#include <vector>
#include <string>

using namespace std;
typedef vector<vector<uint8_t>> vv_char;

class ShardsContainer {
private:
    map<string, pair<vv_char, unsigned>> m_shards;
    unsigned m_threshold;
    unsigned m_nodenum;
public:
    ShardsContainer(): m_threshold(0), m_nodenum(0){};
    ShardsContainer(unsigned node_num);
    void set_pramas(unsigned node_num);
    int new_block(string hash);
    int insert_shard(string hash, unsigned idx, vector<uint8_t> data);
    int get_block(string hash, vv_char& shards);
    int remove(string hash);
    bool enough(string hash);
    string print();
};

#endif