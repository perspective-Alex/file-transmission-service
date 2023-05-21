#include <data.hpp>

Package::Package(std::ifstream& in_f, uint32_t seq_number_, uint32_t seq_total_, uint64_t id_) {
    type = static_cast<uint8_t>(Package::Type::PUT);
    seq_number = seq_number_;
    seq_total = seq_total_;
    size_t max_data_size = Package::max_size - Package::header_size;
    byte buf[max_data_size];
    in_f.read(reinterpret_cast<char*>(buf), max_data_size);
    data.resize(in_f.gcount(), '\0');
    memcpy(data.data(), buf, data.size());
    memcpy(id, &id_, sizeof(id_));
}

Package::Package(uint32_t seq_number_, uint32_t seq_total_, uint64_t id_, uint32_t* checksum) {
    type = static_cast<uint8_t>(Package::Type::ACK);
    seq_number = seq_number_;
    seq_total = seq_total_;
    if (checksum) {
        data.resize(sizeof(*checksum), '\0');
        memcpy(data.data(), checksum, sizeof(*checksum));
    }
    memcpy(id, &id_, sizeof(id_));
}

Package::Package(const std::vector<byte>& src, int len) {
    int offset = 0;
    memcpy(&seq_number, &src[offset], sizeof(seq_number));
    offset += sizeof(seq_number);
    memcpy(&seq_total, &src[offset], sizeof(seq_total));
    offset += sizeof(seq_total);
    memcpy(&type, &src[offset], sizeof(type));
    offset += sizeof(type);
    memcpy(&id, &src[offset], sizeof(id));
    offset += sizeof(id);
    if (len > offset) {
        data.resize(len-offset, '\0');
        for (size_t i=0; i<data.size(); i++) {
            data[i] = src[i+offset];
        }
    }
}

bool Package::empty() const {
    return data.empty();
}

std::vector<byte> Package::vectorize() const {
    size_t package_size = header_size + data.size();
    std::vector<byte> package_buf(package_size,'\0');
    size_t offset = 0;
    memcpy(&package_buf[offset], &seq_number, sizeof(seq_number));
    offset += sizeof(seq_number);
    memcpy(&package_buf[offset], &seq_total, sizeof(seq_total));
    offset += sizeof(seq_total);
    memcpy(&package_buf[offset], &type, sizeof(type));
    offset += sizeof(type);
    memcpy(&package_buf[offset], id, sizeof(id));
    offset += sizeof(id);
    std::copy(data.begin(),data.end(),package_buf.begin()+offset);
    return package_buf;
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