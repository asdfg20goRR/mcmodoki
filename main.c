#if !defined(_WIN32) && !defined(__CYGWIN__)
#define _POSIX_C_SOURCE 200809L
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#if defined(_WIN32) || defined(__CYGWIN__)
#define CLIENT_WINDOWS
#endif

#if defined(__CYGWIN__) && !defined(_WIN32)
#define _WIN32
#endif

#ifdef CLIENT_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <imm.h>
#include <commdlg.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ft2build.h>
#include FT_FREETYPE_H
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define SD_BOTH SHUT_RDWR
#define closesocket close
typedef struct timeval TIMEVAL;
#endif
#include <GLFW/glfw3.h>
#ifdef CLIENT_WINDOWS
#include <GLFW/glfw3native.h>
#endif
#include <GL/glu.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WORLD_SAVE_PATH WORLD_SAVE_DIRECTORY "\\world.dat"
#define WORLD_SAVE_TEMP_PATH WORLD_SAVE_DIRECTORY "\\world.dat.tmp"
#define SELECTED_SKIN_PATH "skin\\skin.png"
#else
#define WORLD_SAVE_PATH WORLD_SAVE_DIRECTORY "/world.dat"
#define WORLD_SAVE_TEMP_PATH WORLD_SAVE_DIRECTORY "/world.dat.tmp"
#define SELECTED_SKIN_PATH "skin/skin.png"
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

#define WORLD_X 64
#define WORLD_Y 24
#define WORLD_Z 64
#define PLAYER_EYE_HEIGHT 1.62f
#define PLAYER_RADIUS 0.35f
#define PLAYER_HEIGHT 1.8f
#define MOVE_SPEED 5.0f
#define SPRINT_SPEED 8.0f
#define JUMP_SPEED 6.5f
#define GRAVITY 18.0f
#define MAX_REACH 6.0f
#define BLOCK_ACTION_INTERVAL (1.0 / 8.0)
#define BLOCK_BREAK_INTERVAL (1.0 / 3.0)
#define MAX_DROPPED_ITEMS 64
#define ARM_SWING_DURATION 0.28f
#define CHAT_INPUT_CAPACITY 128
#define CHAT_LINE_CAPACITY 112
#define CHAT_HISTORY_SIZE 6
#define CHAT_INPUT_HISTORY_SIZE 32
#define CHAT_UTF8_BUFFER_WIDTH 2048
#define CHAT_UTF8_BUFFER_HEIGHT 64
#define WORLD_SAVE_DIRECTORY "world_saves"
#define USER_CONFIG_PATH "user.json"
#define DEFAULT_SKIN_PATH "skin/skin.png"
#define GRASS_TEXTURE_PATH "textures/kusa.png"
#define DIRT_TEXTURE_PATH "textures/dirt.png"
#define STONE_TEXTURE_PATH "textures/stone.png"
#define SERVER_DEFAULT_HOST "127.0.0.1"
#define SERVER_DEFAULT_PORT 25565
#define NETWORK_MAX_LINE 2048
#define NETWORK_BUFFER_SIZE 8192
#define MAX_REMOTE_PLAYERS 16

typedef enum {
    GAME_MODE_SURVIVAL,
    GAME_MODE_CREATIVE
} GameMode;

typedef struct {
    float x;
    float y;
    float z;
} Vec3;

typedef struct {
    Vec3 pos;
    Vec3 vel;
    Vec3 horizontal_velocity;
    float animation_phase;
    float walk_blend;
    float yaw;
    float pitch;
    bool on_ground;
} Player;

typedef struct {
    int x;
    int y;
    int z;
} BlockPos;

typedef enum {
    CAMERA_FIRST_PERSON = 0,
    CAMERA_THIRD_PERSON = 1,
    CAMERA_THIRD_PERSON_FRONT = 2
} CameraMode;

typedef struct {
    unsigned char block;
    int count;
} InventorySlot;

typedef struct {
    unsigned char block;
    Vec3 pos;
    bool active;
} DroppedItem;

typedef struct {
    size_t start;
    size_t end;
    int child_count;
    char type;
} GltfJsonToken;

typedef struct {
    int buffer_view;
    size_t byte_offset;
    size_t count;
    int component_type;
    int component_count;
} GltfAccessor;

typedef struct {
    size_t offset;
    size_t length;
    size_t stride;
} GltfBufferView;

typedef struct {
    int position_accessor;
    int texcoord_accessor;
    int index_accessor;
} GltfPrimitive;

typedef struct {
    float translation[3];
    float rotation[4];
    float scale[3];
    char name[32];
    int mesh;
    int children[32];
    int child_count;
} GltfNode;

typedef struct {
    int id;
    char name[25];
    Player player;
    double last_position_time;
    bool active;
} RemotePlayer;

#define INVENTORY_SLOT_COUNT 9
#define GLTF_MAX_ACCESSORS 512
#define GLTF_MAX_PRIMITIVES 256
#define GLTF_MAX_MESHES 128
#define GLTF_MAX_NODES 128
#define GLTF_MAX_SCENES 32
#define GLTF_MAX_SCENE_ROOTS 64

static unsigned char world[WORLD_X][WORLD_Y][WORLD_Z];
static bool keys[512];
static double mouse_x;
static double mouse_y;
static BlockPos selected_block = { -1, -1, -1 };
static BlockPos place_pos = { -1, -1, -1 };
static bool has_selection = false;
static CameraMode camera_mode = CAMERA_FIRST_PERSON;
static GLuint grass_texture = 0;
static GLuint dirt_texture = 0;
static GLuint stone_texture = 0;
static bool camera_key_down = false;
static bool escape_key_down = false;
static bool drop_key_down = false;
static bool creative_flying = false;
static bool flight_space_was_down = false;
static double last_flight_space_press = -1.0;
static bool left_mouse_down = false;
static bool inventory_open = false;
static bool paused = false;
static bool home_screen = true;
static bool home_mouse_down = false;
static bool online_settings_open;
static int online_settings_field;
static char online_username[25];
static char online_server[256];
static char online_port[6];
static char online_settings_error[128];
static bool chat_open = false;
static bool suppress_next_chat_char = false;
static unsigned int suppressed_chat_codepoint = 0;
static GameMode game_mode = GAME_MODE_SURVIVAL;
static Player *active_player = NULL;
static char chat_input[CHAT_INPUT_CAPACITY];
static size_t chat_input_length = 0;
static char chat_history[CHAT_HISTORY_SIZE][CHAT_LINE_CAPACITY];
static int chat_history_count = 0;
static char chat_input_history[CHAT_INPUT_HISTORY_SIZE][CHAT_INPUT_CAPACITY];
static int chat_input_history_count = 0;
static int chat_input_history_position = -1;
static char chat_input_draft[CHAT_INPUT_CAPACITY];
static int selected_hotbar_slot = 0;
static InventorySlot inventory[INVENTORY_SLOT_COUNT] = {
    { BLOCK_GRASS, 64 }, { BLOCK_DIRT, 64 }, { BLOCK_STONE, 64 },
    { BLOCK_SAND, 64 }, { BLOCK_WOOD, 64 }, { BLOCK_LEAF, 64 },
    { BLOCK_GLASS, 64 }, { BLOCK_AIR, 0 }, { BLOCK_AIR, 0 }
};
static DroppedItem dropped_items[MAX_DROPPED_ITEMS];
static double next_block_action_time = 0.0;
static double next_block_break_time = 0.0;
static float arm_swing_time = 0.0f;
static GLuint chat_text_texture;
#ifdef _WIN32
static HDC chat_text_dc;
static HBITMAP chat_text_bitmap;
static HGDIOBJ chat_text_old_bitmap;
static HFONT chat_text_font;
static int chat_text_font_height;
static unsigned char *chat_text_pixels;
#else
static FT_Library chat_font_library;
static FT_Face chat_font_face;
static int chat_text_font_height;
static bool chat_font_attempted;
#endif
static unsigned char chat_text_rgba[CHAT_UTF8_BUFFER_WIDTH * CHAT_UTF8_BUFFER_HEIGHT * 4];
static unsigned char *player_model_buffer;
static size_t player_model_buffer_size;
static GLuint player_model_texture;
static GltfAccessor player_model_accessors[GLTF_MAX_ACCESSORS];
static int player_model_accessor_count;
static GltfBufferView player_model_views[GLTF_MAX_ACCESSORS];
static GltfPrimitive player_model_primitives[GLTF_MAX_PRIMITIVES];
static int player_model_primitive_count;
static int player_model_mesh_first[GLTF_MAX_MESHES];
static int player_model_mesh_count[GLTF_MAX_MESHES];
static int player_model_mesh_total;
static GltfNode player_model_nodes[GLTF_MAX_NODES];
static int player_model_node_count;
static int player_model_roots[GLTF_MAX_SCENE_ROOTS];
static int player_model_root_count;
static char selected_skin_path[1024] = DEFAULT_SKIN_PATH;
static char player_name[25] = "Player";
static char server_host[256] = SERVER_DEFAULT_HOST;
static unsigned short server_port = SERVER_DEFAULT_PORT;
#ifdef _WIN32
static SOCKET server_socket = INVALID_SOCKET;
static bool winsock_started;
#else
static int server_socket = INVALID_SOCKET;
#endif
static bool multiplayer_connected;
static bool multiplayer_world_active;
static int local_player_id;
static RemotePlayer remote_players[MAX_REMOTE_PLAYERS];
static char network_input[NETWORK_BUFFER_SIZE];
static size_t network_input_length;
static bool network_world_received;
static int network_expected_world_blocks;
static int network_received_world_blocks;
static double next_network_position_time;
static char network_error[256];

static void submit_chat_message(void);
static void draw_chat_overlay(int width, int height);
static void load_chat_input_history(void);
static bool save_chat_input_history(void);
static void draw_ui_text_at(const char *text, float x, float y, float scale);
static void add_chat_line(const char *text);
static bool save_world(const Player *player);
static bool load_world(Player *player);
static void regenerate_world(Player *player);
static bool player_collides(const Player *player);
static bool choose_player_skin(GLFWwindow *window);
static GLuint load_png_texture_internal(const char *path, bool darken_border);
static void generate_flat_world(void);
static bool connect_to_server(Player *player);
static void disconnect_from_server(void);
static void poll_server_messages(void);
static bool send_server_line(const char *line);
static bool network_handle_line(char *line);
static bool network_consume_data(const char *data, size_t length);
static void open_online_settings(void);
static bool save_online_settings(void);

static void start_game(GLFWwindow *window) {
    home_screen = false;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    int width;
    int height;
    glfwGetWindowSize(window, &width, &height);
    mouse_x = (double)width * 0.5;
    mouse_y = (double)height * 0.5;
    glfwSetCursorPos(window, mouse_x, mouse_y);
}

static void return_to_home(GLFWwindow *window) {
    if (multiplayer_world_active) {
        disconnect_from_server();
        multiplayer_world_active = false;
        if (active_player != NULL && !load_world(active_player)) {
            fprintf(stderr, "Regenerating and saving local world after load failure\n");
            regenerate_world(active_player);
            save_world(active_player);
        }
    } else if (active_player != NULL) {
        save_world(active_player);
    }
    home_screen = true;
    paused = false;
    inventory_open = false;
    chat_open = false;
    chat_input_length = 0;
    chat_input[0] = '\0';
    home_mouse_down = true;
    if (active_player != NULL) {
        active_player->horizontal_velocity = (Vec3){ 0.0f, 0.0f, 0.0f };
        active_player->vel = (Vec3){ 0.0f, 0.0f, 0.0f };
    }
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

static void remove_file(const char *path) {
#ifdef _WIN32
    DeleteFileA(path);
#else
    unlink(path);
#endif
}

static bool save_user_config(void) {
    FILE *file = fopen("user.json.tmp", "wb");
    if (file == NULL) {
        fprintf(stderr, "Unable to write user.json.tmp: %s\n", strerror(errno));
        return false;
    }
    bool ok = fprintf(file,
                      "{\n  \"username\": \"%s\",\n  \"id\": %d,\n"
                      "  \"server\": \"%s\",\n  \"port\": %u,\n"
                      "  \"skin\": \"skin/skin.png\"\n}\n",
                      player_name, local_player_id, server_host, (unsigned int)server_port) >= 0;
    if (fclose(file) != 0) ok = false;
    if (!ok) {
        fprintf(stderr, "Unable to write user configuration\n");
        remove_file("user.json.tmp");
        return false;
    }
#ifdef _WIN32
    if (!MoveFileExA("user.json.tmp", USER_CONFIG_PATH,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        fprintf(stderr, "Unable to update user.json (Windows error %lu)\n",
                (unsigned long)GetLastError());
        remove_file("user.json.tmp");
        return false;
    }
#else
    if (rename("user.json.tmp", USER_CONFIG_PATH) != 0) {
        fprintf(stderr, "Unable to update user.json: %s\n", strerror(errno));
        remove_file("user.json.tmp");
        return false;
    }
#endif
    snprintf(selected_skin_path, sizeof selected_skin_path, "%s", DEFAULT_SKIN_PATH);
    return true;
}

static void open_online_settings(void) {
    snprintf(online_username, sizeof online_username, "%s", player_name);
    snprintf(online_server, sizeof online_server, "%s", server_host);
    snprintf(online_port, sizeof online_port, "%u", (unsigned int)server_port);
    online_settings_field = 0;
    online_settings_error[0] = '\0';
    online_settings_open = true;
}

static bool save_online_settings(void) {
    size_t username_length = strlen(online_username);
    size_t server_length = strlen(online_server);
    bool valid_username = username_length > 0 && username_length <= 24;
    bool valid_server = server_length > 0 && server_length < sizeof server_host;
    for (size_t i = 0; i < username_length; ++i) {
        unsigned char c = (unsigned char)online_username[i];
        if (c < 33 || c > 126 || c == '"' || c == '\\') valid_username = false;
    }
    for (size_t i = 0; i < server_length; ++i) {
        unsigned char c = (unsigned char)online_server[i];
        if (c <= 32 || c >= 127 || c == '"' || c == '\\') valid_server = false;
    }
    char *port_end = NULL;
    long port = strtol(online_port, &port_end, 10);
    if (!valid_username || !valid_server || online_port[0] == '\0'
        || port_end == online_port || *port_end != '\0' || port < 1 || port > 65535) {
        snprintf(online_settings_error, sizeof online_settings_error,
                 "Enter a valid username, server address, and port (1-65535).");
        return false;
    }

    char old_username[sizeof player_name];
    char old_server[sizeof server_host];
    unsigned short old_port = server_port;
    snprintf(old_username, sizeof old_username, "%s", player_name);
    snprintf(old_server, sizeof old_server, "%s", server_host);
    snprintf(player_name, sizeof player_name, "%s", online_username);
    snprintf(server_host, sizeof server_host, "%s", online_server);
    server_port = (unsigned short)port;
    if (!save_user_config()) {
        snprintf(player_name, sizeof player_name, "%s", old_username);
        snprintf(server_host, sizeof server_host, "%s", old_server);
        server_port = old_port;
        snprintf(online_settings_error, sizeof online_settings_error,
                 "Could not save user.json. Check the game folder permissions.");
        return false;
    }
    online_settings_error[0] = '\0';
    online_settings_open = false;
    return true;
}

static bool choose_player_skin(GLFWwindow *window) {
#ifdef _WIN32
    char source_path[MAX_PATH] = "";
    OPENFILENAMEA dialog;
    memset(&dialog, 0, sizeof dialog);
    dialog.lStructSize = sizeof dialog;
    dialog.hwndOwner = glfwGetWin32Window(window);
    dialog.lpstrFilter = "PNG image (*.png)\0*.png\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = source_path;
    dialog.nMaxFile = sizeof source_path;
    dialog.lpstrTitle = "Choose player skin";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR
        | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&dialog)) {
        DWORD error = CommDlgExtendedError();
        if (error != 0) {
            fprintf(stderr, "Unable to open skin file dialog (error 0x%08lx)\n",
                    (unsigned long)error);
        }
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char *pixels = stbi_load(source_path, &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == NULL) {
        fprintf(stderr, "Selected skin %s is not a readable image (%s)\n",
                source_path, stbi_failure_reason());
        return false;
    }
    stbi_image_free(pixels);
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Selected skin %s has invalid dimensions\n", source_path);
        return false;
    }

    GLuint new_texture = load_png_texture_internal(source_path, false);
    if (new_texture == 0) return false;

    char source_full_path[MAX_PATH];
    char target_full_path[MAX_PATH];
    DWORD source_length = GetFullPathNameA(source_path, MAX_PATH, source_full_path, NULL);
    DWORD target_length = GetFullPathNameA(SELECTED_SKIN_PATH, MAX_PATH, target_full_path, NULL);
    bool same_file = source_length > 0 && source_length < MAX_PATH
        && target_length > 0 && target_length < MAX_PATH
        && lstrcmpiA(source_full_path, target_full_path) == 0;
    DWORD attributes = GetFileAttributesA("skin");
    if (attributes == INVALID_FILE_ATTRIBUTES && !CreateDirectoryA("skin", NULL)) {
        fprintf(stderr, "Unable to create skin directory (Windows error %lu)\n",
                (unsigned long)GetLastError());
        glDeleteTextures(1, &new_texture);
        return false;
    }
    if (!same_file && !CopyFileA(source_path, SELECTED_SKIN_PATH, FALSE)) {
        fprintf(stderr, "Unable to copy selected skin to skin.png (Windows error %lu)\n",
                (unsigned long)GetLastError());
        glDeleteTextures(1, &new_texture);
        return false;
    }
#else
    (void)window;
    char source_path[4096] = "";
    FILE *picker = popen(
        "zenity --file-selection --title='Choose player skin' "
        "--file-filter='PNG image | *.png' 2>/dev/null",
        "r");
    if (picker == NULL) {
        fprintf(stderr, "Unable to start zenity; install zenity to choose a skin image\n");
        return false;
    }
    if (fgets(source_path, sizeof source_path, picker) == NULL) source_path[0] = '\0';
    int picker_status = pclose(picker);
    size_t source_length = strlen(source_path);
    while (source_length > 0
           && (source_path[source_length - 1] == '\n' || source_path[source_length - 1] == '\r')) {
        source_path[--source_length] = '\0';
    }
    if (source_path[0] == '\0') return false;
    if (picker_status != 0) {
        fprintf(stderr, "Skin file picker failed with status %d\n", picker_status);
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char *pixels = stbi_load(source_path, &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == NULL) {
        fprintf(stderr, "Selected skin %s is not a readable image (%s)\n",
                source_path, stbi_failure_reason());
        return false;
    }
    stbi_image_free(pixels);
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Selected skin %s has invalid dimensions\n", source_path);
        return false;
    }

    GLuint new_texture = load_png_texture_internal(source_path, false);
    if (new_texture == 0) return false;

    struct stat source_stat;
    struct stat target_stat;
    bool same_file = stat(source_path, &source_stat) == 0
        && stat(SELECTED_SKIN_PATH, &target_stat) == 0
        && source_stat.st_dev == target_stat.st_dev && source_stat.st_ino == target_stat.st_ino;
    if (mkdir("skin", 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "Unable to create skin directory: %s\n", strerror(errno));
        glDeleteTextures(1, &new_texture);
        return false;
    }
    if (!same_file) {
        FILE *source = fopen(source_path, "rb");
        FILE *target = source != NULL ? fopen(SELECTED_SKIN_PATH, "wb") : NULL;
        bool copied = target != NULL;
        unsigned char buffer[8192];
        size_t bytes;
        if (copied) {
            while ((bytes = fread(buffer, 1, sizeof buffer, source)) > 0) {
                if (fwrite(buffer, 1, bytes, target) != bytes) {
                    copied = false;
                    break;
                }
            }
            if (ferror(source)) copied = false;
        }
        if (source != NULL) fclose(source);
        if (target != NULL && fclose(target) != 0) copied = false;
        if (!copied) {
            fprintf(stderr, "Unable to copy selected skin to skin.png: %s\n", strerror(errno));
            glDeleteTextures(1, &new_texture);
            return false;
        }
    }
#endif

    if (player_model_texture != 0) glDeleteTextures(1, &player_model_texture);
    player_model_texture = new_texture;
    save_user_config();
    add_chat_line("Player skin updated");
    return true;
}

static bool get_cursor_framebuffer_position(GLFWwindow *window, float *x, float *y) {
    double cursor_x;
    double cursor_y;
    int window_width;
    int window_height;
    int framebuffer_width;
    int framebuffer_height;
    glfwGetCursorPos(window, &cursor_x, &cursor_y);
    glfwGetWindowSize(window, &window_width, &window_height);
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    if (window_width <= 0 || window_height <= 0) return false;
    *x = (float)(cursor_x * framebuffer_width / window_width);
    *y = (float)(cursor_y * framebuffer_height / window_height);
    return true;
}

static void resume_game(GLFWwindow *window) {
    paused = false;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    int width;
    int height;
    glfwGetWindowSize(window, &width, &height);
    mouse_x = (double)width * 0.5;
    mouse_y = (double)height * 0.5;
    glfwSetCursorPos(window, mouse_x, mouse_y);
}

static bool is_ime_composing(GLFWwindow *window) {
#ifdef _WIN32
    HWND native_window = glfwGetWin32Window(window);
    HIMC input_context = ImmGetContext(native_window);
    if (input_context == NULL) return false;
    LONG composition_length = ImmGetCompositionStringW(input_context, GCS_COMPSTR, NULL, 0);
    ImmReleaseContext(native_window, input_context);
    return composition_length > 0;
#else
    (void)window;
    return false;
#endif
}

static void show_warning(GLFWwindow *window, const char *title, const char *message) {
#ifdef _WIN32
    MessageBoxA(glfwGetWin32Window(window), message, title, MB_OK | MB_ICONWARNING);
#else
    (void)window;
    fprintf(stderr, "%s: %s\n", title, message);
#endif
}

static void show_error(GLFWwindow *window, const char *title, const char *message) {
#ifdef _WIN32
    MessageBoxA(glfwGetWin32Window(window), message, title, MB_OK | MB_ICONERROR);
#else
    (void)window;
    fprintf(stderr, "%s: %s\n", title, message);
#endif
}

