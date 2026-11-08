#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#define SERVER_WINSOCK
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET socket_t;
#define CLOSE_SOCKET closesocket
#define SOCKET_ERROR_CODE WSAGetLastError()
#define INVALID_SOCKET_VALUE INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int socket_t;
#define CLOSE_SOCKET close
#define SOCKET_ERROR_CODE errno
#define INVALID_SOCKET_VALUE (-1)
#endif

#define WORLD_X 64
#define WORLD_Y 24
#define WORLD_Z 64
#define MAX_CLIENTS 16
#define MAX_NAME_LENGTH 24
#define MAX_LINE_LENGTH 1024
#define DEFAULT_PORT 25565
#define PLAYER_RADIUS 0.35f
#define PLAYER_HEIGHT 1.8f
#define MAX_REACH 6.0f

enum {
    BLOCK_AIR = 0,
    BLOCK_GRASS = 1,
    BLOCK_DIRT = 2,
    BLOCK_STONE = 3,
    BLOCK_SAND = 4,
    BLOCK_WOOD = 5,
    BLOCK_LEAF = 6,
    BLOCK_GLASS = 7
};

typedef struct {
    socket_t socket;
    int id;
    char name[MAX_NAME_LENGTH + 1];
    char input[MAX_LINE_LENGTH];
    size_t input_length;
    float x;
    float y;
    float z;
    float yaw;
    float pitch;
    int active;
    int authenticated;
} Client;

typedef struct {
    char magic[8];
    unsigned int version;
    unsigned int width;
    unsigned int height;
    unsigned int depth;
} WorldHeader;

static unsigned char world[WORLD_X][WORLD_Y][WORLD_Z];
static Client clients[MAX_CLIENTS];
static socket_t listen_socket = INVALID_SOCKET_VALUE;
static volatile sig_atomic_t running = 1;
static int next_client_id = 1;

static void stop_server(int signal_number) {
    (void)signal_number;
    running = 0;
}

static unsigned char get_block(int x, int y, int z) {
    if (x < 0 || x >= WORLD_X || y < 0 || y >= WORLD_Y || z < 0 || z >= WORLD_Z) {
        return BLOCK_AIR;
    }
    return world[x][y][z];
}

static void create_flat_world(void) {
    memset(world, BLOCK_AIR, sizeof world);
    for (int x = 0; x < WORLD_X; ++x) {
        for (int z = 0; z < WORLD_Z; ++z) {
            for (int y = 0; y < 3; ++y) world[x][y][z] = BLOCK_STONE;
            world[x][3][z] = BLOCK_GRASS;
        }
    }
}

static int load_world(void) {
    FILE *file = fopen("world.dat", "rb");
    if (file == NULL) {
        if (errno != ENOENT) perror("world.dat");
        create_flat_world();
        return 0;
    }

    WorldHeader header;
    size_t blocks_read;
    int trailing;
    if (fread(&header, sizeof header, 1, file) != 1
        || memcmp(header.magic, "BWORLD1", 7) != 0
        || header.version != 1
        || header.width != WORLD_X || header.height != WORLD_Y || header.depth != WORLD_Z) {
        fprintf(stderr, "world.dat has an invalid or incompatible format; creating a new world\n");
        fclose(file);
        create_flat_world();
        return 0;
    }

    blocks_read = fread(world, 1, sizeof world, file);
    trailing = fgetc(file);
    if (ferror(file) || blocks_read != sizeof world || trailing != EOF) {
        fprintf(stderr, "world.dat is truncated or contains unexpected data; creating a new world\n");
        fclose(file);
        create_flat_world();
        return 0;
    }
    fclose(file);
    for (size_t i = 0; i < sizeof world; ++i) {
        if (((unsigned char *)world)[i] > BLOCK_GLASS) {
            fprintf(stderr, "world.dat contains an unknown block type; creating a new world\n");
            create_flat_world();
            return 0;
        }
    }
    printf("Loaded world.dat\n");
    return 1;
}

