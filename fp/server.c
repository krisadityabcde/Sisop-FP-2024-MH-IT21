// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <crypt.h>
#include <stdbool.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 10240
#define USERS_FILE "/home/rafaelega24/SISOP/FP/DiscorIT/users.csv"
#define CHANNELS_FILE "/home/rafaelega24/SISOP/FP/DiscorIT/channels.csv"

typedef struct {
    int socket;
    struct sockaddr_in address;
    char logged_in_user[50];
    char logged_in_role[10];
    char logged_in_channel[50];
    char logged_in_room[50];
} client_info;

client_info *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg);
void daemonize();

void register_user(const char *username, const char *password, client_info *client);
void login_user(const char *username, const char *password, client_info *client);

void create_directory(const char *path, client_info *client);
void create_channel(const char *username, const char *channel, const char *key, client_info *client);
void create_room(const char *username, const char *channel, const char *room, client_info *client);
void list_channels(client_info *client);
void list_rooms(const char *channel, client_info *client);
void list_users(const char *channel, client_info *client);
void join_channel(const char *username, const char *channel, client_info *client);
void verify_key(const char *username, const char *channel, const char *key, client_info *client);
void join_room(const char *channel, const char *room, client_info *client);
void send_chat(const char *username, const char *channel, const char *room, const char *message, client_info *client);
void see_chat(const char *channel, const char *room, client_info *client);
void edit_chat(const char *channel, const char *room, int id_chat, const char *new_text, client_info *client);
void edit_channel(const char *old_channel, const char *new_channel, client_info *client);
void edit_room(const char *channel, const char *old_room, const char *new_room, client_info *client);
void edit_profile_self(const char *username, const char *new_value, bool is_password, client_info *client);

//khusus root
void list_users_root(client_info *client);
void edit_user(const char *target_user, const char *new_value, bool is_password, client_info *client);

void handle_exit(client_info *client);

int main() {
    daemonize();

    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);
    pthread_t tid;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("Gagal membuat socket");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Gagal bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Gagal listen");
        exit(EXIT_FAILURE);
    }

    printf("Server berjalan sebagai daemon pada port %d\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addr_len)) < 0) {
            perror("Gagal melakukan accept");
            exit(EXIT_FAILURE);
        }

        pthread_t tid;
        client_info *client = (client_info *)malloc(sizeof(client_info));
        client->socket = new_socket;
        client->address = address;
        memset(client->logged_in_user, 0, sizeof(client->logged_in_user));
        memset(client->logged_in_role, 0, sizeof(client->logged_in_role));
        memset(client->logged_in_channel, 0, sizeof(client->logged_in_channel));
        memset(client->logged_in_room, 0, sizeof(client->logged_in_room));

        pthread_create(&tid, NULL, handle_client, (void *)client);
    }

    return 0;
}

void daemonize() {
    pid_t pid, sid;

    pid = fork();

    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);

    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    int log_fd = open("/tmp/server.log", O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (log_fd < 0) {
        exit(EXIT_FAILURE);
    }
    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);
}

