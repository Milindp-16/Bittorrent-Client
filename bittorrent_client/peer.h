#pragma once

#include "tracker.h"
#include <string>
#include <vector>
#include <cstdint>
#include <winsock2.h>
#include <ws2tcpip.h>

//eeverytime we want to connect to a new peer in the swarm we make this object
class PeerConnection {
public:
    //each connection has its own socket
    //it acts like a telephone handset when we dial peer's IP window gives us this socket so we can 
    //talk into it and listen it
    SOCKET sock = INVALID_SOCKET;

    //details of the peer we are talking to
    Peer peer;
    std::string peer_id;

    std::string info_hash;

    //the other peer has currently choked us
    bool choked = true;
    //this is our intent towards the other peer towards piece sharing
    bool interested = false;

    PeerConnection(const Peer& p, const std::string& ih, const std::string& pid);
    ~PeerConnection();

    bool connect_to_peer();//tries to connect with the peer by opening a socket
    bool handshake();//does the 68 byte handshake with peer
    bool receive_message();//waits for and processes messages from peer
    bool send_interested();//send a msg to peer telling we want pieces
    bool request_piece(uint32_t index, uint32_t begin, uint32_t length);//requesting for a block of a piece index -> index of the piece,begin -> starting address of the block, length->length of the block 
    bool receive_piece(std::vector<uint8_t>& piece_data, uint32_t length);//receiving piece ,there is vector in parameter because the piece will come in blocks

private:
    //TCP is quite laze. these private methods ensure that only len bytes are sent and received
    bool send_all(const char* buf, int len);
    bool recv_all(char* buf, int len);
};
