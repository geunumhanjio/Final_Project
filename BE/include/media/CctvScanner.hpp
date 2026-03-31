#ifndef CCTV_SCANNER_HPP
#define CCTV_SCANNER_HPP

#include <string>
#include <vector>

class CctvScanner {
public:
    static std::string discoverIp(const std::string& targetMac, int timeoutSeconds = 5);
};

#endif // CCTV_SCANNER_HPP
