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
    //creating a socket with AF_INET ipv4 internet protocol
    //SOCK_STREAM -> TCP
    //IPPROTO_TCP -> transport layer protocol
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return false;

    sockaddr_in clientService; //socket address card - it takes down the exact destination we want to call
    clientService.sin_family = AF_INET; //telling that the address we gonna write down on the address card will be standard IPV4 card
    clientService.sin_addr.s_addr = inet_addr(peer.ip.c_str());//filling the IP address of the destination ,since computer donot understand normal string so inet_addr converts ip string into 32 bit binary number which is needed for network communication.
    //normal computer processors read number right to left but internet expects the number from left to right so hton(Host To Network Short) flips the port number so it can be understood by the internet.
    clientService.sin_port = htons(peer.port);

    DWORD timeout = 3000;
    //we are configuring the settings on our telephone (socket) setsockopt -> set socket options
    //receive timeout -> if it waits for 3 sec for a message to arrive and even then no message is received then we 
    //just hang up the phone and dont wait anymore

    //we send the size along with the pointer because winsock32 is written in native C which has no idea how big the data is
    //therefor we have to explicitly tell that read only till this length
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    //send timeout -> if it tries to send a message and it takes more than 3 sec to send it then we 
    //just hang up the phone and dont wait anymore
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    //trying to call the peer using connect method
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
        //buf+total->tells the socket to leave the total(bytes sent till now) and start after that
        //len-total->tells the socket how many bytes are left to send
        //the function return the bytes which it was successfull to deliver
        int n = send(sock, buf + total, len - total, 0);
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
    buf.push_back(pstr_len);
    buf.insert(buf.end(), pstr, pstr + 19);
    buf.insert(buf.end(), reserved, reserved + 8);
    buf.insert(buf.end(), info_hash.begin(), info_hash.end());
    buf.insert(buf.end(), peer_id.begin(), peer_id.end());

    //buf.data() return the pointer to the first element in the vector
    if (!send_all(buf.data(), buf.size())) return false;

    std::vector<char> resp(68);
    if (!recv_all(resp.data(), 68)) return false;

    if (resp[0] != 19 || std::string(resp.data() + 1, 19) != pstr) {
        return false;
    }

    //if the info_hash does not match then it is not our torrent we are looking for
    std::string recv_hash(resp.data() + 28, 20);
    if (recv_hash != info_hash) {
        return false;
    }

    return true;//handshake is successful
}

//in bittorrent all the messages have a common structure -> 4 bytes length , 1 byte id, then the rest of the payload

bool PeerConnection::send_interested() {
    char msg[5] = {0, 0, 0, 1, 2};
    return send_all(msg, 5);
}


bool PeerConnection::request_piece(uint32_t index, uint32_t begin, uint32_t length) {
    std::vector<char> msg;
    uint32_t len_net = htonl(13); //length of the message
    msg.insert(msg.end(), (char*)&len_net, ((char*)&len_net) + 4);
    msg.push_back(6); //id of the message(6 - request)
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
    uint32_t length = ntohl(len_net);//converting it to integer that computer processor can understand

    if (length == 0) return true;//if length is 0 then it is keep alive msg, we do nothing

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
    uint32_t len_net;
    if (!recv_all((char*)&len_net, 4)) return false;
    uint32_t msg_length = ntohl(len_net);
    if (msg_length == 0) return true; //keep alive msg

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
        if (msg_length > 1) {
            std::vector<char> payload(msg_length - 1);
            recv_all(payload.data(), msg_length - 1);
        }
        return true;
    }
}
