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
    //details of me(Peer)
    std::string peer_id;
    int port = 6881;
    long long uploaded = 0;
    long long downloaded = 0;

    Tracker();
    //list of peers from the swarm to which we can connect to
    std::vector<Peer> request_peers(const TorrentFile& tf);
};