void *handle_client(void *arg) {
    client_info *cli = (client_info *)arg;
    char buffer[BUFFER_SIZE];
    int n;

    while ((n = read(cli->socket, buffer, sizeof(buffer))) > 0) {
        buffer[n] = '\0';
        printf("Pesan dari client: %s\n", buffer);

        char *token = strtok(buffer, " ");
        if (token == NULL) {
            char response[] = "Perintah tidak dikenali";
            if (write(cli->socket, response, strlen(response)) < 0) {
                perror("Gagal mengirim respons ke client");
            }
            continue;
        }

        if (strcmp(token, "REGISTER") == 0) {
            char *username = strtok(NULL, " ");
            char *password = strtok(NULL, " ");
            register_user(username, password, cli);
        } else if (strcmp(token, "LOGIN") == 0) {
            char *username = strtok(NULL, " ");
            char *password = strtok(NULL, " ");
            if (username == NULL || password == NULL) {
                char response[] = "Format perintah LOGIN tidak valid";
                if (write(cli->socket, response, strlen(response)) < 0) {
                    perror("Gagal mengirim respons ke client");
                }
                continue;
            }
            login_user(username, password, cli);
        } else if (strcmp(token, "CREATE") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                char response[] = "Format perintah CREATE tidak valid";
                if (write(cli->socket, response, strlen(response)) < 0) {
                    perror("Gagal mengirim respons ke client");
                }
                continue;
            }
            if (strcmp(token, "CHANNEL") == 0) {
                char *channel = strtok(NULL, " ");
                token = strtok(NULL, " ");
                char *key = strtok(NULL, " ");
                if (channel == NULL || key == NULL) {
                    char response[] = "Penggunaan perintah: CREATE CHANNEL <channel> -k <key>";
                    if (write(cli->socket, response, strlen(response)) < 0) {
                        perror("Gagal mengirim respons ke client");
                    }
                    continue;
                }
                create_channel(cli->logged_in_user, channel, key, cli);
            } else if (strcmp(token, "ROOM") == 0) {
                char *room = strtok(NULL, " ");
                if (room == NULL) {
                    char response[] = "Penggunaan perintah: CREATE ROOM <room>";
                    if (write(cli->socket, response, strlen(response)) < 0) {
                        perror("Gagal mengirim respons ke client");
                    }
                    continue;
                }
                create_room(cli->logged_in_user, cli->logged_in_channel, room, cli);
            } else {
                char response[] = "Format perintah CREATE tidak valid";
                if (write(cli->socket, response, strlen(response)) < 0) {
                    perror("Gagal mengirim respons ke client");
                }
            }
        } else if(strcmp(token, "LIST") == 0){
            token = strtok(NULL, " ");
            if (token == NULL) {
                char response[] = "Format perintah LIST tidak valid";
                if (write(cli->socket, response, strlen(response)) < 0) {
                    perror("Gagal mengirim respons ke client");
                }
                continue;
            }
            if (strcmp(token, "CHANNEL") == 0) {
                list_channels(cli);
            } else if (strcmp(token, "ROOM") == 0) {
                list_rooms(cli->logged_in_channel, cli);
            } else if (strcmp(token, "USER") == 0) {
                strstr(cli->logged_in_role, "ROOT") != NULL ? list_users_root(cli) : list_users(cli->logged_in_channel, cli);
            } else {
                char response[] = "Format perintah LIST tidak valid";
                if (write(cli->socket, response, strlen(response)) < 0) {
                    perror("Gagal mengirim respons ke client");
                }
            }
        } else if (strcmp(token, "JOIN") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                char response[] = "Format perintah JOIN tidak valid";
                if (write(cli->socket, response, strlen(response)) < 0) {
                    perror("Gagal mengirim respons ke client");
                }
                continue;
            }
            if (strlen(cli->logged_in_channel) == 0) {
                char *channel = token;
                join_channel(cli->logged_in_user, channel, cli);
            } else {
                char *room = token;
                join_room(cli->logged_in_channel, room, cli);
            }
        } else if (strcmp(token, "CHAT") == 0) {
            char *message = strtok(NULL, "\"");
            if (message == NULL || strlen(cli->logged_in_channel) == 0 || strlen(cli->logged_in_room) == 0) {
                char response[] = "Format perintah CHAT tidak valid atau anda belum tergabung dalam room";
                if (write(cli->socket, response, strlen(response)) < 0) {
                    perror("Gagal mengirim respons ke client");
                }
                continue;
            }
            send_chat(cli->logged_in_user, cli->logged_in_channel, cli->logged_in_room, message, cli);
        } else if (strcmp(token, "SEE") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL || strcmp(token, "CHAT") != 0 || strlen(cli->logged_in_channel) == 0 || strlen(cli->logged_in_room) == 0) {
                char response[] = "Format perintah SEE CHAT tidak valid atau anda belum tergabung dalam room";
                if (write(cli->socket, response, strlen(response)) < 0) {
                    perror("Gagal mengirim respons ke client");
                }
                continue;
            }
            see_chat(cli->logged_in_channel, cli->logged_in_room, cli);
        } else if (strcmp(token, "EDIT") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                char response[] = "Format perintah EDIT tidak valid";
                if (write(cli->socket, response, strlen(response)) < 0) {
                    perror("Gagal mengirim respons ke client");
                }
                continue;
            }
            if (strcmp(token, "CHAT") == 0) {
                char *id_str = strtok(NULL, " ");
                char *new_text = strtok(NULL, "\"");
                if (id_str == NULL || new_text == NULL) {
                    char response[] = "Penggunaan perintah: EDIT CHAT <id> \"<text>\"";
                    if (write(cli->socket, response, strlen(response)) < 0) {
                        perror("Gagal mengirim respons ke client");
                    }
                    continue;
                }
                int id_chat = atoi(id_str);
                edit_chat(cli->logged_in_channel, cli->logged_in_room, id_chat, new_text, cli);
            } else if (strcmp(token, "CHANNEL") == 0) {
                char *old_channel = strtok(NULL, " ");
                strtok(NULL, " ");  // skip "TO"
                char *new_channel = strtok(NULL, " ");
                if (old_channel == NULL || new_channel == NULL) {
                    char response[] = "Penggunaan perintah: EDIT CHANNEL <old_channel> TO <new_channel>";
                    if (write(cli->socket, response, strlen(response)) < 0) {
                        perror("Gagal mengirim respons ke client");
                    }
                    continue;
                }
                if(strlen(cli->logged_in_channel) > 0 || strlen(cli->logged_in_room) > 0){
                    char response[] = "Anda harus keluar dari channel";
                    if (write(cli->socket, response, strlen(response)) < 0) {
                        perror("Gagal mengirim respons ke client");
                    }
                    continue;
                }else{
                    edit_channel(old_channel, new_channel, cli);
                }
            } else if (strcmp(token, "ROOM") == 0) {
                char *old_room = strtok(NULL, " ");
                strtok(NULL, " ");  // skip "TO"
                char *new_room = strtok(NULL, " ");
                if (old_room == NULL || new_room == NULL) {
                    char response[] = "Penggunaan perintah: EDIT ROOM <old_room> TO <new_room>";
                    if (write(cli->socket, response, strlen(response)) < 0) {
                        perror("Gagal mengirim respons ke client");
                    }
                    continue;
                }
                if(strlen(cli->logged_in_room) > 0){
                    char response[] = "Anda harus keluar dari room";
                    if (write(cli->socket, response, strlen(response)) < 0) {
                        perror("Gagal mengirim respons ke client");
                    }
                    continue;
                }else{
                    edit_room(cli->logged_in_channel, old_room, new_room, cli);
                }
            } else if (strcmp(token, "PROFILE") == 0) {
                strtok(NULL, " ");  // skip "SELF"
                char *option = strtok(NULL, " ");
                char *new_value = strtok(NULL, " ");
                if (option == NULL || new_value == NULL || strcmp(option, "-u") != 0 && strcmp(option, "-p") != 0) {
                    char response[] = "Penggunaan perintah: EDIT PROFILE SELF -u <new_username> atau -p <new_password>";
                    if (write(cli->socket, response, strlen(response)) < 0) {
                        perror("Gagal mengirim respons ke client");
                    }
                    continue;
                }
                bool is_password = (strcmp(option, "-p") == 0);
                edit_profile_self(cli->logged_in_user, new_value, is_password, cli);
            } else if (strcmp(token, "WHERE") == 0) {
                char *target_user = strtok(NULL, " ");
                char *option = strtok(NULL, " ");
                char *new_value = strtok(NULL, " ");
                if (target_user == NULL || option == NULL || new_value == NULL) {
                    char response[] = "Penggunaan perintah: EDIT WHERE <username> -u <new_username> atau -p <new_password>";
                    if (write(cli->socket, response, strlen(response)) < 0) {
                        perror("Gagal mengirim respons ke client");
                    }
                    continue;
                }
                bool is_password = (strcmp(option, "-p") == 0);
                edit_user(target_user, new_value, is_password, cli);
            } else {
                char response[] = "Format perintah EDIT tidak valid";
                if (write(cli->socket, response, strlen(response)) < 0) {
                    perror("Gagal mengirim respons ke client");
                }
            }
        } else if(strcmp(token, "EXIT") == 0) {
            handle_exit(cli);
        } else {
            char response[] = "Perintah tidak dikenali";
            if (write(cli->socket, response, strlen(response)) < 0) {
                perror("Gagal mengirim respons ke client");
            }
        }
    }

    close(cli->socket);
    free(cli);
    pthread_exit(NULL);
}