static void glfw_error_callback(int error, const char *description) {
    fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

static void handle_key_input(GLFWwindow *window, int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;
    if (online_settings_open) {
        if (key == GLFW_KEY_ENTER && action == GLFW_PRESS) {
            save_online_settings();
        } else if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
            online_settings_field = (online_settings_field + 1) % 3;
        } else if (key == GLFW_KEY_BACKSPACE
                   && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            char *field = online_settings_field == 0 ? online_username
                : online_settings_field == 1 ? online_server : online_port;
            size_t length = strlen(field);
            if (length > 0) field[length - 1] = '\0';
            online_settings_error[0] = '\0';
        }
        return;
    }
    if (home_screen) {
        if (key == GLFW_KEY_ENTER && action == GLFW_PRESS) {
            start_game(window);
        } else if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        return;
    }

    if (chat_open) {
        if (key == GLFW_KEY_ENTER && action == GLFW_PRESS) {
            if (!is_ime_composing(window)) submit_chat_message();
        } else if (key == GLFW_KEY_BACKSPACE
                   && (action == GLFW_PRESS || action == GLFW_REPEAT)
                   && chat_input_length > 0) {
            do {
                --chat_input_length;
            } while (chat_input_length > 0
                     && ((unsigned char)chat_input[chat_input_length] & 0xC0u) == 0x80u);
            chat_input[chat_input_length] = '\0';
            chat_input_history_position = -1;
        } else if (key == GLFW_KEY_UP && action == GLFW_PRESS
                   && chat_input_history_count > 0) {
            if (chat_input_history_position < 0) {
                snprintf(chat_input_draft, sizeof chat_input_draft, "%s", chat_input);
                chat_input_history_position = chat_input_history_count - 1;
            } else if (chat_input_history_position > 0) {
                --chat_input_history_position;
            }
            snprintf(chat_input, sizeof chat_input, "%s",
                     chat_input_history[chat_input_history_position]);
            chat_input_length = strlen(chat_input);
        } else if (key == GLFW_KEY_DOWN && action == GLFW_PRESS
                   && chat_input_history_position >= 0) {
            if (chat_input_history_position < chat_input_history_count - 1) {
                ++chat_input_history_position;
                snprintf(chat_input, sizeof chat_input, "%s",
                         chat_input_history[chat_input_history_position]);
            } else {
                chat_input_history_position = -1;
                snprintf(chat_input, sizeof chat_input, "%s", chat_input_draft);
            }
            chat_input_length = strlen(chat_input);
        }
        return;
    }

    if ((key == GLFW_KEY_T || key == GLFW_KEY_SLASH) && action == GLFW_PRESS
        && !paused && !inventory_open) {
        chat_open = true;
        chat_input_length = key == GLFW_KEY_SLASH ? 1 : 0;
        chat_input[0] = key == GLFW_KEY_SLASH ? '/' : '\0';
        if (key == GLFW_KEY_SLASH) chat_input[1] = '\0';
        chat_input_history_position = -1;
        suppress_next_chat_char = true;
        suppressed_chat_codepoint = key == GLFW_KEY_T ? 't' : '/';
        glfwGetCursorPos(window, &mouse_x, &mouse_y);
    } else if (key == GLFW_KEY_E && action == GLFW_PRESS && !paused) {
        inventory_open = !inventory_open;
        glfwSetInputMode(window, GLFW_CURSOR,
                         inventory_open ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        if (!inventory_open) {
            int width;
            int height;
            glfwGetWindowSize(window, &width, &height);
            mouse_x = (double)width * 0.5;
            mouse_y = (double)height * 0.5;
            glfwSetCursorPos(window, mouse_x, mouse_y);
        }
    }
}

static void handle_char_input(GLFWwindow *window, unsigned int codepoint) {
    (void)window;
    if (online_settings_open) {
        if (codepoint < 33 || codepoint > 126) return;
        char *field = online_settings_field == 0 ? online_username
            : online_settings_field == 1 ? online_server : online_port;
        size_t length = strlen(field);
        size_t capacity = online_settings_field == 0 ? sizeof online_username
            : online_settings_field == 1 ? sizeof online_server : sizeof online_port;
        if (online_settings_field == 2 && (codepoint < '0' || codepoint > '9')) return;
        if (online_settings_field == 0 && (codepoint == '"' || codepoint == '\\')) return;
        if (online_settings_field == 1
            && (codepoint == '"' || codepoint == '\\' || codepoint == ' ')) return;
        if (length + 1 < capacity) {
            field[length] = (char)codepoint;
            field[length + 1] = '\0';
            online_settings_error[0] = '\0';
        }
        return;
    }
    if (suppress_next_chat_char) {
        suppress_next_chat_char = false;
        if (codepoint == suppressed_chat_codepoint
            || (suppressed_chat_codepoint == 't' && codepoint == 'T')) {
            return;
        }
    }
    if (!chat_open || codepoint < 32 || codepoint == 0x7f || codepoint > 0x10ffff
        || (codepoint >= 0xd800 && codepoint <= 0xdfff)) return;

    char encoded[4];
    size_t encoded_length;
    if (codepoint <= 0x7f) {
        encoded[0] = (char)codepoint;
        encoded_length = 1;
    } else if (codepoint <= 0x7ff) {
        encoded[0] = (char)(0xc0 | (codepoint >> 6));
        encoded[1] = (char)(0x80 | (codepoint & 0x3f));
        encoded_length = 2;
    } else if (codepoint <= 0xffff) {
        encoded[0] = (char)(0xe0 | (codepoint >> 12));
        encoded[1] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        encoded[2] = (char)(0x80 | (codepoint & 0x3f));
        encoded_length = 3;
    } else {
        encoded[0] = (char)(0xf0 | (codepoint >> 18));
        encoded[1] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
        encoded[2] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        encoded[3] = (char)(0x80 | (codepoint & 0x3f));
        encoded_length = 4;
    }
    if (chat_input_length + encoded_length >= sizeof chat_input) return;
    memcpy(chat_input + chat_input_length, encoded, encoded_length);
    chat_input_length += encoded_length;
    chat_input[chat_input_length] = '\0';
    chat_input_history_position = -1;
}

static void handle_scroll_input(GLFWwindow *window, double xoffset, double yoffset) {
    (void)window;
    (void)xoffset;
    if (inventory_open || paused || chat_open) return;
    if (yoffset > 0.0) {
        selected_hotbar_slot = (selected_hotbar_slot + INVENTORY_SLOT_COUNT - 1)
            % INVENTORY_SLOT_COUNT;
    } else if (yoffset < 0.0) {
        selected_hotbar_slot = (selected_hotbar_slot + 1) % INVENTORY_SLOT_COUNT;
    }
}

static float clamp_f(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static Vec3 vec3(float x, float y, float z) {
    Vec3 value = { x, y, z };
    return value;
}

static Vec3 add_vec3(Vec3 a, Vec3 b) {
    return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static Vec3 sub_vec3(Vec3 a, Vec3 b) {
    return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static Vec3 mul_vec3(Vec3 a, float s) {
    return vec3(a.x * s, a.y * s, a.z * s);
}

static Vec3 cross_vec3(Vec3 a, Vec3 b) {
    return vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

static float length_vec3(Vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static Vec3 normalize_vec3(Vec3 v) {
    float len = length_vec3(v);
    if (len < 0.0001f) {
        return vec3(0.0f, 0.0f, 0.0f);
    }
    return mul_vec3(v, 1.0f / len);
}

static unsigned char block_at(int x, int y, int z) {
    if (x < 0 || x >= WORLD_X || y < 0 || y >= WORLD_Y || z < 0 || z >= WORLD_Z) {
        return BLOCK_AIR;
    }
    return world[x][y][z];
}

static void set_block(int x, int y, int z, unsigned char block) {
    if (x < 0 || x >= WORLD_X || y < 0 || y >= WORLD_Y || z < 0 || z >= WORLD_Z) {
        return;
    }
    world[x][y][z] = block;
}

static bool is_solid(int x, int y, int z) {
    return block_at(x, y, z) != BLOCK_AIR;
}

#define GROUND_LEVEL 4.0f

static int terrain_height(int x, int z) {
    (void)x;
    (void)z;
    return 3;
}

static void generate_flat_world(void) {
    for (int x = 0; x < WORLD_X; ++x) {
        for (int z = 0; z < WORLD_Z; ++z) {
            int top = terrain_height(x, z);
            for (int y = 0; y < WORLD_Y; ++y) {
                if (y < top) {
                    set_block(x, y, z, BLOCK_STONE);
                } else if (y == top) {
                    set_block(x, y, z, BLOCK_GRASS);
                } else {
                    set_block(x, y, z, BLOCK_AIR);
                }
            }
        }
    }
}

static void regenerate_world(Player *player) {
    static const InventorySlot starting_inventory[INVENTORY_SLOT_COUNT] = {
        { BLOCK_GRASS, 64 }, { BLOCK_DIRT, 64 }, { BLOCK_STONE, 64 },
        { BLOCK_SAND, 64 }, { BLOCK_WOOD, 64 }, { BLOCK_LEAF, 64 },
        { BLOCK_GLASS, 64 }, { BLOCK_AIR, 0 }, { BLOCK_AIR, 0 }
    };
    generate_flat_world();
    player->pos = vec3(WORLD_X * 0.5f, 4.0f, WORLD_Z * 0.5f);
    player->vel = vec3(0.0f, 0.0f, 0.0f);
    player->horizontal_velocity = vec3(0.0f, 0.0f, 0.0f);
    player->animation_phase = 0.0f;
    player->walk_blend = 0.0f;
    player->yaw = 0.0f;
    player->pitch = 0.0f;
    player->on_ground = true;
    memcpy(inventory, starting_inventory, sizeof inventory);
    memset(dropped_items, 0, sizeof dropped_items);
    selected_hotbar_slot = 0;
    game_mode = GAME_MODE_SURVIVAL;
    creative_flying = false;
}

static bool write_world_value(FILE *file, const void *value, size_t size) {
    return fwrite(value, size, 1, file) == 1;
}

static bool read_world_value(FILE *file, void *value, size_t size) {
    return fread(value, size, 1, file) == 1;
}

static bool save_world(const Player *player) {
    static const unsigned char magic[8] = { 'B', 'W', 'W', 'O', 'R', 'L', 'D', '1' };
    const uint32_t version = 1;
    const uint32_t dimensions[3] = { WORLD_X, WORLD_Y, WORLD_Z };
    const uint32_t saved_mode = (uint32_t)game_mode;
    const uint32_t saved_slot = (uint32_t)selected_hotbar_slot;
    bool ok = true;

#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(WORLD_SAVE_DIRECTORY);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        if (!CreateDirectoryA(WORLD_SAVE_DIRECTORY, NULL)) {
            DWORD error = GetLastError();
            if (error != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Unable to create %s (Windows error %lu)\n",
                        WORLD_SAVE_DIRECTORY, (unsigned long)error);
                return false;
            }
        }
    } else if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
#else
    struct stat directory_info;
    if (stat(WORLD_SAVE_DIRECTORY, &directory_info) != 0) {
        if (mkdir(WORLD_SAVE_DIRECTORY, 0755) != 0 && errno != EEXIST) {
            fprintf(stderr, "Unable to create %s: %s\n", WORLD_SAVE_DIRECTORY, strerror(errno));
            return false;
        }
    } else if (!S_ISDIR(directory_info.st_mode)) {
#endif
        fprintf(stderr, "%s exists but is not a directory\n", WORLD_SAVE_DIRECTORY);
        return false;
    }

    FILE *file = fopen(WORLD_SAVE_TEMP_PATH, "wb");
    if (file == NULL) {
        fprintf(stderr, "Unable to open %s: %s\n", WORLD_SAVE_TEMP_PATH, strerror(errno));
        return false;
    }

    ok = write_world_value(file, magic, sizeof magic)
        && write_world_value(file, &version, sizeof version)
        && write_world_value(file, dimensions, sizeof dimensions)
        && write_world_value(file, world, sizeof world)
        && write_world_value(file, &saved_mode, sizeof saved_mode)
        && write_world_value(file, &saved_slot, sizeof saved_slot)
        && write_world_value(file, &player->pos, sizeof player->pos)
        && write_world_value(file, &player->yaw, sizeof player->yaw)
        && write_world_value(file, &player->pitch, sizeof player->pitch)
        && write_world_value(file, inventory, sizeof inventory);
    if (fclose(file) != 0) ok = false;
    if (!ok) {
        fprintf(stderr, "Unable to write world save %s\n", WORLD_SAVE_TEMP_PATH);
        remove_file(WORLD_SAVE_TEMP_PATH);
        return false;
    }
#ifdef _WIN32
    if (!MoveFileExA(WORLD_SAVE_TEMP_PATH, WORLD_SAVE_PATH,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        fprintf(stderr, "Unable to replace %s (Windows error %lu)\n",
                WORLD_SAVE_PATH, (unsigned long)GetLastError());
        remove_file(WORLD_SAVE_TEMP_PATH);
        return false;
    }
#else
    if (rename(WORLD_SAVE_TEMP_PATH, WORLD_SAVE_PATH) != 0) {
        fprintf(stderr, "Unable to replace %s: %s\n", WORLD_SAVE_PATH, strerror(errno));
        remove_file(WORLD_SAVE_TEMP_PATH);
        return false;
    }
#endif
    return true;
}

static bool load_world(Player *player) {
    static const unsigned char expected_magic[8] = { 'B', 'W', 'W', 'O', 'R', 'L', 'D', '1' };
    const uint32_t expected_dimensions[3] = { WORLD_X, WORLD_Y, WORLD_Z };
    unsigned char loaded_world[WORLD_X][WORLD_Y][WORLD_Z];
    InventorySlot loaded_inventory[INVENTORY_SLOT_COUNT];
    Player loaded_player = *player;
    uint32_t version = 0;
    uint32_t dimensions[3] = { 0, 0, 0 };
    uint32_t saved_mode = 0;
    uint32_t saved_slot = 0;
    unsigned char magic[sizeof expected_magic];
    FILE *file = fopen(WORLD_SAVE_PATH, "rb");
    if (file == NULL) {
        if (errno == ENOENT) {
            fprintf(stderr, "%s is missing; a new world will be generated\n", WORLD_SAVE_PATH);
        } else {
            fprintf(stderr, "Unable to open %s: %s\n", WORLD_SAVE_PATH, strerror(errno));
        }
        return false;
    }

    bool ok = read_world_value(file, magic, sizeof magic)
        && read_world_value(file, &version, sizeof version)
        && read_world_value(file, dimensions, sizeof dimensions)
        && read_world_value(file, loaded_world, sizeof loaded_world)
        && read_world_value(file, &saved_mode, sizeof saved_mode)
        && read_world_value(file, &saved_slot, sizeof saved_slot)
        && read_world_value(file, &loaded_player.pos, sizeof loaded_player.pos)
        && read_world_value(file, &loaded_player.yaw, sizeof loaded_player.yaw)
        && read_world_value(file, &loaded_player.pitch, sizeof loaded_player.pitch)
        && read_world_value(file, loaded_inventory, sizeof loaded_inventory);
    int trailing_byte = ok ? fgetc(file) : EOF;
    if (fclose(file) != 0) ok = false;
    if (!ok || trailing_byte != EOF || memcmp(magic, expected_magic, sizeof magic) != 0
        || version != 1 || memcmp(dimensions, expected_dimensions, sizeof dimensions) != 0
        || saved_mode > GAME_MODE_CREATIVE || saved_slot >= INVENTORY_SLOT_COUNT
        || !isfinite(loaded_player.pos.x) || !isfinite(loaded_player.pos.y)
        || !isfinite(loaded_player.pos.z) || !isfinite(loaded_player.yaw)
        || !isfinite(loaded_player.pitch)) {
        fprintf(stderr, "%s is invalid or incompatible; starting a new world\n", WORLD_SAVE_PATH);
        return false;
    }

    for (size_t i = 0; i < sizeof loaded_world; ++i) {
        if (((const unsigned char *)loaded_world)[i] > BLOCK_GLASS) {
            fprintf(stderr, "%s contains an invalid block id; starting a new world\n",
                    WORLD_SAVE_PATH);
            return false;
        }
    }
    for (int i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
        if (loaded_inventory[i].block > BLOCK_GLASS
            || loaded_inventory[i].count < 0 || loaded_inventory[i].count > 64
            || ((loaded_inventory[i].block == BLOCK_AIR) != (loaded_inventory[i].count == 0))) {
            fprintf(stderr, "%s contains invalid inventory data; starting a new world\n",
                    WORLD_SAVE_PATH);
            return false;
        }
    }
    if (loaded_player.pos.x < PLAYER_RADIUS
        || loaded_player.pos.x > WORLD_X - PLAYER_RADIUS
        || loaded_player.pos.y < 0.0f
        || loaded_player.pos.y > WORLD_Y - PLAYER_HEIGHT
        || loaded_player.pos.z < PLAYER_RADIUS
        || loaded_player.pos.z > WORLD_Z - PLAYER_RADIUS) {
        fprintf(stderr, "%s contains an invalid player position; starting a new world\n",
                WORLD_SAVE_PATH);
        return false;
    }

    loaded_player.vel = vec3(0.0f, 0.0f, 0.0f);
    loaded_player.horizontal_velocity = vec3(0.0f, 0.0f, 0.0f);
    loaded_player.on_ground = false;
    memcpy(world, loaded_world, sizeof world);
    if (player_collides(&loaded_player)) {
        fprintf(stderr, "%s places the player inside a block; starting a new world\n",
                WORLD_SAVE_PATH);
        generate_flat_world();
        return false;
    }
    memcpy(inventory, loaded_inventory, sizeof inventory);
    game_mode = (GameMode)saved_mode;
    selected_hotbar_slot = (int)saved_slot;
    *player = loaded_player;
    creative_flying = false;
    return true;
}

static bool player_collides(const Player *player) {
    float min_x = player->pos.x - PLAYER_RADIUS;
    float max_x = player->pos.x + PLAYER_RADIUS;
    float min_y = player->pos.y;
    float max_y = player->pos.y + PLAYER_HEIGHT;
    float min_z = player->pos.z - PLAYER_RADIUS;
    float max_z = player->pos.z + PLAYER_RADIUS;

    int x0 = (int)floorf(min_x);
    int x1 = (int)floorf(max_x);
    int y0 = (int)floorf(min_y);
    int y1 = (int)floorf(max_y);
    int z0 = (int)floorf(min_z);
    int z1 = (int)floorf(max_z);

    for (int x = x0; x <= x1; ++x) {
        for (int y = y0; y <= y1; ++y) {
            for (int z = z0; z <= z1; ++z) {
                if (x < 0 || x >= WORLD_X || y < 0 || y >= WORLD_Y || z < 0 || z >= WORLD_Z) {
                    return true;
                }
                if (world[x][y][z] != BLOCK_AIR) {
                    return true;
                }
            }
        }
    }
    return false;
}

static bool player_has_ground_support(const Player *player) {
    float min_x = player->pos.x - PLAYER_RADIUS;
    float max_x = player->pos.x + PLAYER_RADIUS;
    float min_z = player->pos.z - PLAYER_RADIUS;
    float max_z = player->pos.z + PLAYER_RADIUS;
    int x0 = (int)floorf(min_x);
    int x1 = (int)floorf(max_x - 0.0001f);
    int z0 = (int)floorf(min_z);
    int z1 = (int)floorf(max_z - 0.0001f);
    int y = (int)floorf(player->pos.y - 0.001f);

    if (y < 0 || y >= WORLD_Y) return false;
    for (int x = x0; x <= x1; ++x) {
        for (int z = z0; z <= z1; ++z) {
            if (is_solid(x, y, z) && fabsf(player->pos.y - (float)(y + 1)) < 0.05f) {
                return true;
            }
        }
    }
    return false;
}

static bool block_overlaps_player(const Player *player, BlockPos block) {
    float player_min_x = player->pos.x - PLAYER_RADIUS;
    float player_max_x = player->pos.x + PLAYER_RADIUS;
    float player_min_y = player->pos.y;
    float player_max_y = player->pos.y + PLAYER_HEIGHT;
    float player_min_z = player->pos.z - PLAYER_RADIUS;
    float player_max_z = player->pos.z + PLAYER_RADIUS;

    return (float)block.x < player_max_x && (float)(block.x + 1) > player_min_x
        && (float)block.y < player_max_y && (float)(block.y + 1) > player_min_y
        && (float)block.z < player_max_z && (float)(block.z + 1) > player_min_z;
}

static bool resolve_vertical_movement(Player *player, float old_y, float new_y) {
    float min_x = player->pos.x - PLAYER_RADIUS;
    float max_x = player->pos.x + PLAYER_RADIUS;
    float min_z = player->pos.z - PLAYER_RADIUS;
    float max_z = player->pos.z + PLAYER_RADIUS;
    int x0 = (int)floorf(min_x);
    int x1 = (int)floorf(max_x - 0.0001f);
    int z0 = (int)floorf(min_z);
    int z1 = (int)floorf(max_z - 0.0001f);
    bool collided = false;

    player->pos.y = new_y;
    for (int x = x0; x <= x1; ++x) {
        for (int z = z0; z <= z1; ++z) {
            if (x < 0 || x >= WORLD_X || z < 0 || z >= WORLD_Z) {
                continue;
            }
            for (int y = 0; y < WORLD_Y; ++y) {
                if (world[x][y][z] == BLOCK_AIR) {
                    continue;
                }

                float block_bottom = (float)y;
                float block_top = (float)(y + 1);
                if (new_y > old_y
                    && old_y + PLAYER_HEIGHT <= block_bottom
                    && new_y + PLAYER_HEIGHT > block_bottom) {
                    float resolved_y = block_bottom - PLAYER_HEIGHT;
                    if (!collided || resolved_y < player->pos.y) {
                        player->pos.y = resolved_y;
                    }
                    collided = true;
                } else if (new_y < old_y
                           && old_y >= block_top
                           && new_y < block_top) {
                    float resolved_y = block_top;
                    if (!collided || resolved_y > player->pos.y) {
                        player->pos.y = resolved_y;
                    }
                    collided = true;
                }
            }
        }
    }

    if (collided) {
        player->vel.y = 0.0f;
        return new_y < old_y;
    }
    return false;
}

static void move_player_smoothly(Player *player, Vec3 direction, float speed, float dt) {
    if (length_vec3(direction) > 0.0001f) {
        direction = normalize_vec3(direction);
    } else {
        direction = vec3(0.0f, 0.0f, 0.0f);
    }

    float target_x = direction.x * speed;
    float target_z = direction.z * speed;
    float blend = 1.0f - expf(-14.0f * dt);
    player->horizontal_velocity.x += (target_x - player->horizontal_velocity.x) * blend;
    player->horizontal_velocity.z += (target_z - player->horizontal_velocity.z) * blend;

    float old_x = player->pos.x;
    player->pos.x += player->horizontal_velocity.x * dt;
    if (player_collides(player)) {
        player->pos.x = old_x;
        player->horizontal_velocity.x = 0.0f;
    }

    float old_z = player->pos.z;
    player->pos.z += player->horizontal_velocity.z * dt;
    if (player_collides(player)) {
        player->pos.z = old_z;
        player->horizontal_velocity.z = 0.0f;
    }
}

static bool respawn_player(Player *player) {
    const int spawn_x = WORLD_X / 2;
    const int spawn_z = WORLD_Z / 2;

    for (int attempt = 0; attempt < 128; ++attempt) {
        int offset_x = rand() % 11 - 5;
        int offset_z = rand() % 11 - 5;
        if (offset_x * offset_x + offset_z * offset_z > 25) continue;

        int x = spawn_x + offset_x;
        int z = spawn_z + offset_z;
        for (int y = WORLD_Y - 3; y >= 0; --y) {
            if (!is_solid(x, y, z)) continue;

            Player candidate = *player;
            candidate.pos = vec3((float)x + 0.5f, (float)y + 1.0f, (float)z + 0.5f);
            if (player_collides(&candidate)) continue;

            candidate.vel = vec3(0.0f, 0.0f, 0.0f);
            candidate.horizontal_velocity = vec3(0.0f, 0.0f, 0.0f);
            candidate.on_ground = true;
            *player = candidate;
            return true;
        }
    }
    return false;
}

static void move_player(Player *player, float dt) {
    Vec3 forward = vec3(
        sinf(player->yaw),
        0.0f,
        cosf(player->yaw)
    );
    Vec3 right = normalize_vec3(cross_vec3(forward, vec3(0.0f, 1.0f, 0.0f)));

    Vec3 move_dir = vec3(0.0f, 0.0f, 0.0f);
    if (keys['W'] || keys['w']) move_dir = add_vec3(move_dir, forward);
    if (keys['S'] || keys['s']) move_dir = sub_vec3(move_dir, forward);
    if (keys['D'] || keys['d']) move_dir = add_vec3(move_dir, right);
    if (keys['A'] || keys['a']) move_dir = sub_vec3(move_dir, right);

    if (game_mode == GAME_MODE_CREATIVE && creative_flying) {
        bool sprinting = keys[GLFW_KEY_LEFT_CONTROL] || keys[GLFW_KEY_RIGHT_CONTROL];
        move_player_smoothly(player, move_dir, sprinting ? SPRINT_SPEED : MOVE_SPEED, dt);
        float vertical = 0.0f;
        if (keys[GLFW_KEY_SPACE]) vertical += MOVE_SPEED;
        if (keys[GLFW_KEY_LEFT_SHIFT] || keys[GLFW_KEY_RIGHT_SHIFT]) vertical -= MOVE_SPEED;
        if (vertical != 0.0f) {
            float old_y = player->pos.y;
            resolve_vertical_movement(player, old_y, old_y + vertical * dt);
        }
        player->pos.x = clamp_f(player->pos.x, 0.5f, WORLD_X - 0.5f);
        player->pos.z = clamp_f(player->pos.z, 0.5f, WORLD_Z - 0.5f);
        player->pos.y = clamp_f(player->pos.y, 1.0f, WORLD_Y - PLAYER_HEIGHT);
        player->vel.y = 0.0f;
        player->on_ground = false;
        return;
    }

    bool sprinting = keys[GLFW_KEY_LEFT_CONTROL] || keys[GLFW_KEY_RIGHT_CONTROL];
    bool moving_backward = (keys['S'] || keys['s']) && !(keys['W'] || keys['w']);
    float speed = sprinting ? SPRINT_SPEED : MOVE_SPEED;
    if (moving_backward) speed *= 0.7f;
    move_player_smoothly(player, move_dir, speed, dt);

    if (player->on_ground && !player_has_ground_support(player)) {
        player->on_ground = false;
    }

    bool wants_jump = (keys[GLFW_KEY_SPACE] && player->on_ground);
    if (wants_jump) {
        player->vel.y = JUMP_SPEED;
        player->on_ground = false;
    }

    if (!player->on_ground) {
        if (keys[GLFW_KEY_LEFT_SHIFT] || keys[GLFW_KEY_RIGHT_SHIFT]) {
            player->vel.y = -JUMP_SPEED;
        } else {
            player->vel.y -= GRAVITY * dt;
        }
        float old_y = player->pos.y;
        float new_y = old_y + player->vel.y * dt;
        bool hit_floor = resolve_vertical_movement(player, old_y, new_y);
        if (hit_floor) {
            player->on_ground = true;
        }
    } else {
        player->vel.y = 0.0f;
    }

    if (player->pos.y < -10.0f) {
        if (game_mode == GAME_MODE_SURVIVAL) {
            add_chat_line("You died: fell out of the world");
            if (!respawn_player(player)) {
                add_chat_line("Respawn failed: no safe ground within 5 blocks");
            }
        } else {
            player->pos = vec3(WORLD_X * 0.5f, GROUND_LEVEL, WORLD_Z * 0.5f);
            player->vel = vec3(0.0f, 0.0f, 0.0f);
            player->horizontal_velocity = vec3(0.0f, 0.0f, 0.0f);
            player->on_ground = true;
        }
    }
}

static void update_player_animation(Player *player, float dt, bool allow_movement) {
    float speed = allow_movement
        ? hypotf(player->horizontal_velocity.x, player->horizontal_velocity.z) : 0.0f;
    float target_blend = clamp_f((speed - 0.12f) / 2.2f, 0.0f, 1.0f);
    float blend = 1.0f - expf(-12.0f * dt);
    player->walk_blend += (target_blend - player->walk_blend) * blend;
    if (player->walk_blend > 0.001f) {
        player->animation_phase += dt * (5.0f + speed * 1.8f);
        if (player->animation_phase > (float)(M_PI * 2.0)) {
            player->animation_phase = fmodf(player->animation_phase, (float)(M_PI * 2.0));
        }
    }
}

static void handle_mouse_input(GLFWwindow *window, Player *player) {
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    double dx = x - mouse_x;
    double dy = y - mouse_y;
    mouse_x = x;
    mouse_y = y;

    player->yaw -= (float)(dx * 0.0025);
    player->pitch -= (float)(dy * 0.0025);
    player->pitch = clamp_f(player->pitch, -1.56f, 1.56f);

    int window_width;
    int window_height;
    glfwGetWindowSize(window, &window_width, &window_height);
    mouse_x = (double)window_width * 0.5;
    mouse_y = (double)window_height * 0.5;
    glfwSetCursorPos(window, mouse_x, mouse_y);
}

static void block_color(unsigned char block) {
    switch (block) {
        case BLOCK_GRASS: glColor3f(0.35f, 0.72f, 0.25f); break;
        case BLOCK_DIRT: glColor3f(0.62f, 0.42f, 0.20f); break;
        case BLOCK_STONE: glColor3f(0.62f, 0.62f, 0.66f); break;
        case BLOCK_SAND: glColor3f(0.90f, 0.82f, 0.52f); break;
        case BLOCK_WOOD: glColor3f(0.65f, 0.45f, 0.22f); break;
        case BLOCK_LEAF: glColor3f(0.33f, 0.70f, 0.35f); break;
        case BLOCK_GLASS: glColor3f(0.75f, 0.90f, 0.97f); break;
        default: glColor3f(0.8f, 0.8f, 0.8f); break;
    }
}

static void draw_face(float x1, float y1, float z1,
                     float x2, float y2, float z2,
                     float x3, float y3, float z3,
                     float x4, float y4, float z4,
                     float u1, float v1,
                     float u2, float v2,
                     float u3, float v3,
                     float u4, float v4) {
    glBegin(GL_QUADS);
    glTexCoord2f(u1, v1); glVertex3f(x1, y1, z1);
    glTexCoord2f(u2, v2); glVertex3f(x2, y2, z2);
    glTexCoord2f(u3, v3); glVertex3f(x3, y3, z3);
    glTexCoord2f(u4, v4); glVertex3f(x4, y4, z4);
    glEnd();
}

static GLuint load_png_texture_internal(const char *path, bool darken_border) {
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char *pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        fprintf(stderr, "Could not load texture: %s (%s)\n", path, stbi_failure_reason());
        return 0;
    }

    if (darken_border) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (x != 0 && y != 0 && x != width - 1 && y != height - 1) {
                    continue;
                }
                unsigned char *pixel = pixels + ((size_t)y * (size_t)width + (size_t)x) * 4;
                pixel[0] = (unsigned char)(pixel[0] * 0.55f);
                pixel[1] = (unsigned char)(pixel[1] * 0.55f);
                pixel[2] = (unsigned char)(pixel[2] * 0.55f);
            }
        }
    }

    GLuint texture_id = 0;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(pixels);
    return texture_id;
}

