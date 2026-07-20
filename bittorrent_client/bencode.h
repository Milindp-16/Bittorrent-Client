#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>


namespace bencode {

enum class Type {
    Integer,
    String,
    List,
    Dictionary
};

class BNode;

using BInteger = long long;
using BString = std::string;
using BList = std::vector<BNode*>;
using BDictionary = std::map<std::string, BNode*>;

class BNode {
public:
    Type type;
    BInteger int_val;
    BString str_val;
    BList list_val;
    BDictionary dict_val;

    //constructor overloading
    BNode(BInteger v) {
        type = Type::Integer;
        int_val = v;
    }
    BNode(BString v) {
        type = Type::String;
        str_val = v;
    }
    BNode(BList v) {
        type = Type::List;
        list_val = v;
    }
    BNode(BDictionary v) {
        type = Type::Dictionary;
        dict_val = v;
    }

    BInteger get_integer() const { return int_val; }
    BString get_string() const { return str_val; }
    const BList& get_list() const { return list_val; }
    const BDictionary& get_dictionary() const { return dict_val; }

    ~BNode() {
        if (type == Type::List) {
            for (auto node : list_val) delete node;
        } else if (type == Type::Dictionary) {
            for (auto& pair : dict_val) delete pair.second;
        }
    }
};

BNode* decode(const std::string& data, size_t& pos);
BNode* decode(const std::string& data);

std::string encode(const BNode* node);

} // namespace bencode