void create_directory(const char *path, client_info *client) {
    struct stat st = {0};

    if (stat(path, &st) == -1) {
        if (mkdir(path, 0700) < 0) {
            char response[] = "Gagal membuat direktori";
            if (write(client->socket, response, strlen(response)) < 0) {
                perror("Gagal mengirim respons ke client");
            }
        }
    }
}

void register_user(const char *username, const char *password, client_info *client) {
    if (username == NULL || password == NULL) {
        char response[] = "Username atau password tidak boleh kosong";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }
    create_directory("/home/rafaelega24/SISOP/FP/DiscorIT", client);

    FILE *file = fopen(USERS_FILE, "r+");
    if (!file) {
        file = fopen(USERS_FILE, "w+");
        if (!file) {
            perror("Tidak dapat membuka atau membuat file");
            char response[] = "Tidak dapat membuka atau membuat file users.csv";
            if (write(client->socket, response, strlen(response)) < 0) {
                perror("Gagal mengirim respons ke client");
            }
            return;
        }
    }

    char line[256];
    bool user_exists = false;
    int user_count = 0;

    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token && strcmp(token, username) == 0) {
            user_exists = true;
            break;
        }
        user_count++;
    }

    if (user_exists) {
        char response[100];
        snprintf(response, sizeof(response), "%s sudah terdaftar", username);
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        fclose(file);
        return;
    }

    fseek(file, 0, SEEK_END);

    // Generate salt for bcrypt
    char salt[30];
    snprintf(salt, sizeof(salt), "$2y$12$%.22s", "inistringsaltuserpassword");

    char *hash = crypt(password, salt);
    if (hash == NULL) {
        char response[] = "Gagal membuat hash password";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        fclose(file);
        return;
    }

    fprintf(file, "%d,%s,%s,%s\n", user_count + 1, username, hash, user_count == 0 ? "ROOT" : "USER");
    fclose(file);

    char response[100];
    snprintf(response, sizeof(response), "%s berhasil register", username);
    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }
}

void login_user(const char *username, const char *password, client_info *client) {
    FILE *file = fopen(USERS_FILE, "r");
    if (!file) {
        char response[] = "Tidak dapat membuka file users.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char line[256];
    bool user_found = false;

    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, ",");
        token = strtok(NULL, ",");
        if (token && strcmp(token, username) == 0) {
            user_found = true;
            token = strtok(NULL, ","); // Hash password
            char *stored_hash = token;

            char *hash = crypt(password, stored_hash);
            if (strcmp(hash, stored_hash) == 0) {
                snprintf(client->logged_in_user, sizeof(client->logged_in_user), "%s", username);
                token = strtok(NULL, ","); // Role
                snprintf(client->logged_in_role, sizeof(client->logged_in_role), "%s", token);

                char response[BUFFER_SIZE];
                snprintf(response, sizeof(response), "%s berhasil login", username);
                if (write(client->socket, response, strlen(response)) < 0) {
                    perror("Gagal mengirim respons ke client");
                }
            } else {
                char response[] = "Password salah";
                if (write(client->socket, response, strlen(response)) < 0) {
                    perror("Gagal mengirim respons ke client");
                }
            }
            break;
        }
    }

    if (!user_found) {
        char response[] = "Username tidak ditemukan";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    }

    fclose(file);
}

