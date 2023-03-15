/**
 * Copyright 2018 VMware
 * Copyright 2018 Ted Yin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cassert>
#include <stack>

#include "hotstuff/util.h"
#include "hotstuff/consensus.h"
#include <iostream>

#define LOG_INFO HOTSTUFF_LOG_INFO
#define LOG_DEBUG HOTSTUFF_LOG_DEBUG
#define LOG_WARN HOTSTUFF_LOG_WARN
#define LOG_PROTO HOTSTUFF_LOG_PROTO

namespace hotstuff {

struct Commands {
    std::vector<uint256_t> cmds;
    Commands(const std::vector<uint256_t> &cmds): cmds(cmds) {}
    
    void serialize(DataStream &s) const {
        s << htole((uint32_t)cmds.size());
        for (auto cmd: cmds)
            s << cmd;
    }

    void unserialize(DataStream &s) {
        uint32_t n;
        s >> n;
        n = letoh(n);
        cmds.resize(n);
        for (auto &cmd: cmds)
            s >> cmd;
    }
};
/* The core logic of HotStuff, is fairly simple :). */
/*** begin HotStuff protocol logic ***/
HotStuffCore::HotStuffCore(ReplicaID id,
                            privkey_bt &&priv_key):
        b0(new Block(true, 1)),
        b_lock(b0),
        b_exec(b0),
        vheight(0),
        priv_key(std::move(priv_key)),
        tails{b0},
        vote_disabled(false),
        id(id),
        storage(new EntityStorage()) {
                    
            if (RSE::init() != 0) {
                throw std::runtime_error("leo_init failed.");
            }
            LOG_INFO("leo_init success.");
    storage->add_blk(b0);
}

void HotStuffCore::sanity_check_delivered(const block_t &blk) {
    if (!blk->delivered)
        throw std::runtime_error("block not delivered");
}

block_t HotStuffCore::get_delivered_blk(const uint256_t &blk_hash) {
    block_t blk = storage->find_blk(blk_hash);
    if (blk == nullptr || !blk->delivered)
        throw std::runtime_error("block not delivered");
    return blk;
}

bool HotStuffCore::on_deliver_blk(const block_t &blk) {
    if (blk->delivered)
    {
        LOG_WARN("attempt to deliver a block twice");
        return false;
    }
    blk->parents.clear();
    for (const auto &hash: blk->parent_hashes)
        blk->parents.push_back(get_delivered_blk(hash));
    blk->height = blk->parents[0]->height + 1;

    if (blk->qc)
    {
        block_t _blk = storage->find_blk(blk->qc->get_obj_hash());
        if (_blk == nullptr)
            throw std::runtime_error("block referred by qc not fetched");
        blk->qc_ref = std::move(_blk);
    } // otherwise blk->qc_ref remains null

    for (auto pblk: blk->parents) tails.erase(pblk);
    tails.insert(blk);

    blk->delivered = true;
    LOG_DEBUG("deliver %s", std::string(*blk).c_str());
    return true;
}

void HotStuffCore::update_hqc(const block_t &_hqc, const quorum_cert_bt &qc) {
    if (_hqc->height > hqc.first->height)
    {
        hqc = std::make_pair(_hqc, qc->clone());
        on_hqc_update();
    }
}

std::vector<uint256_t> async_decode(RSE rse, std::vector<std::vector<uint8_t>> decode_input){
    std::vector<uint8_t> decode_output;
    std::vector<uint256_t> cmds;
    if(rse.decode(decode_input, decode_output)==0)
    {
        assert(decode_output.size()%32==0);
        unsigned cmd_size = decode_output.size()/32;
        // LOG_PROTO("2-chain: try to recover %d cmds for blk %s", cmd_size, blk1_hash.substr(0,10).c_str());
        
        for(unsigned i=0; i<cmd_size; i++)
        {
            std::vector<uint8_t> cmd(decode_output.begin()+i*32, decode_output.begin()+i*32+32);
            uint256_t c;
            c.from_bytes(cmd);
            cmds.emplace_back(c);
        }
        // LOG_PROTO("2-chain: Decode %d cmds for blk %s", cmd_size, blk1_hash.substr(0,10).c_str());
    }
    else
    {
        // LOG_WARN("2-chain: Failed to decode for blk %s", blk1_hash.substr(0,10).c_str());
    }
    return cmds;
}


