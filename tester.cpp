#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <iostream>
#include <string>

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "Could not create socket" << std::endl;
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);

    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        return 1;
    }

    std::cout << "Connected to server!" << std::endl;

    std::string message;
    while(true){
        std::getline(std::cin, message);
        if (message == "dc"){
            break;
        }
        send(sock, message.data(), message.length(), 0);
        std::cout << "Message sent: " << message << std::endl;

        char buffer[1024] = {0};
        ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            std::cout << "Server echoed: " << buffer << std::endl;
        }
        else{
            std::cout<<"server disconnected or errored\n";
        }
    }

    close(sock);
    return 0;
}