void create_channel(const char *username, const char *channel, const char *key, client_info *client) {
    FILE *channels_file = fopen(CHANNELS_FILE, "r+");
    if (!channels_file) {
        channels_file = fopen(CHANNELS_FILE, "w+");
        if (!channels_file) {
            perror("Tidak dapat membuka atau membuat file channels");
            return;
        }
    }

    char line[256];
    bool channel_exists = false;
    int channel_count = 0;

    while (fgets(line, sizeof(line), channels_file)) {
        char *token = strtok(line, ",");
        token = strtok(NULL, ",");
        if (token && strcmp(token, channel) == 0) {
            channel_exists = true;
            break;
        }
        channel_count++;
    }

    if (channel_exists) {
        char response[100];
        snprintf(response, sizeof(response), "Channel %s sudah ada silakan cari nama lain", channel);
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        fclose(channels_file);
        return;
    }

    fseek(channels_file, 0, SEEK_END);

    char salt[30];
    snprintf(salt, sizeof(salt), "$2y$12$%.22s", "inistringsaltchannelkey");

    char *hash = crypt(key, salt);

    fprintf(channels_file, "%d,%s,%s\n", channel_count + 1, channel, hash);
    fclose(channels_file);

    char path[256];
    snprintf(path, sizeof(path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s", channel);
    create_directory(path, client);

    snprintf(path, sizeof(path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/admin", channel);
    create_directory(path, client);

    snprintf(path, sizeof(path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/admin/auth.csv", channel);
    FILE *auth_file = fopen(path, "w+");
    if (auth_file) {
        // Get user id from users.csv
        char user_id[10];
        FILE *users_file = fopen(USERS_FILE, "r");
        if (!users_file) {
            char response[] = "Gagal membuka file users.csv";
            if (write(client->socket, response, strlen(response)) < 0) {
                perror("Gagal mengirim respons ke client");
            }
            fclose(auth_file);
            return;
        }

        char user_line[256];
        bool user_found = false;

        while (fgets(user_line, sizeof(user_line), users_file)) {
            char *token = strtok(user_line, ",");
            strcpy(user_id, token);
            token = strtok(NULL, ",");
            if (token && strcmp(token, username) == 0) {
                user_found = true;
                break;
            }
        }

        fclose(users_file);

        if (user_found) {
            fprintf(auth_file, "%s,%s,ADMIN\n", user_id, username);
        } else {
            char response[] = "User tidak ditemukan";
            if (write(client->socket, response, strlen(response)) < 0) {
                perror("Gagal mengirim respons ke client");
            }
        }
        fclose(auth_file);
    } else {
        char response[] = "Gagal membuat file auth.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    }

    char response[100];
    snprintf(response, sizeof(response), "Channel %s dibuat", channel);
    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }
}

void create_room(const char *username, const char *channel, const char *room, client_info *client) {
    char auth_path[256];
    snprintf(auth_path, sizeof(auth_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/admin/auth.csv", channel);

    FILE *auth_file = fopen(auth_path, "r");
    if (!auth_file) {
        char response[] = "Gagal membuka file auth.csv atau anda tidak tergabung dalam channel";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char line[256];
    bool is_allowed = false;

    while (fgets(line, sizeof(line), auth_file)) {
        char *token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        if (strcmp(token, username) == 0) {
            token = strtok(NULL, ",");
            if (strstr(token, "ADMIN") != NULL || strstr(token, "ROOT") != NULL){
                is_allowed = true;
                break;
            }
        }
    }

    fclose(auth_file);

    if (!is_allowed) {
        char response[] = "Anda tidak memiliki izin untuk membuat room di channel ini";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char path[256];
    snprintf(path, sizeof(path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/%s", channel, room);
    create_directory(path, client);

    snprintf(path, sizeof(path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/%s/chat.csv", channel, room);
    FILE *chat_file = fopen(path, "w+");
    if(chat_file){
        fclose(chat_file);
    }else{
        char response[] = "Gagal membuat file chat.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }
    
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "Room %s dibuat", room);
    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }
}

void list_channels(client_info *client) {
    char path[256];
    strcpy(path, CHANNELS_FILE);
    FILE *channels_file = fopen(path, "r+");
    if (channels_file == NULL) {
        char response[] = "Gagal membuka file channels.csv atau belum ada channel yang dibuat";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char line[256];
    char response[BUFFER_SIZE] = "";

    while (fgets(line, sizeof(line), channels_file)) {
        char *token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        snprintf(response + strlen(response), sizeof(response) - strlen(response), "%s ", token);
    }

    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }

    fclose(channels_file);
}

void list_rooms(const char *channel, client_info *client) {
    char path[256];
    snprintf(path, sizeof(path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s", channel);
    DIR *dir = opendir(path);
    if (dir == NULL) {
        char response[] = "Gagal membuka direktori channel atau belum ada room yang dibuat";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    struct dirent *entry;
    char response[BUFFER_SIZE] = "";

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 && strcmp(entry->d_name, "admin") != 0) {
            char entry_path[512];
            snprintf(entry_path, sizeof(entry_path), "%s/%s", path, entry->d_name);
            struct stat entry_stat;
            if (stat(entry_path, &entry_stat) == 0 && S_ISDIR(entry_stat.st_mode)) {
                snprintf(response + strlen(response), sizeof(response) - strlen(response), "%s ", entry->d_name);
            }
        }
    }

    if (strlen(response) == 0) {
        snprintf(response, sizeof(response), "Tidak ada room yang ditemukan");
    }

    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }

    closedir(dir);
}

void list_users(const char *channel, client_info *client) {
    char path[256];
    snprintf(path, sizeof(path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/admin/auth.csv", channel);
    FILE *auth_file = fopen(path, "r+");
    if (auth_file == NULL) {
        char response[] = "Gagal membuka file auth.csv atau anda sedang tidak tergabung dalam channel";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char line[256];
    char response[BUFFER_SIZE] = "";

    while (fgets(line, sizeof(line), auth_file)) {
        char *token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        snprintf(response + strlen(response), sizeof(response) - strlen(response), "%s ", token);
    }

    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }

    fclose(auth_file);
}

void list_users_root(client_info *client) {
    FILE *users_file = fopen(USERS_FILE, "r+");
    if (users_file == NULL) {
        char response[] = "Gagal membuka file users.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char line[256];
    char response[BUFFER_SIZE] = "";

    while (fgets(line, sizeof(line), users_file)) {
        char *token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        snprintf(response + strlen(response), sizeof(response) - strlen(response), "%s ", token);
    }

    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }

    fclose(users_file);
}

void join_channel(const char *username, const char *channel, client_info *client) {
    // Check if the channel directory exists
    char channel_path[256];
    snprintf(channel_path, sizeof(channel_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s", channel);
    struct stat st;
    if (stat(channel_path, &st) == -1 || !S_ISDIR(st.st_mode)) {
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "Channel %s tidak ada", channel);
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    // Check if user is ROOT in users.csv
    FILE *users_file = fopen(USERS_FILE, "r");
    if (!users_file) {
        char response[] = "Gagal membuka file users.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char line[256];
    bool is_root = false;
    char user_id[10];

    while (fgets(line, sizeof(line), users_file)) {
        char *token = strtok(line, ",");
        strcpy(user_id, token);
        token = strtok(NULL, ",");
        if (token && strcmp(token, username) == 0) {
            token = strtok(NULL, ",");
            token = strtok(NULL, ",");
            if (strstr(token, "ROOT") != NULL){
                is_root = true;
            }
            break;
        }
    }

    fclose(users_file);

    if (is_root) {
        // If ROOT, join without further checks
        snprintf(client->logged_in_channel, sizeof(client->logged_in_channel), "%s", channel);

        // Ensure ROOT role is recorded in auth.csv
        char auth_path[256];
        snprintf(auth_path, sizeof(auth_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/admin/auth.csv", channel);
        FILE *auth_file = fopen(auth_path, "r+");
        if (auth_file) {
            bool root_exists = false;
            while (fgets(line, sizeof(line), auth_file)) {
                char *token = strtok(line, ",");
                if (token == NULL) continue;
                token = strtok(NULL, ",");
                if (token == NULL) continue;
                if (strcmp(token, username) == 0) {
                    root_exists = true;
                    break;
                }
            }

            if (!root_exists) {
                auth_file = fopen(auth_path, "a");
                if (auth_file) {
                    fprintf(auth_file, "%s,%s,ROOT\n", user_id, username);
                    fclose(auth_file);
                }
            } else {
                fclose(auth_file);
            }
        } else {
            char response[] = "Gagal membuka file auth.csv";
            if (write(client->socket, response, strlen(response)) < 0) {
                perror("Gagal mengirim respons ke client");
            }
            return;
        }

        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "[%s/%s]", username, channel);
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    // Check if user is ADMIN or already USER in auth.csv
    char auth_path[256];
    snprintf(auth_path, sizeof(auth_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/admin/auth.csv", channel);
    FILE *auth_file = fopen(auth_path, "r");
    if (!auth_file) {
        char response[] = "Gagal membuka file auth.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    bool is_admin = false;
    bool is_user = false;

    while (fgets(line, sizeof(line), auth_file)) {
        char *token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        if (strcmp(token, username) == 0) {
            token = strtok(NULL, ",");
            if (strstr(token, "ADMIN") != NULL) {
                is_admin = true;
                break;
            } else if (strstr(token, "USER") != NULL){
                is_user = true;
                break;
            }
        }
    }

    fclose(auth_file);

    if (is_admin || is_user) {
        snprintf(client->logged_in_channel, sizeof(client->logged_in_channel), "%s", channel);
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "[%s/%s]", username, channel);
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return; // ADMIN or already registered USER joined without further checks
    } else {
        // If not ROOT, ADMIN, or already registered USER, prompt for key
        char response[] = "Key: ";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }

        char key[BUFFER_SIZE];
        memset(key, 0, sizeof(key));

        if (recv(client->socket, key, sizeof(key), 0) < 0) {
            perror("Gagal menerima key dari client");
            return;
        }
        verify_key(username, channel, key, client);
    }
}

void verify_key(const char *username, const char *channel, const char *key, client_info *client) {
    FILE *channels_file = fopen(CHANNELS_FILE, "r");
    if (!channels_file) {
        char response[] = "Gagal membuka file channels.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char line[256];
    bool key_valid = false;

    while (fgets(line, sizeof(line), channels_file)) {
        char *token = strtok(line, ",");
        token = strtok(NULL, ",");
        if (token && strcmp(token, channel) == 0) {
            token = strtok(NULL, ","); // Get the stored hash
            if (token) {
                // Trim any trailing whitespace or newline from the stored hash
                char stored_hash[256];
                strncpy(stored_hash, token, sizeof(stored_hash) - 1);
                stored_hash[sizeof(stored_hash) - 1] = '\0';
                stored_hash[strcspn(stored_hash, "\n")] = '\0';

                // Trim newline from key
                char key_trimmed[BUFFER_SIZE];
                strncpy(key_trimmed, key, BUFFER_SIZE - 1);
                key_trimmed[BUFFER_SIZE - 1] = '\0';
                key_trimmed[strcspn(key_trimmed, "\n")] = '\0';

                // Re-generate hash for key using the same salt
                char salt[30];
                snprintf(salt, sizeof(salt), "$2y$12$%.22s", "inistringsaltchannelkey");
                char *hash = crypt(key_trimmed, salt);

                if (hash && strcmp(hash, stored_hash) == 0) {
                    key_valid = true;
                }
            }
            break;
        }
    }

    fclose(channels_file);

    if (key_valid) {
        FILE *users_file = fopen(USERS_FILE, "r");
        if (!users_file) {
            char response[] = "Gagal membuka file users.csv";
            if (write(client->socket, response, strlen(response)) < 0) {
                perror("Gagal mengirim respons ke client");
            }
            return;
        }

        char user_line[256];
        char user_id[10];
        bool user_found = false;

        while (fgets(user_line, sizeof(user_line), users_file)) {
            char *token = strtok(user_line, ",");
            strcpy(user_id, token);
            token = strtok(NULL, ",");
            if (token && strcmp(token, username) == 0) {
                user_found = true;
                break;
            }
        }

        fclose(users_file);

        if (user_found) {
            char auth_path[256];
            snprintf(auth_path, sizeof(auth_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/admin/auth.csv", channel);
            FILE *auth_file = fopen(auth_path, "a");
            if (auth_file) {
                fprintf(auth_file, "%s,%s,USER\n", user_id, username);
                fclose(auth_file);

                snprintf(client->logged_in_channel, sizeof(client->logged_in_channel), "%s", channel);
                char response[BUFFER_SIZE];
                snprintf(response, sizeof(response), "[%s/%s]", username, channel);
                if (write(client->socket, response, strlen(response)) < 0) {
                    perror("Gagal mengirim respons ke client");
                }
            } else {
                char response[] = "Gagal membuka file auth.csv";
                if (write(client->socket, response, strlen(response)) < 0) {
                    perror("Gagal mengirim respons ke client");
                }
            }
        } else {
            char response[] = "User tidak ditemukan";
            if (write(client->socket, response, strlen(response)) < 0) {
                perror("Gagal mengirim respons ke client");
            }
        }
    } else {
        char response[] = "Key salah";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    }
}

void join_room(const char *channel, const char *room, client_info *client) {
    // Check if the room directory exists
    char room_path[256];
    snprintf(room_path, sizeof(room_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/%s", channel, room);
    struct stat st;
    if (stat(room_path, &st) == -1 || !S_ISDIR(st.st_mode)) {
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "Room %s tidak ada di channel %s", room, channel);
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    snprintf(client->logged_in_room, sizeof(client->logged_in_room), "%s", room);
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "[%s/%s/%s]", client->logged_in_user, channel, room);
    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }
}

void send_chat(const char *username, const char *channel, const char *room, const char *message, client_info *client) {
    char path[256];
    snprintf(path, sizeof(path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/%s/chat.csv", channel, room);
    FILE *chat_file = fopen(path, "a+");
    if (!chat_file) {
        char response[] = "Gagal membuka file chat.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    int id_chat = 1;
    char line[512];
    while (fgets(line, sizeof(line), chat_file)) {
        id_chat++;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char date[30];
    strftime(date, sizeof(date), "%d/%m/%Y %H:%M:%S", t);

    fprintf(chat_file, "%s|%d|%s|%s\n", date, id_chat, username, message);
    fclose(chat_file);

    char response[100];
    snprintf(response, sizeof(response), "Pesan berhasil dikirim");
    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }
}

void see_chat(const char *channel, const char *room, client_info *client) {
    char path[256];
    snprintf(path, sizeof(path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/%s/chat.csv", channel, room);
    FILE *chat_file = fopen(path, "r");
    if (!chat_file) {
        char response[] = "Gagal membuka file chat.csv atau belum ada chat di room ini";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char line[512];
    char response[BUFFER_SIZE] = "";
    bool has_chat = false;

    while (fgets(line, sizeof(line), chat_file)) {
        has_chat = true;
        char *date = strtok(line, "|");
        char *id_chat = strtok(NULL, "|");
        char *sender = strtok(NULL, "|");
        char *chat = strtok(NULL, "|");

        chat[strcspn(chat, "\n")] = '\0';
            
        if (date && id_chat && sender && chat) {
            snprintf(response + strlen(response), sizeof(response) - strlen(response), "[%s][%s][%s] “%s” \n", date, id_chat, sender, chat);
        }
    }

    if (!has_chat) {
        snprintf(response, sizeof(response), "Belum ada chat");
    }

    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }

    fclose(chat_file);
}

void edit_chat(const char *channel, const char *room, int id_chat, const char *new_text, client_info *client) {
    char path[256];
    snprintf(path, sizeof(path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/%s/chat.csv", channel, room);
    FILE *chat_file = fopen(path, "r+");
    if (!chat_file) {
        char response[] = "Gagal membuka file chat.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/%s/chat_temp.csv", channel, room);
    FILE *temp_file = fopen(temp_path, "w");
    if (!temp_file) {
        char response[] = "Gagal membuat file sementara untuk edit chat";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        fclose(chat_file);
        return;
    }

    char line[512];
    bool found = false;

    while (fgets(line, sizeof(line), chat_file)) {
        char *date = strtok(line, "|");
        char *id_str = strtok(NULL, "|");
        int id = atoi(id_str);
        char *sender = strtok(NULL, "|");
        char *chat = strtok(NULL, "\n");

        if (id == id_chat) {
            found = true;
            fprintf(temp_file, "%s|%d|%s|%s\n", date, id, sender, new_text);
        } else {
            fprintf(temp_file, "%s|%d|%s|%s\n", date, id, sender, chat);
        }
    }

    fclose(chat_file);
    fclose(temp_file);

    if (found) {
        remove(path);
        rename(temp_path, path);
        char response[100];
        snprintf(response, sizeof(response), "Chat dengan ID %d diedit", id_chat);
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    } else {
        remove(temp_path);
        char response[100];
        snprintf(response, sizeof(response), "Chat dengan ID %d tidak ditemukan", id_chat);
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    }
}

void edit_channel(const char *old_channel, const char *new_channel, client_info *client) {
    char auth_path[256];
    snprintf(auth_path, sizeof(auth_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/admin/auth.csv", old_channel);
    FILE *auth_file = fopen(auth_path, "r");
    if (!auth_file) {
        char response[] = "Gagal membuka file auth.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    FILE *users_file = fopen(USERS_FILE, "r");
    if (!users_file) {
        char response[] = "Gagal membuka file users.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char rootline[256];
    bool is_admin = false;

    while (fgets(rootline, sizeof(rootline), users_file)) {
        char *token = strtok(rootline, ",");
        token = strtok(NULL, ",");
        if (token && strcmp(token, client->logged_in_user) == 0) {
            token = strtok(NULL, ",");
            token = strtok(NULL, ",");
            if (strstr(token, "ROOT") != NULL){
                is_admin = true;
            }
            break;
        }
    }

    fclose(users_file);

    char line[256];

    while (fgets(line, sizeof(line), auth_file)) {
        char *token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        if (strcmp(token, client->logged_in_user) == 0) {
            token = strtok(NULL, ",");
            if (strstr(token, "ADMIN") != NULL) {
                is_admin = true;
                break;
            }
        }
    }

    fclose(auth_file);

    if (!is_admin) {
        char response[] = "Anda tidak memiliki izin untuk mengedit channel";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    FILE *channels_file = fopen(CHANNELS_FILE, "r+");
    if (!channels_file) {
        char response[] = "Gagal membuka file channels.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "/home/rafaelega24/SISOP/FP/DiscorIT/channels_temp.csv");
    FILE *temp_file = fopen(temp_path, "w+");
    if (!temp_file) {
        char response[] = "Gagal membuat file sementara untuk edit channel";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        fclose(channels_file);
        return;
    }

    char line_channel[256];
    bool found = false;
    bool channel_exists = false;

    while (fgets(line_channel, sizeof(line_channel), channels_file)) {
        char *id = strtok(line_channel, ",");
        char *channel_name = strtok(NULL, ",");
        char *key = strtok(NULL, "\n");
        if (channel_name && strcmp(channel_name, new_channel) == 0) {
            channel_exists = true;
            break;
        }
        if (channel_name && strcmp(channel_name, old_channel) == 0) {
            found = true;
            fprintf(temp_file, "%s,%s,%s\n", id, new_channel, key);
        } else {
            fprintf(temp_file, "%s", line_channel);
        }
    }

    fclose(channels_file);
    fclose(temp_file);

    if (channel_exists) {
        remove(temp_path);
        char response[] = "Nama channel sudah digunakan";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    if (found) {
        remove(CHANNELS_FILE);
        rename(temp_path, CHANNELS_FILE);

        char old_path[256];
        snprintf(old_path, sizeof(old_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s", old_channel);
        char new_path[256];
        snprintf(new_path, sizeof(new_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s", new_channel);
        rename(old_path, new_path);

        char response[100];
        snprintf(response, sizeof(response), "%s berhasil diubah menjadi %s", old_channel, new_channel);
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    } else {
        remove(temp_path);
        char response[100];
        snprintf(response, sizeof(response), "Channel %s tidak ditemukan", old_channel);
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    }
}

void edit_room(const char *channel, const char *old_room, const char *new_room, client_info *client) {
    FILE *users_file = fopen(USERS_FILE, "r");
    if (!users_file) {
        char response[] = "Gagal membuka file users.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char rootline[256];
    bool is_admin = false;

    while (fgets(rootline, sizeof(rootline), users_file)) {
        char *token = strtok(rootline, ",");
        token = strtok(NULL, ",");
        if (token && strcmp(token, client->logged_in_user) == 0) {
            token = strtok(NULL, ",");
            token = strtok(NULL, ",");
            if (strstr(token, "ROOT") != NULL){
                is_admin = true;
            }
            break;
        }
    }

    fclose(users_file);

    char auth_path[256];
    snprintf(auth_path, sizeof(auth_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/admin/auth.csv", channel);
    FILE *auth_file = fopen(auth_path, "r+");
    if (!auth_file) {
        char response[] = "Gagal membuka file auth.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char line[256];

    while (fgets(line, sizeof(line), auth_file)) {
        char *token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        if (strcmp(token, client->logged_in_user) == 0) {
            token = strtok(NULL, ",");
            if (strstr(token, "ADMIN") != NULL){
                is_admin = true;
                break;
            }
        }
    }

    fclose(auth_file);

    if (!is_admin) {
        char response[] = "Anda tidak memiliki izin untuk mengedit room";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char check_path[256];
    snprintf(check_path, sizeof(check_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/%s", channel, new_room);
    struct stat st;
    if (stat(check_path, &st) == 0) {
        char response[] = "Nama room sudah digunakan";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char path[256];
    snprintf(path, sizeof(path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/%s", channel, old_room);
    if (stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
        char response[100];
        snprintf(response, sizeof(response), "Room %s tidak ada di channel %s", old_room, channel);
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char old_path[256];
    snprintf(old_path, sizeof(old_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/%s", channel, old_room);
    char new_path[256];
    snprintf(new_path, sizeof(new_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/%s", channel, new_room);

    if (rename(old_path, new_path) == 0) {
        char response[100];
        snprintf(response, sizeof(response), "%s berhasil diubah menjadi %s", old_room, new_room);
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    } else {
        char response[] = "Gagal mengubah nama room";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    }
}

void edit_profile_self(const char *username, const char *new_value, bool is_password, client_info *client) {
    FILE *file = fopen(USERS_FILE, "r+");
    if (!file) {
        char response[] = "Gagal membuka file users.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char line[256];
    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "/home/rafaelega24/SISOP/FP/DiscorIT/users_temp.csv");
    FILE *temp_file = fopen(temp_path, "w");
    if (!temp_file) {
        char response[] = "Gagal membuat file sementara";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        fclose(file);
        return;
    }

    bool found = false;
    bool name_exists = false;
    while (fgets(line, sizeof(line), file)) {
        char *user_id = strtok(line, ",");
        char *user_name = strtok(NULL, ",");
        char *hash = strtok(NULL, ",");
        char *role = strtok(NULL, ",");

        if (user_name && strcmp(user_name, new_value) == 0 && !is_password) {
            name_exists = true;
            break;
        }

        if (user_name && strcmp(user_name, username) == 0) {
            found = true;
            if (is_password) {
                // Generate new hash for the new password
                char salt[30];
                snprintf(salt, sizeof(salt), "$2y$12$%.22s", "inistringsaltuserpassword");
                char *new_hash = crypt(new_value, salt);
                fprintf(temp_file, "%s,%s,%s,%s", user_id, user_name, new_hash, role);
            } else {
                fprintf(temp_file, "%s,%s,%s,%s", user_id, new_value, hash, role);
                snprintf(client->logged_in_user, sizeof(client->logged_in_user), "%s", new_value);
            }
        } else {
            fprintf(temp_file, "%s,%s,%s,%s", user_id, user_name, hash, role);
        }
    }

    fclose(file);
    fclose(temp_file);

    if (name_exists) {
        remove(temp_path);
        char response[] = "Username sudah digunakan";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    if (found) {
        remove(USERS_FILE);
        rename(temp_path, USERS_FILE);
        char response[100];
        snprintf(response, sizeof(response), is_password ? "Password diupdate" : "Profil diupdate");
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    } else {
        remove(temp_path);
        char response[] = "User tidak ditemukan";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    }
}

void edit_user(const char *target_user, const char *new_value, bool is_password, client_info *client) {
    FILE *file = fopen(USERS_FILE, "r+");
    if (!file) {
        char response[] = "Gagal membuka file users.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char line[256];
    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "/home/rafaelega24/SISOP/FP/DiscorIT/users_temp.csv");
    FILE *temp_file = fopen(temp_path, "w+");
    if (!temp_file) {
        char response[] = "Gagal membuat file sementara";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        fclose(file);
        return;
    }

    bool is_root = false;
    bool found = false;
    bool name_exists = false;

    while (fgets(line, sizeof(line), file)) {
        char *user_id = strtok(line, ",");
        char *user_name = strtok(NULL, ",");
        char *hash = strtok(NULL, ",");
        char *role = strtok(NULL, ",");

        if (user_name && strcmp(user_name, client->logged_in_user) == 0) {
            if (strstr(role, "ROOT") != NULL) {
                is_root = true;
            }
        }

        if (user_name && strcmp(user_name, new_value) == 0 && !is_password) {
            name_exists = true;
            break;
        }

        if (user_name && strcmp(user_name, target_user) == 0) {
            found = true;
            if (is_password) {
                // Generate new hash for the new password
                char salt[30];
                snprintf(salt, sizeof(salt), "$2y$12$%.22s", "inistringsaltuserpassword");
                char *new_hash = crypt(new_value, salt);
                fprintf(temp_file, "%s,%s,%s,%s", user_id, user_name, new_hash, role);
            } else {
                fprintf(temp_file, "%s,%s,%s,%s", user_id, new_value, hash, role);
            }
        } else {
            fprintf(temp_file, "%s,%s,%s,%s", user_id, user_name, hash, role);
        }
    }

    fclose(file);
    fclose(temp_file);

    if (!is_root) {
        remove(temp_path);
        char response[] = "Anda tidak memiliki izin untuk mengedit user";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    if (name_exists) {
        remove(temp_path);
        char response[] = "Username sudah digunakan";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    if (found) {
        remove(USERS_FILE);
        rename(temp_path, USERS_FILE);

        char response[100];
        if (is_password) {
            snprintf(response, sizeof(response), "Password %s berhasil diubah", target_user);
        } else {
            snprintf(response, sizeof(response), "%s berhasil diubah menjadi %s", target_user, new_value);
        }
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    } else {
        remove(temp_path);
        char response[] = "User tidak ditemukan";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    }
}

void handle_exit(client_info *client) {
    if (strlen(client->logged_in_room) > 0) {
        memset(client->logged_in_room, 0, sizeof(client->logged_in_room));
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "[%s/%s]", client->logged_in_user, client->logged_in_channel);
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    } else if (strlen(client->logged_in_channel) > 0) {
        memset(client->logged_in_channel, 0, sizeof(client->logged_in_channel));
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "[%s]", client->logged_in_user);
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    } else {
        char response[] = "Anda telah keluar dari aplikasi";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        close(client->socket);
        pthread_exit(NULL);
    }
}