void HotStuffCore::update(const block_t &nblk) {
    /* nblk = b*, blk2 = b'', blk1 = b', blk = b */
#ifndef HOTSTUFF_TWO_STEP
    /* three-step HotStuff */
    const block_t &blk2 = nblk->qc_ref;
    if (blk2 == nullptr) return;
    /* decided blk could possible be incomplete due to pruning */
    if (blk2->decision) return;
    string blk2_hash = get_hex((*blk2).get_hash());
    
    update_hqc(blk2, nblk->qc);

    const block_t &blk1 = blk2->qc_ref;
    if (blk1 == nullptr) return;
    if (blk1->decision) return;

    string blk1_hash = get_hex((*blk1).get_hash());
    if (!sc.enough(blk1_hash))
    {
        LOG_WARN("1-chain: No sufficient Slice for blk %s", blk1_hash.substr(0,10).c_str());
    }
    std::vector<std::vector<uint8_t>> decode_input;
    if(sc.get_block(blk1_hash, decode_input)==0)
    {
        std::future<std::vector<uint256_t> > fu = std::async(async_decode, rse, decode_input);
        futures.insert(std::make_pair(blk1_hash, fu.share()));
    }
    
    if (blk1->height > b_lock->height) b_lock = blk1;

    const block_t &blk = blk1->qc_ref;
    if (blk == nullptr) return;
    if (blk->decision) return;

    /* commit requires direct parent */
    if (blk2->parents[0] != blk1 || blk1->parents[0] != blk) return;
#else
    /* two-step HotStuff */
    const block_t &blk1 = nblk->qc_ref;
    if (blk1 == nullptr) return;
    if (blk1->decision) return;
    update_hqc(blk1, nblk->qc);
    if (blk1->height > b_lock->height) b_lock = blk1;

    const block_t &blk = blk1->qc_ref;
    if (blk == nullptr) return;
    if (blk->decision) return;

    /* commit requires direct parent */
    if (blk1->parents[0] != blk) return;
#endif
    /* otherwise commit */
    std::vector<block_t> commit_queue;
    block_t b;
    for (b = blk; b->height > b_exec->height; b = b->parents[0])
    { /* TODO: also commit the uncles/aunts */
        commit_queue.push_back(b);
    }
    if (b != b_exec)
        throw std::runtime_error("safety breached :( " +
                                std::string(*blk) + " " +
                                std::string(*b_exec));
    for (auto it = commit_queue.rbegin(); it != commit_queue.rend(); it++)
    {
        const block_t &blk = *it;
        string blk_hash = get_hex(blk->get_hash());
        blk->decision = 1;
        do_consensus(blk);
        auto item = futures.find(blk_hash);
        if (item != futures.end()) 
        {
            auto fu = item->second;
            auto blk_cmds = fu.get();
            futures.erase(blk_hash);
            sc.remove(blk_hash);
            LOG_PROTO("3-chain: find blk %s from cmds_db", get_hex10(blk->get_hash()).c_str());
            LOG_PROTO("commit %s", std::string(*blk).c_str());
            for (size_t i = 0; i < blk_cmds.size(); i++)
                do_decide(Finality(id, 1, i, blk->height,
                                blk_cmds[i], blk->get_hash()));
        }
        else
        {
            LOG_WARN("3-chain: Cannot find blk %s from cmds_db", get_hex10(blk->get_hash()).c_str());
        }
    }
    b_exec = blk;
}

