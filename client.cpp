#include <iostream>     // For standard input/output
#include <cstring>      // For memset
#include <vector>       // For byte vectors
#include <sys/types.h>  // System data types
#include <sys/socket.h> // Socket APIs
#include <netinet/in.h> // Internet address structures
#include <arpa/inet.h>  // inet_pton
#include <unistd.h>     // close()

#define PORT 8080

// 1. Same packed header structure used by the server
#pragma pack(push, 1)
struct ProtocolHeader {
    uint8_t  magicByte;   
    uint8_t  commandId;   
    uint16_t payloadLen;  
};
#pragma pack(pop)

int main() {
    // 1. Create client socket file descriptor
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation error\n";
        return 1;
    }

    // 2. Define server address (localhost:8080)
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address / Address not supported\n";
        return 1;
    }

    // 3. Connect to the server
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed. Is the server running?\n";
        return 1;
    }
    std::cout << "[Client] Connected to server successfully.\n";

    // 4. Prepare our custom binary payload data (e.g., 3 bytes of data: 0x01, 0x02, 0x03)
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03};

    // 5. Construct the binary packet header
    ProtocolHeader header;
    header.magicByte  = 0xAA;                       // Our protocol validation byte
    header.commandId  = 0x01;                       // Command 0x01 (e.g., Ping / Status Check)
    header.payloadLen = htons(payload.size());      // Convert host order to Network Byte Order (Big-Endian)

    // 6. Send the header first
    write(sock, &header, sizeof(ProtocolHeader));

    // 7. Send the variable-length payload data second
    write(sock, payload.data(), payload.size());
    std::cout << "[Client] Sent structured binary packet (Header + " << payload.size() << " payload bytes).\n";

    // 8. Read acknowledgement response from server
    char buffer[128] = {0};
    ssize_t bytes_read = read(sock, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        std::cout << "[Client Response Received]: " << buffer << "\n";
    }

    // 9. Close the socket
    close(sock);
    return 0;
}