#include <iostream>     // For standard input/output streams (std::cout, std::cerr)
#include <cstring>      // For memory manipulation functions like memset()
#include <sys/types.h>  // Provides definitions for data types used in system calls
#include <sys/socket.h> // Core socket functions and structures (socket, bind, listen, accept)
#include <netinet/in.h> // Internet domain address structures (sockaddr_in, htons, INADDR_ANY)
#include <arpa/inet.h>  // Definitions for internet operations (inet_ntop to convert IP addresses)
#include <unistd.h>     // Standard symbolic constants and types (close() for file descriptors)
#include <thread>       // C++11 multi-threading library (std::thread)

#define PORT 8080       // Define the port number our server will listen on
#define BUFFER_SIZE 1024// Define the maximum size of data we can read at once (1 KB)

// ==========================================
// WORKER FUNCTION (Runs in its own background thread)
// ==========================================
void handle_client(int client_fd, std::string client_ip) {
    // Print notification that a dedicated worker thread has started for this specific client
    std::cout << "[Worker Thread] Active for client: " << client_ip << " (FD: " << client_fd << ")\n";
    
    char buffer[BUFFER_SIZE]; // Create a character array to hold incoming bytes from the client

    // Continuous loop to keep reading messages from this specific client until they disconnect
    while (true) {
        memset(buffer, 0, BUFFER_SIZE); // Clear/zero out the buffer before reading new data
        
        // read() pauses until the client sends bytes. 
        // It returns the number of bytes read, or <= 0 if the client disconnected.
        ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
        
        if (bytes_read <= 0) {
            std::cout << "[Client " << client_ip << "] Disconnected or connection closed.\n";
            break; // Exit the loop if client drops the connection
        }
        
        // Print the raw text data received from the client
        std::cout << "[Received from " << client_ip << "]: " << buffer;
        
        // write() echoes the exact same bytes back to the client over the socket file descriptor
        write(client_fd, buffer, bytes_read);
    }
    
    // Once the loop breaks (client disconnected), close this client's unique socket file descriptor
    close(client_fd);
}

// ==========================================
// MAIN FUNCTION (Handles initialization and listening)
// ==========================================
int main() {
    // 1. Create the master server socket file descriptor
    // AF_INET = IPv4, SOCK_STREAM = TCP protocol, 0 = default protocol
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        std::cerr << "Socket creation failed!\n";
        return 1;
    }

    // 2. Set socket option SO_REUSEADDR
    // This prevents the "Address already in use" error if you restart the server quickly.
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. Configure the server's IP address and Port binding structure
    sockaddr_in address;
    address.sin_family = AF_INET;         // Set address family to IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Listen on any available network interface on the machine
    address.sin_port = htons(PORT);       // Convert port number (8080) to Network Byte Order (Big-Endian)

    // 4. Bind the socket file descriptor to our specified IP and Port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed!\n";
        close(server_fd);
        return 1;
    }

    // 5. Put the socket into listening mode
    // 5 is the backlog queue size (maximum number of pending client connections waiting to be accepted)
    if (listen(server_fd, 5) < 0) {
        std::cerr << "Listen failed!\n";
        close(server_fd);
        return 1;
    }
    std::cout << "[Server] Multi-threaded TCP Echo Server listening on port " << PORT << "...\n";

    // 6. Main server loop to continuously accept incoming client connections
    while (true) {
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        // accept() is a BLOCKING call. The program pauses here until a client connects.
        // When a client connects, it creates a NEW, unique file descriptor (client_fd) just for them.
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            std::cerr << "Accept failed!\n";
            continue; // Skip to next iteration if accept failed
        }

        // Convert the client's binary IP address into a readable text string (e.g., "127.0.0.1")
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        std::cout << "[Server] New connection accepted from IP: " << client_ip << "\n";

        // 7. Spawn a new background worker thread for this client
        // We pass our worker function (handle_client), the client's unique FD, and their IP address.
        std::thread client_thread(handle_client, client_fd, std::string(client_ip));
        
        // detach() tells C++ to let this thread run independently in the background 
        // without forcing the main thread to wait around for it to finish.
        client_thread.detach(); 
    }

    // Cleanup master socket (though in an infinite `while(true)` loop, this is rarely reached unless interrupted)
    close(server_fd);
    return 0;
}