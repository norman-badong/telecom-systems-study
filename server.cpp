#include <iostream>
#include <cstring>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h> // Required for TCP-specific keep-alive socket options
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>

#define PORT 8080

// 1. Define a strict packed binary header structure
// #pragma pack(push, 1) ensures the compiler doesn't add hidden padding bytes.
#pragma pack(push, 1)
struct ProtocolHeader {
    uint8_t  magicByte;   // Validation marker (e.g., 0xAA)
    uint8_t  commandId;   // Action type (e.g., 0x01 = Ping, 0x02 = Read Status)
    uint16_t payloadLen;  // Length of the incoming payload following this header
};
#pragma pack(pop)

// ==========================================
// ROBUST HELPER: Read an exact number of bytes
// ==========================================
bool read_exact(int sock_fd, uint8_t* buffer, size_t length) {
    size_t total_read = 0;
    while (total_read < length) {
        ssize_t bytes_read = read(sock_fd, buffer + total_read, length - total_read);
        if (bytes_read <= 0) {
            // Client disconnected or read error occurred
            return false;
        }
        total_read += bytes_read;
    }
    return true;
}

void handle_client(int client_fd, std::string client_ip) {
    std::cout << "[Worker Thread] Active for client: " << client_ip << "\n";

    // Enable TCP Keep-Alive
    int optval = 1;
    setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));

    // 1. Read the header robustly (guaranteeing we get all sizeof(ProtocolHeader) bytes)
    ProtocolHeader header;
    if (!read_exact(client_fd, reinterpret_cast<uint8_t*>(&header), sizeof(ProtocolHeader))) {
        std::cerr << "[Error] Client disconnected or sent incomplete header.\n";
        close(client_fd);
        return;
    }

    // Validate Magic Byte
    if (header.magicByte != 0xAA) {
        std::cerr << "[Error] Invalid magic byte received: 0x" << std::hex << (int)header.magicByte << "\n";
        close(client_fd);
        return;
    }

    uint16_t payloadLength = ntohs(header.payloadLen);
    std::cout << "[Parsed Header Successfully]\n";
    std::cout << "  -> Magic Byte : 0x" << std::hex << (int)header.magicByte << "\n";
    std::cout << "  -> Command ID : 0x" << std::hex << (int)header.commandId << "\n";
    std::cout << "  -> Payload Len: " << std::dec << payloadLength << " bytes\n";

    // 2. Read the variable-length payload robustly
    if (payloadLength > 0) {
        std::vector<uint8_t> payload(payloadLength);
        
        // Use our robust loop to ensure we fetch every single byte requested by the header
        if (!read_exact(client_fd, payload.data(), payloadLength)) {
            std::cerr << "[Error] Client disconnected while sending payload.\n";
            close(client_fd);
            return;
        }

        std::cout << "  -> Payload Data Received: ";
        for (uint8_t b : payload) {
            std::cout << "0x" << std::hex << (int)b << " ";
        }
        std::cout << "\n";
    }

    const char* ack = "ACK_PACKET_RECEIVED\n";
    write(client_fd, ack, strlen(ack));

    close(client_fd);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) return 1;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) < 0) {
        close(server_fd);
        return 1;
    }

    std::cout << "[Server] Binary Protocol Server listening on port " << PORT << "...\n";

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_fd < 0) continue;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

        std::thread client_thread(handle_client, client_fd, std::string(client_ip));
        client_thread.detach();
    }

    close(server_fd);
    return 0;
}