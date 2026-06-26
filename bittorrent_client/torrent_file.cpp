#include "torrent_file.h"
#include "bencode.h"
#include "sha1.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

void TorrentFile::load(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) throw std::runtime_error("Could not open torrent file");

    std::ostringstream ss;
    ss << file.rdbuf();
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
    } else if (dict.count("announce-list")) {
        // Fallback or multi-tracker, grab first one for simplicity
        auto alist = dict.at("announce-list")->get_list();
        if (!alist.empty() && alist[0]->type == bencode::Type::List) {
            auto inner = alist[0]->get_list();
            if (!inner.empty()) {
                announce = inner[0]->get_string();
            }
        }
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

        // Calculate info hash
        std::string info_bencoded = bencode::encode(info);
        info_hash = compute_sha1(info_bencoded);
    } else {
        throw std::runtime_error("Missing info dictionary");
    }

    delete root;
    } catch (...) {
        delete root;
        throw;
    }
}
