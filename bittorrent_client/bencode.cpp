#include "bencode.h"
#include <cctype>

namespace bencode {

BNode* decode_integer(const std::string& data, size_t& pos) {
    if (pos >= data.size() || data[pos] != 'i') {
        throw std::runtime_error("Invalid integer start");
    }
    pos++; // skip 'i'
    
    size_t end = data.find('e', pos);
    if (end == std::string::npos) {
        throw std::runtime_error("Invalid integer end");
    }
    
    std::string num_str = data.substr(pos, end - pos);
    pos = end + 1; // move past 'e'
    
    BInteger value = std::stoll(num_str);
    BNode* node = new BNode(value);
    
    return node;
}

BNode* decode_string(const std::string& data, size_t& pos) {
    size_t colon = data.find(':', pos);
    if (colon == std::string::npos) {
        throw std::runtime_error("Invalid string format");
    }
    
    std::string len_str = data.substr(pos, colon - pos);
    size_t len = std::stoull(len_str);
    pos = colon + 1; // move past ':'
    
    if (pos + len > data.size()) {
        throw std::runtime_error("String length out of bounds");
    }
    
    std::string str = data.substr(pos, len);
    pos += len; // move past the string data
    
    BNode* node = new BNode(str);
    
    return node;
}

BNode* decode_list(const std::string& data, size_t& pos) {
    if (pos >= data.size() || data[pos] != 'l') {
        throw std::runtime_error("Invalid list start");
    }
    pos++; // skip 'l'
    
    BList list;
    
    // Keep reading until we find 'e'
    while (pos < data.size() && data[pos] != 'e') {
        BNode* item = decode(data, pos);
        list.push_back(item);
    }
    
    if (pos >= data.size() || data[pos] != 'e') {
        throw std::runtime_error("Invalid list end");
    }
    pos++; // skip 'e'
    
    BNode* node = new BNode(list);
    
    return node;
}

BNode* decode_dictionary(const std::string& data, size_t& pos) {
    if (pos >= data.size() || data[pos] != 'd') {
        throw std::runtime_error("Invalid dictionary start");
    }
    pos++; // skip 'd'
    
    BDictionary dict;
    
    // Keep reading until we find 'e'
    while (pos < data.size() && data[pos] != 'e') {
        // Dictionaries are stored as key-value pairs.
        // Bencode keys are ALWAYS strings.
        BNode* key_node = decode_string(data, pos);
        std::string key_string = key_node->get_string();
        
        // We only needed the node to extract the string, so we can delete it now
        delete key_node; 
        
        // Values can be anything (integer, string, list, or another dictionary)
        BNode* val_node = decode(data, pos);
        
        // Add the pair to our map
        dict[key_string] = val_node;
    }
    
    if (pos >= data.size() || data[pos] != 'e') {
        throw std::runtime_error("Invalid dictionary end");
    }
    pos++; // skip 'e'
    
    BNode* node = new BNode(dict);
    
    return node;
}

BNode* decode(const std::string& data, size_t& pos) {
    if (pos >= data.size()) {
        throw std::runtime_error("Unexpected end of data");
    }
    
    char c = data[pos];
    
    // Route to the correct function based on the first character
    if (c == 'i') {
        return decode_integer(data, pos);
    }
    if (c == 'l') {
        return decode_list(data, pos);
    }
    if (c == 'd') {
        return decode_dictionary(data, pos);
    }
    if (std::isdigit(c)) {
        return decode_string(data, pos);
    }
    
    throw std::runtime_error("Invalid bencode format");
}

BNode* decode(const std::string& data) {
    size_t pos = 0;
    return decode(data, pos);
}

std::string encode(const BNode* node) {
    if (node == nullptr) {
        return "";
    }
    
    if (node->type == Type::Integer) {
        BInteger value = node->get_integer();
        return "i" + std::to_string(value) + "e";
    }
    
    if (node->type == Type::String) {
        std::string s = node->get_string();
        return std::to_string(s.size()) + ":" + s;
    }
    
    if (node->type == Type::List) {
        std::string res = "l";
        for (const auto& item : node->get_list()) {
            res += encode(item);
        }
        res += "e";
        return res;
    }
    
    if (node->type == Type::Dictionary) {
        std::string res = "d";
        for (auto it = node->get_dictionary().begin(); it != node->get_dictionary().end(); ++it) {
            std::string key = it->first;
            BNode* val = it->second;
            
            res += std::to_string(key.size()) + ":" + key; // Encode key
            res += encode(val);                            // Encode value
        }
        res += "e";
        return res;
    }
    
    return "";
}

} // namespace bencode
