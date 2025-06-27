// App.MyIP.cpp

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h> // For clipboard functions

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

void CopyToClipboard(const char* text)
{
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();

    size_t len = strlen(text) + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (hMem) {
        memcpy(GlobalLock(hMem), text, len);
        GlobalUnlock(hMem);
        SetClipboardData(CF_TEXT, hMem);
    }
    CloseClipboard();
}

int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        std::cerr << "gethostname() failed.\n";
        WSACleanup();
        return 1;
    }

    addrinfo hints = {};
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    if (getaddrinfo(hostname, nullptr, &hints, &result) != 0) {
        std::cerr << "getaddrinfo() failed.\n";
        WSACleanup();
        return 1;
    }

    bool found = false;
    for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        sockaddr_in* sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
        // Skip loopback addresses
        if (sockaddr_ipv4->sin_addr.S_un.S_addr != htonl(INADDR_LOOPBACK)) {
            char ipstr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(sockaddr_ipv4->sin_addr), ipstr, sizeof(ipstr));
            std::cout << ipstr << std::endl;
            CopyToClipboard(ipstr); // Copy to clipboard
            found = true;
            break;
        }
    }

    freeaddrinfo(result);
    WSACleanup();

    if (!found) {
        std::cerr << "No non-loopback IPv4 address found.\n";
        return 1;
    }

    return 0;
}
