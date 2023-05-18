#include <data.hpp>

Package::Package(std::ifstream& in_f, uint32_t seq_number_, uint32_t seq_total_, uint64_t id_) {
    type = static_cast<uint8_t>(Package::Type::PUT);
    seq_number = seq_number_;
    seq_total = seq_total_;
    in_f.read(reinterpret_cast<char*>(data), Package::data_size);
    for (uint32_t i=in_f.gcount(); i<Package::data_size; i++) {
        data[i] = 0;
    }
    memcpy(id, &id_, 8);
}

Package::Package(uint32_t seq_number_, uint32_t seq_total_, uint64_t id_, uint32_t* checksum) {
    type = static_cast<uint8_t>(Package::Type::ACK);
    seq_number = seq_number_;
    seq_total = seq_total_;
    if (checksum) {
        memcpy(data, checksum, 4);
    }
    memcpy(id, &id_, 8);
}

bool Package::empty() {
    return data[0] == 0;
}

uint32_t crc32c(uint32_t crc, const byte* buf, size_t len)
{
    int k;
    crc = ~crc;
    while (len--) {
        crc ^= *buf++;
        for (k = 0; k < 8; k++) {
            crc = crc & 1 ? (crc >> 1) ^ 0x82f63b78 : crc >> 1;
        }
    }
    return ~crc;
}

Logger logger;