static GLuint load_png_texture(const char *path) {
    return load_png_texture_internal(path, true);
}

static void gltf_skip_whitespace(const char *json, size_t *cursor) {
    while (json[*cursor] == ' ' || json[*cursor] == '\t'
           || json[*cursor] == '\r' || json[*cursor] == '\n') {
        ++*cursor;
    }
}

static int gltf_parse_json_value(const char *json, size_t *cursor,
                                 GltfJsonToken *tokens, int capacity, int *count) {
    gltf_skip_whitespace(json, cursor);
    if (*count >= capacity) return -1;

    int token_index = (*count)++;
    GltfJsonToken *token = &tokens[token_index];
    token->start = *cursor;
    token->child_count = 0;
    char first = json[*cursor];
    if (first == '{' || first == '[') {
        char closing = first == '{' ? '}' : ']';
        token->type = first == '{' ? 'o' : 'a';
        ++*cursor;
        gltf_skip_whitespace(json, cursor);
        while (json[*cursor] != closing) {
            if (json[*cursor] == '\0') return -1;
            if (first == '{') {
                if (json[*cursor] != '"'
                    || gltf_parse_json_value(json, cursor, tokens, capacity, count) < 0) {
                    return -1;
                }
                ++token->child_count;
                gltf_skip_whitespace(json, cursor);
                if (json[(*cursor)++] != ':') return -1;
            }
            if (gltf_parse_json_value(json, cursor, tokens, capacity, count) < 0) return -1;
            ++token->child_count;
            gltf_skip_whitespace(json, cursor);
            if (json[*cursor] == closing) break;
            if (json[(*cursor)++] != ',') return -1;
            gltf_skip_whitespace(json, cursor);
        }
        ++*cursor;
        token->end = *cursor;
        return token_index;
    }

    if (first == '"') {
        token->type = 's';
        ++*cursor;
        token->start = *cursor;
        while (json[*cursor] != '\0' && json[*cursor] != '"') {
            if (json[*cursor] == '\\') {
                ++*cursor;
                if (json[*cursor] == '\0') return -1;
            }
            ++*cursor;
        }
        if (json[*cursor] != '"') return -1;
        token->end = *cursor;
        ++*cursor;
        return token_index;
    }

    token->type = 'p';
    while (json[*cursor] != '\0' && json[*cursor] != ',' && json[*cursor] != ']'
           && json[*cursor] != '}' && json[*cursor] != ' '
           && json[*cursor] != '\t' && json[*cursor] != '\r' && json[*cursor] != '\n') {
        ++*cursor;
    }
    token->end = *cursor;
    return token_index;
}

static int gltf_next_token(const GltfJsonToken *tokens, int token_count, int index) {
    int next = index + 1;
    if (tokens[index].type == 'o' || tokens[index].type == 'a') {
        while (next < token_count && tokens[next].start < tokens[index].end) ++next;
    }
    return next;
}

static bool gltf_token_equals(const char *json, const GltfJsonToken *token,
                              const char *text) {
    size_t length = strlen(text);
    return token->type == 's' && token->end - token->start == length
        && memcmp(json + token->start, text, length) == 0;
}

static int gltf_object_value(const char *json, const GltfJsonToken *tokens,
                             int token_count, int object, const char *key) {
    if (object < 0 || object >= token_count || tokens[object].type != 'o') return -1;
    int item = object + 1;
    while (item < token_count && tokens[item].start < tokens[object].end) {
        int value = item + 1;
        if (value >= token_count) return -1;
        if (gltf_token_equals(json, &tokens[item], key)) return value;
        item = gltf_next_token(tokens, token_count, value);
    }
    return -1;
}

static int gltf_array_value(const GltfJsonToken *tokens, int token_count,
                            int array, int index) {
    if (array < 0 || array >= token_count || tokens[array].type != 'a' || index < 0) return -1;
    int item = array + 1;
    for (int i = 0; i < index && item < token_count; ++i) {
        item = gltf_next_token(tokens, token_count, item);
    }
    return item < token_count && tokens[item].start < tokens[array].end ? item : -1;
}

static int gltf_array_count(const GltfJsonToken *tokens, int token_count, int array) {
    if (array < 0 || array >= token_count || tokens[array].type != 'a') return 0;
    int count = 0;
    for (int item = array + 1; item < token_count && tokens[item].start < tokens[array].end;
         item = gltf_next_token(tokens, token_count, item)) {
        ++count;
    }
    return count;
}

static bool gltf_token_int(const char *json, const GltfJsonToken *tokens,
                           int token_count, int token, int *value) {
    if (token < 0 || token >= token_count || tokens[token].type != 'p') return false;
    char text[32];
    size_t length = tokens[token].end - tokens[token].start;
    if (length == 0 || length >= sizeof text) return false;
    memcpy(text, json + tokens[token].start, length);
    text[length] = '\0';
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < 0 || parsed > 2147483647L) return false;
    *value = (int)parsed;
    return true;
}

static bool gltf_token_size(const char *json, const GltfJsonToken *tokens,
                            int token_count, int token, size_t *value) {
    if (token < 0 || token >= token_count || tokens[token].type != 'p') return false;
    char text[32];
    size_t length = tokens[token].end - tokens[token].start;
    if (length == 0 || length >= sizeof text) return false;
    memcpy(text, json + tokens[token].start, length);
    text[length] = '\0';
    char *end = NULL;
    unsigned long long parsed = strtoull(text, &end, 10);
    if (end == text || *end != '\0' || parsed > (unsigned long long)SIZE_MAX) return false;
    *value = (size_t)parsed;
    return true;
}

static bool gltf_token_float(const char *json, const GltfJsonToken *tokens,
                             int token_count, int token, float *value) {
    if (token < 0 || token >= token_count || tokens[token].type != 'p') return false;
    char text[48];
    size_t length = tokens[token].end - tokens[token].start;
    if (length == 0 || length >= sizeof text) return false;
    memcpy(text, json + tokens[token].start, length);
    text[length] = '\0';
    char *end = NULL;
    float parsed = strtof(text, &end);
    if (end == text || *end != '\0' || !isfinite(parsed)) return false;
    *value = parsed;
    return true;
}

static bool gltf_token_string(const char *json, const GltfJsonToken *tokens,
                              int token_count, int token, char *output, size_t capacity) {
    if (token < 0 || token >= token_count || tokens[token].type != 's') return false;
    size_t length = tokens[token].end - tokens[token].start;
    if (length == 0 || length >= capacity
        || memchr(json + tokens[token].start, '\\', length) != NULL) {
        return false;
    }
    memcpy(output, json + tokens[token].start, length);
    output[length] = '\0';
    return true;
}

static bool gltf_token_float_array(const char *json, const GltfJsonToken *tokens,
                                   int token_count, int array, float *values, int count) {
    if (array < 0 || array >= token_count || tokens[array].type != 'a'
        || gltf_array_count(tokens, token_count, array) != count) {
        return false;
    }
    for (int i = 0; i < count; ++i) {
        if (!gltf_token_float(json, tokens, token_count,
                              gltf_array_value(tokens, token_count, array, i), &values[i])) {
            return false;
        }
    }
    return true;
}

static unsigned char *gltf_decode_base64(const char *text, size_t length, size_t *decoded_size) {
    unsigned char *decoded = malloc((length / 4 + 1) * 3);
    if (decoded == NULL) return NULL;
    unsigned int accumulator = 0;
    int bits = 0;
    size_t output = 0;
    for (size_t i = 0; i < length; ++i) {
        unsigned char ch = (unsigned char)text[i];
        int value;
        if (ch >= 'A' && ch <= 'Z') value = ch - 'A';
        else if (ch >= 'a' && ch <= 'z') value = ch - 'a' + 26;
        else if (ch >= '0' && ch <= '9') value = ch - '0' + 52;
        else if (ch == '+') value = 62;
        else if (ch == '/') value = 63;
        else if (ch == '=') break;
        else {
            free(decoded);
            return NULL;
        }
        accumulator = (accumulator << 6) | (unsigned int)value;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            decoded[output++] = (unsigned char)(accumulator >> bits);
        }
    }
    *decoded_size = output;
    return decoded;
}

