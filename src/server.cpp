#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <data.hpp>
#include <test.hpp>

#include <iostream>
#include <unordered_map>
#include <unordered_set>

extern Logger logger;

using PackageStorage = std::unordered_map<uint64_t, std::vector<Package>>;
using PackageCount = std::unordered_map<uint64_t, uint32_t>;
using ReceiptConfirmation = std::unordered_set<uint64_t>;

uint32_t computeCheckSum(const std::vector<Package>& file_packages) {
    uint32_t cs = 0;
    for (const Package& fp : file_packages) {
        cs = crc32c(cs, fp.data.data(), fp.data.size());
    }
    return cs;
}

void dumpFile(const std::vector<Package>& file_packages, uint32_t check_sum) {
    std::string dir_path("data");
    if (mkdir(dir_path.c_str(), S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
        if (errno != EEXIST) {
            logger.logErr("can't create dump directory");
            perror(nullptr);
            return;
        }
    }
    std::ofstream out_f(dir_path + "/" + std::to_string(check_sum) + ".gif", std::ios::out | std::ios::binary);
    for (const Package& fp : file_packages) {
        out_f.write(reinterpret_cast<const char*>(fp.data.data()), fp.data.size());
    }
    out_f.close();
}

int main() {
    logger.set("SERVER");
    const short port = 1234;
    //const char* ip = "127.0.0.1";
    const timeval wait_timeout = {15,0};

    int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd == -1) {
        logger.logErr("can't open socket");
        perror(nullptr);
        return -1;
    }
    struct sockaddr_in serv_sockaddr, client_sockaddr;

    serv_sockaddr.sin_family = AF_INET;
    serv_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    //if (!inet_aton(ip, &serv_sockaddr.sin_addr)) {
    //    logger.logErr("inet_aton call failed");
    //    return -1;
    //}

    serv_sockaddr.sin_port = htons(port);
    if (bind(sock_fd,reinterpret_cast<const struct sockaddr*>(&serv_sockaddr),sizeof(serv_sockaddr)) == -1) {
        logger.logErr("can't bind socket fd to specified address");
        perror(nullptr);
        return -1;
    }
    socklen_t client_sockaddr_len = sizeof(client_sockaddr);
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &wait_timeout, sizeof(wait_timeout));
    PackageStorage package_storage;
    PackageCount package_count;
    ReceiptConfirmation package_rc;

    std::vector<byte> package_buf(Package::max_size, '\0');
    logger.log("Waiting for packages...");
    while (true) {
        int msg_length = recvfrom(sock_fd,package_buf.data(),package_buf.size(),0,
                                  reinterpret_cast<struct sockaddr*>(&client_sockaddr),&client_sockaddr_len);
        if (msg_length == 0 && package_buf.size() != 0) {
            logger.log("client ended the communication, finishing...");
            break;
        } else if (msg_length == -1) {
            if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                logger.logErr("recvfrom error occurred");
                perror(nullptr);
            } else {
                logger.logErr("timeout reached, finishing communication...");
                break;
            }
        }
        Package p(package_buf,msg_length);
        uint64_t id;
        memcpy(&id, p.id, sizeof(p.id));
        logger.log("Got a package %lu:%d from %s", id, p.seq_number, inet_ntoa(client_sockaddr.sin_addr));
        if (package_storage[id].empty()) {
            package_storage.at(id).resize(p.seq_total);
            package_count[id] = 0;
        }
        auto prc_it = package_rc.find(id);
        if (prc_it != package_rc.end()) {
            logger.log("Restarting receipt of file %lu", id);
            std::fill(package_storage.at(id).begin(), package_storage.at(id).end(), Package());
            package_storage.at(id).at(p.seq_number) = p;
            package_count.at(id) = 1;
            package_rc.erase(prc_it);
        } else {
            if (package_storage.at(id).at(p.seq_number).empty()) {
                //spoilOnePackage(p);
                package_storage.at(id).at(p.seq_number) = p;
                package_count.at(id)++;
            }
        }

        Package reply_package;
        if (package_count.at(id) == p.seq_total) {
            uint32_t cs = computeCheckSum(package_storage.at(id));
            reply_package = Package(p.seq_number, package_count.at(id), id, &cs);
            package_rc.insert(id);
            logger.log("sending last ACK package with checksum=%u for file %lu", cs, id);
            dumpFile(package_storage.at(id), cs);
        } else {
            reply_package = Package(p.seq_number, package_count.at(id), id, nullptr);
        }
        std::vector<byte> send_package_buf = reply_package.vectorize();
        if (sendto(sock_fd,send_package_buf.data(),send_package_buf.size(),0,
                   reinterpret_cast<const struct sockaddr*>(&client_sockaddr),client_sockaddr_len) == -1) {
            logger.logErr("can't reply to client %s", inet_ntoa(client_sockaddr.sin_addr));
            perror(nullptr);
        }
    }
    close(sock_fd);
    return 0;
}