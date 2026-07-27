#include "peer.h"
#include <iostream>

#pragma comment(lib, "ws2_32.lib")



bool PeerConnection::connect_to_peer() {
    //creating a socket with AF_INET(address family internet) -> tells window that we are using IPv4 
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return false;

    sockaddr_in clientService; //socket address card - it takes down the exact destination we want to call
    clientService.sin_family = AF_INET; //indicates that we will use standard IPV4 
    clientService.sin_addr.s_addr = inet_addr(peer.ip.c_str());//filling the IP address of the destination,inet_addr converts ip string into 32 bit integer
    //normal computer processors read number right to left(little Endian) but internet expects the number from left to right so htons(Host To Network Short) flips the port number so it can be understood by the internet.
    clientService.sin_port = htons(peer.port);

    set_timeout(10000); //10 second timeout for send/receive operations

    //trying to call the peer using connect method
    //SOCKADDR -> dummy structure provided by the windows API - using it we can typecast any client service, making it dynamic
    if (connect(sock, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
        std::cerr << "  [Debug] connect() failed with error: " << WSAGetLastError() << "\n";
        //if the connection is failed it will return false and close socket
        closesocket(sock);
        sock = INVALID_SOCKET;
        return false;
    }
    return true;
}

//we give this function 2 attributes buf -> memory address of the first box, len -> total number of bytes we want to send
bool PeerConnection::send_all(const char* buf, int len) {
    int total = 0;
    while (total < len) {
        //built in windows function to send the message
        int n = send(sock, buf + total, len - total, 0); //0 is the advance network flag
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

//buf->empty box where data will be stored ,len->how much data to expect
bool PeerConnection::recv_all(char* buf, int len) {
    int total = 0;
    while (total < len) {
        //built in windows function to receive the message
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
    buf.push_back(pstr_len); //insert the number 19(1 byte)
    buf.insert(buf.end(), pstr, pstr + 19); //string "BitTorrent protocol"(19 bytes)
    buf.insert(buf.end(), reserved, reserved + 8); //reserved array(8 bytes)
    buf.insert(buf.end(), info_hash.begin(), info_hash.end()); //info_hash(20 bytes)
    buf.insert(buf.end(), peer_id.begin(), peer_id.end()); //peer_id(20 bytes)

    //buf.data() return the pointer to the first element in the vector
    if (!send_all(buf.data(), buf.size())) return false;

    std::vector<char> resp(68);
    if (!recv_all(resp.data(), 68)) return false;

    //general check
    if (resp[0] != 19 || std::string(resp.data() + 1, 19) != pstr) {
        return false;
    }

    //if the info_hash does not match then return false
    std::string recv_hash(resp.data() + 28, 20);
    if (recv_hash != info_hash) {
        return false;
    }

    return true;//handshake is successful
}

bool PeerConnection::send_interested() {
    //the payload length is 1 and the id is 2 for interested message
    char msg[5] = {0, 0, 0, 1, 2}; //the id is 2 which tells the peer we are interested in pieces
    return send_all(msg, 5);
}


bool PeerConnection::request_piece(uint32_t index, uint32_t begin, uint32_t length) {
    std::vector<char> msg; //act as bucket
    uint32_t len_net = htonl(13); //length of the message(1 byte->id,4 byte->index of the piece you want,4 byte->begin offset,4 bytes -> length (how many bytes you want))
    msg.insert(msg.end(), (char*)&len_net, ((char*)&len_net) + 4);
    msg.push_back(6); //id of the message(6 - request) -> implicit conversion since it if of 1 byte
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
    uint32_t length = ntohl(len_net);//big endian -> little endian

    if (length == 0) return true;//keep alive msg

    char id;
    if (!recv_all(&id, 1)) return false;

    if (length > 1) {
        std::vector<char> payload(length - 1);
        if (!recv_all(payload.data(), length - 1)) return false;

        if (id == 0) choked = true; //choke
        else if (id == 1) choked = false; // unchoke
        else if (id == 5) {} // bitfield
    } else {
        if (id == 0) choked = true;
        else if (id == 1) choked = false;
    }
    return true;
}

bool PeerConnection::receive_piece(std::vector<uint8_t>& piece_data, uint32_t length) {
    //loop until we actually receive a piece(ID 7) message
    //because the peer might send keep-alive, have, or choke messages in between
    while (true) {
        uint32_t len_net;
        if (!recv_all((char*)&len_net, 4)) return false;
        uint32_t msg_length = ntohl(len_net);
        if (msg_length == 0) continue; //keep alive msg -> loop back and wait for real data

        char id;
        if (!recv_all(&id, 1)) return false;

        //if the id is 7 then it is a piece message
        if (id == 7) {
            uint32_t index_net, begin_net;
            if (!recv_all((char*)&index_net, 4)) return false;
            if (!recv_all((char*)&begin_net, 4)) return false;

            uint32_t block_length = msg_length - 9;
            std::vector<char> block_data(block_length);
            if (!recv_all(block_data.data(), block_length)) return false;

            uint32_t begin = ntohl(begin_net);
            if (begin + block_length <= piece_data.size()) {
                //overwrites the data instead of creating (insert)
                //because char and uint8_t are 1 byte numbers we can directly copy them because it performs implicit conversion
                std::copy(block_data.begin(), block_data.end(), piece_data.begin() + begin);
            }
            return true;
        } else {
            //non-piece message -> handle state changes, then loop back
            if (id == 0) choked = true;
            else if (id == 1) choked = false;

            if (msg_length > 1) {
                std::vector<char> payload(msg_length - 1);
                recv_all(payload.data(), msg_length - 1);
            }

            //if peer choked us mid-transfer, abort
            if (choked) return false;
            //otherwise loop back and wait for the actual piece message
        }
    }
}

void PeerConnection::set_timeout(int ms) {
    DWORD timeout = ms;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
}
