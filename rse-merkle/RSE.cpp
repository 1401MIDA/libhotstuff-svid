//
// Created by alang on 2022/10/24.
//

#include "RSE.h"
#include <iostream>
#include <cstring>

RSE::RSE(unsigned int node_num) {
    m_recovery_count = (node_num - 1) / 3;
    m_original_count = node_num - m_recovery_count;
}

RSE::RSE(unsigned int original_count, unsigned int recovery_count)
        :m_original_count(original_count), m_recovery_count(recovery_count) {}

/// call leo_init()
/// Perform static initialization for the library, verifying that the platform
/// is supported.
///
/// Returns 0 on success and other values on failure.
int RSE::init() {
    return leo_init();
}

void RSE::set_params(unsigned node_num) {
    m_recovery_count = (node_num - 1) / 3;
    m_original_count = node_num - m_recovery_count;
}

unsigned RSE::get_original_count() {
    return m_original_count;
}

unsigned RSE::get_recovery_count() {
    return m_recovery_count;
}

string RSE::print() {
    string s="rse params: m_original_count = ";
    s+= to_string(m_original_count);
    s+= ", m_recovery_count = ";
    s+= to_string(m_recovery_count);
    return s;
};

/// return value:
/// Leopard_Success           =  0, Operation succeeded
///
/// Leopard_NeedMoreData      = -1, Not enough recovery data received
/// Leopard_TooMuchData       = -2, Buffer counts are too high
/// Leopard_InvalidSize       = -3, Buffer size must be a multiple of 64 bytes
/// Leopard_InvalidCounts     = -4, Invalid counts provided
/// Leopard_InvalidInput      = -5, A function parameter was invalid
/// Leopard_Platform          = -6, Platform is unsupported
/// Leopard_CallInitialize    = -7, Call leo_init() first
/// leopard_InitialFailed     = -8, Call leo_init() but return failed
/// leopard_WorkCountFailed   = -9, Failed to Calculate WorkCount
int RSE::encode(const vector<uint8_t> &input, vector<vector<uint8_t>> &output) {
    // Calculate buffer_bytes

    uint64_t data_bytes = input.size();
    uint64_t total_bytes = data_bytes + 8 + 8;
    uint64_t slice_bytes = static_cast<unsigned>((total_bytes + m_original_count - 1) / m_original_count);
    uint64_t buffer_bytes = (slice_bytes + 64 - 1) / 64 * 64;

    cout << "encode slice_bytes= " << slice_bytes << ", buffer_bytes= " << buffer_bytes << endl;

    // Prepare input_data

    uint64_t input_bytes = slice_bytes * m_original_count;
    uint8_t* input_data = new uint8_t[input_bytes];
    memset(input_data, 0, input_bytes);
    memcpy(input_data, &data_bytes, 8);
    memcpy(input_data+8, &slice_bytes, 8);
    memcpy(input_data+16, input.data(), data_bytes);

    // Calculate encode_work_count

    const unsigned encode_work_count = leo_encode_work_count(m_original_count, m_recovery_count);
    if ( encode_work_count == 0 ) {
        return -9;
    }

    // Allocate memory

    vector<uint8_t*> original_data(m_original_count);
    vector<uint8_t*> encode_work_data(encode_work_count);
    for (unsigned i = 0, count = m_original_count; i < count; ++i)
        original_data[i] = leopard::SIMDSafeAllocate(buffer_bytes);
    for (unsigned i = 0, count = encode_work_count; i < count; ++i)
        encode_work_data[i] = leopard::SIMDSafeAllocate(buffer_bytes);

    // Generate original_data

    for (unsigned i = 0; i < m_original_count; ++i) {
        memset(original_data[i], 0, buffer_bytes);
        memcpy(original_data[i], input_data + i * slice_bytes, slice_bytes);
    }

    // Encode

    LeopardResult e_rst = leo_encode(
            buffer_bytes,
            m_original_count,
            m_recovery_count,
            encode_work_count,
            (void**)&original_data[0],
            (void**)&encode_work_data[0]
    );
    if (e_rst != 0)  {
        return e_rst;
    }

    // Generate output

    for (int i = 0; i < m_original_count; i++) {
//        output.emplace_back((char*)original_data[i], (char*)original_data[i]+slice_bytes);
        output.emplace_back((uint8_t*)original_data[i], (uint8_t*)original_data[i]+buffer_bytes);
    }
    for (int i = 0; i < m_recovery_count; i++) {
//        output.emplace_back((char*)encode_work_data[i], (char*)encode_work_data[i]+slice_bytes);
        output.emplace_back((uint8_t*)encode_work_data[i], (uint8_t*)encode_work_data[i]+buffer_bytes);
    }

    // Free Memory

    for (unsigned i = 0, count = m_original_count; i < count; ++i)
        leopard::SIMDSafeFree(original_data[i]);
    for (unsigned i = 0, count = encode_work_count; i < count; ++i)
        leopard::SIMDSafeFree(encode_work_data[i]);
    delete[] input_data;

    return 0;
}

