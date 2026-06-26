#include "torrent_file.h"
#include "tracker.h"
#include "peer.h"
#include "sha1.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    try {
        TorrentFile tf;
        tf.load("debian.torrent");

        std::cout << "Loaded Torrent:\n";
        std::cout << "  Name: " << tf.name << "\n";
        std::cout << "  Announce: " << tf.announce << "\n";
        std::cout << "  Length: " << tf.length << "\n";
        std::cout << "  Piece Length: " << tf.piece_length << "\n";
        std::cout << "  Info Hash (Hex): ";
        for (unsigned char c : tf.info_hash) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)c;
        }
        std::cout << std::dec << "\n\n";

        Tracker tracker;
        std::cout << "Requesting peers from tracker...\n";
        auto peers = tracker.request_peers(tf);

        if (peers.empty()) {
            std::cerr << "No peers found.\n";
            WSACleanup();
            return 1;
        }

        std::cout << "Found " << peers.size() << " peers.\n";

        bool piece_downloaded = false;

        for (const auto& p : peers) {
            std::cout << "Trying to connect to peer " << p.ip << ":" << p.port << "...\n";
            PeerConnection pc(p, tf.info_hash, tracker.peer_id);

            if (!pc.connect_to_peer()) {
                std::cout << "  Connection failed.\n";
                continue;
            }

            std::cout << "  Connected. Sending Handshake...\n";
            if (!pc.handshake()) {
                std::cout << "  Handshake failed.\n";
                continue;
            }

            std::cout << "  Handshake successful.\n";
            
            // Wait for bitfield/unchoke
            for (int i = 0; i < 5; i++) {
                pc.receive_message();
            }

            std::cout << "  Sending Interested...\n";
            pc.send_interested();

            // Wait to be unchoked
            int wait_count = 0;
            while (pc.choked && wait_count < 10) {
                pc.receive_message();
                wait_count++;
            }

            if (pc.choked) {
                std::cout << "  Peer did not unchoke us.\n";
                continue;
            }

            std::cout << "  Unchoked! Requesting Piece 0...\n";

            uint32_t piece_idx = 0;
            uint32_t piece_len = std::min((long long)tf.piece_length, tf.length);
            std::vector<uint8_t> piece_data(piece_len);
            
            uint32_t block_size = 16384;
            uint32_t downloaded = 0;

            bool download_failed = false;
            while (downloaded < piece_len) {
                uint32_t req_len = std::min(block_size, piece_len - downloaded);
                if (!pc.request_piece(piece_idx, downloaded, req_len)) {
                    std::cout << "  Failed to request piece block.\n";
                    download_failed = true;
                    break;
                }
                
                if (!pc.receive_piece(piece_data, req_len)) {
                    std::cout << "  Failed to receive piece block.\n";
                    download_failed = true;
                    break;
                }
                downloaded += req_len;
                std::cout << "  Downloaded " << downloaded << " / " << piece_len << " bytes.\n";
            }

            if (!download_failed && downloaded == piece_len) {
                std::cout << "  Piece 0 downloaded successfully!\n";
                
                // Verify SHA1
                std::string hash = compute_sha1(piece_data);
                std::string expected_hash = tf.pieces.substr(0, 20);
                
                if (hash == expected_hash) {
                    std::cout << "  Hash verified successfully!\n";
                    
                    std::ofstream out("piece_0.dat", std::ios::binary);
                    out.write((char*)piece_data.data(), piece_data.size());
                    std::cout << "  Saved to piece_0.dat\n";
                    piece_downloaded = true;
                    break;
                } else {
                    std::cout << "  Hash verification failed.\n";
                }
            }
        }

        if (!piece_downloaded) {
            std::cout << "\nFailed to download piece 0 from any peer.\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    WSACleanup();
    return 0;
}
