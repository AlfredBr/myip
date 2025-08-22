// App.MyIP.cpp

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#include <string>
#include <vector>

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

std::string WideToNarrow(const wchar_t* wideStr)
{
    if (!wideStr) return "";
    
    int size = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    
    std::vector<char> buffer(size);
    WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, buffer.data(), size, nullptr, nullptr);
    
    return std::string(buffer.data());
}

struct AdapterInfo {
    std::string ipAddress;
    std::string dnsSuffix;
    std::string adapterName;
};

std::vector<AdapterInfo> GetAdapterAddresses()
{
    std::vector<AdapterInfo> adapters;
    
    ULONG bufferSize = 0;
    DWORD result = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &bufferSize);
    
    if (result != ERROR_BUFFER_OVERFLOW) {
        return adapters;
    }
    
    PIP_ADAPTER_ADDRESSES adapterAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(bufferSize));
    if (!adapterAddresses) {
        return adapters;
    }
    
    result = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapterAddresses, &bufferSize);
    
    if (result == NO_ERROR) {
        PIP_ADAPTER_ADDRESSES currentAdapter = adapterAddresses;
        
        while (currentAdapter) {
            // Skip loopback and non-operational adapters
            if (currentAdapter->IfType != IF_TYPE_SOFTWARE_LOOPBACK && 
                currentAdapter->OperStatus == IfOperStatusUp) {
                
                PIP_ADAPTER_UNICAST_ADDRESS unicastAddress = currentAdapter->FirstUnicastAddress;
                
                while (unicastAddress) {
                    if (unicastAddress->Address.lpSockaddr->sa_family == AF_INET) {
                        sockaddr_in* sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(unicastAddress->Address.lpSockaddr);
                        
                        // Skip loopback addresses
                        if (sockaddr_ipv4->sin_addr.S_un.S_addr != htonl(INADDR_LOOPBACK)) {
                            char ipstr[INET_ADDRSTRLEN];
                            inet_ntop(AF_INET, &(sockaddr_ipv4->sin_addr), ipstr, sizeof(ipstr));
                            
                            AdapterInfo info;
                            info.ipAddress = ipstr;
                            info.dnsSuffix = WideToNarrow(currentAdapter->DnsSuffix);
                            info.adapterName = WideToNarrow(currentAdapter->FriendlyName);
                            
                            adapters.push_back(info);
                        }
                    }
                    unicastAddress = unicastAddress->Next;
                }
            }
            currentAdapter = currentAdapter->Next;
        }
    }
    
    free(adapterAddresses);
    return adapters;
}

bool IsAllFlag(const char* arg)
{
    return (strcmp(arg, "/all") == 0 || strcmp(arg, "-all") == 0 || strcmp(arg, "--all") == 0);
}

int main(int argc, char* argv[])
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    bool showAll = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (IsAllFlag(argv[i])) {
            showAll = true;
            break;
        }
    }

    std::vector<AdapterInfo> adapters = GetAdapterAddresses();
    
    if (adapters.empty()) {
        std::cerr << "No network adapters found.\n";
        WSACleanup();
        return 1;
    }

    if (showAll) {
        // Show all non-loopback IP addresses
        for (const auto& adapter : adapters) {
            std::cout << adapter.ipAddress;
            if (!adapter.dnsSuffix.empty()) {
                std::cout << " (DNS: " << adapter.dnsSuffix << ")";
            }
            if (!adapter.adapterName.empty()) {
                std::cout << " [" << adapter.adapterName << "]";
            }
            std::cout << std::endl;
        }
    } else {
        // Find first adapter with DNS suffix, or fallback to first adapter
        std::string selectedIP;
        bool found = false;
        
        // First pass: look for adapter with DNS suffix
        for (const auto& adapter : adapters) {
            if (!adapter.dnsSuffix.empty()) {
                selectedIP = adapter.ipAddress;
                found = true;
                break;
            }
        }
        
        // Fallback: use first adapter if no DNS suffix found
        if (!found && !adapters.empty()) {
            selectedIP = adapters[0].ipAddress;
            found = true;
        }
        
        if (found) {
            std::cout << selectedIP << std::endl;
            CopyToClipboard(selectedIP.c_str());
        } else {
            std::cerr << "No suitable IP address found.\n";
            WSACleanup();
            return 1;
        }
    }

    WSACleanup();
    return 0;
}
