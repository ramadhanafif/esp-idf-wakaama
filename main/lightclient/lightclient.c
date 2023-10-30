
#include "connection.h"
#include "liblwm2m.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern lwm2m_object_t *get_object_device(void);
extern void free_object_device(lwm2m_object_t *objectP);
extern lwm2m_object_t *get_server_object(void);
extern void free_server_object(lwm2m_object_t *object);
extern lwm2m_object_t *get_security_object(void);
extern void free_security_object(lwm2m_object_t *objectP);
extern char *get_server_uri(lwm2m_object_t *objectP, uint16_t secObjInstID);
extern lwm2m_object_t *get_test_object(void);
extern void free_test_object(lwm2m_object_t *object);

#define MAX_PACKET_SIZE 2048

int g_reboot      = 0;
static int g_quit = 0;

void *lwm2m_connect_server(uint16_t secObjInstID, void *userData)
{
}

void lwm2m_close_connection(void *sessionH, void *userData)
{
}

void print_state(lwm2m_context_t *lwm2mH)
{
    lwm2m_server_t *targetP;

    fprintf(stderr, "State: ");
    switch (lwm2mH->state) {
    case STATE_INITIAL:
        fprintf(stderr, "STATE_INITIAL");
        break;
    case STATE_BOOTSTRAP_REQUIRED:
        fprintf(stderr, "STATE_BOOTSTRAP_REQUIRED");
        break;
    case STATE_BOOTSTRAPPING:
        fprintf(stderr, "STATE_BOOTSTRAPPING");
        break;
    case STATE_REGISTER_REQUIRED:
        fprintf(stderr, "STATE_REGISTER_REQUIRED");
        break;
    case STATE_REGISTERING:
        fprintf(stderr, "STATE_REGISTERING");
        break;
    case STATE_READY:
        fprintf(stderr, "STATE_READY");
        break;
    default:
        fprintf(stderr, "Unknown !");
        break;
    }
    fprintf(stderr, "\r\n");

    targetP = lwm2mH->bootstrapServerList;

    if (lwm2mH->bootstrapServerList == NULL) {
        fprintf(stderr, "No Bootstrap Server.\r\n");
    } else {
        fprintf(stderr, "Bootstrap Servers:\r\n");
        for (targetP = lwm2mH->bootstrapServerList; targetP != NULL; targetP = targetP->next) {
            fprintf(stderr, " - Security Object ID %d", targetP->secObjInstID);
            fprintf(stderr, "\tHold Off Time: %lu s", (unsigned long)targetP->lifetime);
            fprintf(stderr, "\tstatus: ");
            switch (targetP->status) {
            case STATE_DEREGISTERED:
                fprintf(stderr, "DEREGISTERED\r\n");
                break;
            case STATE_BS_HOLD_OFF:
                fprintf(stderr, "CLIENT HOLD OFF\r\n");
                break;
            case STATE_BS_INITIATED:
                fprintf(stderr, "BOOTSTRAP INITIATED\r\n");
                break;
            case STATE_BS_PENDING:
                fprintf(stderr, "BOOTSTRAP PENDING\r\n");
                break;
            case STATE_BS_FINISHED:
                fprintf(stderr, "BOOTSTRAP FINISHED\r\n");
                break;
            case STATE_BS_FAILED:
                fprintf(stderr, "BOOTSTRAP FAILED\r\n");
                break;
            default:
                fprintf(stderr, "INVALID (%d)\r\n", (int)targetP->status);
            }
            fprintf(stderr, "\r\n");
        }
    }

    if (lwm2mH->serverList == NULL) {
        fprintf(stderr, "No LWM2M Server.\r\n");
    } else {
        fprintf(stderr, "LWM2M Servers:\r\n");
        for (targetP = lwm2mH->serverList; targetP != NULL; targetP = targetP->next) {
            fprintf(stderr, " - Server ID %d", targetP->shortID);
            fprintf(stderr, "\tstatus: ");
            switch (targetP->status) {
            case STATE_DEREGISTERED:
                fprintf(stderr, "DEREGISTERED\r\n");
                break;
            case STATE_REG_PENDING:
                fprintf(stderr, "REGISTRATION PENDING\r\n");
                break;
            case STATE_REGISTERED:
                fprintf(
                    stderr,
                    "REGISTERED\tlocation: \"%s\"\tLifetime: %lus\r\n",
                    targetP->location,
                    (unsigned long)targetP->lifetime);
                break;
            case STATE_REG_UPDATE_PENDING:
                fprintf(stderr, "REGISTRATION UPDATE PENDING\r\n");
                break;
            case STATE_REG_UPDATE_NEEDED:
                fprintf(stderr, "REGISTRATION UPDATE REQUIRED\r\n");
                break;
            case STATE_DEREG_PENDING:
                fprintf(stderr, "DEREGISTRATION PENDING\r\n");
                break;
            case STATE_REG_FAILED:
                fprintf(stderr, "REGISTRATION FAILED\r\n");
                break;
            default:
                fprintf(stderr, "INVALID (%d)\r\n", (int)targetP->status);
            }
            fprintf(stderr, "\r\n");
        }
    }
}