static int save_world(void) {
    FILE *file = fopen("world.dat.tmp", "wb");
    if (file == NULL) {
        perror("Unable to create world.dat.tmp");
        return 0;
    }
    WorldHeader header = { "BWORLD1", 1, WORLD_X, WORLD_Y, WORLD_Z };
    int ok = fwrite(&header, sizeof header, 1, file) == 1
        && fwrite(world, 1, sizeof world, file) == sizeof world;
    if (fclose(file) != 0) ok = 0;
    if (!ok) {
        fprintf(stderr, "Failed to write world.dat.tmp\n");
        remove("world.dat.tmp");
        return 0;
    }
#ifdef SERVER_WINSOCK
    if (!MoveFileExA("world.dat.tmp", "world.dat",
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        fprintf(stderr, "Unable to replace world.dat (Windows error %lu)\n",
                (unsigned long)GetLastError());
        return 0;
    }
#else
    if (rename("world.dat.tmp", "world.dat") != 0) {
        perror("Unable to rename world.dat.tmp");
        return 0;
    }
#endif
    return 1;
}

static int send_all(socket_t socket, const char *data, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        int chunk = (int)(length - sent);
#ifdef SERVER_WINSOCK
        int result = send(socket, data + sent, chunk, 0);
#else
        ssize_t result = send(socket, data + sent, (size_t)chunk, 0);
#endif
        if (result <= 0) return 0;
        sent += (size_t)result;
    }
    return 1;
}

static int send_line(Client *client, const char *line) {
    return send_all(client->socket, line, strlen(line));
}

static void broadcast_line(const char *line, int except_id) {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active && clients[i].authenticated && clients[i].id != except_id) {
            if (!send_line(&clients[i], line)) {
                shutdown(clients[i].socket, 2);
            }
        }
    }
}

static void disconnect_client(Client *client) {
    if (!client->active) return;
    if (client->authenticated) {
        char message[64];
        snprintf(message, sizeof message, "LEAVE %d\n", client->id);
        broadcast_line(message, client->id);
        printf("%s left (id %d)\n", client->name, client->id);
    }
    CLOSE_SOCKET(client->socket);
    memset(client, 0, sizeof *client);
    client->socket = INVALID_SOCKET_VALUE;
}

static int send_world_snapshot(Client *client) {
    char line[128];
    int block_count = 0;
    for (int x = 0; x < WORLD_X; ++x)
        for (int y = 0; y < WORLD_Y; ++y)
            for (int z = 0; z < WORLD_Z; ++z)
                if (world[x][y][z] != BLOCK_AIR) ++block_count;

    snprintf(line, sizeof line, "WORLD_BEGIN %d\n", block_count);
    if (!send_line(client, line)) return 0;
    for (int x = 0; x < WORLD_X; ++x) {
        for (int y = 0; y < WORLD_Y; ++y) {
            for (int z = 0; z < WORLD_Z; ++z) {
                unsigned char block = world[x][y][z];
                if (block == BLOCK_AIR) continue;
                snprintf(line, sizeof line, "BLOCK %d %d %d %u\n", x, y, z, block);
                if (!send_line(client, line)) return 0;
            }
        }
    }
    return send_line(client, "WORLD_END\n");
}

static int valid_player_name(const char *name) {
    size_t length = strlen(name);
    if (length == 0 || length > MAX_NAME_LENGTH) return 0;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (c < 33 || c > 126 || c == '"' || c == '\\') return 0;
    }
    return 1;
}

static int player_intersects_block(const Client *client, int x, int y, int z) {
    float min_x = client->x - PLAYER_RADIUS;
    float max_x = client->x + PLAYER_RADIUS;
    float min_y = client->y;
    float max_y = client->y + PLAYER_HEIGHT;
    float min_z = client->z - PLAYER_RADIUS;
    float max_z = client->z + PLAYER_RADIUS;
    return (float)x < max_x && (float)(x + 1) > min_x
        && (float)y < max_y && (float)(y + 1) > min_y
        && (float)z < max_z && (float)(z + 1) > min_z;
}

