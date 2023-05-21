#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <numeric>
#include <algorithm>
#include <random>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <cassert>

class Logger {
public:
    Logger() {}
    void set(std::string caller_) {
        caller = caller_;
        header = caller_;
    };
    template<class... Args>
    void log(const char* fmessage, Args... args) {
        std::string out(fmessage);
        out = "<" + header + "> -- " + out + "\n";
        printf(out.c_str(), args...);
    }
    template<class... Args>
    void logErr(const char* fmessage, Args... args) {
        std::string out(fmessage);
        out = "<" + header + "> -- " + out + "\n";
        fprintf(stderr, out.c_str(), args...);
    }
private:
    std::string caller;
    std::string header;
};

//enum class byte : uint8_t {};

typedef unsigned char byte;

struct Package {
    enum class Type : uint8_t { ACK=0, PUT=1 };
    static const size_t max_size = 1472; // 1472
    uint32_t seq_number;
    uint32_t seq_total;
    uint8_t type;
    byte id[8];
    static const size_t header_size = sizeof(seq_number) + sizeof(seq_total) + sizeof(type) + sizeof(id);

    std::vector<byte> data;

    Package() = default;
    Package(std::ifstream& in_f, uint32_t seq_number_, uint32_t seq_total_, uint64_t id_);
    Package(uint32_t seq_number_, uint32_t seq_total_, uint64_t id_, uint32_t* checksum);
    Package(const std::vector<byte>& src, int len);
    Package(const Package&) = default;
    Package(Package&&) = default;
    Package& operator=(const Package&) = default;
    bool empty() const;
    std::vector<byte> vectorize() const;
};

uint32_t crc32c(uint32_t crc, const byte* buf, size_t len);