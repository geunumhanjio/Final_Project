#include "media/CctvScanner.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ctime>
#include <vector>

#pragma pack(push, 1)
typedef struct {
    unsigned char  nMode;
    unsigned char  chPacketID[18];
    char           chMAC[18];
    char           chIP[16];
    char           chSubnetMask[16];
    char           chGateway[16];
    char           chPassword[20];
    char           is_only_support_sunapi;
    unsigned short nPort;
    unsigned char  nStatus;
    char           chDeviceName[10];
    char           reserved2;
    unsigned short nHttpPort;
    unsigned short nDevicePort;
    unsigned short nTcpPort;
    unsigned short nUdpPort;
    unsigned short nUploadPort;
    unsigned short nMulticastPort;
    unsigned char  nNetworkMode;
    char           chDDNS[128];
    char           chAlias[32];
    char           chNewModelName[32];
    unsigned char  nModelType;
    unsigned short nVersion;
    unsigned char  nHttpMode;
    unsigned char  reserved3;
    unsigned short nHttpsPort;
    unsigned char  nSupportedProtocol;
    unsigned char  nPasswordStatus;
} DATAPACKET_EXT_V4;
#pragma pack(pop)

enum {
    DEF_REQ_SCAN_EXT = 6,
    DEF_RES_SCAN_EXT = 12,
};

std::string CctvScanner::discoverIp(const std::string& targetMac, int timeoutSeconds) {
    std::cout << "📡 [Scanner] Searching for CCTV with MAC: " << targetMac << "..." << std::endl;

    // 1. Setup Receiving Socket
    int recv_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (recv_sock < 0) return "";

    int opt_reuse = 1;
    setsockopt(recv_sock, SOL_SOCKET, SO_REUSEADDR, &opt_reuse, sizeof(opt_reuse));

    struct timeval tv = { timeoutSeconds, 0 };
    setsockopt(recv_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in recv_addr = {};
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    recv_addr.sin_port = htons(7711);

    if (bind(recv_sock, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0) {
        close(recv_sock);
        return "";
    }

    // 2. Setup Sending Socket (Broadcast)
    int send_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sock < 0) { close(recv_sock); return ""; }

    int bcast = 1;
    setsockopt(send_sock, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast));

    struct sockaddr_in send_addr = {};
    send_addr.sin_family = AF_INET;
    send_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    send_addr.sin_port = htons(7701);

    // 3. Prepare Request Packet
    DATAPACKET_EXT_V4 req = {};
    req.nMode = DEF_REQ_SCAN_EXT;
    const char* my_id = "IMROKGEUN12345678";
    memcpy(req.chPacketID, my_id, 18);

    sendto(send_sock, &req, sizeof(req), 0, (struct sockaddr*)&send_addr, sizeof(send_addr));

    // 4. Wait for response
    char buffer[2048];
    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    time_t start = time(NULL);

    std::string discoveredIp = "";
    while (time(NULL) - start < timeoutSeconds) {
        int n = recvfrom(recv_sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender_addr, &sender_len);
        if (n < (int)sizeof(DATAPACKET_EXT_V4)) continue;

        DATAPACKET_EXT_V4* res = (DATAPACKET_EXT_V4*)buffer;
        if (memcmp(res->chPacketID, my_id, 18) == 0 && res->nMode == DEF_RES_SCAN_EXT) {
            std::cout << "📥 [Scanner] Discovered: MAC=" << res->chMAC << " IP=" << res->chIP << std::endl;
            if (targetMac == res->chMAC) {
                discoveredIp = res->chIP;
                std::cout << "✅ [Scanner] Target Found! IP: " << discoveredIp << std::endl;
                break;
            }
        }
    }

    close(send_sock);
    close(recv_sock);
    return discoveredIp;
}