static unsigned char *gltf_load_uri_bytes(const char *json, const GltfJsonToken *tokens,
                                          int token_count, int uri_token, size_t *byte_count) {
    if (uri_token < 0 || uri_token >= token_count || tokens[uri_token].type != 's') {
        return NULL;
    }
    const char *uri_text = json + tokens[uri_token].start;
    size_t uri_length = tokens[uri_token].end - tokens[uri_token].start;
    if (uri_length >= 5 && memcmp(uri_text, "data:", 5) == 0) {
        const char *base64 = NULL;
        for (size_t i = 5; i + 8 <= uri_length; ++i) {
            if (memcmp(uri_text + i, ";base64,", 8) == 0) {
                base64 = uri_text + i + 8;
                break;
            }
        }
        if (base64 == NULL) return NULL;
        size_t encoded_length = (size_t)((json + tokens[uri_token].end) - base64);
        return gltf_decode_base64(base64, encoded_length, byte_count);
    }

    char uri[1024];
    if (!gltf_token_string(json, tokens, token_count, uri_token, uri, sizeof uri)) return NULL;

    FILE *file = fopen(uri, "rb");
    if (file == NULL) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long file_size = ftell(file);
    if (file_size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    unsigned char *bytes = malloc((size_t)file_size == 0 ? 1 : (size_t)file_size);
    if (bytes == NULL) {
        fclose(file);
        return NULL;
    }
    if (fread(bytes, 1, (size_t)file_size, file) != (size_t)file_size) {
        free(bytes);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *byte_count = (size_t)file_size;
    return bytes;
}

static int gltf_accessor_component_count(const char *json, const GltfJsonToken *tokens,
                                         int token_count, int type_token) {
    if (gltf_token_equals(json, &tokens[type_token], "SCALAR")) return 1;
    if (gltf_token_equals(json, &tokens[type_token], "VEC2")) return 2;
    if (gltf_token_equals(json, &tokens[type_token], "VEC3")) return 3;
    if (gltf_token_equals(json, &tokens[type_token], "VEC4")) return 4;
    (void)token_count;
    return 0;
}

static bool gltf_parse_accessor(const char *json, const GltfJsonToken *tokens, int token_count,
                                int object, GltfAccessor *accessor) {
    int view_token = gltf_object_value(json, tokens, token_count, object, "bufferView");
    int type_token = gltf_object_value(json, tokens, token_count, object, "type");
    int offset_token = gltf_object_value(json, tokens, token_count, object, "byteOffset");
    int count_token = gltf_object_value(json, tokens, token_count, object, "count");
    int component_token = gltf_object_value(json, tokens, token_count, object, "componentType");
    if (!gltf_token_int(json, tokens, token_count, view_token, &accessor->buffer_view)
        || !gltf_token_size(json, tokens, token_count, count_token, &accessor->count)
        || !gltf_token_int(json, tokens, token_count, component_token, &accessor->component_type)
        || type_token < 0) {
        return false;
    }
    accessor->byte_offset = 0;
    if (offset_token >= 0
        && !gltf_token_size(json, tokens, token_count, offset_token, &accessor->byte_offset)) {
        return false;
    }
    accessor->component_count =
        gltf_accessor_component_count(json, tokens, token_count, type_token);
    return accessor->component_count > 0;
}

static void generate_default_username(void) {
    snprintf(player_name, sizeof player_name, "Player%04u",
             (unsigned int)(rand() % 10000));
}

static void load_user_skin_selection(void) {
    bool regenerate = false;
    FILE *file = fopen(USER_CONFIG_PATH, "rb");
    if (file == NULL) {
        if (errno == ENOENT) {
            fprintf(stderr, "%s is missing; creating default settings\n", USER_CONFIG_PATH);
            generate_default_username();
            regenerate = true;
        } else {
            fprintf(stderr, "Unable to open %s: %s\n", USER_CONFIG_PATH, strerror(errno));
            return;
        }
    } else {
        char json[4096];
        size_t length = fread(json, 1, sizeof json - 1, file);
        bool read_error = ferror(file) != 0;
        bool too_large = !feof(file);
        fclose(file);
        if (read_error || too_large) {
            fprintf(stderr, "%s %s; regenerating default settings\n", USER_CONFIG_PATH,
                    too_large ? "is too large" : "could not be read");
            generate_default_username();
            regenerate = true;
        } else {
            json[length] = '\0';
            GltfJsonToken tokens[256];
            int token_count = 0;
            size_t cursor = 0;
            int root = gltf_parse_json_value(json, &cursor, tokens,
                                             (int)(sizeof tokens / sizeof tokens[0]),
                                             &token_count);
            gltf_skip_whitespace(json, &cursor);
            if (root < 0 || tokens[root].type != 'o' || cursor != length) {
                fprintf(stderr, "%s is invalid; regenerating default settings\n",
                        USER_CONFIG_PATH);
                generate_default_username();
                regenerate = true;
            } else {
                int skin_token = gltf_object_value(json, tokens, token_count, root, "skin");
                char path[sizeof selected_skin_path];
                if (skin_token < 0) {
                    regenerate = true;
                } else if (gltf_token_string(json, tokens, token_count, skin_token,
                                             path, sizeof path)
                           && strncmp(path, "skin/", 5) == 0
                           && strstr(path, "..") == NULL && strchr(path, '\\') == NULL
                           && strchr(path, ':') == NULL) {
                    snprintf(selected_skin_path, sizeof selected_skin_path, "%s", path);
                } else {
                    fprintf(stderr, "%s has an invalid skin path; using %s\n",
                            USER_CONFIG_PATH, DEFAULT_SKIN_PATH);
                    regenerate = true;
                }

                int username_token = gltf_object_value(json, tokens, token_count, root,
                                                       "username");
                char name[sizeof player_name];
                bool valid_name = gltf_token_string(json, tokens, token_count, username_token,
                                                    name, sizeof name);
                size_t name_length = valid_name ? strlen(name) : 0;
                if (!valid_name || name_length == 0 || name_length > 24) {
                    generate_default_username();
                    regenerate = true;
                } else {
                    for (size_t i = 0; i < name_length; ++i) {
                        unsigned char c = (unsigned char)name[i];
                        if (c < 33 || c > 126 || c == '"' || c == '\\') valid_name = false;
                    }
                    if (valid_name) {
                        snprintf(player_name, sizeof player_name, "%s", name);
                    } else {
                        generate_default_username();
                        regenerate = true;
                    }
                }

                int host_token = gltf_object_value(json, tokens, token_count, root, "server");
                char host[sizeof server_host];
                bool valid_host = gltf_token_string(json, tokens, token_count, host_token,
                                                    host, sizeof host);
                size_t host_length = valid_host ? strlen(host) : 0;
                if (host_length == 0) {
                    regenerate = true;
                } else {
                    for (size_t i = 0; i < host_length; ++i) {
                        unsigned char c = (unsigned char)host[i];
                        if (c <= 32 || c >= 127 || c == '"' || c == '\\') valid_host = false;
                    }
                    if (valid_host) {
                        snprintf(server_host, sizeof server_host, "%s", host);
                    } else {
                        regenerate = true;
                    }
                }

                int port_token = gltf_object_value(json, tokens, token_count, root, "port");
                int port = 0;
                if (gltf_token_int(json, tokens, token_count, port_token, &port)
                    && port > 0 && port <= 65535) {
                    server_port = (unsigned short)port;
                } else {
                    regenerate = true;
                }
                int id_token = gltf_object_value(json, tokens, token_count, root, "id");
                int id = 0;
                if (id_token < 0) {
                    regenerate = true;
                } else if (gltf_token_int(json, tokens, token_count, id_token, &id) && id >= 0) {
                    local_player_id = id;
                } else {
                    regenerate = true;
                }
            }
        }
    }

    if (regenerate) {
        if (save_user_config()) {
            fprintf(stderr, "Regenerated missing fields in %s\n", USER_CONFIG_PATH);
        } else {
            fprintf(stderr, "Could not regenerate %s; using defaults for this session\n",
                    USER_CONFIG_PATH);
        }
    }
}

static bool load_player_model(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Unable to open player model %s\n", path);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        fprintf(stderr, "Unable to seek player model %s\n", path);
        return false;
    }
    long file_size = ftell(file);
    if (file_size <= 0 || file_size > 16 * 1024 * 1024 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        fprintf(stderr, "Player model %s has an unsupported file size\n", path);
        return false;
    }
    char *json = malloc((size_t)file_size + 1);
    GltfJsonToken *tokens = malloc(((size_t)file_size + 1) * sizeof *tokens);
    if (json == NULL || tokens == NULL) {
        free(json);
        free(tokens);
        fclose(file);
        fprintf(stderr, "Unable to allocate memory for player model %s\n", path);
        return false;
    }
    bool read_ok = fread(json, 1, (size_t)file_size, file) == (size_t)file_size;
    fclose(file);
    if (!read_ok) {
        free(json);
        free(tokens);
        fprintf(stderr, "Unable to read player model %s\n", path);
        return false;
    }
    json[file_size] = '\0';
    int token_count = 0;
    size_t cursor = 0;
    int root = gltf_parse_json_value(json, &cursor, tokens, (int)file_size + 1, &token_count);
    gltf_skip_whitespace(json, &cursor);
    if (root < 0 || cursor != (size_t)file_size || tokens[root].type != 'o') {
        fprintf(stderr, "Player model %s contains invalid JSON\n", path);
        free(json);
        free(tokens);
        return false;
    }

    int buffers = gltf_object_value(json, tokens, token_count, root, "buffers");
    int buffer_object = gltf_array_value(tokens, token_count, buffers, 0);
    int buffer_uri = gltf_object_value(json, tokens, token_count, buffer_object, "uri");
    size_t buffer_size = 0;
    unsigned char *buffer = gltf_load_uri_bytes(json, tokens, token_count, buffer_uri,
                                                 &buffer_size);
    if (buffer == NULL) {
        fprintf(stderr, "Unable to load player model buffer referenced by %s\n", path);
        free(json);
        free(tokens);
        return false;
    }

    GltfBufferView views[GLTF_MAX_ACCESSORS];
    int views_count = 0;
    int views_token = gltf_object_value(json, tokens, token_count, root, "bufferViews");
    int view_total = gltf_array_count(tokens, token_count, views_token);
    bool valid = view_total > 0 && view_total <= GLTF_MAX_ACCESSORS;
    for (int i = 0; valid && i < view_total; ++i) {
        int object = gltf_array_value(tokens, token_count, views_token, i);
        int buffer_index = 0;
        size_t offset = 0;
        size_t length = 0;
        size_t stride = 0;
        int buffer_index_token = gltf_object_value(json, tokens, token_count, object, "buffer");
        int offset_token = gltf_object_value(json, tokens, token_count, object, "byteOffset");
        int length_token = gltf_object_value(json, tokens, token_count, object, "byteLength");
        int stride_token = gltf_object_value(json, tokens, token_count, object, "byteStride");
        if ((buffer_index_token >= 0
             && !gltf_token_int(json, tokens, token_count, buffer_index_token, &buffer_index))
            || (offset_token >= 0
                && !gltf_token_size(json, tokens, token_count, offset_token, &offset))
            || !gltf_token_size(json, tokens, token_count, length_token, &length)
            || (stride_token >= 0
                && !gltf_token_size(json, tokens, token_count, stride_token, &stride))
            || buffer_index != 0 || offset > buffer_size || length > buffer_size - offset) {
            valid = false;
            break;
        }
        views[i] = (GltfBufferView){ offset, length, stride };
        ++views_count;
    }

    player_model_accessor_count = 0;
    int accessors_token = gltf_object_value(json, tokens, token_count, root, "accessors");
    int accessor_total = gltf_array_count(tokens, token_count, accessors_token);
    if (accessor_total <= 0 || accessor_total > GLTF_MAX_ACCESSORS) valid = false;
    for (int i = 0; valid && i < accessor_total; ++i) {
        int object = gltf_array_value(tokens, token_count, accessors_token, i);
        if (!gltf_parse_accessor(json, tokens, token_count, object, &player_model_accessors[i])
            || player_model_accessors[i].buffer_view >= views_count
            || player_model_accessors[i].byte_offset > views[player_model_accessors[i].buffer_view].length) {
            valid = false;
            break;
        }
        ++player_model_accessor_count;
    }

    player_model_primitive_count = 0;
    player_model_mesh_total = 0;
    int meshes_token = gltf_object_value(json, tokens, token_count, root, "meshes");
    int mesh_total = gltf_array_count(tokens, token_count, meshes_token);
    if (mesh_total <= 0 || mesh_total > GLTF_MAX_MESHES) valid = false;
    for (int i = 0; valid && i < mesh_total; ++i) {
        int mesh = gltf_array_value(tokens, token_count, meshes_token, i);
        int primitive_list = gltf_object_value(json, tokens, token_count, mesh, "primitives");
        int primitive_total = gltf_array_count(tokens, token_count, primitive_list);
        if (primitive_total <= 0 || primitive_total > GLTF_MAX_PRIMITIVES - player_model_primitive_count) {
            valid = false;
            break;
        }
        player_model_mesh_first[i] = player_model_primitive_count;
        player_model_mesh_count[i] = primitive_total;
        for (int j = 0; j < primitive_total; ++j) {
            int primitive = gltf_array_value(tokens, token_count, primitive_list, j);
            int attributes = gltf_object_value(json, tokens, token_count, primitive, "attributes");
            int mode = 4;
            int mode_token = gltf_object_value(json, tokens, token_count, primitive, "mode");
            int position = gltf_object_value(json, tokens, token_count, attributes, "POSITION");
            int texcoord = gltf_object_value(json, tokens, token_count, attributes, "TEXCOORD_0");
            int indices = gltf_object_value(json, tokens, token_count, primitive, "indices");
            GltfPrimitive parsed = { -1, -1, -1 };
            if ((mode_token >= 0 && !gltf_token_int(json, tokens, token_count, mode_token, &mode))
                || mode != 4
                || !gltf_token_int(json, tokens, token_count, position, &parsed.position_accessor)
                || !gltf_token_int(json, tokens, token_count, texcoord, &parsed.texcoord_accessor)
                || parsed.position_accessor >= player_model_accessor_count
                || parsed.texcoord_accessor >= player_model_accessor_count
                || (indices >= 0
                    && !gltf_token_int(json, tokens, token_count, indices, &parsed.index_accessor))
                || (parsed.index_accessor >= 0
                    && parsed.index_accessor >= player_model_accessor_count)) {
                valid = false;
                break;
            }
            player_model_primitives[player_model_primitive_count++] = parsed;
        }
        ++player_model_mesh_total;
    }

    player_model_node_count = 0;
    int nodes_token = gltf_object_value(json, tokens, token_count, root, "nodes");
    int node_total = gltf_array_count(tokens, token_count, nodes_token);
    if (node_total <= 0 || node_total > GLTF_MAX_NODES) valid = false;
    for (int i = 0; valid && i < node_total; ++i) {
        int object = gltf_array_value(tokens, token_count, nodes_token, i);
        GltfNode node = { .translation = {0.0f, 0.0f, 0.0f},
                          .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
                          .scale = {1.0f, 1.0f, 1.0f},
                          .mesh = -1, .child_count = 0 };
        int name_token = gltf_object_value(json, tokens, token_count, object, "name");
        int translation = gltf_object_value(json, tokens, token_count, object, "translation");
        int rotation = gltf_object_value(json, tokens, token_count, object, "rotation");
        int scale = gltf_object_value(json, tokens, token_count, object, "scale");
        int mesh = gltf_object_value(json, tokens, token_count, object, "mesh");
        int children = gltf_object_value(json, tokens, token_count, object, "children");
        if ((name_token >= 0
             && !gltf_token_string(json, tokens, token_count, name_token,
                                   node.name, sizeof node.name))
            || (translation >= 0 && !gltf_token_float_array(json, tokens, token_count, translation,
                                                          node.translation, 3))
            || (rotation >= 0 && !gltf_token_float_array(json, tokens, token_count, rotation,
                                                         node.rotation, 4))
            || (scale >= 0 && !gltf_token_float_array(json, tokens, token_count, scale,
                                                       node.scale, 3))
            || (mesh >= 0 && (!gltf_token_int(json, tokens, token_count, mesh, &node.mesh)
                              || node.mesh >= player_model_mesh_total))) {
            valid = false;
            break;
        }
        node.child_count = gltf_array_count(tokens, token_count, children);
        if (node.child_count > (int)(sizeof node.children / sizeof node.children[0])) {
            valid = false;
            break;
        }
        for (int child = 0; child < node.child_count; ++child) {
            if (!gltf_token_int(json, tokens, token_count,
                                gltf_array_value(tokens, token_count, children, child),
                                &node.children[child])
                || node.children[child] >= node_total) {
                valid = false;
                break;
            }
        }
        player_model_nodes[player_model_node_count++] = node;
    }

    player_model_root_count = 0;
    int scenes_token = gltf_object_value(json, tokens, token_count, root, "scenes");
    int scene_token = gltf_object_value(json, tokens, token_count, root, "scene");
    int scene_index = 0;
    if (scene_token >= 0 && !gltf_token_int(json, tokens, token_count, scene_token, &scene_index)) {
        valid = false;
    }
    int scene = gltf_array_value(tokens, token_count, scenes_token, scene_index);
    int scene_nodes = gltf_object_value(json, tokens, token_count, scene, "nodes");
    int root_total = gltf_array_count(tokens, token_count, scene_nodes);
    if (root_total <= 0 || root_total > GLTF_MAX_SCENE_ROOTS) valid = false;
    for (int i = 0; valid && i < root_total; ++i) {
        if (!gltf_token_int(json, tokens, token_count,
                            gltf_array_value(tokens, token_count, scene_nodes, i),
                            &player_model_roots[i])
            || player_model_roots[i] >= player_model_node_count) {
            valid = false;
            break;
        }
        ++player_model_root_count;
    }

    int images_token = gltf_object_value(json, tokens, token_count, root, "images");
    int image = gltf_array_value(tokens, token_count, images_token, 0);
    int image_uri = gltf_object_value(json, tokens, token_count, image, "uri");
    char texture_path[1024];
    if (valid && gltf_token_string(json, tokens, token_count, image_uri,
                                   texture_path, sizeof texture_path)) {
        snprintf(texture_path, sizeof texture_path, "%s", selected_skin_path);
        GLuint texture = load_png_texture_internal(texture_path, false);
        if (texture != 0) {
            if (player_model_texture != 0) glDeleteTextures(1, &player_model_texture);
            player_model_texture = texture;
        } else {
            valid = false;
        }
    } else {
        valid = false;
    }

    if (valid) {
        free(player_model_buffer);
        player_model_buffer = buffer;
        player_model_buffer_size = buffer_size;
        buffer = NULL;
        memcpy(player_model_views, views, (size_t)views_count * sizeof views[0]);
    } else {
        fprintf(stderr, "Player model %s uses unsupported or invalid glTF data\n", path);
    }
    free(buffer);
    free(json);
    free(tokens);
    return valid;
}

static void draw_cube(float x, float y, float z, unsigned char block) {
    GLuint texture = 0;
    if (block == BLOCK_GRASS) texture = grass_texture;
    if (block == BLOCK_DIRT) texture = dirt_texture;
    if (block == BLOCK_STONE) texture = stone_texture;

    if (texture != 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture);
        glColor3f(1.0f, 1.0f, 1.0f);
    } else {
        glDisable(GL_TEXTURE_2D);
        block_color(block);
    }

    if (!is_solid((int)x, (int)y, (int)z - 1)) {
        draw_face(x, y, z, x + 1.0f, y, z, x + 1.0f, y + 1.0f, z, x, y + 1.0f, z,
                  0, 0, 1, 0, 1, 1, 0, 1);
    }
    if (!is_solid((int)x, (int)y, (int)z + 1)) {
        draw_face(x, y, z + 1.0f, x, y + 1.0f, z + 1.0f,
                  x + 1.0f, y + 1.0f, z + 1.0f, x + 1.0f, y, z + 1.0f,
                  0, 0, 1, 0, 1, 1, 0, 1);
    }
    if (!is_solid((int)x, (int)y - 1, (int)z)) {
        draw_face(x, y, z, x + 1.0f, y, z, x + 1.0f, y, z + 1.0f, x, y, z + 1.0f,
                  0, 0, 1, 0, 1, 1, 0, 1);
    }
    if (!is_solid((int)x, (int)y + 1, (int)z)) {
        draw_face(x, y + 1.0f, z + 1.0f, x + 1.0f, y + 1.0f, z + 1.0f,
                  x + 1.0f, y + 1.0f, z, x, y + 1.0f, z,
                  0, 0, 1, 0, 1, 1, 0, 1);
    }
    if (!is_solid((int)x - 1, (int)y, (int)z)) {
        draw_face(x, y, z, x, y, z + 1.0f, x, y + 1.0f, z + 1.0f, x, y + 1.0f, z,
                  0, 0, 1, 0, 1, 1, 0, 1);
    }
    if (!is_solid((int)x + 1, (int)y, (int)z)) {
        draw_face(x + 1.0f, y, z + 1.0f, x + 1.0f, y, z,
                  x + 1.0f, y + 1.0f, z, x + 1.0f, y + 1.0f, z + 1.0f,
                  0, 0, 1, 0, 1, 1, 0, 1);
    }

    glDisable(GL_TEXTURE_2D);
}

static Vec3 get_camera_position(Player *player) {
    Vec3 forward = vec3(
        cosf(player->pitch) * sinf(player->yaw),
        sinf(player->pitch),
        cosf(player->pitch) * cosf(player->yaw)
    );
    Vec3 camera_pos = player->pos;
    camera_pos.y += PLAYER_EYE_HEIGHT;

    if (camera_mode != CAMERA_FIRST_PERSON) {
        Vec3 back = mul_vec3(normalize_vec3(forward), -5.5f);
        if (camera_mode == CAMERA_THIRD_PERSON_FRONT) {
            back = mul_vec3(back, -1.0f);
        }
        camera_pos = add_vec3(camera_pos, back);
        camera_pos.y += 1.0f;
    }

    return camera_pos;
}

static Vec3 clip_third_person_camera(const Player *player, Vec3 camera_pos) {
    Vec3 eye = player->pos;
    eye.y += PLAYER_EYE_HEIGHT;
    Vec3 ray = sub_vec3(camera_pos, eye);
    float distance = length_vec3(ray);
    if (distance <= 0.001f) return camera_pos;

    int steps = (int)ceilf(distance / 0.1f);
    Vec3 last_clear = eye;
    for (int step = 1; step <= steps; ++step) {
        float progress = (float)step / (float)steps;
        Vec3 sample = add_vec3(eye, mul_vec3(ray, progress));
        if (is_solid((int)floorf(sample.x), (int)floorf(sample.y),
                     (int)floorf(sample.z))) {
            return last_clear;
        }
        last_clear = sample;
    }
    return camera_pos;
}

static size_t gltf_component_size(int component_type) {
    switch (component_type) {
        case 5121: return 1;
        case 5123: return 2;
        case 5125:
        case 5126: return 4;
        default: return 0;
    }
}

static bool gltf_accessor_data(int accessor_index, int expected_components,
                               int expected_component_type, const unsigned char **data,
                               size_t *stride) {
    if (accessor_index < 0 || accessor_index >= player_model_accessor_count) return false;
    const GltfAccessor *accessor = &player_model_accessors[accessor_index];
    if (accessor->buffer_view < 0 || accessor->buffer_view >= GLTF_MAX_ACCESSORS
        || accessor->component_count != expected_components
        || (expected_component_type != 0 && accessor->component_type != expected_component_type)) {
        return false;
    }
    size_t component_size = gltf_component_size(accessor->component_type);
    if (component_size == 0 || (size_t)expected_components > SIZE_MAX / component_size) {
        return false;
    }
    size_t element_size = component_size * (size_t)expected_components;
    const GltfBufferView *view = &player_model_views[accessor->buffer_view];
    size_t element_stride = view->stride == 0 ? element_size : view->stride;
    if (element_stride < element_size || accessor->byte_offset > view->length
        || accessor->count == 0
        || accessor->byte_offset > SIZE_MAX - element_size
        || accessor->count - 1
            > (SIZE_MAX - accessor->byte_offset - element_size) / element_stride) {
        return false;
    }
    size_t last_byte = accessor->byte_offset
        + (accessor->count - 1) * element_stride + element_size;
    if (last_byte > view->length || view->offset > player_model_buffer_size
        || last_byte > player_model_buffer_size - view->offset) {
        return false;
    }
    *data = player_model_buffer + view->offset + accessor->byte_offset;
    *stride = element_stride;
    return true;
}

static bool gltf_read_index(const unsigned char *data, int component_type,
                            unsigned int *value) {
    switch (component_type) {
        case 5121:
            *value = data[0];
            return true;
        case 5123: {
            uint16_t parsed;
            memcpy(&parsed, data, sizeof parsed);
            *value = parsed;
            return true;
        }
        case 5125: {
            uint32_t parsed;
            memcpy(&parsed, data, sizeof parsed);
            *value = parsed;
            return true;
        }
        default:
            return false;
    }
}

static void draw_player_model_mesh(int mesh_index) {
    if (mesh_index < 0 || mesh_index >= player_model_mesh_total) return;
    int first = player_model_mesh_first[mesh_index];
    int count = player_model_mesh_count[mesh_index];
    for (int i = 0; i < count; ++i) {
        const GltfPrimitive *primitive = &player_model_primitives[first + i];
        const unsigned char *positions;
        const unsigned char *texcoords;
        size_t position_stride;
        size_t texcoord_stride;
        if (!gltf_accessor_data(primitive->position_accessor, 3, 5126,
                                &positions, &position_stride)
            || !gltf_accessor_data(primitive->texcoord_accessor, 2, 5126,
                                   &texcoords, &texcoord_stride)) {
            continue;
        }

        const GltfAccessor *position_accessor =
            &player_model_accessors[primitive->position_accessor];
        const GltfAccessor *index_accessor = primitive->index_accessor >= 0
            ? &player_model_accessors[primitive->index_accessor] : NULL;
        const unsigned char *indices = NULL;
        size_t index_stride = 0;
        size_t vertex_count = position_accessor->count;
        if (index_accessor != NULL) {
            if (index_accessor->component_count != 1
                || !gltf_accessor_data(primitive->index_accessor, 1, 0,
                                       &indices, &index_stride)
                || (index_accessor->component_type != 5121
                    && index_accessor->component_type != 5123
                    && index_accessor->component_type != 5125)) {
                continue;
            }
            vertex_count = index_accessor->count;
        }

        glBegin(GL_TRIANGLES);
        for (size_t vertex = 0; vertex < vertex_count; ++vertex) {
            unsigned int vertex_index = (unsigned int)vertex;
            if (indices != NULL
                && !gltf_read_index(indices + vertex * index_stride,
                                    index_accessor->component_type, &vertex_index)) {
                continue;
            }
            if (vertex_index >= position_accessor->count
                || vertex_index >= player_model_accessors[primitive->texcoord_accessor].count) {
                continue;
            }
            float uv[2];
            float position[3];
            memcpy(position, positions + (size_t)vertex_index * position_stride, sizeof position);
            memcpy(uv, texcoords + (size_t)vertex_index * texcoord_stride, sizeof uv);
            glTexCoord2f(uv[0], uv[1]);
            glVertex3f(position[0], position[1], position[2]);
        }
        glEnd();
    }
}