block_t HotStuffCore::on_propose(const std::vector<uint256_t> &cmds,
                            const std::vector<block_t> &parents,
                            bytearray_t &&extra) {
    if (parents.empty())
        throw std::runtime_error("empty parents");
    for (const auto &_: parents) tails.erase(_);
    /* create the new block */
    // todo: cmds -> cmds.hash
    Commands c(cmds);
    std::vector<uint256_t> cmd_hash = {salticidae::get_hash(c)};
    block_t bnew = storage->add_blk(
        new Block(parents, cmd_hash,
            hqc.second->clone(), std::move(extra),
            parents[0]->height + 1,
            hqc.first,
            nullptr
        ));
    const uint256_t bnew_hash = bnew->get_hash();
    bnew->self_qc = create_quorum_cert(bnew_hash);
    on_deliver_blk(bnew);
    update(bnew);
    
    vector<uint8_t> encode_input;
    vector<uint8_t> tmp;
    for(size_t i = 0; i < cmds.size(); i++){
        tmp = cmds[i].to_bytes();
        encode_input.insert(encode_input.end(), tmp.begin(), tmp.end());
        LOG_PROTO("cmds %d, len %d", i, tmp.size());
    }
    LOG_PROTO("encode_input size %d", encode_input.size());
    
    // Encode

    vector<vector<uint8_t>> encode_output;
    if (rse.encode(encode_input, encode_output) != 0) {
        throw std::runtime_error("encode error");
    }

    MerkleTree mt(encode_output);
    vector<MerkleProof> proofs = mt.proofs();
    std::vector<Proposal> props;
    for(auto proof: proofs) {
        Slice slice(proof, bnew_hash);
        LOG_PROTO("create %s", std::string(slice).c_str());
        Proposal prop(id, slice, bnew, nullptr);
        props.emplace_back(prop);
    }
    // Slice slice(proofs[1]);
    // Proposal prop(id, slice, bnew, nullptr);
    LOG_PROTO("propose %s", std::string(*bnew).c_str());
    if (bnew->height <= vheight)
        throw std::runtime_error("new block should be higher than vheight");
    /* self-receive the proposal (no need to send it through the network) */
    on_receive_proposal(props[get_id()]);
    on_propose_(props[get_id()]);
    /* boradcast to other replicas */
    // do_broadcast_proposal(prop);
    do_broadcast_proposal_with_slice(props);
    
    return bnew;
}

void HotStuffCore::on_receive_proposal(const Proposal &prop) {
    LOG_PROTO("got %s", std::string(prop).c_str());
    
    assert(prop.s_hash == salticidae::get_hash(prop.slice));
    assert(prop.slice.validate());
    on_receive_slice(prop.slice);
    do_broadcast_slice(prop.slice);
    LOG_PROTO("broadcast %s", std::string(prop.slice).c_str());

    bool self_prop = prop.proposer == get_id();
    block_t bnew = prop.blk;
    if (!self_prop)
    {
        sanity_check_delivered(bnew);
        update(bnew);
    }
    bool opinion = false;
    if (bnew->height > vheight)
    {
        if (bnew->qc_ref && bnew->qc_ref->height > b_lock->height)
        {
            opinion = true; // liveness condition
            vheight = bnew->height;
        }
        else
        {   // safety condition (extend the locked branch)
            block_t b;
            for (b = bnew;
                b->height > b_lock->height;
                b = b->parents[0]);
            if (b == b_lock) /* on the same branch */
            {
                opinion = true;
                vheight = bnew->height;
            }
        }
    }
    LOG_PROTO("now state: %s", std::string(*this).c_str());
    if (!self_prop && bnew->qc_ref)
        on_qc_finish(bnew->qc_ref);
    on_receive_proposal_(prop);
    if (opinion && !vote_disabled)
        do_vote(prop.proposer,
            Vote(id, bnew->get_hash(),
                create_part_cert(*priv_key, bnew->get_hash()), this));
}

void HotStuffCore::on_receive_vote(const Vote &vote) {
    LOG_PROTO("got %s", std::string(vote).c_str());
    LOG_PROTO("now state: %s", std::string(*this).c_str());
    block_t blk = get_delivered_blk(vote.blk_hash);
    assert(vote.cert);
    size_t qsize = blk->voted.size();
    if (qsize >= config.nmajority) return;
    if (!blk->voted.insert(vote.voter).second)
    {
        LOG_WARN("duplicate vote for %s from %d", get_hex10(vote.blk_hash).c_str(), vote.voter);
        return;
    }
    auto &qc = blk->self_qc;
    if (qc == nullptr)
    {
        LOG_WARN("vote for block not proposed by itself");
        qc = create_quorum_cert(blk->get_hash());
    }
    qc->add_part(vote.voter, *vote.cert);
    if (qsize + 1 == config.nmajority)
    {
        qc->compute();
        update_hqc(blk, qc);
        on_qc_finish(blk);
    }
}