int RSE::decode(const vector<vector<uint8_t>> &input, vector<uint8_t> &output) {
    uint64_t buffer_bytes=0;
    for (int i=0; i<input.size(); i++){
        if (input[i].size()!=0) {
            buffer_bytes = input[i].size();
        }
    }

    if (buffer_bytes == 0 || buffer_bytes%64 != 0) {
        return -1;
    }
    cout << "decode buffer_bytes= " << buffer_bytes << endl;

    unsigned lost_index = 0;
    uint64_t data_bytes;
    uint64_t slice_bytes;

    if(input[0].size() != 0)
    {
        memcpy(&data_bytes, input[0].data(), 8);
        memcpy(&slice_bytes, input[0].data()+8, 8);
        cout << "decode data_bytes= " << data_bytes << ", slice_bytes= " << slice_bytes << endl;

        for (; lost_index < m_original_count; ++lost_index) {
            if(input[lost_index].size()==0)
                break;
            output.insert(output.end(), input[lost_index].begin(), input[lost_index].begin()+slice_bytes);  
        }
        if(lost_index==m_original_count)
        {
            output.erase(output.begin(), output.begin()+16);
            output.erase(output.begin()+data_bytes, output.end());
            return 0;
        }
    }


    // Calculate decode_work_count

    unsigned decode_work_count = leo_decode_work_count(m_original_count, m_recovery_count);
    if (decode_work_count == 0 ) {
        return -9;
    }

    // Allocate memory

    vector<uint8_t*> original_data(m_original_count);
    vector<uint8_t*> recovery_data(m_recovery_count);
    vector<uint8_t*> decode_work_data(decode_work_count);
    for (unsigned i = 0, count = m_original_count; i < count; ++i)
        original_data[i] = leopard::SIMDSafeAllocate(buffer_bytes);
    for (unsigned i = 0, count = m_recovery_count; i < count; ++i)
        recovery_data[i] = leopard::SIMDSafeAllocate(buffer_bytes);
    for (unsigned i = 0, count = decode_work_count; i < count; ++i)
        decode_work_data[i] = leopard::SIMDSafeAllocate(buffer_bytes);


    for (unsigned i = 0, count = m_original_count; i < count; ++i) {
        if (input[i].size() == 0) {
            leopard::SIMDSafeFree(original_data[i]);
            original_data[i] = nullptr;
        }
        else {
            memcpy(original_data[i], input[i].data(), buffer_bytes);
        }
    }

    for (unsigned i = 0, count = m_recovery_count; i < count; ++i) {
        if (input[m_original_count+i].size() == 0) {
            leopard::SIMDSafeFree(recovery_data[i]);
            recovery_data[i] = nullptr;
        }
        else {
            memcpy(recovery_data[i], input[m_original_count+i].data(), buffer_bytes);
        }
    }

    // Decode

    LeopardResult d_rst = leo_decode(
            buffer_bytes,
            m_original_count,
            m_recovery_count,
            decode_work_count,
            (void**)&original_data[0],
            (void**)&recovery_data[0],
            (void**)&decode_work_data[0]
    );
    if (d_rst != 0)  {
        return d_rst;
    }

    if (input[0].size() == 0) {
        memcpy(&data_bytes, decode_work_data[0], 8);
        memcpy(&slice_bytes, decode_work_data[0]+8, 8);
        cout << "decode data_bytes= " << data_bytes << ", slice_bytes= " << slice_bytes << endl;
    }


    for (; lost_index < m_original_count; ++lost_index) {
        if (input[lost_index].size() == 0) {
            vector<uint8_t> tmp((uint8_t *)decode_work_data[lost_index], (uint8_t *)decode_work_data[lost_index]+slice_bytes);
            output.insert(output.end(), tmp.begin(), tmp.end());
        }
        else {
            output.insert(output.end(), input[lost_index].begin(), input[lost_index].begin()+slice_bytes);
        }
    }

    output.erase(output.begin(), output.begin()+16);
    output.erase(output.begin()+data_bytes, output.end());

    // Free Memory

    for (unsigned i = 0, count = m_original_count; i < count; ++i)
        leopard::SIMDSafeFree(original_data[i]);
    for (unsigned i = 0, count = m_recovery_count; i < count; ++i)
        leopard::SIMDSafeFree(recovery_data[i]);
    for (unsigned i = 0, count = decode_work_count; i < count; ++i)
        leopard::SIMDSafeFree(decode_work_data[i]);

    return 0;
}