static void draw_player_model_node(int node_index, int depth, float walk_swing,
                                   bool airborne) {
    if (node_index < 0 || node_index >= player_model_node_count
        || depth > player_model_node_count) {
        return;
    }
    const GltfNode *node = &player_model_nodes[node_index];
    glPushMatrix();
    glTranslatef(node->translation[0], node->translation[1], node->translation[2]);
    float quaternion_w = clamp_f(node->rotation[3], -1.0f, 1.0f);
    float angle = 2.0f * acosf(quaternion_w);
    float axis_length = sqrtf(node->rotation[0] * node->rotation[0]
                              + node->rotation[1] * node->rotation[1]
                              + node->rotation[2] * node->rotation[2]);
    if (axis_length > 0.00001f && angle > 0.00001f) {
        glRotatef(angle * (180.0f / (float)M_PI),
                  node->rotation[0] / axis_length,
                  node->rotation[1] / axis_length,
                  node->rotation[2] / axis_length);
    }
    if (node->child_count > 0) {
        float limb_angle = 0.0f;
        if (strcmp(node->name, "Right Arm") == 0) {
            limb_angle = airborne ? -0.95f : walk_swing;
        } else if (strcmp(node->name, "Left Arm") == 0) {
            limb_angle = airborne ? -0.95f : -walk_swing;
        } else if (strcmp(node->name, "Right Leg") == 0) {
            limb_angle = airborne ? 0.35f : -walk_swing;
        } else if (strcmp(node->name, "Left Leg") == 0) {
            limb_angle = airborne ? -0.25f : walk_swing;
        }
        if (limb_angle != 0.0f) {
            glRotatef(limb_angle * (180.0f / (float)M_PI), 1.0f, 0.0f, 0.0f);
        }
    }
    glScalef(node->scale[0], node->scale[1], node->scale[2]);
    if (node->mesh >= 0) draw_player_model_mesh(node->mesh);
    for (int i = 0; i < node->child_count; ++i) {
        draw_player_model_node(node->children[i], depth + 1, walk_swing, airborne);
    }
    glPopMatrix();
}

static void draw_player_model(const Player *player, bool local_player) {
    if ((local_player && camera_mode == CAMERA_FIRST_PERSON) || player_model_buffer == NULL
        || player_model_texture == 0) {
        return;
    }

    glPushMatrix();
    float bob = player->on_ground
        ? fabsf(sinf(player->animation_phase * 2.0f)) * 0.055f * player->walk_blend
        : 0.0f;
    float walk_swing = sinf(player->animation_phase) * 0.68f * player->walk_blend;
    glTranslatef(player->pos.x, player->pos.y + bob, player->pos.z);
    glRotatef((player->yaw + (float)M_PI) * (180.0f / (float)M_PI),
              0.0f, 1.0f, 0.0f);
    glScalef(0.88f, 0.88f, 0.88f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, player_model_texture);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.05f);
    glDisable(GL_CULL_FACE);
    glColor3f(1.0f, 1.0f, 1.0f);
    for (int i = 0; i < player_model_root_count; ++i) {
        draw_player_model_node(player_model_roots[i], 0, walk_swing, !player->on_ground);
    }
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glPopMatrix();
}

static void draw_remote_players(void) {
    for (int i = 0; i < MAX_REMOTE_PLAYERS; ++i) {
        if (remote_players[i].active) {
            draw_player_model(&remote_players[i].player, false);
        }
    }
}

static void draw_dropped_item(const DroppedItem *item) {
    float half = 0.16f;
    float x0 = item->pos.x - half;
    float x1 = item->pos.x + half;
    float y0 = item->pos.y;
    float y1 = item->pos.y + half * 2.0f;
    float z0 = item->pos.z - half;
    float z1 = item->pos.z + half;
    block_color(item->block);
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x0, y1, z0);
    glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y0, z1);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0);
    glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1); glVertex3f(x1, y0, z1);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y0, z0);
    glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0); glVertex3f(x0, y1, z0);
    glEnd();
}

static void draw_first_person_arm(int width, int height) {
    if (camera_mode != CAMERA_FIRST_PERSON || inventory_open || paused) return;

    float center_x = (float)width - 260.0f;
    float bottom_y = (float)height;
    float swing_progress = (ARM_SWING_DURATION - arm_swing_time) / ARM_SWING_DURATION;
    float swing = arm_swing_time > 0.0f ? sinf(swing_progress * (float)M_PI) : 0.0f;
    glDisable(GL_TEXTURE_2D);
    glPushMatrix();
    glTranslatef(-42.0f * swing, -58.0f * swing, 0.0f);
    glColor3f(0.94f, 0.94f, 0.94f);
    glBegin(GL_QUADS);
    glVertex2f(center_x + 148.0f, bottom_y);
    glVertex2f(center_x + 198.0f, bottom_y);
    glVertex2f(center_x + 132.0f, bottom_y - 132.0f);
    glVertex2f(center_x + 92.0f, bottom_y - 116.0f);

    glColor3f(0.78f, 0.78f, 0.78f);
    glVertex2f(center_x + 198.0f, bottom_y);
    glVertex2f(center_x + 213.0f, bottom_y);
    glVertex2f(center_x + 147.0f, bottom_y - 132.0f);
    glVertex2f(center_x + 132.0f, bottom_y - 132.0f);

    glColor3f(0.94f, 0.94f, 0.94f);
    glVertex2f(center_x + 92.0f, bottom_y - 116.0f);
    glVertex2f(center_x + 132.0f, bottom_y - 132.0f);
    glVertex2f(center_x + 117.0f, bottom_y - 170.0f);
    glVertex2f(center_x + 77.0f, bottom_y - 154.0f);

    glColor3f(0.82f, 0.82f, 0.82f);
    glVertex2f(center_x + 132.0f, bottom_y - 132.0f);
    glVertex2f(center_x + 147.0f, bottom_y - 132.0f);
    glVertex2f(center_x + 132.0f, bottom_y - 170.0f);
    glVertex2f(center_x + 117.0f, bottom_y - 170.0f);
    glEnd();
    glPopMatrix();
}

static void draw_ui_rect(float x, float y, float width, float height,
                         float red, float green, float blue, float alpha) {
    glColor4f(red, green, blue, alpha);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

static void draw_ui_digit(int digit, float x, float y, float scale) {
    static const unsigned char glyphs[10][5] = {
        {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7},
        {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7},
        {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7},
        {7, 5, 7, 1, 7}
    };
    if (digit < 0 || digit > 9) return;

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    for (int row = 0; row < 5; ++row) {
        for (int column = 0; column < 3; ++column) {
            if (glyphs[digit][row] & (1u << (2 - column))) {
                float px = x + (float)column * scale;
                float py = y + (float)row * scale;
                glVertex2f(px, py);
                glVertex2f(px + scale, py);
                glVertex2f(px + scale, py + scale);
                glVertex2f(px, py + scale);
            }
        }
    }
    glEnd();
}

#ifndef _WIN32
static uint32_t next_utf8_codepoint(const unsigned char **cursor) {
    const unsigned char *text = *cursor;
    uint32_t codepoint;
    size_t continuation_count;
    if (text[0] < 0x80) {
        *cursor = text + 1;
        return text[0];
    }
    if ((text[0] & 0xe0) == 0xc0) {
        codepoint = text[0] & 0x1f;
        continuation_count = 1;
    } else if ((text[0] & 0xf0) == 0xe0) {
        codepoint = text[0] & 0x0f;
        continuation_count = 2;
    } else if ((text[0] & 0xf8) == 0xf0) {
        codepoint = text[0] & 0x07;
        continuation_count = 3;
    } else {
        *cursor = text + 1;
        return 0xfffd;
    }
    ++text;
    for (size_t i = 0; i < continuation_count; ++i, ++text) {
        if ((*text & 0xc0) != 0x80) {
            *cursor = *cursor + 1;
            return 0xfffd;
        }
        codepoint = (codepoint << 6) | (*text & 0x3f);
    }
    *cursor = text;
    return codepoint;
}

static bool ensure_chat_font(void) {
    static const char *font_paths[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    };
    if (chat_font_face != NULL) return true;
    if (chat_font_attempted) return false;
    chat_font_attempted = true;
    if (FT_Init_FreeType(&chat_font_library) != 0) {
        fprintf(stderr, "Unable to initialize FreeType for chat text\n");
        return false;
    }
    for (size_t i = 0; i < sizeof font_paths / sizeof font_paths[0]; ++i) {
        if (access(font_paths[i], R_OK) == 0
            && FT_New_Face(chat_font_library, font_paths[i], 0, &chat_font_face) == 0) {
            return true;
        }
    }
    fprintf(stderr, "No supported chat font found; install Noto Sans CJK or DejaVu Sans\n");
    FT_Done_FreeType(chat_font_library);
    chat_font_library = NULL;
    return false;
}
#endif

static void draw_ui_text_at(const char *text, float x, float y, float scale) {
    bool has_non_ascii = false;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p >= 0x80) {
            has_non_ascii = true;
            break;
        }
    }
    if (has_non_ascii) {
#ifdef _WIN32
        WCHAR wide_text[CHAT_INPUT_CAPACITY * 2];
        int wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1,
                                               wide_text, (int)(sizeof wide_text / sizeof wide_text[0]));
        if (wide_length > 0) {
            int font_height = (int)lroundf(scale * 7.0f);
            if (font_height < 8) font_height = 8;
            if (font_height >= CHAT_UTF8_BUFFER_HEIGHT) font_height = CHAT_UTF8_BUFFER_HEIGHT - 2;
            if (chat_text_dc == NULL) {
                chat_text_dc = CreateCompatibleDC(NULL);
                if (chat_text_dc != NULL) {
                    BITMAPINFO bitmap_info;
                    memset(&bitmap_info, 0, sizeof bitmap_info);
                    bitmap_info.bmiHeader.biSize = sizeof bitmap_info.bmiHeader;
                    bitmap_info.bmiHeader.biWidth = CHAT_UTF8_BUFFER_WIDTH;
                    bitmap_info.bmiHeader.biHeight = -CHAT_UTF8_BUFFER_HEIGHT;
                    bitmap_info.bmiHeader.biPlanes = 1;
                    bitmap_info.bmiHeader.biBitCount = 32;
                    bitmap_info.bmiHeader.biCompression = BI_RGB;
                    void *bitmap_pixels = NULL;
                    chat_text_bitmap = CreateDIBSection(chat_text_dc, &bitmap_info, DIB_RGB_COLORS,
                                                         &bitmap_pixels, NULL, 0);
                    if (chat_text_bitmap != NULL && bitmap_pixels != NULL) {
                        chat_text_old_bitmap = SelectObject(chat_text_dc, chat_text_bitmap);
                        chat_text_pixels = bitmap_pixels;
                    }
                }
            }
            if (chat_text_dc != NULL && chat_text_bitmap != NULL) {
                if (chat_text_font == NULL || chat_text_font_height != font_height) {
                    if (chat_text_font != NULL) {
                        SelectObject(chat_text_dc, GetStockObject(SYSTEM_FONT));
                        DeleteObject(chat_text_font);
                        chat_text_font = NULL;
                    }
                    chat_text_font = CreateFontW(-font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                                                 FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                                 CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                                 DEFAULT_PITCH | FF_DONTCARE, L"Yu Gothic UI");
                    if (chat_text_font != NULL) {
                        SelectObject(chat_text_dc, chat_text_font);
                        chat_text_font_height = font_height;
                    }
                }
                memset(chat_text_pixels, 0,
                       (size_t)CHAT_UTF8_BUFFER_WIDTH * CHAT_UTF8_BUFFER_HEIGHT * 4);
                SetBkMode(chat_text_dc, TRANSPARENT);
                SetTextColor(chat_text_dc, RGB(255, 255, 255));
                TextOutW(chat_text_dc, 1, 0, wide_text, wide_length - 1);
                SIZE text_size;
                if (GetTextExtentPoint32W(chat_text_dc, wide_text, wide_length - 1, &text_size)) {
                    int text_width = text_size.cx + 2;
                    if (text_width > CHAT_UTF8_BUFFER_WIDTH) text_width = CHAT_UTF8_BUFFER_WIDTH;
                    for (int row = 0; row < font_height + 2; ++row) {
                        for (int column = 0; column < text_width; ++column) {
                            size_t index = ((size_t)row * CHAT_UTF8_BUFFER_WIDTH + column) * 4;
                            unsigned char intensity = chat_text_pixels[index + 2];
                            chat_text_rgba[index] = 255;
                            chat_text_rgba[index + 1] = 255;
                            chat_text_rgba[index + 2] = 255;
                            chat_text_rgba[index + 3] = intensity;
                        }
                    }
                    if (chat_text_texture == 0) glGenTextures(1, &chat_text_texture);
                    glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT);
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, chat_text_texture);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, text_width, font_height + 2, 0,
                                 GL_RGBA, GL_UNSIGNED_BYTE, chat_text_rgba);
                    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                    glBegin(GL_QUADS);
                    glTexCoord2f(0.0f, 0.0f); glVertex2f(x, y);
                    glTexCoord2f(1.0f, 0.0f); glVertex2f(x + (float)text_width, y);
                    glTexCoord2f(1.0f, 1.0f); glVertex2f(x + (float)text_width, y + font_height + 2.0f);
                    glTexCoord2f(0.0f, 1.0f); glVertex2f(x, y + font_height + 2.0f);
                    glEnd();
                    glPopAttrib();
                }
            }
            return;
        }
#else
        int font_height = (int)lroundf(scale * 7.0f);
        if (font_height < 8) font_height = 8;
        if (font_height >= CHAT_UTF8_BUFFER_HEIGHT) font_height = CHAT_UTF8_BUFFER_HEIGHT - 2;
        if (!ensure_chat_font()) return;
        if (chat_text_font_height != font_height) {
            if (FT_Set_Pixel_Sizes(chat_font_face, 0, (FT_UInt)font_height) != 0) return;
            chat_text_font_height = font_height;
        }
        memset(chat_text_rgba, 0, sizeof chat_text_rgba);
        int pen_x = 2;
        int baseline = (int)(chat_font_face->size->metrics.ascender >> 6);
        FT_UInt previous_glyph = 0;
        const unsigned char *cursor = (const unsigned char *)text;
        while (*cursor != '\0') {
            uint32_t codepoint = next_utf8_codepoint(&cursor);
            FT_UInt glyph_index = FT_Get_Char_Index(chat_font_face, codepoint);
            if (FT_HAS_KERNING(chat_font_face) && previous_glyph != 0 && glyph_index != 0) {
                FT_Vector kerning;
                if (FT_Get_Kerning(chat_font_face, previous_glyph, glyph_index,
                                   FT_KERNING_DEFAULT, &kerning) == 0) {
                    pen_x += (int)(kerning.x >> 6);
                }
            }
            if (FT_Load_Glyph(chat_font_face, glyph_index, FT_LOAD_DEFAULT) != 0
                || FT_Render_Glyph(chat_font_face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
                previous_glyph = glyph_index;
                continue;
            }
            FT_GlyphSlot glyph = chat_font_face->glyph;
            int glyph_x = pen_x + glyph->bitmap_left;
            int glyph_y = baseline - glyph->bitmap_top;
            for (unsigned int row = 0; row < glyph->bitmap.rows; ++row) {
                int target_y = glyph_y + (int)row;
                if (target_y < 0 || target_y >= font_height + 2) continue;
                int source_row = glyph->bitmap.pitch >= 0
                    ? (int)row : (int)(glyph->bitmap.rows - 1 - row);
                const unsigned char *source = glyph->bitmap.buffer
                    + (size_t)source_row * (size_t)abs(glyph->bitmap.pitch);
                for (unsigned int column = 0; column < glyph->bitmap.width; ++column) {
                    int target_x = glyph_x + (int)column;
                    if (target_x < 0 || target_x >= CHAT_UTF8_BUFFER_WIDTH) continue;
                    size_t pixel = ((size_t)target_y * CHAT_UTF8_BUFFER_WIDTH
                                    + (size_t)target_x) * 4;
                    unsigned char alpha = source[column];
                    if (alpha > chat_text_rgba[pixel + 3]) {
                        chat_text_rgba[pixel] = 255;
                        chat_text_rgba[pixel + 1] = 255;
                        chat_text_rgba[pixel + 2] = 255;
                        chat_text_rgba[pixel + 3] = alpha;
                    }
                }
            }
            pen_x += (int)(glyph->advance.x >> 6);
            previous_glyph = glyph_index;
        }
        int text_width = pen_x + 2;
        if (text_width > CHAT_UTF8_BUFFER_WIDTH) text_width = CHAT_UTF8_BUFFER_WIDTH;
        if (text_width < 1) return;
        if (chat_text_texture == 0) glGenTextures(1, &chat_text_texture);
        glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, chat_text_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, text_width, font_height + 2, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, chat_text_rgba);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(x, y);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(x + (float)text_width, y);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(x + (float)text_width, y + font_height + 2.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(x, y + font_height + 2.0f);
        glEnd();
        glPopAttrib();
        return;
#endif
    }

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    for (const char *ch = text; *ch; ++ch) {
        unsigned char rows[7] = { 0, 0, 0, 0, 0, 0, 0 };
        char glyph = *ch;
        switch (glyph) {
            case 'A': { const unsigned char g[7] = {14,17,17,31,17,17,17}; memcpy(rows, g, sizeof rows); break; }
            case 'B': { const unsigned char g[7] = {30,17,17,30,17,17,30}; memcpy(rows, g, sizeof rows); break; }
            case 'C': { const unsigned char g[7] = {14,17,16,16,16,17,14}; memcpy(rows, g, sizeof rows); break; }
            case 'D': { const unsigned char g[7] = {30,17,17,17,17,17,30}; memcpy(rows, g, sizeof rows); break; }
            case 'E': { const unsigned char g[7] = {31,16,16,30,16,16,31}; memcpy(rows, g, sizeof rows); break; }
            case 'F': { const unsigned char g[7] = {31,16,16,30,16,16,16}; memcpy(rows, g, sizeof rows); break; }
            case 'G': { const unsigned char g[7] = {14,17,16,23,17,17,15}; memcpy(rows, g, sizeof rows); break; }
            case 'H': { const unsigned char g[7] = {17,17,17,31,17,17,17}; memcpy(rows, g, sizeof rows); break; }
            case 'I': { const unsigned char g[7] = {31,4,4,4,4,4,31}; memcpy(rows, g, sizeof rows); break; }
            case 'J': { const unsigned char g[7] = {7,2,2,2,2,18,12}; memcpy(rows, g, sizeof rows); break; }
            case 'K': { const unsigned char g[7] = {17,18,20,24,20,18,17}; memcpy(rows, g, sizeof rows); break; }
            case 'L': { const unsigned char g[7] = {16,16,16,16,16,16,31}; memcpy(rows, g, sizeof rows); break; }
            case 'M': { const unsigned char g[7] = {17,27,21,21,17,17,17}; memcpy(rows, g, sizeof rows); break; }
            case 'N': { const unsigned char g[7] = {17,25,25,21,19,19,17}; memcpy(rows, g, sizeof rows); break; }
            case 'O': { const unsigned char g[7] = {14,17,17,17,17,17,14}; memcpy(rows, g, sizeof rows); break; }
            case 'P': { const unsigned char g[7] = {30,17,17,30,16,16,16}; memcpy(rows, g, sizeof rows); break; }
            case 'Q': { const unsigned char g[7] = {14,17,17,17,21,18,13}; memcpy(rows, g, sizeof rows); break; }
            case 'R': { const unsigned char g[7] = {30,17,17,30,20,18,17}; memcpy(rows, g, sizeof rows); break; }
            case 'S': { const unsigned char g[7] = {15,16,16,14,1,1,30}; memcpy(rows, g, sizeof rows); break; }
            case 'T': { const unsigned char g[7] = {31,4,4,4,4,4,4}; memcpy(rows, g, sizeof rows); break; }
            case 'U': { const unsigned char g[7] = {17,17,17,17,17,17,14}; memcpy(rows, g, sizeof rows); break; }
            case 'V': { const unsigned char g[7] = {17,17,17,17,17,10,4}; memcpy(rows, g, sizeof rows); break; }
            case 'W': { const unsigned char g[7] = {17,17,17,21,21,21,10}; memcpy(rows, g, sizeof rows); break; }
            case 'X': { const unsigned char g[7] = {17,17,10,4,10,17,17}; memcpy(rows, g, sizeof rows); break; }
            case 'Y': { const unsigned char g[7] = {17,17,10,4,4,4,4}; memcpy(rows, g, sizeof rows); break; }
            case 'Z': { const unsigned char g[7] = {31,1,2,4,8,16,31}; memcpy(rows, g, sizeof rows); break; }
            case 'a': { const unsigned char g[7] = {0,0,14,1,15,17,15}; memcpy(rows, g, sizeof rows); break; }
            case 'b': { const unsigned char g[7] = {16,16,30,17,17,17,30}; memcpy(rows, g, sizeof rows); break; }
            case 'c': { const unsigned char g[7] = {0,0,14,16,16,17,14}; memcpy(rows, g, sizeof rows); break; }
            case 'd': { const unsigned char g[7] = {1,1,15,17,17,17,15}; memcpy(rows, g, sizeof rows); break; }
            case 'e': { const unsigned char g[7] = {0,0,14,17,31,16,14}; memcpy(rows, g, sizeof rows); break; }
            case 'f': { const unsigned char g[7] = {6,9,8,28,8,8,8}; memcpy(rows, g, sizeof rows); break; }
            case 'g': { const unsigned char g[7] = {0,0,15,17,15,1,14}; memcpy(rows, g, sizeof rows); break; }
            case 'h': { const unsigned char g[7] = {16,16,30,17,17,17,17}; memcpy(rows, g, sizeof rows); break; }
            case 'i': { const unsigned char g[7] = {4,0,12,4,4,4,14}; memcpy(rows, g, sizeof rows); break; }
            case 'j': { const unsigned char g[7] = {2,0,6,2,2,18,12}; memcpy(rows, g, sizeof rows); break; }
            case 'k': { const unsigned char g[7] = {16,16,18,20,24,20,18}; memcpy(rows, g, sizeof rows); break; }
            case 'l': { const unsigned char g[7] = {12,4,4,4,4,4,14}; memcpy(rows, g, sizeof rows); break; }
            case 'm': { const unsigned char g[7] = {0,0,26,21,21,21,21}; memcpy(rows, g, sizeof rows); break; }
            case 'n': { const unsigned char g[7] = {0,0,30,17,17,17,17}; memcpy(rows, g, sizeof rows); break; }
            case 'o': { const unsigned char g[7] = {0,0,14,17,17,17,14}; memcpy(rows, g, sizeof rows); break; }
            case 'p': { const unsigned char g[7] = {0,0,30,17,30,16,16}; memcpy(rows, g, sizeof rows); break; }
            case 'q': { const unsigned char g[7] = {0,0,15,17,15,1,1}; memcpy(rows, g, sizeof rows); break; }
            case 'r': { const unsigned char g[7] = {0,0,22,25,16,16,16}; memcpy(rows, g, sizeof rows); break; }
            case 's': { const unsigned char g[7] = {0,0,15,16,14,1,30}; memcpy(rows, g, sizeof rows); break; }
            case 't': { const unsigned char g[7] = {8,8,28,8,8,9,6}; memcpy(rows, g, sizeof rows); break; }
            case 'u': { const unsigned char g[7] = {0,0,17,17,17,19,13}; memcpy(rows, g, sizeof rows); break; }
            case 'v': { const unsigned char g[7] = {0,0,17,17,17,10,4}; memcpy(rows, g, sizeof rows); break; }
            case 'w': { const unsigned char g[7] = {0,0,17,17,21,21,10}; memcpy(rows, g, sizeof rows); break; }
            case 'x': { const unsigned char g[7] = {0,0,17,10,4,10,17}; memcpy(rows, g, sizeof rows); break; }
            case 'y': { const unsigned char g[7] = {0,0,17,17,15,1,14}; memcpy(rows, g, sizeof rows); break; }
            case 'z': { const unsigned char g[7] = {0,0,31,2,4,8,31}; memcpy(rows, g, sizeof rows); break; }
            case '0': { const unsigned char g[7] = {14,17,19,21,25,17,14}; memcpy(rows, g, sizeof rows); break; }
            case '1': { const unsigned char g[7] = {4,12,4,4,4,4,14}; memcpy(rows, g, sizeof rows); break; }
            case '2': { const unsigned char g[7] = {14,17,1,2,4,8,31}; memcpy(rows, g, sizeof rows); break; }
            case '3': { const unsigned char g[7] = {30,1,1,14,1,1,30}; memcpy(rows, g, sizeof rows); break; }
            case '4': { const unsigned char g[7] = {2,6,10,18,31,2,2}; memcpy(rows, g, sizeof rows); break; }
            case '5': { const unsigned char g[7] = {31,16,16,30,1,1,30}; memcpy(rows, g, sizeof rows); break; }
            case '6': { const unsigned char g[7] = {14,16,16,30,17,17,14}; memcpy(rows, g, sizeof rows); break; }
            case '7': { const unsigned char g[7] = {31,1,2,4,8,8,8}; memcpy(rows, g, sizeof rows); break; }
            case '8': { const unsigned char g[7] = {14,17,17,14,17,17,14}; memcpy(rows, g, sizeof rows); break; }
            case '9': { const unsigned char g[7] = {14,17,17,15,1,1,14}; memcpy(rows, g, sizeof rows); break; }
            case ':': { const unsigned char g[7] = {0,4,4,0,4,4,0}; memcpy(rows, g, sizeof rows); break; }
            case '/': { const unsigned char g[7] = {1,2,2,4,8,8,16}; memcpy(rows, g, sizeof rows); break; }
            case '.': { const unsigned char g[7] = {0,0,0,0,0,12,12}; memcpy(rows, g, sizeof rows); break; }
            case '-': { const unsigned char g[7] = {0,0,0,31,0,0,0}; memcpy(rows, g, sizeof rows); break; }
            case '_': { const unsigned char g[7] = {0,0,0,0,0,0,31}; memcpy(rows, g, sizeof rows); break; }
            case '>': { const unsigned char g[7] = {16,8,4,2,4,8,16}; memcpy(rows, g, sizeof rows); break; }
            default: break;
        }

        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if (rows[row] & (1u << (4 - column))) {
                    float px = x + (float)column * scale;
                    float py = y + (float)row * scale;
                    glVertex2f(px, py);
                    glVertex2f(px + scale, py);
                    glVertex2f(px + scale, py + scale);
                    glVertex2f(px, py + scale);
                }
            }
        }
        x += 6.0f * scale;
    }
    glEnd();
}

