# 🌐 BitTorrent Client

A lightweight BitTorrent client built from scratch in **C++** that implements the core peer-to-peer file sharing protocol — from parsing `.torrent` files to downloading verified pieces from real-world swarms.

---

## 📡 How BitTorrent Works

BitTorrent is a **peer-to-peer (P2P)** protocol where files aren't downloaded from a single server. Instead, many users (called **peers**) share small chunks of the file with each other simultaneously.

```
┌─────────────┐       1. Announce        ┌─────────────┐
│             │ ───────────────────────►  │   Tracker   │
│   You       │ ◄───────────────────────  │   Server    │
│  (Client)   │       2. Peer List        └─────────────┘
│             │
│             │       3. Handshake        ┌─────────────┐
│             │ ◄────────────────────────►│   Peer A    │
│             │       4. Piece Data       └─────────────┘
│             │
│             │       3. Handshake        ┌─────────────┐
│             │ ◄────────────────────────►│   Peer B    │
│             │       4. Piece Data       └─────────────┘
└─────────────┘
```

### The Communication Lifecycle

| Step | Action | Description |
|------|--------|-------------|
| **1** | **Parse `.torrent` file** | Extract metadata — tracker URL, file name, piece hashes, and the unique `info_hash` |
| **2** | **Contact the Tracker** | Send an HTTP GET request with `info_hash`, `peer_id`, and stats. The tracker responds with a list of peers in the swarm |
| **3** | **TCP Handshake with Peers** | Open a TCP socket to a peer and exchange a 68-byte handshake containing the protocol identifier and `info_hash` |
| **4** | **Message Exchange** | Send `Interested` → Wait to be `Unchoked` → Request 16KB blocks of a piece |
| **5** | **Download & Verify** | Reassemble blocks into a complete piece and verify its integrity using SHA-1 hash comparison |

### Key Concepts

- **Torrent File** — A `.torrent` file is a bencoded dictionary containing the tracker URL, file metadata, and SHA-1 hashes for every piece
- **Info Hash** — A 20-byte SHA-1 hash of the raw info dictionary — the unique fingerprint that identifies a torrent across the entire network
- **Pieces** — The file is split into fixed-size pieces (typically 256KB). Each piece has a SHA-1 hash for integrity verification
- **Bencode** — A simple encoding format used by BitTorrent with 4 data types: integers (`i42e`), strings (`4:spam`), lists (`l...e`), and dictionaries (`d...e`)
- **Peers** — Other clients in the swarm. A peer that has the complete file is called a **Seeder**; one still downloading is a **Leecher**

---

## 🏗️ Project Architecture

```
bittorrent_client/
├── main.cpp            # Entry point — orchestrates the full download pipeline
├── bencode.cpp/.h      # Bencode encoder/decoder (parses .torrent file format)
├── torrent_file.cpp/.h # Parses .torrent files and extracts metadata + info_hash
├── tracker.cpp/.h      # Communicates with the tracker server via HTTP (WinINet)
├── peer.cpp/.h         # TCP peer connections — handshake, messaging, piece transfer
├── sha1.cpp/.h         # SHA-1 hashing for info_hash and piece verification
├── build.bat           # Build script (g++ with MinGW)
└── debian.torrent      # Sample torrent file for testing
```

---

## 🛠️ Tech Stack

| Component | Technology |
|-----------|------------|
| **Language** | C++14 |
| **Compiler** | MinGW g++ |
| **Networking (HTTP)** | Windows Internet API (`WinINet`) — for tracker communication |
| **Networking (TCP)** | Winsock2 — for peer-to-peer socket connections |
| **Hashing** | Windows Cryptography API (`advapi32`) — for SHA-1 computation |
| **Encoding** | Custom Bencode parser/encoder |

---

## 🚀 Build & Run

### Build

```bash
cd bittorrent_client
build.bat
```

### Run

```bash
btclient.exe
```

The client will:
1. Load `debian.torrent`
2. Contact the tracker and discover peers
3. Attempt to download **Piece 0** from the swarm
4. Verify the piece with SHA-1
5. Save the verified piece to `piece_0.dat`

---

## 📁 Supported Torrent Files

This client supports **single-file** and **multi-file** `.torrent` files that use **HTTP/HTTPS trackers**. It has been tested with:

- ✅ **Debian** ISO torrents (`debian-xx.x.x-amd64-netinst.iso.torrent`)
- ✅ **Ubuntu** ISO torrents

> **Note:** UDP trackers and magnet links are not currently supported.

---

## ⚙️ Current Limitations

- Downloads **Piece 0 only** (proof-of-concept — full file assembly not yet implemented)
- Single-threaded peer connections (tries peers sequentially)
- Windows-only (uses WinINet and Winsock2)
- No upload/seeding capability

---

## 📜 License

This project is for educational purposes — built to deeply understand the BitTorrent protocol from the ground up.
