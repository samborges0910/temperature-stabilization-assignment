#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <math.h>
#include "utils.h"

#define numExternals 4
#define EPSILON 0.001f  // Stabilization threshold

int* establishConnectionsFromExternalProcesses()
{

    // This socket is used by the server (i.e, Central process) to listen for
    // connections fromt the External process.
    int socket_desc;

    // Array containing the file descriptor of each server-client socket.
    // There will be 4 client sockets, one for each external process.
    //
    // Note that this array is declared as static so the function can return it
    // to the caller function. A static int variable remains in memory while
    // the program is running. A normal local variable is destroyed when a
    // function call where the variable was declared returns. We want this
    // array to persist.
    static int client_socket[numExternals];

    unsigned int client_size;
    struct sockaddr_in server_addr, client_addr;

    // Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_desc < 0) {
        printf("Error while creating socket\n");
        exit(1); // exit(1) terminates the program in case an error is found
    }
    printf("Socket created successfully\n");

    // Set port and IP
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");


    // Bind to the set port and IP
    if (bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Couldn't bind to the port\n");
        close(socket_desc);
        exit(1);
    }
    printf("Done with binding\n");

    // Listen for clients
    if (listen(socket_desc, numExternals) < 0) {
        printf("Error while listening\n");
        close(socket_desc);
        exit(1); // Kills server for better handling
    }
    printf("\n\nListening for incoming connections...\n\n");

    printf("-------------------- Initial connections ---------------------------------\n");

    //========================================================
    //  Connections from externals
    //========================================================

    int externalCount = 0;
    client_size = sizeof(client_addr); // Lets the accept() function know the size of client to avoid overwriting code

    while (externalCount < numExternals) {

       // Accept an incoming connection:
        client_socket[externalCount] = accept(socket_desc, (struct sockaddr*)&client_addr, &client_size);

        if (client_socket[externalCount] < 0) {
            printf("Can't accept\n");
            exit(1);
        }

        printf("One external process connected at IP: %s and port: %i\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        externalCount++;
    }
    printf("--------------------------------------------------------------------------\n");
    printf("All four external processes are now connected\n");
    printf("--------------------------------------------------------------------------\n\n");

    return client_socket; // Pointer to the array of file descriptors of client sockets
}

int main(int argc, char *argv[]) //for receiving command-line arguments
{
    int socket_desc;
    // unsigned int client_size;
    // struct  sockaddr_in server_addr, client_addr;

    if (argc != 2) { // Input should respect format ./server <temperature>
        fprintf(stderr, "Usage: %s <initial central temperature>\n", argv[0]);
        return 1;
    }

    // Read initial central temperature from command line
    float centralTemp = atof(argv[1]); // Adding centralTemp as the central temperature
    printf("Initial central temperature: %.3f\n", centralTemp);

    // Messages received from clients (externals).
    struct msg messageFromClient;

    // Establish client connections and return
    // an array of file descriptors of client sockets.
    int* client_socket = establishConnectionsFromExternalProcesses();

    bool stable = false;
    float previousTemperatures[numExternals] = {0}; // Stores clients from last iteration

    while (!stable) {

        // Array to store temperatures from clients
        float temperature[numExternals];

        // Receive messages from the 4 external processes
        for (int i = 0; i < numExternals; i++) {
            ssize_t r = recv(client_socket[i], (void *)&messageFromClient, sizeof(messageFromClient), 0); //
            if (r <= 0) {
                printf("Couldn't receive from client %d (recv returned %zd)\n", i + 1, r);
                for (int j = 0; j < numExternals; j++) close(client_socket[j]);
                return -1;
            }

            temperature[i] = messageFromClient.T;
            printf("Temperature of External Process (%d) = %.5f\n", i + 1, temperature[i]);
        }

        // Modify temperature
        float sumClients = 0.0f; // Sum of the client temperatures
        for (int i = 0; i < numExternals; i++)
            sumClients += temperature[i];

        centralTemp = (2.0f * centralTemp + sumClients) / 6.0f; // Used to find the central temperature
        printf("Updated Central Temperature: %.5f\n", centralTemp);

        // Construct message with updated temperature
        struct msg updated_msg;
        updated_msg.T = centralTemp;
        updated_msg.Index = 0; // Index of central server

        // Send updated temperatures to the 4 external processes clients
        for (int i = 0; i < numExternals; i++) {
            if (send(client_socket[i], (const void *)&updated_msg, sizeof(updated_msg), 0) < 0) {
                printf("Can't send\n");
                return -1;
            }
        }

        printf("\n");

        // Check stability condition
        stable = true;

        for (int i = 0; i < numExternals; i++) {
            if (fabs(temperature[i] - previousTemperatures[i]) >= EPSILON) { //checks if the current and the previous temperature difference is greater than or equal to the the threshold
                stable = false;
            }
            previousTemperatures[i] = temperature[i];  // Update previous temperatures
        }

        usleep(100000); // Gives the process some time to complete the iteration before going to the next one, used if input with bigger values (100 miliseconds)
    }

    printf("\nSystem has been stabilized\n");
    printf("Final Central Temperature: %.5f\n", centralTemp);

    struct msg done_msg;
    done_msg.T = centralTemp;
    done_msg.Index = -1;

    for (int i = 0; i < numExternals; i++) {
        send(client_socket[i], (const void *)&done_msg, sizeof(done_msg), 0); // Tells the client that the server is done, temperature is stabilized
    }

    // Close sockets
    for (int i = 0; i < numExternals; i++)
        close(client_socket[i]);

    close(socket_desc);

    return 0;
}