static void draw_ui_text(const char *text, float center_x, float y, float scale) {
    float width = 0.0f;
    for (const unsigned char *ch = (const unsigned char *)text; *ch; ++ch) {
        if ((*ch & 0xC0u) == 0x80u) continue;
        width += ((*ch & 0x80u) != 0 ? 12.0f : 6.0f) * scale;
    }
    draw_ui_text_at(text, center_x - width * 0.5f, y, scale);
}

static void draw_pause_menu(int width, int height) {
    float center_x = (float)width * 0.5f;
    float center_y = (float)height * 0.5f;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_ui_rect(0.0f, 0.0f, (float)width, (float)height, 0.0f, 0.0f, 0.0f, 0.62f);
    draw_ui_rect(center_x - 190.0f, center_y - 185.0f, 380.0f, 370.0f,
                 0.10f, 0.10f, 0.10f, 0.96f);
    draw_ui_text("PAUSED", center_x, center_y - 150.0f, 5.0f);
    draw_ui_rect(center_x - 140.0f, center_y - 105.0f, 280.0f, 58.0f,
                 0.18f, 0.52f, 0.20f, 1.0f);
    draw_ui_text("RESUME", center_x, center_y - 88.0f, 3.0f);
    draw_ui_rect(center_x - 140.0f, center_y - 30.0f, 280.0f, 58.0f,
                 0.18f, 0.32f, 0.55f, 1.0f);
    draw_ui_text("CHANGE SKIN", center_x, center_y - 13.0f, 2.5f);
    draw_ui_rect(center_x - 140.0f, center_y + 45.0f, 280.0f, 58.0f,
                 0.34f, 0.20f, 0.19f, 1.0f);
    draw_ui_text("HOME", center_x, center_y + 62.0f, 3.0f);
    draw_ui_text("ESC: RESUME", center_x, center_y + 137.0f, 2.0f);
    glDisable(GL_BLEND);
}

static void draw_home_menu(int width, int height) {
    float center_x = (float)width * 0.5f;
    float center_y = (float)height * 0.5f;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_ui_rect(0.0f, 0.0f, (float)width, (float)height, 0.02f, 0.05f, 0.09f, 0.68f);
    draw_ui_rect(center_x - 220.0f, center_y - 250.0f, 440.0f, 500.0f,
                 0.05f, 0.07f, 0.09f, 0.94f);
    draw_ui_rect(center_x - 172.0f, center_y - 165.0f, 42.0f, 42.0f,
                 0.22f, 0.62f, 0.25f, 1.0f);
    draw_ui_rect(center_x - 158.0f, center_y - 179.0f, 42.0f, 42.0f,
                 0.38f, 0.76f, 0.31f, 1.0f);
    draw_ui_text("BLOCK WORLD", center_x + 20.0f, center_y - 210.0f, 5.0f);
    draw_ui_text("A SMALL VOXEL ADVENTURE", center_x, center_y - 160.0f, 2.0f);
    draw_ui_rect(center_x - 140.0f, center_y - 125.0f, 280.0f, 58.0f,
                 0.18f, 0.52f, 0.20f, 1.0f);
    draw_ui_text("PLAY", center_x, center_y - 108.0f, 4.0f);
    draw_ui_rect(center_x - 140.0f, center_y - 55.0f, 280.0f, 58.0f,
                 0.16f, 0.40f, 0.62f, 1.0f);
    draw_ui_text("JOIN SERVER", center_x, center_y - 38.0f, 3.0f);
    draw_ui_rect(center_x - 140.0f, center_y + 15.0f, 280.0f, 58.0f,
                 0.18f, 0.32f, 0.55f, 1.0f);
    draw_ui_text("ONLINE SETTINGS", center_x, center_y + 32.0f, 2.5f);
    draw_ui_rect(center_x - 140.0f, center_y + 85.0f, 280.0f, 58.0f,
                 0.34f, 0.20f, 0.19f, 1.0f);
    draw_ui_text("EXIT GAME", center_x, center_y + 102.0f, 3.0f);
    char profile[128];
    snprintf(profile, sizeof profile, "%.24s  SERVER %.64s:%u",
             player_name, server_host, (unsigned int)server_port);
    draw_ui_text_at(profile, center_x - 205.0f, center_y + 170.0f, 1.5f);
    draw_ui_text("USERNAME, ADDRESS, PORT & SKIN", center_x, center_y + 200.0f, 1.5f);
    glDisable(GL_BLEND);
}

static void draw_online_settings(int width, int height) {
    float center_x = (float)width * 0.5f;
    float center_y = (float)height * 0.5f;
    const float field_x = center_x - 190.0f;
    const float field_width = 380.0f;
    const float field_y[3] = { center_y - 125.0f, center_y - 50.0f, center_y + 25.0f };
    const char *labels[3] = { "USERNAME", "SERVER ADDRESS", "PORT" };
    const char *values[3] = { online_username, online_server, online_port };

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_ui_rect(0.0f, 0.0f, (float)width, (float)height, 0.0f, 0.0f, 0.0f, 0.72f);
    draw_ui_rect(center_x - 245.0f, center_y - 265.0f, 490.0f, 530.0f,
                 0.06f, 0.08f, 0.10f, 0.98f);
    draw_ui_text("ONLINE SETTINGS", center_x, center_y - 225.0f, 3.5f);
    for (int i = 0; i < 3; ++i) {
        draw_ui_text_at(labels[i], field_x, field_y[i] - 25.0f, 1.5f);
        draw_ui_rect(field_x, field_y[i], field_width, 44.0f,
                     i == online_settings_field ? 0.12f : 0.10f,
                     i == online_settings_field ? 0.23f : 0.12f,
                     i == online_settings_field ? 0.34f : 0.15f, 1.0f);
        char displayed_value[64];
        snprintf(displayed_value, sizeof displayed_value, "%.48s",
                 values[i][0] != '\0' ? values[i] : "(empty)");
        draw_ui_text_at(displayed_value,
                        field_x + 12.0f, field_y[i] + 13.0f, 1.7f);
    }

    draw_ui_rect(center_x - 140.0f, center_y + 92.0f, 280.0f, 48.0f,
                 0.18f, 0.32f, 0.55f, 1.0f);
    draw_ui_text("CHANGE SKIN", center_x, center_y + 107.0f, 2.2f);
    draw_ui_rect(center_x - 190.0f, center_y + 160.0f, 180.0f, 48.0f,
                 0.18f, 0.52f, 0.20f, 1.0f);
    draw_ui_text("SAVE", center_x - 100.0f, center_y + 175.0f, 2.5f);
    draw_ui_rect(center_x + 10.0f, center_y + 160.0f, 180.0f, 48.0f,
                 0.34f, 0.20f, 0.19f, 1.0f);
    draw_ui_text("CANCEL", center_x + 100.0f, center_y + 175.0f, 2.2f);
    if (online_settings_error[0] != '\0') {
        draw_ui_text_at(online_settings_error, center_x - 220.0f, center_y + 225.0f, 1.2f);
    } else {
        draw_ui_text("TAB: NEXT FIELD   ENTER: SAVE   ESC: CANCEL",
                     center_x, center_y + 225.0f, 1.2f);
    }
    glDisable(GL_BLEND);
}

static void draw_inventory_slot(int index, float x, float y, float size, bool selected) {
    draw_ui_rect(x, y, size, size, 0.10f, 0.10f, 0.10f, 0.88f);
    draw_ui_rect(x + 2.0f, y + 2.0f, size - 4.0f, size - 4.0f,
                 selected ? 0.95f : 0.48f,
                 selected ? 0.88f : 0.48f,
                 selected ? 0.25f : 0.48f,
                 1.0f);

    const InventorySlot *slot = &inventory[index];
    if (slot->block != BLOCK_AIR && slot->count > 0) {
        block_color(slot->block);
        glBegin(GL_QUADS);
        glVertex2f(x + size * 0.25f, y + size * 0.22f);
        glVertex2f(x + size * 0.75f, y + size * 0.22f);
        glVertex2f(x + size * 0.75f, y + size * 0.72f);
        glVertex2f(x + size * 0.25f, y + size * 0.72f);
        glEnd();

        int count = slot->count;
        float digit_scale = 2.0f;
        float digits_width = count >= 100 ? 18.0f : (count >= 10 ? 11.0f : 4.0f);
        float digit_x = x + size - digits_width - 4.0f;
        float digit_y = y + size - 14.0f;
        if (count >= 100) {
            draw_ui_digit(count / 100, digit_x, digit_y, digit_scale);
            digit_x += 7.0f;
        }
        if (count >= 10) {
            draw_ui_digit((count / 10) % 10, digit_x, digit_y, digit_scale);
            digit_x += 7.0f;
        }
        draw_ui_digit(count % 10, digit_x, digit_y, digit_scale);
    }
}

static const char *block_name(unsigned char block) {
    switch (block) {
        case BLOCK_GRASS: return "GRASS";
        case BLOCK_DIRT: return "DIRT";
        case BLOCK_STONE: return "STONE";
        case BLOCK_SAND: return "SAND";
        case BLOCK_WOOD: return "WOOD";
        case BLOCK_LEAF: return "LEAVES";
        case BLOCK_GLASS: return "GLASS";
        default: return "EMPTY";
    }
}

static void draw_hud(int width, int height) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    float slot_size = 48.0f;
    float gap = 3.0f;
    float total_width = INVENTORY_SLOT_COUNT * slot_size + (INVENTORY_SLOT_COUNT - 1) * gap;
    float start_x = ((float)width - total_width) * 0.5f;
    float y = (float)height - slot_size - 18.0f;

    for (int i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
        draw_inventory_slot(i, start_x + i * (slot_size + gap), y, slot_size,
                            i == selected_hotbar_slot);
    }

    draw_ui_text(block_name(inventory[selected_hotbar_slot].block),
                 (float)width * 0.5f, y - 25.0f, 2.0f);

    if (inventory_open) {
        float panel_width = 3.0f * 62.0f + 32.0f;
        float panel_height = 3.0f * 62.0f + 54.0f;
        float panel_x = ((float)width - panel_width) * 0.5f;
        float panel_y = ((float)height - panel_height) * 0.5f;
        draw_ui_rect(panel_x, panel_y, panel_width, panel_height, 0.04f, 0.04f, 0.04f, 0.88f);

        float grid_x = panel_x + 16.0f;
        float grid_y = panel_y + 24.0f;
        for (int i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
            int column = i % 3;
            int row = i / 3;
            draw_inventory_slot(i, grid_x + column * 62.0f, grid_y + row * 62.0f, 56.0f,
                                i == selected_hotbar_slot);
        }
    }
    glDisable(GL_BLEND);
}

static void draw_world(Player *player) {
    GLFWwindow *window = glfwGetCurrentContext();
    int width;
    int height;
    glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0 || height <= 0) {
        return;
    }

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(70.0, (double)width / (double)height, 0.1, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    Vec3 forward = vec3(
        cosf(player->pitch) * sinf(player->yaw),
        sinf(player->pitch),
        cosf(player->pitch) * cosf(player->yaw)
    );
    Vec3 camera_pos = get_camera_position(player);
    if (camera_mode != CAMERA_FIRST_PERSON) {
        camera_pos = clip_third_person_camera(player, camera_pos);
    }
    Vec3 target = add_vec3(player->pos, vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f));
    if (camera_mode != CAMERA_THIRD_PERSON_FRONT) {
        target = add_vec3(target, forward);
    }
    Vec3 up = vec3(0.0f, 1.0f, 0.0f);
    gluLookAt(camera_pos.x, camera_pos.y, camera_pos.z,
              target.x, target.y, target.z,
              up.x, up.y, up.z);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    for (int x = 0; x < WORLD_X; ++x) {
        for (int y = 0; y < WORLD_Y; ++y) {
            for (int z = 0; z < WORLD_Z; ++z) {
                if (world[x][y][z] != BLOCK_AIR) {
                    draw_cube((float)x, (float)y, (float)z, world[x][y][z]);
                }
            }
        }
    }

    for (int i = 0; i < MAX_DROPPED_ITEMS; ++i) {
        if (dropped_items[i].active) {
            draw_dropped_item(&dropped_items[i]);
        }
    }

    draw_player_model(player, true);
    if (multiplayer_world_active) {
        draw_remote_players();
    }

    if (has_selection) {
        glDisable(GL_CULL_FACE);
        glColor3f(0.0f, 0.0f, 0.0f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        float x = (float)selected_block.x;
        float y = (float)selected_block.y;
        float z = (float)selected_block.z;
        const float v[8][3] = {
            {x, y, z}, {x + 1, y, z}, {x + 1, y, z + 1}, {x, y, z + 1},
            {x, y + 1, z}, {x + 1, y + 1, z}, {x + 1, y + 1, z + 1}, {x, y + 1, z + 1}
        };
        int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
        };
        for (int i = 0; i < 12; ++i) {
            glVertex3f(v[edges[i][0]][0], v[edges[i][0]][1], v[edges[i][0]][2]);
            glVertex3f(v[edges[i][1]][0], v[edges[i][1]][1], v[edges[i][1]][2]);
        }
        glEnd();
    }

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, (double)width, (double)height, 0.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    if (home_screen) {
        draw_home_menu(width, height);
        if (online_settings_open) draw_online_settings(width, height);
    } else {
        float center_x = (float)width * 0.5f;
        float center_y = (float)height * 0.5f;
        if (!inventory_open && !chat_open) {
            draw_first_person_arm(width, height);
            glColor3f(1.0f, 1.0f, 1.0f);
            glBegin(GL_LINES);
            glVertex2f(center_x - 8.0f, center_y);
            glVertex2f(center_x + 8.0f, center_y);
            glVertex2f(center_x, center_y - 8.0f);
            glVertex2f(center_x, center_y + 8.0f);
            glEnd();
        }
        draw_hud(width, height);
        draw_chat_overlay(width, height);
        if (paused) {
            draw_pause_menu(width, height);
        }
    }
    glEnable(GL_CULL_FACE);
}

static void update_selection(Player *player) {
    Vec3 eye = player->pos;
    eye.y += PLAYER_EYE_HEIGHT;
    Vec3 direction = vec3(
        cosf(player->pitch) * sinf(player->yaw),
        sinf(player->pitch),
        cosf(player->pitch) * cosf(player->yaw)
    );
    direction = normalize_vec3(direction);

    BlockPos last = { (int)floorf(eye.x), (int)floorf(eye.y), (int)floorf(eye.z) };
    BlockPos hit = { -1, -1, -1 };
    BlockPos prev = { -1, -1, -1 };
    for (float t = 0.0f; t <= MAX_REACH; t += 0.05f) {
        Vec3 sample = add_vec3(eye, mul_vec3(direction, t));
        int wx = (int)floorf(sample.x);
        int wy = (int)floorf(sample.y);
        int wz = (int)floorf(sample.z);
        prev = last;
        last.x = wx;
        last.y = wy;
        last.z = wz;
        if (is_solid(wx, wy, wz)) {
            hit.x = wx;
            hit.y = wy;
            hit.z = wz;
            place_pos = prev;
            selected_block = hit;
            has_selection = true;
            return;
        }
    }

    has_selection = false;
    selected_block = (BlockPos){ -1, -1, -1 };
    place_pos = (BlockPos){ -1, -1, -1 };
}

static bool add_item_to_inventory(unsigned char block) {
    for (int i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
        if (inventory[i].block == block && inventory[i].count < 64) {
            ++inventory[i].count;
            return true;
        }
    }
    for (int i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
        if (inventory[i].block == BLOCK_AIR || inventory[i].count == 0) {
            inventory[i].block = block;
            inventory[i].count = 1;
            return true;
        }
    }
    return false;
}

static void copy_utf8_text(char *destination, size_t capacity, const char *source) {
    size_t output_length = 0;
    const unsigned char *cursor = (const unsigned char *)source;
    while (*cursor != '\0') {
        size_t character_length = 1;
        if ((*cursor & 0xE0u) == 0xC0u) character_length = 2;
        else if ((*cursor & 0xF0u) == 0xE0u) character_length = 3;
        else if ((*cursor & 0xF8u) == 0xF0u) character_length = 4;
        if (output_length + character_length >= capacity) break;
        memcpy(destination + output_length, cursor, character_length);
        output_length += character_length;
        cursor += character_length;
    }
    destination[output_length] = '\0';
}

static void add_chat_line(const char *text) {
    if (chat_history_count == CHAT_HISTORY_SIZE) {
        memmove(chat_history, chat_history + 1,
                (CHAT_HISTORY_SIZE - 1) * sizeof chat_history[0]);
        --chat_history_count;
    }
    copy_utf8_text(chat_history[chat_history_count], CHAT_LINE_CAPACITY, text);
    ++chat_history_count;
}

static RemotePlayer *find_remote_player(int id) {
    for (int i = 0; i < MAX_REMOTE_PLAYERS; ++i) {
        if (remote_players[i].active && remote_players[i].id == id) return &remote_players[i];
    }
    return NULL;
}

static RemotePlayer *get_remote_player(int id, const char *name) {
    RemotePlayer *remote = find_remote_player(id);
    if (remote == NULL) {
        for (int i = 0; i < MAX_REMOTE_PLAYERS; ++i) {
            if (!remote_players[i].active) {
                remote = &remote_players[i];
                memset(remote, 0, sizeof *remote);
                remote->active = true;
                remote->id = id;
                break;
            }
        }
    }
    if (remote != NULL && name != NULL) {
        snprintf(remote->name, sizeof remote->name, "%s", name);
    }
    return remote;
}

