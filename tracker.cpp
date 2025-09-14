#include "types/tracker.h"
#include <iostream>

void PeerTracker::addPeer(int id, const std::string& hostname) {

    // Don't add outselves as a peer
    if (id == myId) {
        return;
    }

    peers.emplace_back(id, hostname);
}

void PeerTracker::recordHello(int peerId) {
    std::lock_guard<std::mutex> lock(mtx); // For thread safety
    helloRecords.insert(peerId);
}

void PeerTracker::recordAck(int peerId) {
    std::lock_guard<std::mutex> lock(mtx); // For thread safety
    ackRecords.insert(peerId);
}

bool PeerTracker::allPeersReady() const {
    std::lock_guard<std::mutex> lock(mtx);

    // Peers are ready if everyone has sent us an ACK and have sent Hellos
    return ackRecords.size() == (totalProcesses - 1) && helloRecords.size() == (totalProcesses - 1);
}

std::vector<int> PeerTracker::getUnackPeers() const {
    std::lock_guard<std::mutex> lock(mtx); // thread safety
    std::vector<int> unacked;

    for (const auto& peer: peers) {
        if (ackRecords.find(peer.id) == ackRecords.end()) {
            unacked.push_back(peer.id);
        }
    }

    return unacked;
}


const Peer* PeerTracker::getPeer(int id) const {
    for (const auto& peer: peers) {
        if (peer.id == id) return &peer;
    }
    
    return nullptr;
}

bool PeerTracker::shouldAckPeer(int peerId) const {
    std::lock_guard<std::mutex> lock(mtx);

    // We should ack a peer if we've received a hello from them
    return helloRecords.find(peerId) != helloRecords.end();
}

void PeerTracker::printStatus() const {
    std::lock_guard<std::mutex> lock(mtx);

    fprintf(stderr, "Process %d Status- %zu/%d ACKs, %zu/%d Hellos\n",
        myId, ackRecords.size(), totalProcesses - 1, helloRecords.size(), totalProcesses - 1);
}