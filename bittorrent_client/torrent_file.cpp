#include "torrent_file.h"
#include "bencode.h"
#include "sha1.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

void TorrentFile::load(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary); //reads data from the file present at filepath & read in binary 
    if (!file) throw std::runtime_error("Could not open torrent file");

    std::ostringstream ss;
    ss << file.rdbuf(); //dumps the content into 'ss' stream
    std::string data = ss.str();

    bencode::BNode* root = nullptr;
    try {
        root = bencode::decode(data);
        if (!root || root->type != bencode::Type::Dictionary) {
            throw std::runtime_error("Invalid torrent file format");
        }

        const auto& dict = root->get_dictionary();
        
        if (dict.count("announce")) {
            announce = dict.at("announce")->get_string();
        }

        if (dict.count("info")) {
            auto info = dict.at("info");
            if (info->type != bencode::Type::Dictionary) {
                throw std::runtime_error("Info is not a dictionary");
            }

            const auto& info_dict = info->get_dictionary();
            
            if (info_dict.count("name")) {
                name = info_dict.at("name")->get_string();
            }
            if (info_dict.count("piece length")) {
                piece_length = info_dict.at("piece length")->get_integer();
            }
            if (info_dict.count("pieces")) {
                pieces = info_dict.at("pieces")->get_string();
            }
            if (info_dict.count("length")) {
                length = info_dict.at("length")->get_integer();
            } else if (info_dict.count("files")) {
                //it contains a list of dictionaries each having length and url
                auto files = info_dict.at("files")->get_list();
                for (const auto& file_node : files) {
                    const auto& file_dict = file_node->get_dictionary();
                    if (file_dict.count("length")) {
                        length += file_dict.at("length")->get_integer();
                    }
                }
            }

            // Calculate info hash from RAW bytes in the original .torrent file.
            // We must NOT re-encode the parsed tree, because re-encoding may produce
            // different bytes than the original (key ordering, etc.), giving a wrong hash.
            size_t info_key_pos = data.find("4:infod");
            if (info_key_pos == std::string::npos) {
                throw std::runtime_error("Could not locate raw info dictionary in torrent data");
            }
            size_t info_val_start = info_key_pos + 6; // skip past "4:info", now pointing at 'd'
            size_t info_val_end = info_val_start;
            bencode::BNode* temp = bencode::decode(data, info_val_end); // advances past the dict
            delete temp; // we only needed the position advancement
            std::string raw_info = data.substr(info_val_start, info_val_end - info_val_start);
            info_hash = compute_sha1(raw_info);
        } else {
            throw std::runtime_error("Missing info dictionary");
        }

        delete root;
    } catch (...) {
        delete root;
        throw;
    }
}
