#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    // 1. Create socket file descriptor
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        std::cerr << "Socket creation failed!\n";
        return 1;
    }

    // Optional: Allow reuse of port immediately after stopping the server
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Bind socket to port
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed!\n";
        close(server_fd);
        return 1;
    }

    // 3. Listen for connections
    if (listen(server_fd, 3) < 0) {
        std::cerr << "Listen failed!\n";
        close(server_fd);
        return 1;
    }
    std::cout << "[Server] Listening on port " << PORT << "...\n";

    // 4. Accept an incoming connection (Blocking call)
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
    
    if (client_fd < 0) {
        std::cerr << "Accept failed!\n";
        close(server_fd);
        return 1;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    std::cout << "[Server] Connected to client IP: " << client_ip << " (FD: " << client_fd << ")\n";

    // 5. Read incoming raw bytes from the client
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
    
    if (bytes_read > 0) {
        std::cout << "[Server] Received " << bytes_read << " raw bytes:\n";
        std::cout << "----------------------------------------\n";
        std::cout << buffer << "\n";
        std::cout << "----------------------------------------\n";

        // 6. Echo the bytes back to the client
        const char* response = "Hello from C++ Network Server! Packet received.\n";
        write(client_fd, response, strlen(response));
    } else {
        std::cout << "[Server] Client disconnected or read failed.\n";
    }

    // Clean up file descriptors
    close(client_fd);
    close(server_fd);
    std::cout << "[Server] Shutting down cleanly.\n";
    return 0;
}