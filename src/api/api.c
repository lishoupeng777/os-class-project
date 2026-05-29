#include "vfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

#define API_PORT 8888
#define BUFFER_SIZE 4096

typedef struct {
    int socket;
    char buffer[BUFFER_SIZE];
} api_connection;

void send_json_response(api_connection *conn, const char *json) {
    char response[8192];
    int len = (int)strlen(json);
    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n%s", len, json);
    
#ifdef _WIN32
    send(conn->socket, response, strlen(response), 0);
#else
    write(conn->socket, response, strlen(response));
#endif
}

void send_error(api_connection *conn, const char *error) {
    char json[512];
    snprintf(json, sizeof(json), "{\"error\": \"%s\"}", error);
    send_json_response(conn, json);
}

void handle_get_status(api_connection *conn) {
    char json[2048];
    super_block *sb = (super_block *)virtual_disk;
    
    int used_blocks = sb->fsize - sb->ffree_num;
    int used_inodes = sb->isize - sb->ifree_num;
    
    snprintf(json, sizeof(json),
        "{"
        "\"total_blocks\": %d,"
        "\"used_blocks\": %d,"
        "\"total_inodes\": %d,"
        "\"used_inodes\": %d,"
        "\"current_user\": \"%s\""
        "}",
        FILEBLK, used_blocks,
        sb->isize, used_inodes,
        current_user ? current_user->u_name : "none");
    
    send_json_response(conn, json);
}

void handle_list_dir(api_connection *conn) {
    if (current_user == NULL) {
        send_error(conn, "Not logged in");
        return;
    }
    
    // 简化实现：返回示例数据
    char json[2048] = 
        "{"
        "\"entries\": ["
        "{"
        "\"name\": \".\", \"type\": \"dir\", \"size\": 32, \"owner\": 0, \"perm\": \"rwx\""
        "},"
        "{"
        "\"name\": \"..\", \"type\": \"dir\", \"size\": 32, \"owner\": 0, \"perm\": \"rwx\""
        "}"
        "]"
        "}";
    send_json_response(conn, json);
}

void handle_get_pwd(api_connection *conn) {
    if (current_user == NULL) {
        send_error(conn, "Not logged in");
        return;
    }
    
    char json[512];
    snprintf(json, sizeof(json),
        "{\"cwd\": \"/\", \"user\": \"%s\", \"uid\": %d}",
        current_user->u_name, current_user->u_uid);
    send_json_response(conn, json);
}

void handle_request(api_connection *conn, char *request) {
    if (strstr(request, "GET /api/status")) {
        handle_get_status(conn);
    } else if (strstr(request, "GET /api/dir")) {
        handle_list_dir(conn);
    } else if (strstr(request, "GET /api/pwd")) {
        handle_get_pwd(conn);
    } else {
        send_error(conn, "Unknown endpoint");
    }
}

int api_start() {
#ifdef _WIN32
    WSADATA wsa;
    typedef int socklen_t;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return -1;
    }
#else
    #include <sys/types.h>
#endif
    
    int server_socket;
    struct sockaddr_in server_addr;
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port = htons(API_PORT);
    
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        return -1;
    }
    
    listen(server_socket, 5);
    printf("API Server running on http://localhost:%d\n", API_PORT);
    
    while (1) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, (socklen_t *)&client_len);
        
        if (client_socket < 0) continue;
        
        api_connection conn;
        conn.socket = client_socket;
        memset(conn.buffer, 0, BUFFER_SIZE);
        
#ifdef _WIN32
        recv(client_socket, conn.buffer, BUFFER_SIZE - 1, 0);
        handle_request(&conn, conn.buffer);
        closesocket(client_socket);
#else
        read(client_socket, conn.buffer, BUFFER_SIZE - 1);
        handle_request(&conn, conn.buffer);
        close(client_socket);
#endif
    }
    
    return 0;
}