static int any_player_intersects_block(int x, int y, int z) {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active && clients[i].authenticated
            && player_intersects_block(&clients[i], x, y, z)) return 1;
    }
    return 0;
}

static float distance_to_block(const Client *client, int x, int y, int z) {
    float dx = client->x + 0.5f - ((float)x + 0.5f);
    float dy = client->y + 1.62f - ((float)y + 0.5f);
    float dz = client->z + 0.5f - ((float)z + 0.5f);
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

static int handle_hello(Client *client, const char *line) {
    char name[MAX_NAME_LENGTH + 1];
    char extra;
    if (sscanf(line, "HELLO %24s %c", name, &extra) != 1 || !valid_player_name(name)) {
        send_line(client, "ERROR Invalid name. Use 1-24 printable ASCII characters.\n");
        return 0;
    }
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active && clients[i].authenticated
            && strcmp(clients[i].name, name) == 0) {
            send_line(client, "ERROR Name is already connected.\n");
            return 0;
        }
    }

    client->id = next_client_id++;
    snprintf(client->name, sizeof client->name, "%s", name);
    client->x = WORLD_X * 0.5f;
    client->y = 4.0f;
    client->z = WORLD_Z * 0.5f;
    char welcome[128];
    snprintf(welcome, sizeof welcome, "WELCOME %d %d %d %d %d\n",
             client->id, WORLD_X, WORLD_Y, WORLD_Z, MAX_CLIENTS);
    if (!send_line(client, welcome)) return 0;
    if (!send_world_snapshot(client)) return 0;

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        Client *other = &clients[i];
        if (!other->active || !other->authenticated) continue;
        char player_line[160];
        snprintf(player_line, sizeof player_line, "PLAYER %d %s %.3f %.3f %.3f %.4f %.4f\n",
                 other->id, other->name, other->x, other->y, other->z, other->yaw, other->pitch);
        if (!send_line(client, player_line)) return 0;
    }

    client->authenticated = 1;
    char joined[160];
    snprintf(joined, sizeof joined, "JOIN %d %s %.3f %.3f %.3f\n",
             client->id, client->name, client->x, client->y, client->z);
    broadcast_line(joined, client->id);
    printf("%s joined (id %d)\n", client->name, client->id);
    return 1;
}

