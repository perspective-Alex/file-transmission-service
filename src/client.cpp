#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <dirent.h>

#include <data.hpp>

#include <deque>
#include <unordered_map>
#include <tuple>

extern Logger logger;

using DataSetCS = std::unordered_map<uint64_t, uint32_t>;
using PackageStorage = std::unordered_map<uint64_t, std::vector<Package>>;
using PackageStorageIndex = std::pair<uint64_t, uint32_t>;
using ReceiptConfirmation = std::unordered_map<uint64_t, std::vector<char>>;

void readFile(std::ifstream& in_f, std::deque<Package>& deq, uint64_t id) {
    in_f.seekg(0, std::ios::end);
    int fsize = in_f.tellg();
    logger.log("file size = %d bytes", fsize);
    int req_p_count = fsize / (Package::max_size - Package::header_size);
    if (fsize % (Package::max_size - Package::header_size)) {
        req_p_count++;
    }
    in_f.seekg(0, std::ios::beg);
    int i = 0;
    while (in_f.peek() != EOF) {
        deq.emplace_back(in_f, i, req_p_count, id);
        i++;
    }
}

std::deque<Package> readFiles(std::string dir_path, ReceiptConfirmation& rc, PackageStorage& ps) {
    int i = 0;
    DIR* dir = opendir(dir_path.c_str());
    dirent* entry;
    std::deque<Package> res;
    if (!dir) {
        logger.logErr("can't open specified file directory");
        perror(nullptr);
        return res;
    }
    std::ifstream in_f;
    int sz;
    while ((entry = readdir(dir))) {
        if (entry->d_type != DT_REG) {
            continue;
        }
        logger.log("file %s", entry->d_name);
        in_f.open(dir_path + "/" + entry->d_name, std::ios::in | std::ios::binary);
        sz = res.size();
        uint64_t id = i;
        readFile(in_f, res, id);
        in_f.close();
        rc[id].resize(res.size() - sz, 0);
        ps[id].resize(res.size() - sz);
        for (int j = 0; j<(int)res.size()-sz; j++) {
            ps[id][j] = res[sz+j];
        }
        i++;
    }
    in_f.close();
    logger.log("read %d files", i);
    return res;
}

void shufflePackageQueue(std::deque<Package>& deq) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(deq.begin(), deq.end(), gen);
}

