#include "peer.h"
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

PeerConnection::PeerConnection(const Peer& p, const std::string& ih, const std::string& pid) {
    peer = p;
    info_hash = ih;
    peer_id = pid;
}

PeerConnection::~PeerConnection() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
}

bool PeerConnection::connect_to_peer() {
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return false;

    sockaddr_in clientService; 
    clientService.sin_family = AF_INET;
    clientService.sin_addr.s_addr = inet_addr(peer.ip.c_str());
    clientService.sin_port = htons(peer.port);

    DWORD timeout = 3000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    if (connect(sock, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
        closesocket(sock);
        sock = INVALID_SOCKET;
        return false;
    }
    return true;
}

bool PeerConnection::send_all(const char* buf, int len) {
    int total = 0;
    while (total < len) {
        int n = send(sock, buf + total, len - total, 0);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

bool PeerConnection::recv_all(char* buf, int len) {
    int total = 0;
    while (total < len) {
        int n = recv(sock, buf + total, len - total, 0);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

bool PeerConnection::handshake() {
    char pstr[] = "BitTorrent protocol";
    char pstr_len = 19;
    char reserved[8] = {0};

    std::vector<char> buf;
    buf.push_back(pstr_len);
    buf.insert(buf.end(), pstr, pstr + 19);
    buf.insert(buf.end(), reserved, reserved + 8);
    buf.insert(buf.end(), info_hash.begin(), info_hash.end());
    buf.insert(buf.end(), peer_id.begin(), peer_id.end());

    if (!send_all(buf.data(), buf.size())) return false;

    std::vector<char> resp(68);
    if (!recv_all(resp.data(), 68)) return false;

    if (resp[0] != 19 || std::string(resp.data() + 1, 19) != pstr) {
        return false;
    }

    std::string recv_hash(resp.data() + 28, 20);
    if (recv_hash != info_hash) {
        return false;
    }

    return true;
}

bool PeerConnection::send_interested() {
    char msg[5] = {0, 0, 0, 1, 2};
    return send_all(msg, 5);
}

bool PeerConnection::request_piece(uint32_t index, uint32_t begin, uint32_t length) {
    std::vector<char> msg;
    uint32_t len_net = htonl(13);
    msg.insert(msg.end(), (char*)&len_net, ((char*)&len_net) + 4);
    msg.push_back(6);
    uint32_t idx_net = htonl(index);
    msg.insert(msg.end(), (char*)&idx_net, ((char*)&idx_net) + 4);
    uint32_t bgn_net = htonl(begin);
    msg.insert(msg.end(), (char*)&bgn_net, ((char*)&bgn_net) + 4);
    uint32_t lgth_net = htonl(length);
    msg.insert(msg.end(), (char*)&lgth_net, ((char*)&lgth_net) + 4);

    return send_all(msg.data(), msg.size());
}

bool PeerConnection::receive_message() {
    uint32_t len_net;
    if (!recv_all((char*)&len_net, 4)) return false;
    uint32_t length = ntohl(len_net);

    if (length == 0) return true;

    char id;
    if (!recv_all(&id, 1)) return false;

    if (length > 1) {
        std::vector<char> payload(length - 1);
        if (!recv_all(payload.data(), length - 1)) return false;

        if (id == 0) choked = true;
        else if (id == 1) choked = false; // unchoke
        else if (id == 5) {} // bitfield
    } else {
        if (id == 0) choked = true;
        else if (id == 1) choked = false;
    }
    return true;
}

bool PeerConnection::receive_piece(std::vector<uint8_t>& piece_data, uint32_t length) {
    uint32_t len_net;
    if (!recv_all((char*)&len_net, 4)) return false;
    uint32_t msg_length = ntohl(len_net);
    if (msg_length == 0) return true;

    char id;
    if (!recv_all(&id, 1)) return false;

    if (id == 7) {
        uint32_t index_net, begin_net;
        if (!recv_all((char*)&index_net, 4)) return false;
        if (!recv_all((char*)&begin_net, 4)) return false;

        uint32_t block_length = msg_length - 9;
        std::vector<char> block_data(block_length);
        if (!recv_all(block_data.data(), block_length)) return false;

        uint32_t begin = ntohl(begin_net);
        if (begin + block_length <= piece_data.size()) {
            std::copy(block_data.begin(), block_data.end(), piece_data.begin() + begin);
        }
        return true;
    } else {
        if (msg_length > 1) {
            std::vector<char> payload(msg_length - 1);
            recv_all(payload.data(), msg_length - 1);
        }
        return true;
    }
}