static bool network_handle_line(char *line) {
    int id;
    char name[25];
    float x, y, z, yaw, pitch;
    int bx, by, bz, block;

    if (strncmp(line, "WELCOME ", 8) == 0) {
        int width, height, depth, max_players;
        if (sscanf(line + 8, "%d %d %d %d %d", &local_player_id,
                   &width, &height, &depth, &max_players) != 5
            || local_player_id <= 0 || width != WORLD_X || height != WORLD_Y
            || depth != WORLD_Z || max_players <= 0) {
            snprintf(network_error, sizeof network_error, "Server sent an invalid WELCOME");
            return false;
        }
        player_name[sizeof player_name - 1] = '\0';
        return true;
    }
    if (strncmp(line, "WORLD_BEGIN ", 12) == 0) {
        if (sscanf(line + 12, "%d", &network_expected_world_blocks) != 1
            || network_expected_world_blocks < 0
            || network_expected_world_blocks > WORLD_X * WORLD_Y * WORLD_Z) {
            snprintf(network_error, sizeof network_error, "Server sent an invalid world snapshot");
            return false;
        }
        memset(world, BLOCK_AIR, sizeof world);
        network_received_world_blocks = 0;
        return true;
    }
    if (sscanf(line, "BLOCK %d %d %d %d", &bx, &by, &bz, &block) == 4) {
        if (bx < 0 || bx >= WORLD_X || by < 0 || by >= WORLD_Y
            || bz < 0 || bz >= WORLD_Z || block < BLOCK_AIR || block > BLOCK_GLASS) {
            snprintf(network_error, sizeof network_error, "Server sent an invalid block update");
            return false;
        }
        if (block != BLOCK_AIR && world[bx][by][bz] == BLOCK_AIR) {
            ++network_received_world_blocks;
        }
        world[bx][by][bz] = (unsigned char)block;
        return true;
    }
    if (strcmp(line, "WORLD_END") == 0) {
        if (network_received_world_blocks != network_expected_world_blocks) {
            snprintf(network_error, sizeof network_error, "Server world snapshot was incomplete");
            return false;
        }
        network_world_received = true;
        return true;
    }
    if (sscanf(line, "PLAYER %d %24s %f %f %f %f %f",
               &id, name, &x, &y, &z, &yaw, &pitch) == 7) {
        if (id == local_player_id) return true;
        if (!isfinite(x) || !isfinite(y) || !isfinite(z)
            || !isfinite(yaw) || !isfinite(pitch)) return true;
        RemotePlayer *remote = get_remote_player(id, name);
        if (remote != NULL) {
            double now = glfwGetTime();
            if (remote->last_position_time > 0.0) {
                double elapsed = now - remote->last_position_time;
                if (elapsed > 0.001 && elapsed < 1.0) {
                    float inverse_elapsed = (float)(1.0 / elapsed);
                    float velocity_x = (x - remote->player.pos.x) * inverse_elapsed;
                    float velocity_y = (y - remote->player.pos.y) * inverse_elapsed;
                    float velocity_z = (z - remote->player.pos.z) * inverse_elapsed;
                    remote->player.horizontal_velocity = vec3(
                        clamp_f(velocity_x, -10.0f, 10.0f), 0.0f,
                        clamp_f(velocity_z, -10.0f, 10.0f));
                    remote->player.vel.y = clamp_f(velocity_y, -10.0f, 10.0f);
                    remote->player.on_ground = fabsf(velocity_y) < 0.7f;
                }
            } else {
                remote->player.on_ground = true;
            }
            remote->player.pos = vec3(x, y, z);
            remote->player.yaw = yaw;
            remote->player.pitch = pitch;
            remote->last_position_time = now;
        }
        return true;
    }
    if (sscanf(line, "JOIN %d %24s %f %f %f", &id, name, &x, &y, &z) == 5) {
        if (id == local_player_id) return true;
        RemotePlayer *remote = get_remote_player(id, name);
        if (remote != NULL) {
            remote->player.pos = vec3(x, y, z);
            remote->player.on_ground = true;
            remote->last_position_time = glfwGetTime();
        }
        char message[CHAT_LINE_CAPACITY];
        snprintf(message, sizeof message, "%s joined (ID %d)", name, id);
        add_chat_line(message);
        return true;
    }
    if (sscanf(line, "LEAVE %d", &id) == 1) {
        RemotePlayer *remote = find_remote_player(id);
        if (remote != NULL) {
            char message[CHAT_LINE_CAPACITY];
            snprintf(message, sizeof message, "%s left (ID %d)", remote->name, id);
            add_chat_line(message);
            remote->active = false;
        }
        return true;
    }
    if (strncmp(line, "CHAT ", 5) == 0) {
        int text_offset = 0;
        if (sscanf(line, "CHAT %d %24s %n", &id, name, &text_offset) == 2
            && text_offset > 0) {
            char message[CHAT_LINE_CAPACITY];
            char prefix[48];
            snprintf(prefix, sizeof prefix, "#%d %s: ", id, name);
            size_t prefix_length = strlen(prefix);
            memcpy(message, prefix, prefix_length);
            copy_utf8_text(message + prefix_length, sizeof message - prefix_length,
                           line + text_offset);
            add_chat_line(message);
        }
        return true;
    }
    if (sscanf(line, "EDIT_OK BREAK %d %d %d %d", &bx, &by, &bz, &block) == 4) {
        (void)bx;
        (void)by;
        (void)bz;
        if (block > BLOCK_AIR && block <= BLOCK_GLASS && game_mode == GAME_MODE_SURVIVAL) {
            int target_slot = -1;
            for (int i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
                if (inventory[i].block == block && inventory[i].count < 64) {
                    target_slot = i;
                    break;
                }
            }
            if (target_slot < 0) {
                for (int i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
                    if (inventory[i].block == BLOCK_AIR || inventory[i].count == 0) {
                        target_slot = i;
                        break;
                    }
                }
            }
            if (target_slot >= 0) {
                if (inventory[target_slot].block == BLOCK_AIR) {
                    inventory[target_slot].block = (unsigned char)block;
                }
                ++inventory[target_slot].count;
            }
        }
        return true;
    }
    if (sscanf(line, "EDIT_OK PLACE %d %d %d %d", &bx, &by, &bz, &block) == 4) {
        (void)bx;
        (void)by;
        (void)bz;
        (void)block;
        if (game_mode != GAME_MODE_CREATIVE) {
            InventorySlot *slot = &inventory[selected_hotbar_slot];
            if (slot->block != BLOCK_AIR && slot->count > 0) {
                --slot->count;
                if (slot->count == 0) slot->block = BLOCK_AIR;
            }
        }
        return true;
    }
    if (strncmp(line, "ERROR ", 6) == 0) {
        snprintf(network_error, sizeof network_error, "%.*s",
                 (int)sizeof network_error - 1, line + 6);
        add_chat_line(network_error);
        return true;
    }
    if (strcmp(line, "PONG") == 0) return true;
    if (line[0] != '\0') {
        fprintf(stderr, "Ignoring unknown server message: %s\n", line);
    }
    return true;
}

static bool network_consume_data(const char *data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        char value = data[i];
        if (value == '\n') {
            network_input[network_input_length] = '\0';
            if (network_input_length > 0
                && network_input[network_input_length - 1] == '\r') {
                network_input[network_input_length - 1] = '\0';
            }
            if (!network_handle_line(network_input)) return false;
            network_input_length = 0;
        } else {
            if (value == '\0' || network_input_length + 1 >= sizeof network_input) {
                snprintf(network_error, sizeof network_error, "Server sent an oversized line");
                return false;
            }
            network_input[network_input_length++] = value;
        }
    }
    return true;
}

static bool send_server_line(const char *line) {
    if (!multiplayer_connected || server_socket == INVALID_SOCKET) return false;
    size_t length = strlen(line);
    size_t sent = 0;
    while (sent < length) {
        int chunk = (int)(length - sent);
        int result = send(server_socket, line + sent, chunk, 0);
        if (result == SOCKET_ERROR || result == 0) {
            snprintf(network_error, sizeof network_error, "Server connection lost");
            return false;
        }
        sent += (size_t)result;
    }
    return true;
}

static void disconnect_from_server(void) {
    if (server_socket != INVALID_SOCKET) {
        shutdown(server_socket, SD_BOTH);
        closesocket(server_socket);
        server_socket = INVALID_SOCKET;
    }
    multiplayer_connected = false;
    local_player_id = 0;
    memset(remote_players, 0, sizeof remote_players);
    network_input_length = 0;
}

static bool connect_to_server(Player *player) {
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    char port_text[8];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    snprintf(port_text, sizeof port_text, "%u", (unsigned int)server_port);

#ifdef _WIN32
    if (!winsock_started) {
        WSADATA data;
        int result = WSAStartup(MAKEWORD(2, 2), &data);
        if (result != 0) {
            snprintf(network_error, sizeof network_error, "Winsock initialization failed (%d)", result);
            return false;
        }
        winsock_started = true;
    }
#endif
    int lookup = getaddrinfo(server_host, port_text, &hints, &addresses);
    if (lookup != 0) {
        snprintf(network_error, sizeof network_error, "Cannot resolve %.180s (%d)",
                 server_host, lookup);
        return false;
    }

    SOCKET connected_socket = INVALID_SOCKET;
    for (struct addrinfo *address = addresses; address != NULL; address = address->ai_next) {
        connected_socket = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (connected_socket == INVALID_SOCKET) continue;
#ifdef _WIN32
        DWORD timeout_ms = 5000;
        setsockopt(connected_socket, SOL_SOCKET, SO_RCVTIMEO,
                   (const char *)&timeout_ms, sizeof timeout_ms);
        setsockopt(connected_socket, SOL_SOCKET, SO_SNDTIMEO,
                   (const char *)&timeout_ms, sizeof timeout_ms);
#else
        struct timeval timeout = { 5, 0 };
        setsockopt(connected_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
        setsockopt(connected_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);
#endif
        if (connect(connected_socket, address->ai_addr, (int)address->ai_addrlen) == 0) break;
        closesocket(connected_socket);
        connected_socket = INVALID_SOCKET;
    }
    freeaddrinfo(addresses);
    if (connected_socket == INVALID_SOCKET) {
        snprintf(network_error, sizeof network_error, "Could not connect to %.180s:%u",
                 server_host, (unsigned int)server_port);
        return false;
    }

    server_socket = connected_socket;
    multiplayer_connected = false;
    local_player_id = 0;
    network_input_length = 0;
    network_world_received = false;
    network_expected_world_blocks = 0;
    network_received_world_blocks = 0;
    network_error[0] = '\0';

    char hello[64];
    snprintf(hello, sizeof hello, "HELLO %s\n", player_name);
    int hello_length = (int)strlen(hello);
    if (send(server_socket, hello, hello_length, 0) != hello_length) {
        snprintf(network_error, sizeof network_error, "Failed to send player identity");
        disconnect_from_server();
        return false;
    }

    while (!network_world_received) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(server_socket, &read_set);
        TIMEVAL timeout = { 10, 0 };
#ifdef _WIN32
        int ready = select(0, &read_set, NULL, NULL, &timeout);
#else
        int ready = select(server_socket + 1, &read_set, NULL, NULL, &timeout);
#endif
        if (ready <= 0) {
            snprintf(network_error, sizeof network_error,
                     ready == 0 ? "Timed out waiting for server world" : "Server receive failed");
            disconnect_from_server();
            return false;
        }
        char buffer[NETWORK_BUFFER_SIZE];
        int received = recv(server_socket, buffer, sizeof buffer, 0);
        if (received <= 0 || !network_consume_data(buffer, (size_t)(received > 0 ? received : 0))) {
            if (network_error[0] == '\0') {
                snprintf(network_error, sizeof network_error, "Server closed during world transfer");
            }
            disconnect_from_server();
            return false;
        }
    }

    if (local_player_id <= 0) {
        snprintf(network_error, sizeof network_error, "Server did not assign a player ID");
        disconnect_from_server();
        return false;
    }
    player->pos = vec3(WORLD_X * 0.5f, 4.0f, WORLD_Z * 0.5f);
    player->vel = vec3(0.0f, 0.0f, 0.0f);
    player->horizontal_velocity = vec3(0.0f, 0.0f, 0.0f);
    player->on_ground = true;
    multiplayer_connected = true;
    multiplayer_world_active = true;
    game_mode = GAME_MODE_SURVIVAL;
    creative_flying = false;
    next_network_position_time = 0.0;
    char joined_message[CHAT_LINE_CAPACITY];
    snprintf(joined_message, sizeof joined_message, "Joined %.48s:%u as %.24s (ID %d)",
             server_host, (unsigned int)server_port, player_name, local_player_id);
    add_chat_line(joined_message);
    save_user_config();
    return true;
}

static void poll_server_messages(void) {
    if (!multiplayer_connected) return;
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(server_socket, &read_set);
    TIMEVAL timeout = { 0, 0 };
#ifdef _WIN32
    int ready = select(0, &read_set, NULL, NULL, &timeout);
#else
    int ready = select(server_socket + 1, &read_set, NULL, NULL, &timeout);
#endif
    if (ready == SOCKET_ERROR) {
        add_chat_line("Server connection check failed");
        disconnect_from_server();
        return;
    }
    while (ready > 0 && FD_ISSET(server_socket, &read_set)) {
        char buffer[NETWORK_BUFFER_SIZE];
        int received = recv(server_socket, buffer, sizeof buffer, 0);
        if (received <= 0) {
            add_chat_line("Disconnected from server");
            disconnect_from_server();
            return;
        }
        if (!network_consume_data(buffer, (size_t)received)) {
            add_chat_line(network_error[0] != '\0' ? network_error : "Invalid server response");
            disconnect_from_server();
            return;
        }
        FD_ZERO(&read_set);
        FD_SET(server_socket, &read_set);
#ifdef _WIN32
        ready = select(0, &read_set, NULL, NULL, &timeout);
#else
        ready = select(server_socket + 1, &read_set, NULL, NULL, &timeout);
#endif
    }
}

static void skip_json_whitespace(const char **cursor) {
    while (**cursor == ' ' || **cursor == '\t' || **cursor == '\r' || **cursor == '\n') {
        ++*cursor;
    }
}

static bool consume_json_char(const char **cursor, char expected) {
    if (**cursor != expected) return false;
    ++*cursor;
    return true;
}

static bool append_history_byte(char *output, size_t capacity, size_t *length, unsigned char byte) {
    if (*length + 1 >= capacity) return false;
    output[(*length)++] = (char)byte;
    output[*length] = '\0';
    return true;
}

static bool parse_json_history_string(const char **cursor, char *output, size_t capacity) {
    if (**cursor != '"') return false;
    ++*cursor;
    size_t length = 0;
    output[0] = '\0';
    while (**cursor != '\0' && **cursor != '"') {
        unsigned char value = (unsigned char)*(*cursor)++;
        if (value == '\\') {
            if (**cursor == '\0') return false;
            char escaped = *(*cursor)++;
            switch (escaped) {
                case '"': value = '"'; break;
                case '\\': value = '\\'; break;
                case '/': value = '/'; break;
                case 'b': value = '\b'; break;
                case 'f': value = '\f'; break;
                case 'n': value = '\n'; break;
                case 'r': value = '\r'; break;
                case 't': value = '\t'; break;
                case 'u': {
                    unsigned int codepoint = 0;
                    for (int i = 0; i < 4; ++i) {
                        if (**cursor == '\0') return false;
                        unsigned char digit = (unsigned char)*(*cursor)++;
                        if (!isxdigit(digit)) return false;
                        codepoint = codepoint * 16u
                            + (unsigned int)(isdigit(digit) ? digit - '0'
                                : (unsigned char)tolower(digit) - 'a' + 10);
                    }
                    if (codepoint > 0x7f) return false;
                    value = (unsigned char)codepoint;
                    break;
                }
                default: return false;
            }
        } else if (value < 0x20) {
            return false;
        }
        if (!append_history_byte(output, capacity, &length, value)) return false;
    }
    if (**cursor != '"') return false;
    ++*cursor;
    return true;
}

static void load_chat_input_history(void) {
    FILE *file = fopen("history.json", "rb");
    if (file == NULL) {
        if (errno != ENOENT) {
            fprintf(stderr, "Unable to open history.json: %s\n", strerror(errno));
        }
        return;
    }

    char json[CHAT_INPUT_HISTORY_SIZE * (CHAT_INPUT_CAPACITY * 6 + 4) + 32];
    size_t length = fread(json, 1, sizeof json - 1, file);
    bool read_error = ferror(file) != 0;
    bool too_large = !feof(file);
    fclose(file);
    if (read_error || too_large) {
        fprintf(stderr, "Unable to read history.json%s\n",
                too_large ? ": file is too large" : "");
        return;
    }
    json[length] = '\0';

    const char *cursor = json;
    skip_json_whitespace(&cursor);
    if (!consume_json_char(&cursor, '{')) goto invalid_history;
    skip_json_whitespace(&cursor);
    char key[16];
    if (!parse_json_history_string(&cursor, key, sizeof key)
        || strcmp(key, "history") != 0) goto invalid_history;
    skip_json_whitespace(&cursor);
    if (!consume_json_char(&cursor, ':')) goto invalid_history;
    skip_json_whitespace(&cursor);
    if (!consume_json_char(&cursor, '[')) goto invalid_history;

    char loaded[CHAT_INPUT_HISTORY_SIZE][CHAT_INPUT_CAPACITY];
    int loaded_count = 0;
    skip_json_whitespace(&cursor);
    if (*cursor != ']') {
        for (;;) {
            char entry[CHAT_INPUT_CAPACITY];
            if (!parse_json_history_string(&cursor, entry, sizeof entry)) {
                goto invalid_history;
            }
            if (loaded_count == CHAT_INPUT_HISTORY_SIZE) {
                memmove(loaded, loaded + 1,
                        (CHAT_INPUT_HISTORY_SIZE - 1) * sizeof loaded[0]);
                --loaded_count;
            }
            snprintf(loaded[loaded_count++], CHAT_INPUT_CAPACITY, "%s", entry);
            skip_json_whitespace(&cursor);
            if (*cursor == ']') break;
            if (!consume_json_char(&cursor, ',')) goto invalid_history;
            skip_json_whitespace(&cursor);
        }
    }
    if (!consume_json_char(&cursor, ']')) goto invalid_history;
    skip_json_whitespace(&cursor);
    if (!consume_json_char(&cursor, '}')) goto invalid_history;
    skip_json_whitespace(&cursor);
    if (*cursor != '\0') goto invalid_history;

    memcpy(chat_input_history, loaded, (size_t)loaded_count * sizeof loaded[0]);
    chat_input_history_count = loaded_count;
    return;

invalid_history:
    fprintf(stderr, "history.json has an invalid or unsupported format; history was not loaded\n");
}

static bool save_chat_input_history(void) {
    FILE *file = fopen("history.json", "wb");
    if (file == NULL) {
        fprintf(stderr, "Unable to save history.json: %s\n", strerror(errno));
        return false;
    }

    bool ok = fputs("{\"history\":[", file) >= 0;
    for (int i = 0; ok && i < chat_input_history_count; ++i) {
        if (i > 0 && fputc(',', file) == EOF) ok = false;
        if (ok && fputc('"', file) == EOF) ok = false;
        for (const unsigned char *p = (const unsigned char *)chat_input_history[i];
             ok && *p; ++p) {
            if (*p == '"' || *p == '\\') {
                if (fputc('\\', file) == EOF || fputc(*p, file) == EOF) ok = false;
            } else if (*p < 0x20) {
                if (fprintf(file, "\\u%04x", (unsigned int)*p) < 0) ok = false;
            } else if (fputc(*p, file) == EOF) {
                ok = false;
            }
        }
        if (ok && fputc('"', file) == EOF) ok = false;
    }
    if (ok && fputs("]}\n", file) < 0) ok = false;
    if (fclose(file) != 0) ok = false;
    if (!ok) {
        fprintf(stderr, "Unable to write history.json\n");
    }
    return ok;
}

static bool block_id_from_name(const char *name, unsigned char *block) {
    if (strcmp(name, "grass") == 0 || strcmp(name, "kusa") == 0) *block = BLOCK_GRASS;
    else if (strcmp(name, "dirt") == 0) *block = BLOCK_DIRT;
    else if (strcmp(name, "stone") == 0) *block = BLOCK_STONE;
    else if (strcmp(name, "sand") == 0) *block = BLOCK_SAND;
    else if (strcmp(name, "wood") == 0) *block = BLOCK_WOOD;
    else if (strcmp(name, "leaf") == 0 || strcmp(name, "leaves") == 0) *block = BLOCK_LEAF;
    else if (strcmp(name, "glass") == 0) *block = BLOCK_GLASS;
    else return false;
    return true;
}

