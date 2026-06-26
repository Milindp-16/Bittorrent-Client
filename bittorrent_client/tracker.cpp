#include "tracker.h"
#include "bencode.h"
#include <windows.h>
#include <wininet.h>
#include <iostream>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "wininet.lib")

Tracker::Tracker() {
    //generating random 20 characters peer_id 
    peer_id = "-PC0001-";
    for (int i = 0; i < 12; i++) {
        peer_id += std::to_string(rand() % 10);
    }
}

//it performs url encoding so that url is valid and readable
std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (unsigned char c : value) {
        //safe characters
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else { //unsafe characters are replaced with %xx where xx is the hex code of the character
            escaped << std::uppercase << '%' << std::setw(2) << int((unsigned char) c);
            escaped << std::nouppercase;
        }
    }
    return escaped.str();
}

std::vector<Peer> Tracker::request_peers(const TorrentFile& tf) {
    std::vector<Peer> peers;

    //grab the tracker url from the torrent file
    std::string url = tf.announce;
    
    //reject udp protocols
    if (url.rfind("udp://", 0) == 0) {
        std::cerr << "Warning: UDP trackers are not supported in this basic HTTP tracker implementation." << std::endl;
        return peers;
    }



    //chopping the full url into different parts hostname and path
    bool is_https = url.rfind("https://", 0) == 0;
    std::string no_proto = url.substr(is_https ? 8 : 7);//removing https:// or http:// from the url
    size_t slash_pos = no_proto.find('/');
    //slash is not found
    if (slash_pos == std::string::npos) {
        //put the slash at the end
        slash_pos = no_proto.length();
        no_proto += "/";
    }
    std::string hostname = no_proto.substr(0, slash_pos);
    std::string path = no_proto.substr(slash_pos);

    //finding the port if present in the url
    size_t colon_pos = hostname.find(':');
    int port = is_https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    if (colon_pos != std::string::npos) {
        port = std::stoi(hostname.substr(colon_pos + 1));
        hostname = hostname.substr(0, colon_pos);
    }


    //building the query parameters string
    std::string query = "?info_hash=" + url_encode(tf.info_hash) +
                        "&peer_id=" + url_encode(peer_id) +
                        "&port=" + std::to_string(this->port) +
                        "&uploaded=" + std::to_string(this->uploaded) +
                        "&downloaded=" + std::to_string(this->downloaded) +
                        "&left=" + std::to_string(tf.length) +
                        "&compact=1" +
                        "&numwant=50"; //compact format is used to reduce the size of the response

    path += query;

    //HINTERNET holds a ticket that the winsock will use to identify our application
    //InternetOpenA is used to open a session to the internet
    //A at the end of the function means it expexts standard text strings not wide unicode strings
    HINTERNET hSession = InternetOpenA("C++ BitTorrent Client/1.0",
                                       INTERNET_OPEN_TYPE_PRECONFIG, //telling to use default internet settings
                                       NULL, NULL, 0);

    if (!hSession) throw std::runtime_error("InternetOpenA failed");

    //connecting to the tracker server
    HINTERNET hConnect = InternetConnectA(hSession, hostname.c_str(), port,
                                          NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);
    if (!hConnect) {
        //clearing older tickets before closing the session
        InternetCloseHandle(hSession);
        throw std::runtime_error("InternetConnectA failed");
    }

    DWORD requestFlags = INTERNET_FLAG_RELOAD;
    if (is_https) {
        requestFlags |= INTERNET_FLAG_SECURE;
    }

    //opening a get request to the tracker server
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "GET", path.c_str(),
                                          NULL, NULL, NULL, requestFlags, 1);

    if (!hRequest) {
        //clearing older tickets before closing the connection
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hSession);
        throw std::runtime_error("HttpOpenRequestA failed");
    }

    if (is_https) {
        DWORD dwFlags = 0;
        DWORD dwBuffLen = sizeof(dwFlags);
        if (InternetQueryOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, &dwBuffLen)) {
            dwFlags |= SECURITY_FLAG_IGNORE_REVOCATION | SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
            InternetSetOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags));
        }
    }

    BOOL bResults = HttpSendRequestA(hRequest, NULL, 0, NULL, 0);

    if (!bResults) {
        DWORD err = GetLastError();
        //clearing older tickets before closing the request
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hSession);
        throw std::runtime_error("HttpSendRequestA failed with error code: " + std::to_string(err));
    }

    //receiving response data from the tracker server

    std::string response_data;
    //integer variable dwSize will hold the number of bytes available to read from the response
    DWORD dwSize = 0; //DWORD is custom data type for Windows API, under the hood it is actually unsigned int
    //integer variable dwDownloaded will hold the number of bytes actually read from the response
    DWORD dwDownloaded = 0;
    do {
        dwSize = 0;
        //we have to send the hRequest everytime to check if there is data available to read from the Network Buffer
        if (!InternetQueryDataAvailable(hRequest, &dwSize, 0, 0)) {
            break;
        }
        if (dwSize == 0) break;
        
        //creating a buffer of size dwSize + 1 to hold the temp data from the windows
        std::vector<char> buffer(dwSize + 1);
        //it reaches into the mailbox(network buffer), grabs the pages and puts them into buffer
        if (InternetReadFile(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded)) {
            response_data.append(buffer.data(), dwDownloaded);
        }
    } while (dwSize > 0);

    //we have downloaded the entire response now we are closing the handles
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hSession);

    //the response_data contains bencoded dictionary data, we are decoding it to extract the peer info
    try {
        bencode::BNode* root = bencode::decode(response_data);
        try {
            if (root && root->type == bencode::Type::Dictionary) {
                auto dict = root->get_dictionary();
                if (dict.count("failure reason")) {
                    throw std::runtime_error("Tracker error: " + dict.at("failure reason")->get_string());
                }

                if (dict.count("peers")) {
                    //trackers send peer IPs in compact form to save BW
                    //every 6 byte in this string represents exactly 1 peer
                    //4 bytes for IP address + 2 bytes for port number
                    auto peers_node = dict.at("peers");
                    if (peers_node->type == bencode::Type::String) {
                        std::string peers_str = peers_node->get_string();
                        for (size_t i = 0; i + 6 <= peers_str.size(); i += 6) {
                            Peer p;
                            unsigned char ip1 = peers_str[i];
                            unsigned char ip2 = peers_str[i+1];
                            unsigned char ip3 = peers_str[i+2];
                            unsigned char ip4 = peers_str[i+3];
                            p.ip = std::to_string(ip1) + "." + std::to_string(ip2) + "." + std::to_string(ip3) + "." + std::to_string(ip4);
                            
                            unsigned char port1 = peers_str[i+4];
                            unsigned char port2 = peers_str[i+5];
                            p.port = (port1 << 8) | port2;
                            
                            peers.push_back(p);
                        }
                    } else if (peers_node->type == bencode::Type::List) {//normal found in dictionary type peers -> list<dictionary>
                        for (auto& peer_item : peers_node->get_list()) {
                            auto pdict = peer_item->get_dictionary();
                            Peer p;
                            p.ip = pdict.at("ip")->get_string();
                            p.port = pdict.at("port")->get_integer();
                            peers.push_back(p);
                        }
                    }
                }
            }
            delete root;
        } catch (...) {
            delete root;
            throw;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse tracker response: " << e.what() << std::endl;
    }

    return peers;
}