static int handle_client_line(Client *client, char *line) {
    if (!client->authenticated) {
        if (strncmp(line, "HELLO ", 6) != 0) {
            send_line(client, "ERROR Send HELLO <name> first.\n");
            return 0;
        }
        return handle_hello(client, line);
    }

    if (strcmp(line, "PING") == 0) return send_line(client, "PONG\n");

    if (strncmp(line, "CHAT ", 5) == 0) {
        char *text = line + 5;
        if (*text == '\0') return 1;
        char output[MAX_LINE_LENGTH + 64];
        snprintf(output, sizeof output, "CHAT %d %s %s\n", client->id, client->name, text);
        broadcast_line(output, -1);
        printf("[%s] %s\n", client->name, text);
        return 1;
    }

    if (strncmp(line, "POS ", 4) == 0) {
        float x, y, z, yaw, pitch;
        char extra;
        if (sscanf(line + 4, "%f %f %f %f %f %c", &x, &y, &z, &yaw, &pitch, &extra) != 5
            || !isfinite(x) || !isfinite(y) || !isfinite(z)
            || !isfinite(yaw) || !isfinite(pitch)
            || x < 0.0f || x >= WORLD_X || y < 0.0f || y >= WORLD_Y
            || z < 0.0f || z >= WORLD_Z) {
            return send_line(client, "ERROR Invalid player position.\n");
        }
        client->x = x;
        client->y = y;
        client->z = z;
        client->yaw = yaw;
        client->pitch = pitch;
        char output[160];
        snprintf(output, sizeof output, "PLAYER %d %s %.3f %.3f %.3f %.4f %.4f\n",
                 client->id, client->name, x, y, z, yaw, pitch);
        broadcast_line(output, client->id);
        return 1;
    }

    if (strncmp(line, "BREAK ", 6) == 0) {
        int x, y, z;
        char extra;
        if (sscanf(line + 6, "%d %d %d %c", &x, &y, &z, &extra) != 3
            || x < 0 || x >= WORLD_X || y < 0 || y >= WORLD_Y || z < 0 || z >= WORLD_Z
            || get_block(x, y, z) == BLOCK_AIR
            || distance_to_block(client, x, y, z) > MAX_REACH) {
            return send_line(client, "ERROR Cannot break that block.\n");
        }
        unsigned char broken_block = get_block(x, y, z);
        world[x][y][z] = BLOCK_AIR;
        char output[64];
        snprintf(output, sizeof output, "BLOCK %d %d %d 0\n", x, y, z);
        broadcast_line(output, -1);
        snprintf(output, sizeof output, "EDIT_OK BREAK %d %d %d %u\n",
                 x, y, z, (unsigned int)broken_block);
        if (!send_line(client, output)) return 0;
        if (!save_world()) return 0;
        return 1;
    }

    if (strncmp(line, "PLACE ", 6) == 0) {
        int x, y, z, block;
        char extra;
        if (sscanf(line + 6, "%d %d %d %d %c", &x, &y, &z, &block, &extra) != 4
            || x < 0 || x >= WORLD_X || y < 0 || y >= WORLD_Y || z < 0 || z >= WORLD_Z
            || block < BLOCK_GRASS || block > BLOCK_GLASS
            || get_block(x, y, z) != BLOCK_AIR
            || any_player_intersects_block(x, y, z)
            || distance_to_block(client, x, y, z) > MAX_REACH) {
            return send_line(client, "ERROR Cannot place that block.\n");
        }
        world[x][y][z] = (unsigned char)block;
        char output[64];
        snprintf(output, sizeof output, "BLOCK %d %d %d %d\n", x, y, z, block);
        broadcast_line(output, -1);
        snprintf(output, sizeof output, "EDIT_OK PLACE %d %d %d %d\n",
                 x, y, z, block);
        if (!send_line(client, output)) return 0;
        if (!save_world()) return 0;
        return 1;
    }

    send_line(client, "ERROR Unknown command.\n");
    return 1;
}

static int handle_client_data(Client *client) {
    char buffer[2048];
#ifdef SERVER_WINSOCK
    int received = recv(client->socket, buffer, sizeof buffer, 0);
#else
    ssize_t received = recv(client->socket, buffer, sizeof buffer, 0);
#endif
    if (received <= 0) return 0;

    for (int i = 0; i < received; ++i) {
        char c = buffer[i];
        if (c == '\n') {
            client->input[client->input_length] = '\0';
            if (client->input_length > 0 && client->input[client->input_length - 1] == '\r') {
                client->input[client->input_length - 1] = '\0';
            }
            if (!handle_client_line(client, client->input)) return 0;
            client->input_length = 0;
        } else if (c == '\0' || client->input_length + 1 >= sizeof client->input) {
            send_line(client, "ERROR Input line too long.\n");
            return 0;
        } else {
            client->input[client->input_length++] = c;
        }
    }
    return 1;
}

static int parse_port(const char *text, unsigned short *port) {
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < 1 || value > 65535) return 0;
    *port = (unsigned short)value;
    return 1;
}

