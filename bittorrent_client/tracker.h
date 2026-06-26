#pragma once

#include "torrent_file.h"
#include <string>
#include <vector>
#include <cstdint>

struct Peer {
    std::string ip;
    uint16_t port;
};

class Tracker {
public:
    std::string peer_id;
    //default port for bittorrent where other peers listen for incoming connections
    int port = 6881;
    long long uploaded = 0;
    long long downloaded = 0;

    Tracker();
    //list of peers from the swarm to which we can connect to
    std::vector<Peer> request_peers(const TorrentFile& tf);
};
