#pragma once

#include <string>
#include <vector>

class TorrentFile {
public:
    //contains the url of the tracker server which know the active swarm
    std::string announce;
    long long length = 0; // Total size
    //The suggested file or folder name. When your C++ program 
    //finally downloads all the pieces and stitches them together,
    std::string name;
    long long piece_length = 0;
    std::string pieces;
    //unique fingerprint of torrent itself - by encoding the entire info dictionary and taking SHA1 hash
    std::string info_hash; // 20 bytes

    void load(const std::string& filepath);
};
