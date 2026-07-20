#pragma once

#include <string>
#include <vector>

class TorrentFile {
public:
    std::string announce;//url of the tracker server
    long long length = 0; // Total size
    std::string name;
    long long piece_length = 0;
    std::string pieces;//concatenation of sha1 hashes of all the pieces(20B*(# pieces))
    //unique fingerprint of torrent itself - by encoding the entire info dictionary and taking SHA1 hash
    std::string info_hash; // 20 bytes

    void load(const std::string& filepath); //parses .torrent file from hard drive and extract metadata
};