#define OBJ_COUNT 4

int main(int argc, char *argv[])
{
    client_data_t data;
    lwm2m_context_t *lwm2mH = NULL;
    lwm2m_object_t *objArray[OBJ_COUNT];

    // const char *localPort = "56830";
    const char *name = "testlwm2mclient";

    int result;
    int opt;

    memset(&data, 0, sizeof(client_data_t));

    data.addressFamily = AF_INET6;

    opt = 1;
    while (opt < argc) {
        if (argv[opt] == NULL || argv[opt][0] != '-' || argv[opt][2] != 0) {
            print_usage();
            return 0;
        }
        switch (argv[opt][1]) {
        case 'n':
            opt++;
            if (opt >= argc) {
                print_usage();
                return 0;
            }
            name = argv[opt];
            break;
        case 'l':
            opt++;
            if (opt >= argc) {
                print_usage();
                return 0;
            }
            localPort = argv[opt];
            break;
        case '4':
            data.addressFamily = AF_INET;
            break;
        case 'S':
            opt++;
            if (opt >= argc) {
                print_usage();
                return 0;
            }
            uint16_t coap_block_size_arg;
            if (1 == sscanf(argv[opt], "%" SCNu16, &coap_block_size_arg) &&
                lwm2m_set_coap_block_size(coap_block_size_arg)) {
                break;
            } else {
                print_usage();
                return 0;
            }
        default:
            print_usage();
            return 0;
        }
        opt += 1;
    }

    /*
     *This call an internal function that create an IPv6 socket on the port 5683.
     */
    fprintf(stderr, "Trying to bind LWM2M Client to port %s\r\n", localPort);
    data.sock = create_socket(localPort, data.addressFamily);
    if (data.sock < 0) {
        fprintf(stderr, "Failed to open socket: %d %s\r\n", errno, strerror(errno));
        return -1;
    }

    /*
     * Now the main function fill an array with each object, this list will be later passed to liblwm2m.
     * Those functions are located in their respective object file.
     */
    objArray[0] = get_security_object();
    if (NULL == objArray[0]) {
        fprintf(stderr, "Failed to create security object\r\n");
        return -1;
    }
    data.securityObjP = objArray[0];

    objArray[1] = get_server_object();
    if (NULL == objArray[1]) {
        fprintf(stderr, "Failed to create server object\r\n");
        return -1;
    }

    objArray[2] = get_object_device();
    if (NULL == objArray[2]) {
        fprintf(stderr, "Failed to create Device object\r\n");
        return -1;
    }

    objArray[3] = get_test_object();
    if (NULL == objArray[3]) {
        fprintf(stderr, "Failed to create Test object\r\n");
        return -1;
    }

    /*
     * The liblwm2m library is now initialized with the functions that will be in
     * charge of communication
     */
    lwm2mH = lwm2m_init(&data);
    if (NULL == lwm2mH) {
        fprintf(stderr, "lwm2m_init() failed\r\n");
        return -1;
    }

    /*
     * We configure the liblwm2m library with the name of the client - which shall be unique for each client -
     * the number of objects we will be passing through and the objects array
     */
    result = lwm2m_configure(lwm2mH, name, NULL, NULL, OBJ_COUNT, objArray);
    if (result != 0) {
        fprintf(stderr, "lwm2m_configure() failed: 0x%X\r\n", result);
        return -1;
    }

    /*
     * We catch Ctrl-C signal for a clean exit
     */
    signal(SIGINT, handle_sigint);

    fprintf(stdout, "LWM2M Client \"%s\" started on port %s.\r\nUse Ctrl-C to exit.\r\n\n", name, localPort);

    /*
     * We now enter in a while loop that will handle the communications from the server
     */
    while (0 == g_quit) {
        struct timeval tv;
        fd_set readfds;

        tv.tv_sec  = 60;
        tv.tv_usec = 0;

        FD_ZERO(&readfds);
        FD_SET(data.sock, &readfds);

        print_state(lwm2mH);

        /*
         * This function does two things:
         *  - first it does the work needed by liblwm2m (eg. (re)sending some packets).
         *  - Secondly it adjusts the timeout value (default 60s) depending on the state of the transaction
         *    (eg. retransmission) and the time before the next operation
         */
        result = lwm2m_step(lwm2mH, &(tv.tv_sec));
        if (result != 0) {
            fprintf(stderr, "lwm2m_step() failed: 0x%X\r\n", result);
            return -1;
        }

        /*
         * This part wait for an event on the socket until "tv" timed out (set
         * with the precedent function)
         */
        result = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);

        if (result < 0) {
            if (errno != EINTR) {
                fprintf(stderr, "Error in select(): %d %s\r\n", errno, strerror(errno));
            }
        } else if (result > 0) {
            uint8_t buffer[MAX_PACKET_SIZE];
            ssize_t numBytes;

            /*
             * If an event happens on the socket
             */
            if (FD_ISSET(data.sock, &readfds)) {
                struct sockaddr_storage addr;
                socklen_t addrLen;

                addrLen = sizeof(addr);

                /*
                 * We retrieve the data received
                 */
                numBytes = recvfrom(data.sock, buffer, MAX_PACKET_SIZE, 0, (struct sockaddr *)&addr, &addrLen);

                if (0 > numBytes) {
                    fprintf(stderr, "Error in recvfrom(): %d %s\r\n", errno, strerror(errno));
                } else if (numBytes >= MAX_PACKET_SIZE) {
                    fprintf(stderr, "Received packet >= MAX_PACKET_SIZE\r\n");
                } else if (0 < numBytes) {
                    connection_t *connP;

                    connP = connection_find(data.connList, &addr, addrLen);
                    if (connP != NULL) {
                        /*
                         * Let liblwm2m respond to the query depending on the context
                         */
                        lwm2m_handle_packet(lwm2mH, buffer, (size_t)numBytes, connP);
                    } else {
                        /*
                         * This packet comes from an unknown peer
                         */
                        fprintf(stderr, "received bytes ignored!\r\n");
                    }
                }
            }
        }
    }

    /*
     * Finally when the loop is left, we unregister our client from it
     */
    lwm2m_close(lwm2mH);
    close(data.sock);
    connection_free(data.connList);

    free_security_object(objArray[0]);
    free_server_object(objArray[1]);
    free_object_device(objArray[2]);
    free_test_object(objArray[3]);

    fprintf(stdout, "\r\n\n");

    return 0;
}
