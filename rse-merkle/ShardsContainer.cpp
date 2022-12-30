#include "ShardsContainer.h"

ShardsContainer::ShardsContainer(unsigned node_num) {
    m_nodenum = node_num;
    m_threshold = node_num - (node_num - 1) / 3;
}
void ShardsContainer::set_pramas(unsigned node_num) {
    m_nodenum = node_num;
    m_threshold = node_num - (node_num - 1) / 3;
}

int ShardsContainer::new_block(string hash) {
    if(m_shards.count(hash) != 0) {
        return -1;
    }
    vv_char s(m_nodenum);
    pair<vv_char, unsigned> shards(s, 0);
    m_shards.insert(make_pair(hash, shards));

    return 0;
}

int ShardsContainer::insert_shard(string hash, unsigned idx, vector<uint8_t> data) {
    if(m_shards.count(hash) == 0) {
        return -1;
    }
    if(m_shards[hash].first[idx].size() != 0) {
        return -2;
    }
    m_shards[hash].first[idx] = data;
    m_shards[hash].second++;
    return 0;
}

int ShardsContainer::get_block(string hash, vv_char& shards) {
    if(m_shards.count(hash) == 0) {
        return -1;
    }
    if(m_shards[hash].second < m_threshold) {
        return -2;
    }
    shards = m_shards[hash].first;
    return 0;
}

int ShardsContainer::remove(string hash) {
    if(m_shards.erase(hash) == 0) {
        return -1;
    }
    return 0;
}

string ShardsContainer::print() {
    string s="sc pramas: m_nodenum = ";
    s+= to_string(m_nodenum);
    s+= ", m_threshold = ";
    s+= to_string(m_threshold);
    return s;
}