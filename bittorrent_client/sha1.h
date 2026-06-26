#pragma once

#include <string>
#include <vector>
#include <cstdint>

std::string compute_sha1(const std::string& data);
std::string compute_sha1(const std::vector<uint8_t>& data);
