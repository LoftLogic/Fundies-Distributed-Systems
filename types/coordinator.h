#ifndef COORDINATOR_H
#define COORDINATOR_H

#include "protocol.h"
#include "tracker.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <set>
#include <vector>
#include <atomic>


/**
 * Coordinates UDP messages:
 * 1. Startup phase (parse config file, discover peers, whoami, bind UDP socket)
 * 2. Discovery phase (senderThread will send out HELLOs to peers that havent acked us, and listener thread hears HELLO and sends ACK back and
 *      marks peers that have acked us)
 * 3. Ready phase (once all peers ACKed us for our HELLOs, set ready to true)
 */
class UDPCoordinator {
    private:
        std::string configFilePath_;

        // Tracks which peers exist
        std::unique_ptr<PeerTracker> tracker;

        int socket_fd;

        // "go" signal, activates when all peers have acknowledged eachother
        std::atomic<bool> ready;

        // "shutdown" flag for background threads (for clean shutdown)
        std::atomic<bool> shouldStop;

        // Sequence number for outgoing messages (for duplicates, packet loss or out of order, kind of like a stamp)
        unsigned messageSequence;

        /* SETUP */

        // Returns true if successful
        bool createSocket(); 

        // Binds the socket to the specified port, returns true if successful
        bool bindSocket(unsigned port); 

        // Resolves hostnames from configfile to IP addresses
        bool resolvePeerAddresses(); 

        /* Parsing */

        // Reads and parses config file to get all processes
        bool parseConfigFile(const std::string& configFilePath);
        int getMyIdFromConfig();


        /* Message Handling */

        // Sends a UDP message to a peer
        void sendMessage(const Peer& peer, const Message& msg);

        // Sends Hello messages to all peers that haven't acknowledged us yet
        void broadcastHello();

        // Sends an ACK message in response to hello
        void sendAck(unsigned peerId);

        /* Threads */

        // Listens for incoming UDP messages
        void listenerThread();

        // Broadcasts HELLOs to unconfirmed peers
        void senderThread();

        void cleanup();
    
    public:
        UDPCoordinator(const std::string& configFilePath);
        ~UDPCoordinator();

        bool coordinate();

        UDPCoordinator(const UDPCoordinator&) = delete;
        UDPCoordinator& operator=(const UDPCoordinator&) = delete;
};

#endif // COORDINATOR_H