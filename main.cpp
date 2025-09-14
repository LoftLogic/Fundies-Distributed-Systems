#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

#include "types/coordinator.h"

/**
 * Main File for CS7610 Project 1- Docker and UDP
 * Goals:
 *  Have n programs coordinate with eachother with UDP messages before starting a task
 *  Open a UDP socket and wait for messages from other programs (n - 1)
 *  The program will print "READY" to stderr when it hears a message from all other programs
 *  The program will read from a configuration file to get information about the other processes
 */

/**
 * Program Flow:
 * 1. Container starts up and reads config file to figure out which it is
 * 2. Creates UDP socket, bind to 8080 (default)
 * 3. Starts two threads - listener and sender
 * 4. Sender thread sends Hellos to everyone until they all ACK
 * 5. Listener thread receives messages and tracks who we've heard from
 * 6. When we get confirmation from all peers, print "READY" to stderr
 */

int main(int argc, char *argv[]) {
    if (argc != 3 || strcmp(argv[1], "-h") != 0) {
        fprintf(stderr, "Usage: %s -h <hostfile>\n", argv[0]);
        return -1;
    }

    const char* hostfilePath = argv[2];

    try {
        UDPCoordinator coordinator(hostfilePath);

        if (!coordinator.coordinate()) {
            fprintf(stderr, "ERROR: Coordination failed\n");
            return 1;
        }

        // Coordination was succesful and READY was printed (so keep process alive to ensure delivery)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 0;
    } catch (const std::exception& e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        return 1;
    }
}