static void submit_chat_message(void) {
    GLFWwindow *window = glfwGetCurrentContext();
    if (chat_input_length == 0) {
        chat_open = false;
        chat_input_history_position = -1;
        if (window != NULL) glfwGetCursorPos(window, &mouse_x, &mouse_y);
        return;
    }

    char message[CHAT_INPUT_CAPACITY];
    memcpy(message, chat_input, chat_input_length + 1);
    if (chat_input_history_count == CHAT_INPUT_HISTORY_SIZE) {
        memmove(chat_input_history, chat_input_history + 1,
                (CHAT_INPUT_HISTORY_SIZE - 1) * sizeof chat_input_history[0]);
        --chat_input_history_count;
    }
    snprintf(chat_input_history[chat_input_history_count], CHAT_INPUT_CAPACITY,
             "%s", message);
    ++chat_input_history_count;
    save_chat_input_history();
    chat_input_history_position = -1;
    chat_input_draft[0] = '\0';
    chat_input_length = 0;
    chat_input[0] = '\0';
    chat_open = false;
    if (window != NULL) glfwGetCursorPos(window, &mouse_x, &mouse_y);

    if (message[0] != '/') {
        if (multiplayer_world_active) {
            char packet[CHAT_INPUT_CAPACITY + 8];
            snprintf(packet, sizeof packet, "CHAT %s\n", message);
            if (!send_server_line(packet)) {
                add_chat_line("Unable to send chat message");
            }
            return;
        }
        char line[CHAT_LINE_CAPACITY];
        memcpy(line, "YOU: ", 5);
        copy_utf8_text(line + 5, sizeof line - 5, message);
        add_chat_line(line);
        return;
    }

    for (char *p = message; *p; ++p) {
        *p = (char)tolower((unsigned char)*p);
    }

    char *command = strtok(message + 1, " ");
    char *argument = strtok(NULL, " ");
    char *extra = strtok(NULL, " ");
    if (command == NULL) {
        add_chat_line("Unknown command. Type /help");
        return;
    }

    if (strcmp(command, "help") == 0 && argument == NULL) {
        add_chat_line("/gamemode survival|creative");
        add_chat_line("/give <block> [count], /clear");
        add_chat_line("/save");
    } else if (strcmp(command, "gamemode") == 0 && argument != NULL && extra == NULL) {
        if (strcmp(argument, "survival") == 0 || strcmp(argument, "0") == 0) {
            game_mode = GAME_MODE_SURVIVAL;
            creative_flying = false;
            if (active_player != NULL) {
                active_player->on_ground = false;
                active_player->vel.y = 0.0f;
            }
            add_chat_line("Game mode: SURVIVAL");
        } else if (strcmp(argument, "creative") == 0 || strcmp(argument, "1") == 0) {
            game_mode = GAME_MODE_CREATIVE;
            creative_flying = true;
            flight_space_was_down = false;
            last_flight_space_press = -1.0;
            if (active_player != NULL) {
                active_player->on_ground = false;
                active_player->vel.y = 0.0f;
            }
            add_chat_line("Game mode: CREATIVE");
        } else {
            add_chat_line("Usage: /gamemode survival|creative");
        }
    } else if (strcmp(command, "give") == 0 && argument != NULL) {
        unsigned char block;
        int count = 1;
        char *count_text = extra;
        extra = strtok(NULL, " ");
        char *count_end = NULL;
        long requested_count = count_text == NULL ? 1 : strtol(count_text, &count_end, 10);
        if (count_text != NULL && (count_end == count_text || *count_end != '\0'
                                   || requested_count < 1 || requested_count > 576)) {
            add_chat_line("Count must be between 1 and 576");
        } else if (extra != NULL) {
            add_chat_line("Usage: /give <block> [count]");
        } else if (!block_id_from_name(argument, &block)) {
            add_chat_line("Unknown block. Type /help");
        } else {
            count = (int)requested_count;
            int given = 0;
            while (given < count && add_item_to_inventory(block)) ++given;
            char line[CHAT_LINE_CAPACITY];
            snprintf(line, sizeof line, "Gave %d %s", given, block_name(block));
            add_chat_line(line);
        }
    } else if (strcmp(command, "clear") == 0 && argument == NULL) {
        for (int i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
            inventory[i] = (InventorySlot){ BLOCK_AIR, 0 };
        }
        add_chat_line("Inventory cleared");
    } else if (strcmp(command, "save") == 0 && argument == NULL) {
        if (multiplayer_world_active) {
            add_chat_line("Server saves shared world changes automatically");
        } else {
            add_chat_line(active_player != NULL && save_world(active_player)
                              ? "World saved to world_saves/world.dat"
                              : "World save failed; see the error log");
        }
    } else {
        add_chat_line("Unknown command. Type /help");
    }
}

static void draw_chat_overlay(int width, int height) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (game_mode == GAME_MODE_CREATIVE) {
        draw_ui_text_at(creative_flying ? "CREATIVE FLY: ON" : "CREATIVE FLY: OFF",
                        12.0f, 12.0f, 2.0f);
    } else {
        draw_ui_text_at("SURVIVAL", 12.0f, 12.0f, 2.0f);
    }
    if (multiplayer_world_active) {
        char identity[64];
        snprintf(identity, sizeof identity, "%s #%d", player_name, local_player_id);
        draw_ui_text_at(identity, 12.0f, 34.0f, 2.0f);
        int shown = 0;
        for (int i = 0; i < MAX_REMOTE_PLAYERS && shown < 5; ++i) {
            if (!remote_players[i].active) continue;
            char online[64];
            snprintf(online, sizeof online, "%s #%d",
                     remote_players[i].name, remote_players[i].id);
            draw_ui_text_at(online, 12.0f, 56.0f + shown * 19.0f, 1.5f);
            ++shown;
        }
    }
    if (chat_open) {
        float panel_y = (float)height - 190.0f;
        draw_ui_rect(8.0f, panel_y, (float)width - 16.0f, 180.0f,
                     0.0f, 0.0f, 0.0f, 0.72f);
        int first = chat_history_count > 6 ? chat_history_count - 6 : 0;
        float text_y = panel_y + 10.0f;
        for (int i = first; i < chat_history_count; ++i) {
            draw_ui_text_at(chat_history[i], 18.0f, text_y, 2.0f);
            text_y += 21.0f;
        }
        char prompt[CHAT_INPUT_CAPACITY + 4];
        snprintf(prompt, sizeof prompt, "> %s_", chat_input);
        draw_ui_text_at(prompt, 18.0f, (float)height - 32.0f, 2.0f);
    } else if (chat_history_count > 0) {
        int first = chat_history_count > 3 ? chat_history_count - 3 : 0;
        for (int i = first; i < chat_history_count; ++i) {
            draw_ui_text_at(chat_history[i], 12.0f,
                            (float)height - 125.0f + (i - first) * 21.0f, 2.0f);
        }
    }
    glDisable(GL_BLEND);
}

static void drop_selected_item(const Player *player) {
    InventorySlot *slot = &inventory[selected_hotbar_slot];
    if (slot->block == BLOCK_AIR || slot->count <= 0) return;

    for (int i = 0; i < MAX_DROPPED_ITEMS; ++i) {
        if (!dropped_items[i].active) {
            Vec3 forward = vec3(sinf(player->yaw), 0.0f, cosf(player->yaw));
            dropped_items[i].block = slot->block;
            dropped_items[i].pos = add_vec3(player->pos, mul_vec3(forward, 1.5f));
            dropped_items[i].pos.y += 0.5f;
            dropped_items[i].active = true;

            --slot->count;
            if (slot->count == 0) slot->block = BLOCK_AIR;
            return;
        }
    }
}

static void update_dropped_items(const Player *player, float dt) {
    for (int i = 0; i < MAX_DROPPED_ITEMS; ++i) {
        DroppedItem *item = &dropped_items[i];
        if (!item->active) continue;

        if (item->pos.y > GROUND_LEVEL) {
            item->pos.y -= GRAVITY * dt;
            if (item->pos.y < GROUND_LEVEL) item->pos.y = GROUND_LEVEL;
        }

        Vec3 offset = sub_vec3(item->pos, player->pos);
        if (length_vec3(offset) < 1.15f && add_item_to_inventory(item->block)) {
            item->active = false;
        }
    }
}

static void apply_mouse_clicks(Player *player) {
    if (!has_selection) {
        return;
    }

    double now = glfwGetTime();
    if (now < next_block_action_time) {
        return;
    }

    if (glfwGetMouseButton(glfwGetCurrentContext(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        if (now < next_block_break_time) {
            return;
        }
        unsigned char block = block_at(selected_block.x, selected_block.y, selected_block.z);
        if (game_mode == GAME_MODE_CREATIVE) {
            if (multiplayer_world_active) {
                char packet[64];
                snprintf(packet, sizeof packet, "BREAK %d %d %d\n",
                         selected_block.x, selected_block.y, selected_block.z);
                if (send_server_line(packet)) {
                    has_selection = false;
                    next_block_action_time = now + BLOCK_ACTION_INTERVAL;
                    next_block_break_time = now + BLOCK_BREAK_INTERVAL;
                    arm_swing_time = ARM_SWING_DURATION;
                }
                return;
            }
            set_block(selected_block.x, selected_block.y, selected_block.z, BLOCK_AIR);
            has_selection = false;
            next_block_action_time = now + BLOCK_ACTION_INTERVAL;
            next_block_break_time = now + BLOCK_BREAK_INTERVAL;
            arm_swing_time = ARM_SWING_DURATION;
            return;
        }
        int target_slot = -1;
        for (int i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
            if (inventory[i].block == block && inventory[i].count < 64) {
                target_slot = i;
                break;
            }
        }
        if (target_slot < 0) {
            for (int i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
                if (inventory[i].block == BLOCK_AIR || inventory[i].count == 0) {
                    target_slot = i;
                    break;
                }
            }
        }
        if (target_slot >= 0) {
            if (multiplayer_world_active) {
                char packet[64];
                snprintf(packet, sizeof packet, "BREAK %d %d %d\n",
                         selected_block.x, selected_block.y, selected_block.z);
                if (send_server_line(packet)) {
                    has_selection = false;
                    next_block_action_time = now + BLOCK_ACTION_INTERVAL;
                    next_block_break_time = now + BLOCK_BREAK_INTERVAL;
                    arm_swing_time = ARM_SWING_DURATION;
                }
                return;
            }
            if (inventory[target_slot].block == BLOCK_AIR) {
                inventory[target_slot].block = block;
            }
            ++inventory[target_slot].count;
            set_block(selected_block.x, selected_block.y, selected_block.z, BLOCK_AIR);
            has_selection = false;
            next_block_action_time = now + BLOCK_ACTION_INTERVAL;
            next_block_break_time = now + BLOCK_BREAK_INTERVAL;
            arm_swing_time = ARM_SWING_DURATION;
            return;
        }
    }

    if (glfwGetMouseButton(glfwGetCurrentContext(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        if (place_pos.x >= 0 && place_pos.y >= 0 && place_pos.z >= 0) {
            if (!is_solid(place_pos.x, place_pos.y, place_pos.z)) {
                if (!block_overlaps_player(player, place_pos)) {
                    InventorySlot *slot = &inventory[selected_hotbar_slot];
                    if (slot->block != BLOCK_AIR && slot->count > 0) {
                        if (multiplayer_world_active) {
                            char packet[64];
                            snprintf(packet, sizeof packet, "PLACE %d %d %d %u\n",
                                     place_pos.x, place_pos.y, place_pos.z,
                                     (unsigned int)slot->block);
                            if (send_server_line(packet)) {
                                next_block_action_time = now + BLOCK_ACTION_INTERVAL;
                                arm_swing_time = ARM_SWING_DURATION;
                            }
                            return;
                        }
                        set_block(place_pos.x, place_pos.y, place_pos.z, slot->block);
                        if (game_mode != GAME_MODE_CREATIVE) {
                            --slot->count;
                            if (slot->count == 0) {
                                slot->block = BLOCK_AIR;
                            }
                        }
                        next_block_action_time = now + BLOCK_ACTION_INTERVAL;
                        arm_swing_time = ARM_SWING_DURATION;
                    }
                }
            }
        }
    }
}

static void select_inventory_slot_at_cursor(GLFWwindow *window) {
    double mouse_window_x;
    double mouse_window_y;
    int window_width;
    int window_height;
    int framebuffer_width;
    int framebuffer_height;
    glfwGetCursorPos(window, &mouse_window_x, &mouse_window_y);
    glfwGetWindowSize(window, &window_width, &window_height);
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    if (window_width <= 0 || window_height <= 0) return;

    float mouse_x_fb = (float)(mouse_window_x * framebuffer_width / window_width);
    float mouse_y_fb = (float)(mouse_window_y * framebuffer_height / window_height);
    float panel_x = ((float)framebuffer_width - (3.0f * 62.0f + 32.0f)) * 0.5f;
    float panel_y = ((float)framebuffer_height - (3.0f * 62.0f + 54.0f)) * 0.5f;
    float grid_x = panel_x + 16.0f;
    float grid_y = panel_y + 24.0f;

    for (int i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
        int column = i % 3;
        int row = i / 3;
        float x = grid_x + column * 62.0f;
        float y = grid_y + row * 62.0f;
        if (mouse_x_fb >= x && mouse_x_fb <= x + 56.0f
            && mouse_y_fb >= y && mouse_y_fb <= y + 56.0f) {
            selected_hotbar_slot = i;
            return;
        }
    }
}

int main(void) {
    srand((unsigned int)time(NULL));
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    GLFWwindow *window = glfwCreateWindow(1920, 1080, "OpenGL Minecraft-like Demo", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create window. Ensure the display server is available and the required GLFW/OpenGL DLLs are on PATH.\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, handle_key_input);
    glfwSetCharCallback(window, handle_char_input);
    glfwSetScrollCallback(window, handle_scroll_input);
    glfwSwapInterval(1);
    glClearColor(0.53f, 0.77f, 1.0f, 1.0f);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    int window_width;
    int window_height;
    glfwGetWindowSize(window, &window_width, &window_height);
    mouse_x = (double)window_width * 0.5;
    mouse_y = (double)window_height * 0.5;
    glfwSetCursorPos(window, mouse_x, mouse_y);

    grass_texture = load_png_texture(GRASS_TEXTURE_PATH);
    dirt_texture = load_png_texture(DIRT_TEXTURE_PATH);
    stone_texture = load_png_texture(STONE_TEXTURE_PATH);
    load_user_skin_selection();
    load_player_model("model.gltf");
    Player player = {
        .pos = { WORLD_X * 0.5f, 4.0f, WORLD_Z * 0.5f },
        .vel = { 0.0f, 0.0f, 0.0f },
        .horizontal_velocity = { 0.0f, 0.0f, 0.0f },
        .yaw = 0.0f,
        .pitch = 0.0f,
        .on_ground = true
    };
    if (!load_world(&player)) {
        fprintf(stderr, "Generating and saving a new local world\n");
        regenerate_world(&player);
        save_world(&player);
    }
    active_player = &player;
    load_chat_input_history();

    double previous_frame_time = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double current_frame_time = glfwGetTime();
        float frame_time = (float)(current_frame_time - previous_frame_time);
        previous_frame_time = current_frame_time;
        if (multiplayer_connected) {
            poll_server_messages();
        }
        if (frame_time < 0.0f) frame_time = 0.0f;
        if (frame_time > 0.05f) frame_time = 0.05f;
        if (arm_swing_time > 0.0f) {
            arm_swing_time -= frame_time;
            if (arm_swing_time < 0.0f) arm_swing_time = 0.0f;
        }
        bool escape_pressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        if (escape_pressed && !escape_key_down) {
            if (home_screen) {
                if (online_settings_open) {
                    online_settings_open = false;
                } else {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
            } else if (chat_open) {
                if (!is_ime_composing(window)) {
                    chat_open = false;
                    chat_input_length = 0;
                    chat_input[0] = '\0';
                    glfwGetCursorPos(window, &mouse_x, &mouse_y);
                }
            } else if (inventory_open) {
                inventory_open = false;
                paused = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            } else {
                paused = !paused;
                glfwSetInputMode(window, GLFW_CURSOR,
                                 paused ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            }
            if (!paused) resume_game(window);
        }
        escape_key_down = escape_pressed;

        if (home_screen) {
            bool mouse_pressed =
                glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            if (mouse_pressed && !home_mouse_down) {
                float x;
                float y;
                if (get_cursor_framebuffer_position(window, &x, &y)) {
                    int framebuffer_width;
                    int framebuffer_height;
                    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
                    float center_x = (float)framebuffer_width * 0.5f;
                    float center_y = (float)framebuffer_height * 0.5f;
                    if (online_settings_open) {
                        if (x >= center_x - 190.0f && x <= center_x + 190.0f) {
                            if (y >= center_y - 125.0f && y <= center_y - 81.0f) {
                                online_settings_field = 0;
                            } else if (y >= center_y - 50.0f && y <= center_y - 6.0f) {
                                online_settings_field = 1;
                            } else if (y >= center_y + 25.0f && y <= center_y + 69.0f) {
                                online_settings_field = 2;
                            }
                        }
                        if (x >= center_x - 140.0f && x <= center_x + 140.0f
                            && y >= center_y + 92.0f && y <= center_y + 140.0f) {
                            choose_player_skin(window);
                        } else if (x >= center_x - 190.0f && x <= center_x - 10.0f
                                   && y >= center_y + 160.0f && y <= center_y + 208.0f) {
                            save_online_settings();
                        } else if (x >= center_x + 10.0f && x <= center_x + 190.0f
                                   && y >= center_y + 160.0f && y <= center_y + 208.0f) {
                            online_settings_open = false;
                        }
                    } else if (x >= center_x - 140.0f && x <= center_x + 140.0f
                        && y >= center_y - 125.0f && y <= center_y - 67.0f) {
                        start_game(window);
                    } else if (x >= center_x - 140.0f && x <= center_x + 140.0f
                               && y >= center_y - 55.0f && y <= center_y + 3.0f) {
                        if (!save_world(&player)) {
                            show_error(window, "World save failed",
                                       "Could not save your single-player world before joining.");
                        } else if (connect_to_server(&player)) {
                            start_game(window);
                        } else {
                            if (!load_world(&player)) {
                                fprintf(stderr, "Regenerating and saving local world after join failure\n");
                                regenerate_world(&player);
                                save_world(&player);
                            }
                            show_warning(window, "Unable to join server", network_error);
                        }
                    } else if (x >= center_x - 140.0f && x <= center_x + 140.0f
                               && y >= center_y + 15.0f && y <= center_y + 73.0f) {
                        open_online_settings();
                    } else if (x >= center_x - 140.0f && x <= center_x + 140.0f
                               && y >= center_y + 85.0f && y <= center_y + 143.0f) {
                        glfwSetWindowShouldClose(window, GLFW_TRUE);
                    }
                }
            }
            home_mouse_down = mouse_pressed;
            draw_world(&player);
            glfwSwapBuffers(window);
            glfwPollEvents();
            continue;
        }

        bool camera_pressed = (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS);
        if (!paused && !inventory_open && !chat_open && camera_pressed && !camera_key_down) {
            camera_mode = (CameraMode)((camera_mode + 1) % 3);
            camera_key_down = true;
        }
        if (!camera_pressed) {
            camera_key_down = false;
        }

        for (int i = 0; i <= GLFW_KEY_LAST; ++i) {
            keys[i] = (glfwGetKey(window, i) == GLFW_PRESS);
        }

        bool space_down = keys[GLFW_KEY_SPACE];
        if (space_down && !flight_space_was_down && !paused && !inventory_open && !chat_open
            && game_mode == GAME_MODE_CREATIVE) {
            double now = glfwGetTime();
            if (last_flight_space_press >= 0.0 && now - last_flight_space_press <= 0.35) {
                creative_flying = !creative_flying;
                player.vel.y = 0.0f;
                player.on_ground = false;
                add_chat_line(creative_flying ? "Fly mode: ON" : "Fly mode: OFF");
                last_flight_space_press = -1.0;
            } else {
                last_flight_space_press = now;
            }
        }
        flight_space_was_down = space_down;

        if (!chat_open) {
            for (int slot = 0; slot < INVENTORY_SLOT_COUNT; ++slot) {
                if (keys[GLFW_KEY_1 + slot]) {
                    selected_hotbar_slot = slot;
                }
            }
        }

        bool drop_pressed = keys[GLFW_KEY_Q];
        if (!paused && !chat_open && drop_pressed && !drop_key_down) {
            drop_selected_item(&player);
        }
        drop_key_down = drop_pressed;

        bool mouse_left_pressed =
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (paused) {
            if (mouse_left_pressed && !left_mouse_down) {
                float x;
                float y;
                if (get_cursor_framebuffer_position(window, &x, &y)) {
                    int framebuffer_width;
                    int framebuffer_height;
                    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
                    float center_x = (float)framebuffer_width * 0.5f;
                    float center_y = (float)framebuffer_height * 0.5f;
                    if (x >= center_x - 140.0f && x <= center_x + 140.0f
                        && y >= center_y - 105.0f && y <= center_y - 47.0f) {
                        resume_game(window);
                    } else if (x >= center_x - 140.0f && x <= center_x + 140.0f
                               && y >= center_y - 30.0f && y <= center_y + 28.0f) {
                        choose_player_skin(window);
                    } else if (x >= center_x - 140.0f && x <= center_x + 140.0f
                               && y >= center_y + 45.0f && y <= center_y + 103.0f) {
                        return_to_home(window);
                    }
                }
            }
            left_mouse_down = mouse_left_pressed;
        } else if (chat_open) {
            left_mouse_down = mouse_left_pressed;
        } else if (inventory_open) {
            if (mouse_left_pressed && !left_mouse_down) {
                select_inventory_slot_at_cursor(window);
            }
            move_player(&player, frame_time);
            update_dropped_items(&player, frame_time);
        } else {
            handle_mouse_input(window, &player);
            update_selection(&player);
            apply_mouse_clicks(&player);
            move_player(&player, frame_time);
            update_dropped_items(&player, frame_time);
        }
        left_mouse_down = mouse_left_pressed;

        update_player_animation(&player, frame_time,
                                !paused && !inventory_open && !chat_open);
        for (int i = 0; i < MAX_REMOTE_PLAYERS; ++i) {
            RemotePlayer *remote = &remote_players[i];
            if (!remote->active) continue;
            if (current_frame_time - remote->last_position_time > 0.2) {
                remote->player.horizontal_velocity = vec3(0.0f, 0.0f, 0.0f);
                remote->player.vel.y = 0.0f;
                remote->player.on_ground = true;
            }
            update_player_animation(&remote->player, frame_time, true);
        }

        if (multiplayer_connected && current_frame_time >= next_network_position_time) {
            char packet[160];
            snprintf(packet, sizeof packet, "POS %.4f %.4f %.4f %.5f %.5f\n",
                     player.pos.x, player.pos.y, player.pos.z, player.yaw, player.pitch);
            if (!send_server_line(packet)) {
                add_chat_line("Unable to update player position");
                disconnect_from_server();
            } else {
                next_network_position_time = current_frame_time + 0.05;
            }
        }

        draw_world(&player);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    if (multiplayer_world_active) {
        disconnect_from_server();
        multiplayer_world_active = false;
    } else {
        save_world(&player);
    }
    save_chat_input_history();
#ifdef _WIN32
    if (chat_text_dc != NULL && chat_text_old_bitmap != NULL) {
        SelectObject(chat_text_dc, chat_text_old_bitmap);
    }
    if (chat_text_font != NULL) DeleteObject(chat_text_font);
    if (chat_text_bitmap != NULL) DeleteObject(chat_text_bitmap);
    if (chat_text_dc != NULL) DeleteDC(chat_text_dc);
#else
    if (chat_font_face != NULL) FT_Done_Face(chat_font_face);
    if (chat_font_library != NULL) FT_Done_FreeType(chat_font_library);
#endif
    if (chat_text_texture != 0) glDeleteTextures(1, &chat_text_texture);
    if (player_model_texture != 0) glDeleteTextures(1, &player_model_texture);
    free(player_model_buffer);
#ifdef _WIN32
    if (winsock_started) WSACleanup();
#endif
    glfwTerminate();
    return 0;
}
