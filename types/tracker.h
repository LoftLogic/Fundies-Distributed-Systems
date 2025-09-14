#ifndef TRACKER_H
#define TRACKER_H

#include <string>
#include <vector>
#include <set>
#include <mutex>
#include <netinet/in.h>
#include <cstring>

/**
 * Represents a remote process being coordinated with.
 * A peer is basically just an endpoint that is talked to.
 */
struct Peer {
    int id;
    std::string hostname;
    struct sockaddr_in addr; // IPv4 socket type, includes address faily, port, IP address
    bool addrResolved;

    Peer(int id, const std::string &hostname) : id(id), hostname(hostname), addrResolved(false) {
        memset(&addr, 0, sizeof(addr));
    }

};

/**
 * Tracks the state of peers - who we've heard from, who we've acked, etc.
 * Also handles thread safety
 */
class PeerTracker {
    private:
        mutable std::mutex mtx;
        std::vector<Peer> peers;
        std::set<int> helloRecords; // IDs of peers who have sent HELLO
        std::set<int> ackRecords; // IDs of peers who have sent ACK
        int myId;
        int totalProcesses;

    public:
        PeerTracker(int myId, int totalProcesses) : myId(myId), totalProcesses(totalProcesses) {}

        // Adds a peer from hostfile to peers
        void addPeer(int id, const std::string& hostname);

        // Adds id to hello records
        void recordHello(int peerId);

        // Adds id to ack records
        void recordAck(int peerId);

        // Check if full coordination is achieved
        bool allPeersReady() const;

        // Get peers that we need acknowledgments from
        std::vector<int> getUnackPeers() const;

        // Get peer by ID, returns nullptr if not found
        const Peer* getPeer(int id) const;

        // Gets all peers
        std::vector<Peer>& getAllPeers() { return peers; }

        // Checks if we need to acknowledge a peer
        bool shouldAckPeer(int peerId) const;

        // For debugging
        void printStatus() const; 

        int getMyId() const { return myId; }
        int getTotalProcesses() const { return totalProcesses; }
};

#endif // TRACKER_H