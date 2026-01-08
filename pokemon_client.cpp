#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <cstdlib>
#include <unistd.h>
#include <cstring>

using namespace std;

#define SERVER_PORT 4869
#define MAX_LINE 256

int main(int argc, char* argv[]) {
    struct hostent* hp;
    struct sockaddr_in sin;
    char* host;
    int s;

    if (argc == 2) {
        host = argv[1];
    }
    else {
        host = (char*)"127.0.0.1";  // Default to localhost
    }

    /* translate host name into peer's IP address */
    hp = gethostbyname(host);
    if (!hp) {
        cerr << "pokemon-client: unknown host: " << host << endl;
        exit(1);
    }

    /* build address data structure */
    memset((char*)&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    memcpy((char*)&sin.sin_addr, hp->h_addr, hp->h_length);
    sin.sin_port = htons(SERVER_PORT);

    /* active open */
    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("pokemon-client: socket");
        exit(1);
    }
    if (connect(s, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        perror("pokemon-client: connect");
        close(s);
        exit(1);
    }

    cout << "Connected to server! Type messages (Ctrl+D to quit):" << endl;

    // Prepare for select()
    fd_set read_fds;
    int max_fd = s;  // s is the socket descriptor
    char buf[MAX_LINE];

    while (1) {

        FD_ZERO(&read_fds);   // Clear and set up the file descriptor set
        FD_SET(STDIN_FILENO, &read_fds);  // Monitor keyboard input
        FD_SET(s, &read_fds);              // Monitor server socket

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL); // Use select to wait for activity

        if (activity < 0) {
            perror("select error");
            break;
        }

        // Check if user types anything
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (!cin.getline(buf, sizeof(buf))) {
                cout << "Disconnecting..." << endl;
                break;
            }

            int len = strlen(buf) + 1;  // sends message to server
            if (send(s, buf, len, 0) < 0) {
                perror("send failed");
                break;
            }
        }

        if (FD_ISSET(s, &read_fds)) {  //did server send a message
            int len = recv(s, buf, sizeof(buf), 0);

            if (len <= 0) {
                if (len == 0) {
                    cout << "Server disconnected." << endl;
                }
                else {
                    perror("recv error");
                }
                break;
            }
            buf[len] = '\0';    // Display servers message
            cout << buf << endl;
        }
    }

    close(s);
    return 0;
}
