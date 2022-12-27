//
// Created by alang on 2022/10/24.
//

#ifndef RSE_MERKEL_RSE_H
#define RSE_MERKEL_RSE_H

#include "leopard.h"
#include "LeopardCommon.h"
#include <vector>

using namespace std;

class RSE {
private:
    unsigned m_original_count;
    unsigned m_recovery_count;
public:
    RSE():m_original_count(0), m_recovery_count(0){};
    RSE(unsigned node_num);
    RSE(unsigned original_count, unsigned recovery_count);
    static int init();
    void set_params(unsigned node_num);
    unsigned get_original_count();
    unsigned get_recovery_count();
    string print();
    int encode(const vector<uint8_t> &input, vector<vector<uint8_t>> &output);
    int decode(const vector<vector<uint8_t>> &input,  vector<uint8_t> &output);
};


#endif //RSE_MERKEL_RSE_H
