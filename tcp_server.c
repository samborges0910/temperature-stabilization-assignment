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

#define numExternals 4 // Number of external processes
#define EPSILON 0.001f // Threshold for stabilization

int* establishConnectionsFromExternalProcesses() {

    int socket_desc;
    static int client_socket[numExternals];
    unsigned int client_size;
    struct sockaddr_in server_addr, client_addr;

    // Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_desc < 0) {
        printf("Error while creating socket\n");
        exit(0);
    }
    printf("Socket created successfully\n");

    // Set port and IP
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");


    // Bind to the set port and IP
    if (bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr))<0) {
        printf("Couldn't bind to the port\n");
        exit(0);
    }
    printf("Done with binding\n");

    // Listen for clients
    if (listen(socket_desc, numExternals) < 0) {
        printf("Error while listening\n");
        exit(0);
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
            exit(0);
        }
        printf("One external process connected at IP: %s and port: %i\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        externalCount++;
    }

    printf("--------------------------------------------------------------------------\n");
    printf("All four external processes are now connected\n");
    printf("--------------------------------------------------------------------------\n\n");

    return client_socket; // Pointer to the array of file descriptors of client sockets
}

int main(int argc, char *argv[]) {// Changed parameters to accept the server input

    int socket_desc;
    float centralTemp = atof(argv[1]); // Adding centralTemp as the central temperature
    printf("Initial central temperature: %.3f\n", centralTemp);
    struct msg messageFromClient;
    int* client_socket = establishConnectionsFromExternalProcesses();
    bool stable = false; // Checks stability
    float previousTemperatures[numExternals] = {0}; // Stores clients from last iteration

    while (!stable) {

        // Array to store temperatures from clients
        float temperature[numExternals];

        // Receive messages from the 4 external processes
        for (int i = 0; i < numExternals; i++) {

            // Receive client's message
            if (recv(client_socket[i], (void *)&messageFromClient, sizeof(messageFromClient), 0) < 0){
                printf("Couldn't receive\n");
                return -1;
            }

            temperature[i] = messageFromClient.T;
            printf("Temperature of External Process (%d) = %.5f\n", i + 1, temperature[i]);
        }

        // Modify temperature
        float sum_clients = 0.0f; // Sum of the client temperatures
        for (int i = 0; i < numExternals; i++)
            sum_clients += temperature[i];

        centralTemp = (2.0f * centralTemp + sum_clients) / 6.0f; // Used to find the central temperature
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
    }
    printf("\nSystem has been stabilized\n");
    printf("Final Central Temperature: %.5f\n", centralTemp);

    // Constructing done message
    struct msg done_msg;
    done_msg.T = centralTemp;
    done_msg.Index = -1;

    for (int i = 0; i < numExternals; i++) {
        send(client_socket[i], (const void *)&done_msg, sizeof(done_msg), 0); // Tells the client that the server is done, temperature is stabilized
    }

    // Close all sockets
    for (int i = 0; i < numExternals; i++)
        close(client_socket[i]);

    close(socket_desc);

    return 0;
}
