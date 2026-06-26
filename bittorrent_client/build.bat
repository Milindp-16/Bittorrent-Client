@echo off
echo Building BitTorrent Client...
g++ -std=c++14 -o bittorrent bencode.cpp sha1.cpp torrent_file.cpp tracker.cpp peer.cpp main.cpp -lws2_32 -ladvapi32 -lwininet
if %errorlevel% neq 0 (
    echo Build failed.
) else (
    echo Build succeeded.
)