void HotStuffCore::on_receive_slice(const Slice &slice) {
    LOG_PROTO("got %s", std::string(slice).c_str());
    if (slice.validate()==false)
    {
        LOG_WARN("Invalide Slice %s", std::string(slice).c_str());
        return;
    }
    string blk_hash = get_hex(slice.m_blk_hash);
    if (sc.insert_shard(blk_hash, slice.m_index, slice.m_data)!=0)
    {
        LOG_WARN("Repeated acceptance of Slice %s", std::string(slice).c_str());
        return;
    }
    LOG_PROTO("sc insert %s", std::string(slice).c_str());

}

/*** end HotStuff protocol logic ***/
void HotStuffCore::on_init(uint32_t nfaulty) {
    config.nmajority = config.nreplicas - nfaulty;
    b0->qc = create_quorum_cert(b0->get_hash());
    b0->qc->compute();
    b0->self_qc = b0->qc->clone();
    b0->qc_ref = b0;
    hqc = std::make_pair(b0, b0->qc->clone());
}

void HotStuffCore::prune(uint32_t staleness) {
    block_t start;
    /* skip the blocks */
    for (start = b_exec; staleness; staleness--, start = start->parents[0])
        if (!start->parents.size()) return;
    std::stack<block_t> s;
    start->qc_ref = nullptr;
    s.push(start);
    while (!s.empty())
    {
        auto &blk = s.top();
        if (blk->parents.empty())
        {
            storage->try_release_blk(blk);
            s.pop();
            continue;
        }
        blk->qc_ref = nullptr;
        s.push(blk->parents.back());
        blk->parents.pop_back();
    }
}

void HotStuffCore::add_replica(ReplicaID rid, const PeerId &peer_id,
                                pubkey_bt &&pub_key) {
    config.add_replica(rid,
            ReplicaInfo(rid, peer_id, std::move(pub_key)));
    b0->voted.insert(rid);
}

promise_t HotStuffCore::async_qc_finish(const block_t &blk) {
    if (blk->voted.size() >= config.nmajority)
        return promise_t([](promise_t &pm) {
            pm.resolve();
        });
    auto it = qc_waiting.find(blk);
    if (it == qc_waiting.end())
        it = qc_waiting.insert(std::make_pair(blk, promise_t())).first;
    return it->second;
}

void HotStuffCore::on_qc_finish(const block_t &blk) {
    auto it = qc_waiting.find(blk);
    if (it != qc_waiting.end())
    {
        it->second.resolve();
        qc_waiting.erase(it);
    }
}

promise_t HotStuffCore::async_wait_proposal() {
    return propose_waiting.then([](const Proposal &prop) {
        return prop;
    });
}

promise_t HotStuffCore::async_wait_receive_proposal() {
    return receive_proposal_waiting.then([](const Proposal &prop) {
        return prop;
    });
}

promise_t HotStuffCore::async_hqc_update() {
    return hqc_update_waiting.then([this]() {
        return hqc.first;
    });
}

void HotStuffCore::on_propose_(const Proposal &prop) {
    auto t = std::move(propose_waiting);
    propose_waiting = promise_t();
    t.resolve(prop);
}

void HotStuffCore::on_receive_proposal_(const Proposal &prop) {
    auto t = std::move(receive_proposal_waiting);
    receive_proposal_waiting = promise_t();
    t.resolve(prop);
}

void HotStuffCore::on_hqc_update() {
    auto t = std::move(hqc_update_waiting);
    hqc_update_waiting = promise_t();
    t.resolve();
}

HotStuffCore::operator std::string () const {
    DataStream s;
    s << "<hotstuff "
      << "hqc=" << get_hex10(hqc.first->get_hash()) << " "
      << "hqc.height=" << std::to_string(hqc.first->height) << " "
      << "b_lock=" << get_hex10(b_lock->get_hash()) << " "
      << "b_exec=" << get_hex10(b_exec->get_hash()) << " "
      << "vheight=" << std::to_string(vheight) << " "
      << "tails=" << std::to_string(tails.size()) << ">";
    return s;
}

}