DataSetCS computeCheckSum(std::deque<Package>& deq) {
    DataSetCS id2check_sum;
    for (Package& p : deq) {
        uint64_t id;
        memcpy(&id, p.id, sizeof(p.id));
        id2check_sum[id] = crc32c(id2check_sum[id], p.data.data(), p.data.size());
    }
    return id2check_sum;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        logger.logErr("Usage: ./client <path_to_directory_with_files>");
        return -1;
    }
    logger.set("CLIENT");
    const char* server_ip = "127.0.0.1";
    //const char* server_ip = "192.168.0.101";
    const short server_port = 1234;
    const int byte_rate = 1 * 128 * 1024;
    const int package_rate = byte_rate / Package::max_size;
    const timeval recvfrom_timeout = {1,0};
    const timeval rtt_time_thres = {5,0};
    auto timeval2usec = [](const timeval& t) -> long long { return t.tv_sec*1e6 + t.tv_usec; };
    auto getDuration = [&timeval2usec](const timeval& t1, const timeval& t2) -> long long {return timeval2usec(t2) - timeval2usec(t1); };
    const long long recvfrom_timeout_us = timeval2usec(recvfrom_timeout);

    int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd == -1) {
        logger.logErr("can't open socket");
        perror(nullptr);
        return -1;
    }
    struct sockaddr_in serv_sockaddr;
    serv_sockaddr.sin_family = AF_INET;
    if (!inet_aton(server_ip, &serv_sockaddr.sin_addr)) {
        logger.logErr("inet_aton call failed");
        return -1;
    }
    serv_sockaddr.sin_port = htons(server_port);
    socklen_t server_sockaddr_len = sizeof(serv_sockaddr);
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &recvfrom_timeout, sizeof(recvfrom_timeout));

    std::string data_dir_path(argv[1]);
    ReceiptConfirmation package_rc;
    PackageStorage package_storage;
    std::deque<Package> package_deq = readFiles(data_dir_path, package_rc, package_storage);
    DataSetCS cs = computeCheckSum(package_deq);
    logger.log("Package count = %lu", package_deq.size());
    shufflePackageQueue(package_deq);
    {
        std::ostringstream css;
        uint64_t id;
        uint32_t v;
        css << "{ ";
        for (auto pair : cs) {
            std::tie(id,v) = pair;
            css << id << "," << v << " ";
        }
        css << "}";
        logger.log("Checksums: %s", css.str().c_str());
    }
    logger.log("Bit rate set to %d Bps -> Package rate = %d pps", byte_rate, package_rate);

    size_t total_file_count = cs.size(), success_file_count = 0;
    timeval send_start_t{}, recv_start_t{}, cur_t{};

    long long dur;
    int i;
    std::deque<std::pair<PackageStorageIndex,timeval>> v_package_deq;
    std::vector<byte> package_buf(Package::max_size,'\0');
    while (success_file_count < total_file_count) {
        gettimeofday(&send_start_t, NULL);
        gettimeofday(&cur_t, NULL);
        dur = getDuration(send_start_t, cur_t);
        i = 0;
        while (i < package_rate && dur < 1*1e6 && !package_deq.empty()) {
            Package& p = package_deq.front();
            uint64_t id;
            memcpy(&id, p.id, sizeof(p.id));
            if (package_rc.at(id).at(p.seq_number) == 0) {
                // for "resend scenario": last check if there is still no confirmation from server
                std::vector<byte> p_vec = p.vectorize();
                if (sendto(sock_fd,p_vec.data(),p_vec.size(),0,
                           reinterpret_cast<const struct sockaddr*>(&serv_sockaddr),sizeof(serv_sockaddr)) == -1) {
                    logger.logErr("server is unavailable, terminating...");
                    perror(nullptr);
                    return -1;
                }
                logger.log("sent package %lu:%d", id, p.seq_number);
                i++;
                gettimeofday(&cur_t, NULL);
                v_package_deq.emplace_back(std::make_pair(id, p.seq_number),cur_t);
            } else {
                logger.log("got confirmation for package %lu:%d (after timeout), no need to resend");
            }
            package_deq.pop_front();
            gettimeofday(&cur_t, NULL);
            dur = getDuration(send_start_t, cur_t);
        }
        logger.log("sent %d packages recently", i);

        gettimeofday(&recv_start_t, NULL);
        gettimeofday(&cur_t, NULL);
        dur = getDuration(recv_start_t, cur_t);
        while (dur < recvfrom_timeout_us) {
            int msg_length = recvfrom(sock_fd, package_buf.data(), package_buf.size(), 0,
                                      reinterpret_cast<struct sockaddr*>(&serv_sockaddr), &server_sockaddr_len);
            if (msg_length == -1) {
                if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                    logger.logErr("recvfrom error occurred");
                    perror(nullptr);
                }
            } else {
                Package p(package_buf, msg_length);
                uint64_t id;
                memcpy(&id, p.id, sizeof(p.id));
                logger.log("received package %lu:%d", id, p.seq_number);
                package_rc.at(id).at(p.seq_number) = 1;
                if (p.seq_total == package_rc.at(id).size()) {
                    logger.log("got confirmation for file id=%lu", id);
                    uint32_t server_checksum;
                    memcpy(&server_checksum, p.data.data(), sizeof(server_checksum));
                    if (server_checksum == cs.at(id)) {
                        logger.log("checksums match, (server %u vs client %u)", server_checksum, cs.at(id));
                        success_file_count++;
                    } else {
                        logger.log("checksums don't match, (server %u vs client %u), resending full file", server_checksum, cs.at(id));
                        package_deq.insert(package_deq.end(), package_storage.at(id).begin(),
                                           package_storage.at(id).end());
                        std::fill(package_rc.at(id).begin(), package_rc.at(id).end(), 0);
                        shufflePackageQueue(package_deq);
                    }
                }
            }
            gettimeofday(&cur_t, NULL);
            dur = getDuration(recv_start_t, cur_t);
        }
        gettimeofday(&cur_t, NULL);
        timeval send_time;
        PackageStorageIndex package_storage_index;
        uint64_t id;
        uint32_t seq_number;
        size_t cur_pdeq_sz = package_deq.size();
        while (!v_package_deq.empty()) {
            auto& elem = v_package_deq.front();
            std::tie(package_storage_index, send_time) = elem;
            if (getDuration(send_time, cur_t) < timeval2usec(rtt_time_thres)) {
                break;
            }
            std::tie(id,seq_number) = package_storage_index;
            if (package_rc.at(id).at(seq_number) == 0) {
                logger.log("RTT timeout reached, adding package %lu:%d to send queue again", id, seq_number);
                package_deq.insert(package_deq.end(), package_storage.at(id).at(seq_number));
            }
            v_package_deq.pop_front();
        }
        if (package_deq.size() > cur_pdeq_sz) {
            // it means - insert occurred -> shuffle needed
            shufflePackageQueue(package_deq);
        }

        gettimeofday(&cur_t, NULL);
        long long time_passed = getDuration(send_start_t, cur_t);
        long long wait_time = 1e6 - time_passed;
        if (wait_time > 0) {
            usleep(wait_time);
        }
    }
    sendto(sock_fd, nullptr, 0, 0, reinterpret_cast<const struct sockaddr*>(&serv_sockaddr),sizeof(serv_sockaddr)); // i.e. model the `shutdown(sock_fd, SHUT_WR)`;
    close(sock_fd);
    return 0;
}