int main(int argc, char **argv) {
    unsigned short port = DEFAULT_PORT;
    if (argc > 2 || (argc == 2 && !parse_port(argv[1], &port))) {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        return 2;
    }

    signal(SIGINT, stop_server);
    signal(SIGTERM, stop_server);
#ifdef SERVER_WINSOCK
    WSADATA winsock_data;
    if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif

    if (!load_world() && !save_world()) {
        fprintf(stderr, "Unable to save the regenerated server world; aborting startup\n");
#ifdef SERVER_WINSOCK
        WSACleanup();
#endif
        return 1;
    }
    for (int i = 0; i < MAX_CLIENTS; ++i) clients[i].socket = INVALID_SOCKET_VALUE;

    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET_VALUE) {
        fprintf(stderr, "Unable to create socket (%d)\n", SOCKET_ERROR_CODE);
#ifndef SERVER_WINSOCK
        perror("socket");
#endif
#ifdef SERVER_WINSOCK
        WSACleanup();
#endif
        return 1;
    }
    int reuse = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof reuse);

    struct sockaddr_in address;
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(listen_socket, (struct sockaddr *)&address, sizeof address) != 0) {
        fprintf(stderr, "Unable to bind port %u (%d)\n", port, SOCKET_ERROR_CODE);
        CLOSE_SOCKET(listen_socket);
#ifdef SERVER_WINSOCK
        WSACleanup();
#endif
        return 1;
    }
    if (listen(listen_socket, MAX_CLIENTS) != 0) {
        fprintf(stderr, "Unable to listen on port %u (%d)\n", port, SOCKET_ERROR_CODE);
        CLOSE_SOCKET(listen_socket);
#ifdef SERVER_WINSOCK
        WSACleanup();
#endif
        return 1;
    }

    printf("Block World dedicated server listening on TCP port %u\n", port);
    printf("Maximum players: %d. Press Ctrl+C to stop.\n", MAX_CLIENTS);

    while (running) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(listen_socket, &read_set);
        socket_t max_socket = listen_socket;
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i].active) {
                FD_SET(clients[i].socket, &read_set);
                if (clients[i].socket > max_socket) max_socket = clients[i].socket;
            }
        }
#ifdef SERVER_WINSOCK
        TIMEVAL timeout = { 0, 250000 };
        int ready = select(0, &read_set, NULL, NULL, &timeout);
#else
        struct timeval timeout = { 0, 250000 };
        int ready = select(max_socket + 1, &read_set, NULL, NULL, &timeout);
#endif
        if (ready < 0) {
            if (!running) break;
#ifdef SERVER_WINSOCK
            if (WSAGetLastError() == WSAEINTR) continue;
#else
            if (errno == EINTR) continue;
#endif
            fprintf(stderr, "select failed (%d)\n", SOCKET_ERROR_CODE);
            break;
        }
        if (ready == 0) continue;

        if (FD_ISSET(listen_socket, &read_set)) {
            struct sockaddr_in peer_address;
#ifdef SERVER_WINSOCK
            int peer_length = sizeof peer_address;
#else
            socklen_t peer_length = sizeof peer_address;
#endif
            socket_t peer_socket = accept(listen_socket, (struct sockaddr *)&peer_address,
                                          &peer_length);
            if (peer_socket != INVALID_SOCKET_VALUE) {
                int slot = -1;
                for (int i = 0; i < MAX_CLIENTS; ++i) {
                    if (!clients[i].active) {
                        slot = i;
                        break;
                    }
                }
                if (slot < 0) {
                    const char full[] = "ERROR Server is full.\n";
                    send_all(peer_socket, full, sizeof full - 1);
                    CLOSE_SOCKET(peer_socket);
                } else {
                    memset(&clients[slot], 0, sizeof clients[slot]);
                    clients[slot].socket = peer_socket;
                    clients[slot].active = 1;
                    printf("Connection from %s\n", inet_ntoa(peer_address.sin_addr));
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i].active && FD_ISSET(clients[i].socket, &read_set)) {
                if (!handle_client_data(&clients[i])) disconnect_client(&clients[i]);
            }
        }
    }

    for (int i = 0; i < MAX_CLIENTS; ++i) disconnect_client(&clients[i]);
    save_world();
    if (listen_socket != INVALID_SOCKET_VALUE) CLOSE_SOCKET(listen_socket);
#ifdef SERVER_WINSOCK
    WSACleanup();
#endif
    printf("Server stopped.\n");
    return 0;
}
