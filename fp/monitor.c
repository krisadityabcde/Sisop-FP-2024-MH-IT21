#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <crypt.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int server_fd;
bool running = true;
char username[50];
char channel[50] = "";
char room[50] = "";

void connect_to_server() {
    struct sockaddr_in serv_addr;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        exit(EXIT_FAILURE);
    }
}

void handle_command(const char *command) {
    if (command == NULL) {
        printf("Perintah tidak boleh kosong\n");
        return;
    }

    if (send(server_fd, command, strlen(command), 0) < 0) {
        perror("Gagal mengirim perintah ke server");
    }

    char response[BUFFER_SIZE];
    memset(response, 0, sizeof(response));
    if (recv(server_fd, response, BUFFER_SIZE - 1, 0) < 0) {
        perror("Gagal menerima respons dari server");
    } else {
        if (strstr(response, "Key:") != NULL) {
            char key[50];
            printf("Key: ");
            fgets(key, sizeof(key), stdin);
            key[strcspn(key, "\n")] = '\0';

            if (send(server_fd, key, strlen(key), 0) < 0) {
                perror("Gagal mengirim key ke server");
            }

            memset(response, 0, sizeof(response));
            if (recv(server_fd, response, BUFFER_SIZE - 1, 0) < 0) {
                perror("Gagal menerima respons dari server");
            } else {
                if (strstr(response, "Key salah") != NULL) {
                    // Reset state if key is invalid
                    if (strlen(room) > 0) {
                        room[0] = '\0';
                    } else if (strlen(channel) > 0) {
                        channel[0] = '\0';
                    }
                }
            }
        } else if (strstr(response, "tidak ada") != NULL || strstr(response, "Key salah") != NULL || strstr(response, "Anda telah diban") != NULL || strstr(response, "tidak dikenali") != NULL){
            if (strlen(room) > 0) {
                room[0] = '\0';
            } else if (strlen(channel) > 0) {
                channel[0] = '\0';
            }
            printf("%s\n", response);
        } else if (strstr(response, "Anda telah keluar dari aplikasi") != NULL) {
            close(server_fd);
            exit(0);
        } else {
            printf("%s\n", response);
        }
    }
}

void display_chat_history(const char *filepath) {
    FILE *chat_file = fopen(filepath, "r");
    if (!chat_file) {
        perror("Gagal membuka file");
        return;
    }

    char response[BUFFER_SIZE];
    memset(response, 0, sizeof(response));
    char line[BUFFER_SIZE];
    bool has_chat = false;

    while (fgets(line, sizeof(line), chat_file)) {
        has_chat = true;
        char *date = strtok(line, "|");
        char *id_chat = strtok(NULL, "|");
        char *sender = strtok(NULL, "|");
        char *chat = strtok(NULL, "|");

        chat[strcspn(chat, "\n")] = '\0';

        if (date && id_chat && sender && chat) {
            snprintf(response + strlen(response), sizeof(response) - strlen(response), "[%s][%s][%s] \"%s\"\n", date, id_chat, sender, chat);
        }
    }

    fclose(chat_file);

    if (!has_chat) {
        snprintf(response, sizeof(response), "Belum ada chat\n");
    }

    printf("\033[H\033[J");

    printf("%s", response);

    printf("[%s/%s/%s] ", username, channel, room);

    fflush(stdout);
}

void *monitor_csv(void *arg) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/%s/chat.csv", channel, room);
    struct stat file_stat;
    time_t last_mod_time = 0;

    while (running) {
        if (stat(filepath, &file_stat) == 0) {
            if (file_stat.st_mtime != last_mod_time) {
                last_mod_time = file_stat.st_mtime;
                printf("\033[H\033[J");

                display_chat_history(filepath);
            }
        } else {
            perror("Gagal mendapatkan status file");
        }
        sleep(1);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc <= 3) {
        printf("Penggunaan: %s LOGIN username -p password\n", argv[0]);
        return 1;
    }

    connect_to_server();

    char command[BUFFER_SIZE];
    memset(command, 0, sizeof(command));

    if (strcmp(argv[1], "LOGIN") == 0) {
        if (argc < 5 || strcmp(argv[3], "-p") != 0) {
            printf("Penggunaan: %s LOGIN username -p password\n", argv[0]);
            return 1;
        }

        snprintf(username, sizeof(username), "%s", argv[2]);
        char *password = argv[4];

        snprintf(command, sizeof(command), "LOGIN %s %s", username, password);

        if (send(server_fd, command, strlen(command), 0) < 0) {
            perror("Gagal mengirim perintah ke server");
        }

        char response[BUFFER_SIZE];
        memset(response, 0, sizeof(response));

        if (recv(server_fd, response, BUFFER_SIZE - 1, 0) < 0) {
            perror("Gagal menerima respons dari server");
        } else {
            printf("%s\n", response);

            if (strstr(response, "berhasil login") != NULL) {
                while (1) {
                    printf("[%s] ", username);

                    if (fgets(command, BUFFER_SIZE, stdin) == NULL) {
                        printf("Gagal membaca perintah\n");
                        continue;
                    }
                    command[strcspn(command, "\n")] = '\0';

                    char *token = strtok(command, " ");
                    if (token == NULL) {
                        printf("Penggunaan: -channel <nama_channel> -room <nama_room>\n");
                        continue;
                    }

                    if (strcmp(token, "-channel") == 0) {
                        token = strtok(NULL, " ");
                        if (token == NULL) {
                            printf("Penggunaan: -channel <nama_channel> -room <nama_room>\n");
                            continue;
                        }
                        snprintf(channel, sizeof(channel), "%s", token);
                        token = strtok(NULL, " ");
                        if(token == NULL){
                            printf("Penggunaan: -channel <nama_channel> -room <nama_room>\n");
                            continue;
                        }
                        if(strcmp(token, "-room") != 0){
                            printf("Penggunaan: -channel <nama_channel> -room <nama_room>\n");
                            continue;
                        }
                        token = strtok(NULL, " ");
                        if(token == NULL){
                            printf("Penggunaan: -channel <nama_channel> -room <nama_room>\n");
                            continue;
                        }
                        snprintf(room, sizeof(room), "%s", token);

                        pthread_t csv_thread;
                        running = true;
                        if (pthread_create(&csv_thread, NULL, monitor_csv, NULL) != 0) {
                            perror("Gagal membuat thread untuk memonitor CSV");
                            continue;
                        }

                        pthread_detach(csv_thread);

                        char filepath[256];
                        snprintf(filepath, sizeof(filepath), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/%s/chat.csv", channel, room);
                        display_chat_history(filepath);
                    } else if (strcmp(token, "EXIT") == 0) {
                        if (strlen(room) > 0 || strlen(channel) > 0) {
                            room[0] = '\0';
                            channel[0] = '\0';
                            running = false;
                            printf("\033[H\033[J");
                        } else {
                            handle_command(command);
                            break;
                        }
                    } else {
                        printf("Penggunaan: -channel <nama_channel> -room <nama_room>\nJika ingin keluar, gunakan perintah EXIT\n");
                    }
                }
            }
            close(server_fd);
            return 0;
        }
    } else {
        printf("Penggunaan: %s LOGIN username -p password\n", argv[0]);
        return 1;
    }
    close(server_fd);
    return 0;
}
