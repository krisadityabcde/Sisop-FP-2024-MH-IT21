# Sisop-FP-2024-MH-IT21
## Anggota Kelompok:
- Rafael Ega Krisaditya	(5027231025)
- Rama Owarianto Putra Suharjito	(5027231049)
- Danar Bagus Rasendriya	(5027231055)

Pada final project ini, praktikan diminta untuk membuat sebuah aplikasi tiruan Discord dengan konsep-konsep yang sudah di pelajari di modul-modul sebelumnya.

## Disclaimer
Program server, discorit, dan monitor TIDAK DIPERBOLEHKAN menggunakan perintah system();

## Bagaimana Program Diakses
- Untuk mengakses DiscorIT, user perlu membuka program client (discorit). discorit hanya bekerja sebagai client yang mengirimkan request user kepada server.
- Program server berjalan sebagai server yang menerima semua request dari client dan mengembalikan response kepada client sesuai ketentuan pada soal. Program server berjalan sebagai daemon. 
- Untuk hanya menampilkan chat, user perlu membuka program client (monitor). Lebih lengkapnya pada poin monitor.
- Program client dan server berinteraksi melalui socket.
- Server dapat terhubung dengan lebih dari satu client.

## Tree
DiscorIT/
- channels.csv
- users.csv
- channel1/
    - admin/
        - auth.csv
        - user.log
    - room1/
        - chat.csv
    - room2/
        - chat.csv
    - room3/
        - chat.csv
- channel2/
    - admin/
        - auth.csv
        - user.log
    - room1/
        - chat.csv
    - room2/
        - chat.csv
    - room3/
        - chat.csv

## Keterangan setiap File
| File          | Isi                                                                 |
|---------------|-----------------------------------------------------------------------------|
| users.csv     | id_user	int (mulai dari 1)
||name		string
||password	string (di encrypt menggunakan bcrypt biar ga tembus)
||global_role	string (pilihannya: ROOT / USER)
| channels.csv  | id_channel	int  (mulai dari 1)
||channel	string
||key		string (di encrypt menggunakan bcrypt biar ga tembus) |
| auth.csv      | id_user	int
||name		string
||role		string (pilihannya: ROOT/ADMIN/USER/BANNED)   |
| user.log      | Semua log aktivitas admin dan root di dalam suatu channel              |
| chat.csv      |date		int
||id_chat	number  (mulai dari 1)
||sender 	string
||chat		string     |

## A. Autentikasi

- Setiap user harus memiliki username dan password untuk mengakses DiscorIT. Username, password, dan global role disimpan dalam file user.csv.
- Jika tidak ada user lain dalam sistem, user pertama yang mendaftar otomatis mendapatkan role "ROOT". Username harus bersifat unique dan password wajib di encrypt menggunakan menggunakan bcrypt.

### Register
```c
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

    char salt[SALT_SIZE];
    snprintf(salt, sizeof(salt), "$2y$12$%.22s", "inistringsaltuntukbcrypt");
    char hash[BCRYPT_HASHSIZE];
    bcrypt_hashpw(password, salt, hash);

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
```

Di function ini, awalnya akan dibuat direktori DiscorIT jika belum ada. Lalu membuat file `users.csv` jika belum ada. Setelah itu, mengecek apakah username sudah pernah digunakan sebelumnya, apabila belum dilanjutkan dengan proses meminta password dan enkripsi dengan bcrypt. Setelah registrasi berhasil data user baru akan disimpan sesuai dengan ketentuan format file `users.csv`

### Format
./discorit REGISTER username -p password    
username berhasil register

./discorit REGISTER same_username -p password   
username sudah terdaftar


### Contoh
./discorit REGISTER qurbancare -p qurban123     
qurbancare berhasil register

./discorit REGISTER qurbancare -p qurban123     
qurbancare sudah terdaftar

### Screenshot
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/2cc7ad27-9b26-41fb-bdde-de04a31fa09e)

![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/0a36b5be-79bf-464b-8a6f-03062ecd6f8c)

### Login
```c
void login_user(const char *username, const char *password, client_info *client) {
    FILE *file = fopen(USERS_FILE, "r");
    if (!file) {
        char response[] = "Tidak dapat membuka file users.csv atau user belum terdaftar";
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

            if (bcrypt_checkpw(password, stored_hash) == 0){
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
```
Pertama, keberadaan file `users.csv` akan dicek, lalu keberadaan user yang ingin login juga akan dicek, jika ada lanjut dengan dekripsi password, dan jika tidak ada maka permintaan login akan ditolak.

### Format
./discorit LOGIN username -p password   
username berhasil login     
[username] (bisa memberi input setelah tanda kurung)


### Contoh
./discorit LOGIN qurbancare -p qurban123    
qurbancare berhasil login   
[qurbancare] 

### Screenshot
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/917ea10f-c711-4b38-bac4-f2198c238477)

## B. Bagaimana DiscorIT digunakan
### List Channel
```c
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

    if(strlen(response) == 0){
        snprintf(response, sizeof(response), "Tidak ada channel yang ditemukan");
    }

    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }

    fclose(channels_file);
}
```
Pada function ini, keberadaan channel akan dicek di file `channels.csv` karena pembuatan channel otomatis menuliskan data terkait channel ke dalam file tersebut.

### Format
[user] LIST CHANNEL     
channel1 channel2 channel3


### Contoh
[qurbancare] LIST CHANNEL       
care bancar qurb

### Screenshot
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/23a9d508-0216-4c40-bd99-0efa31984e42)

### List Room dan User dalam Channel
```c
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
```

```c
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
```
Perbedaannya dengan list channel, pada function list room pengecekan dilakukan dengan listing directory dengan mengecualikan direktori di dalam direktori agar tidak terjadi pembacaan rekursif dan juga mengecualikan folder admin yang menjadi bagian dari suatu channel tapi bukan direktori room.

Sementara untuk list user dalam channel dilakukan dengan pengecekan pada `auth.csv` karena user yang pernah join suatu channel akan ototmatis tercatat akasesnya di file itu.

### Format
[user/channel] LIST ROOM    
room1 room2 room3   
[user/channel] LIST USER    
user1 user2 user3

### Contoh
[qurbancare/care] LIST ROOM     
urban banru runab   
[qurbancare/care] LIST USER     
root admin qurbancare

### Screenshot
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/8fefde7d-dc3f-4399-bed8-435f9b40a9b7)

![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/1413265e-4852-4706-866b-e47ef08a4375)

### Join Channel
```c
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
        char *name = token;
        if (token && strstr(name, username) != NULL){
            token = strtok(NULL, ",");
            token = strtok(NULL, ",");
            char *role = token;
            if (strstr(role, "ROOT") != NULL){
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

    // Check if user is ADMIN/USER/BANNED in auth.csv
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
    bool is_banned = false;

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
            } else if (strstr(token, "BANNED") != NULL){
                is_banned = true;
                break;
            }
        }
    }

    fclose(auth_file);

    if (is_banned) {
        char response[] = "Anda telah diban, silahkan menghubungi admin";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

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
```
Pertama, akan dilakukan pengecekan terhadap keberadaan channel, kedua akan dilakukan pengecekan apakah pengguna yang hendak join channel meruapakan root user atau bukan, jika iya maka diperbolehkan untuk join channel tanpa perlu menginputkan key. Setelah join, data dari pengguna akan disimpan di dalam `auth.csv` utnuk kepeluar join berikutnya. Selanjutnya, jika pengguna bukan root, akan dicek apakah pengguna merupakan admin channel atau user biasa yang sudah pernah join ke channel yang dimaksud sebelumnya, jika iya maka pengguna tersebut juga tidak perlu menginputkan key untuk join ke channel. Sementara itu, apabila pengguna merupakan user yang diban, maka akses masuk akan ditolak sepenuhnya, dan jika tidak maka pengguna merupakan user yang baru pertama kali bergabung dan perlu memasukkan key yang nantinya akan diverivikasi sebagai akses untuk masuk ke dalam channel.
```c
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
            char *stored_hash = token;
            stored_hash[strcspn(stored_hash, "\n")] = 0;
            if(token && bcrypt_checkpw(key, stored_hash) == 0){
                key_valid = true;
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
```
Ini adalah function utilitas untuk verifikasi channel key.

### Format
[user] JOIN channel     
Key: key    
[user/channel] 


### Contoh
[qurbancare] JOIN care      
Key: care123    
[qurbancare/care] 

### Screenshot
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/11163de2-bb8a-48e4-8318-a4b57752f265)

### Join Room
Konsep dari join room lebih sederhana karena tidak perlu verifikasi keberadaan user (masuk channel = user exist). Yang perlu dilakukan hanyalah melakukan pengecekan keberadaan room.
```c
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
```

### Format
[user/channel] JOIN room
[user/channel/room] 


### Contoh
[qurbancare/care] JOIN urban
[qurbancare/care/urban] 

### Screenshot
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/a5ca2173-e56f-4149-a952-3d26751fce48)

### Fitur Chat

Setiap user dapat mengirim pesan dalam chat. ID pesan chat dimulai dari 1 dan semua pesan disimpan dalam file chat.csv. User dapat melihat pesan-pesan chat yang ada dalam room. Serta user dapat edit dan delete pesan yang sudah dikirim dengan menggunakan ID pesan.

```c
void send_chat(const char *username, const char *channel, const char *room, const char *message, client_info *client) {
    char *startquote = strchr(message, '\"');
    char *endquote = strrchr(message, '\"');

    if (startquote == NULL || endquote == NULL || startquote == endquote) {
        char response[] = "Penggunaan: CHAT \"<pesan>\"";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char message_trimmed[BUFFER_SIZE];
    memset(message_trimmed, 0, sizeof(message_trimmed));
    strncpy(message_trimmed, message + 1, endquote - startquote - 1);

    if(strlen(message_trimmed) == 0){
        char response[] = "Pesan tidak boleh kosong";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

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

    // Get the last chat ID
    int last_id = 0;
    char line[512];
    while (fgets(line, sizeof(line), chat_file)) {
        char *token = strtok(line, "|"); // date
        token = strtok(NULL, "|"); // id_chat
        if (token) {
            last_id = atoi(token);
        }
    }

    int id_chat = last_id + 1;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char date[30];
    strftime(date, sizeof(date), "%d/%m/%Y %H:%M:%S", t);

    fprintf(chat_file, "%s|%d|%s|%s\n", date, id_chat, username, message_trimmed);
    fclose(chat_file);

    char response[100];
    snprintf(response, sizeof(response), "Pesan berhasil dikirim");
    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }
}
```

```c
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
```

```c
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
```

```c
void delete_chat(const char *channel, const char *room, int chat_id, client_info *client) {
    char path[256];
    snprintf(path, sizeof(path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/%s/chat.csv", channel, room);

    FILE *file = fopen(path, "r");
    if (!file) {
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
        char response[] = "Gagal membuat file sementara";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        fclose(file);
        return;
    }

    char line[256];
    bool found = false;

    while (fgets(line, sizeof(line), file)) {
        char *date = strtok(line, "|");
        char *id_str = strtok(NULL, "|");
        int id = atoi(id_str);
        char *sender = strtok(NULL, "|");
        char *chat = strtok(NULL, "\n");

        if (id == chat_id) {
            found = true;
        } else {
            fprintf(temp_file, "%s|%d|%s|%s\n", date, id, sender, chat);
        }
    }

    fclose(file);
    fclose(temp_file);

    if (found) {
        remove(path);
        rename(temp_path, path);
        char response[100];
        snprintf(response, sizeof(response), "Chat dengan id %d berhasil dihapus selamanya", chat_id);
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    } else {
        remove(temp_path);
        char response[100];
        snprintf(response, sizeof(response), "Chat dengan id %d tidak ditemukan", chat_id);
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    }
}
```

Function-function diatas menggunakan konsep yang diminta oleh soal untuk mengirimkan chat, melihat chat, mengedit chat, dan menghapus chat berdasarkan id. Pencatatan chat juga dilakukan sesuai dengan format yang diminta untuk setiap file `chat.csv` yang berada pada tiap room yang ada.

### Format
[user/channel/room] CHAT "text"     
[user/channel/room] SEE CHAT        
sebelumnya      
[dd/mm/yyyy HH:MM:SS][id][username] “text”      
sesudahnya      
[user/channel/room] EDIT CHAT id “text”     
[user/channel/room] DEL CHAT id         

### Contoh
[qurbancare/care/urban] CHAT “hallo”        
[qurbancare/care/urban] SEE CHAT        
sebelumnya      
[05/06/2024 23:22:12][3][qurbancare]     “hallo”        
sesudahnya      
[qurbancare/care/urban] EDIT CHAT 3 “hi”        
[qurbancare/care/urban] DEL CHAT 3      

### Screenshot
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/0542aed9-78bf-4f6d-b9d7-c28190ec408a)

![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/472f0c8e-44b2-43ea-94ab-388adaca0482)

![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/322527cc-1b60-4c25-bfb0-2f6293b963ae)

![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/0b46f5fb-f236-4d1d-a458-0c5cc448ff92)

## C. Root
- Akun yang pertama kali mendaftar otomatis mendapatkan peran "root".
- Root dapat masuk ke channel manapun tanpa key dan create, update, dan delete pada channel dan room, mirip dengan admin [D].
- Root memiliki kemampuan khusus untuk mengelola user, seperti: list, edit, dan Remove.

Untuk poin pertama, sudah diterapkan di function `register_user()` Dimana jika belum ada user yang melakukan register, maka user pertama yang register akan terdaftar sebagai root.

```c
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
```
```c
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
                char salt[SALT_SIZE];
                snprintf(salt, sizeof(salt), "$2y$12$%.22s", "inistringsaltuntukbcrypt");
                char new_hash[BCRYPT_HASHSIZE];
                bcrypt_hashpw(new_value, salt,new_hash);

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
```
```c
void remove_user_root(const char *target_user, client_info *client) {
    FILE *rootfile = fopen(USERS_FILE, "r");
    if (!rootfile) {
        char response[] = "Gagal membuka file users.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    bool is_root = false;
    char line[256];

    while (fgets(line, sizeof(line), rootfile)) {
        char *token = strtok(line, ",");
        token = strtok(NULL, ",");
        if (token && strcmp(token, client->logged_in_user) == 0) {
            token = strtok(NULL, ",");
            token = strtok(NULL, ",");
            if (strstr(token, "ROOT") != NULL) {
                is_root = true;
                break;
            }
        }
    }

    if (!is_root) {
        char response[] = "Anda tidak memiliki izin untuk menghapus user secara permanen";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        fclose(rootfile);
        return;
    }

    fclose(rootfile);

    FILE *file = fopen(USERS_FILE, "r");
    if (!file) {
        char response[] = "Gagal membuka file users.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        fclose(file);
        return;
    }

    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "/home/rafaelega24/SISOP/FP/DiscorIT/users_temp.csv");
    FILE *temp_file = fopen(temp_path, "w+");
    if (!temp_file) {
        char response[] = "Gagal membuat file sementara";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        fclose(temp_file);
        return;
    }

    bool found = false;

    while (fgets(line, sizeof(line), file)) {
        char *user_id = strtok(line, ",");
        char *user_name = strtok(NULL, ",");
        char *hash = strtok(NULL, ",");
        char *role = strtok(NULL, ",");

        if (user_name && strcmp(user_name, target_user) == 0) {
            found = true;
            continue;  // Skip writing this user to temp file
        }
        fprintf(temp_file, "%s,%s,%s,%s", user_id, user_name, hash, role);
    }

    fclose(file);
    fclose(temp_file);

    if (found) {
        remove(USERS_FILE);
        rename(temp_path, USERS_FILE);
        char response[100];
        snprintf(response, sizeof(response), "%s berhasil dihapus", target_user);
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
```
Untuk list user, function ini khusus untuk user root saja di mana akan dilakukan pengecekan jika user yang login adalah root maka akan dialhkan ke function ini. Untuk edit user opsi perubahan (-u/-p) sudah dilakukan di awal lebih rinci pada function `handle_client()` dan akan di-pass ke function ini sebagai variabel bool `is_password` untuk menentukan apakah yang perlu diubah adalah username atau password. Dan terakhir, untuk remove user, hanya menghapus dari file `users.csv` saja sehingga untuk login kembali pengguna harus meregistrasi akun lagi.


### Format
[user] LIST USER        
user1 user2 user3       
[user] EDIT WHERE user1 -u user01       
user1 berhasil diubah menjadi user01    
[user] EDIT WHERE user01 -p secretpass  
password user01 berhasil diubah     
[user] REMOVE user01    
user01 berhasil dihapus     

### Contoh
[root] LIST USER    
naupan qurbancare bashmi    
[root] EDIT WHERE naupan -u zika    
naupan berhasil diubah menjadi zika     
[root] EDIT WHERE zika -p 123zika   
password zika berhasil diubah   
[root] REMOVE zika      
zika berhasil dihapus   

### Screenshot
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/f89fd2e1-ba50-4641-a907-e91a62e8d8c6)

![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/89c1ef5b-94e1-418c-b581-abde8c24cdc2)

Before
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/0e8c65c2-eedd-4c5f-aafa-ba1758fe502d)

After
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/edc41e66-457d-4149-ac60-bb32eb1708ac)

![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/a4e1e4c8-2bde-4f6a-8b04-64b9ce92936a)

## D. Admin Channel
- Setiap user yang membuat channel otomatis menjadi admin di channel tersebut. Informasi tentang user disimpan dalam file auth.csv.
- Admin dapat create, update, dan delete pada channel dan room, serta dapat remove, ban, dan unban user di channel mereka.

Poin pertama sudah diterapkan di function `create_channel()` dan konsepnya sama dengan `register_user()`

### Channel
```c
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

    char salt[SALT_SIZE];
    snprintf(salt, sizeof(salt), "$2y$12$%.22s", "inistringsaltuntukbcrypt");
    char hash[BCRYPT_HASHSIZE];
    bcrypt_hashpw(key, salt, hash);

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

    char log_path[256];
    snprintf(log_path, sizeof(log_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/admin/user.log", channel);
    FILE *log_file = fopen(log_path, "w+");
    if (log_file) {
        fclose(log_file);
    } else {
        char response[] = "Gagal membuat file user.log";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
    }

    char response[100];
    snprintf(response, sizeof(response), "Channel %s dibuat", channel);
    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }

    char log_message[100];
    snprintf(log_message, sizeof(log_message), "ADMIN membuat channel %s", channel);
    log_activity(channel, log_message);
}
```
```c
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
    bool is_root = false;

    while (fgets(rootline, sizeof(rootline), users_file)) {
        char *token = strtok(rootline, ",");
        token = strtok(NULL, ",");
        if (token && strcmp(token, client->logged_in_user) == 0) {
            token = strtok(NULL, ",");
            token = strtok(NULL, ",");
            if (strstr(token, "ROOT") != NULL){
                is_root = true;
            }
            break;
        }
    }

    fclose(users_file);

    char line[256];
    bool is_admin = false;
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

    if (!is_admin && !is_root) {
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
        char *key = strtok(NULL, ",");
        if (channel_name && strcmp(channel_name, new_channel) == 0) {
            channel_exists = true;
            break;
        }
        if (channel_name && strcmp(channel_name, old_channel) == 0) {
            found = true;
            fprintf(temp_file, "%s,%s,%s", id, new_channel, key);
        } else {
            fprintf(temp_file, "%s,%s,%s", id, channel_name, key);
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

        char log_message[100];
        if(is_root){
            snprintf(log_message, sizeof(log_message), "ROOT mengubah channel %s menjadi %s", old_channel, new_channel);
        } else {
            snprintf(log_message, sizeof(log_message), "ADMIN mengubah channel %s menjadi %s", old_channel, new_channel);
        }
        log_activity(new_channel, log_message);

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
```
```c
void delete_channel(const char *channel, client_info *client) {
    FILE *users_file = fopen(USERS_FILE, "r");
    if (!users_file) {
        char response[] = "Gagal membuka file users.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char line[256];
    bool is_admin = false;

    while (fgets(line, sizeof(line), users_file)) {
        char *token = strtok(line, ",");
        token = strtok(NULL, ",");
        if (token && strcmp(token, client->logged_in_user) == 0) {
            token = strtok(NULL, ",");
            token = strtok(NULL, ",");
            if (strstr(token, "ROOT") != NULL) {
                is_admin = true;
            }
            break;
        }
    }

    fclose(users_file);

    if (!is_admin) {
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

        while (fgets(line, sizeof(line), auth_file)) {
            char *token = strtok(line, ",");
            if (token == NULL) continue;
            token = strtok(NULL, ",");
            if (token == NULL) continue;
            if (strcmp(token, client->logged_in_user) == 0) {
                token = strtok(NULL, ",");
                if (strstr(token, "ADMIN") != NULL) {
                    is_admin = true;
                }
                break;
            }
        }

        fclose(auth_file);
    }

    if (!is_admin) {
        char response[] = "Anda tidak memiliki izin untuk menghapus channel ini";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char path[256];
    snprintf(path, sizeof(path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s", channel);
    struct stat st;
    if (stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
        char response[] = "Channel tidak ditemukan";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    // Delete directory recursively
    delete_directory(path);

    // Update channels.csv
    FILE *channels_file = fopen(CHANNELS_FILE, "r");
    if (!channels_file) {
        char response[] = "Gagal membuka file channels.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "/home/rafaelega24/SISOP/FP/DiscorIT/channels_temp.csv");
    FILE *temp_file = fopen(temp_path, "w");
    if (!temp_file) {
        char response[] = "Gagal membuat file sementara";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        fclose(channels_file);
        return;
    }

    while (fgets(line, sizeof(line), channels_file)) {
        char *token = strtok(line, ",");
        token = strtok(NULL, ",");
        if (token && strcmp(token, channel) != 0) {
            fprintf(temp_file, "%s", line);
        }
    }

    fclose(channels_file);
    fclose(temp_file);

    remove(CHANNELS_FILE);
    rename(temp_path, CHANNELS_FILE);

    char response[100];
    snprintf(response, sizeof(response), "%s berhasil dihapus", channel);
    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }

    char log_message[100];
    snprintf(log_message, sizeof(log_message), "%s menghapus channel %s", client->logged_in_role, channel);
    log_activity(channel, log_message);
}
```

### Format
[user] CREATE CHANNEL channel -k key    
Channel channel dibuat  
[user] EDIT CHANNEL old_channel TO new_channel      
old_channel berhasil diubah menjadi new_channel     
[user] DEL CHANNEL channel  
channel berhasil dihapus

### Contoh
[qurbancare] CREATE CHANNEL care -k care123     
Channel care dibuat     
[qurbancare] EDIT CHANNEL care TO cera  
care berhasil diubah menjadi cera   
[qurbancare] DEL CHANNEL cera   
cera berhasil dihapus

### Screenshot
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/c140e48c-6946-43bb-a440-6641bb676573)

![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/e57a4993-c265-487c-9058-d6b3f286d907)

![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/43321aea-ab4f-4c64-9a9d-a93f5ebbf023)

![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/13101ce0-fe83-4fc7-a307-2c4ddccca1c5)

### Room
```c
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
    bool is_admin = false;
    bool is_root = false;

    while (fgets(line, sizeof(line), auth_file)) {
        char *token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        if (strcmp(token, username) == 0) {
            token = strtok(NULL, ",");
            if (strstr(token, "ADMIN") != NULL) {
                is_admin = true;
            } else if (strstr(token, "ROOT") != NULL) {
                is_root = true;
            }
        }
    }

    fclose(auth_file);

    if (!is_admin && !is_root) {
        char response[] = "Anda tidak memiliki izin untuk membuat room di channel ini";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char check_path[256];
    snprintf(check_path, sizeof(check_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/%s", channel, room);
    struct stat st;
    if (stat(check_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        char response[] = "Nama room sudah digunakan";
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

    char log_message[100];
    if(is_root){
        snprintf(log_message, sizeof(log_message), "ROOT membuat room %s", room);
    }else{
        snprintf(log_message, sizeof(log_message), "ADMIN membuat room %s", room);
    }
    log_activity(channel, log_message);
}
```
```c
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
    bool is_root = false;

    while (fgets(rootline, sizeof(rootline), users_file)) {
        char *token = strtok(rootline, ",");
        token = strtok(NULL, ",");
        if (token && strcmp(token, client->logged_in_user) == 0) {
            token = strtok(NULL, ",");
            token = strtok(NULL, ",");
            if (strstr(token, "ROOT") != NULL){
                is_root = true;
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

    if (!is_admin && !is_root) {
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
        char log_message[100];
        if(is_root){
            snprintf(log_message, sizeof(log_message), "ROOT mengubah room %s menjadi %s", old_room, new_room);
        } else {
            snprintf(log_message, sizeof(log_message), "ADMIN mengubah room %s menjadi %s", old_room, new_room);
        }
        log_activity(channel, log_message);

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
```
```c
void delete_room(const char *channel, const char *room, client_info *client) {
        bool is_admin = false;
        bool is_root = false;
        char auth_path[256];
        char line[256];
        snprintf(auth_path, sizeof(auth_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/admin/auth.csv", channel);
        FILE *auth_file = fopen(auth_path, "r");
        if (!auth_file) {
            char response[] = "Gagal membuka file auth.csv";
            if (write(client->socket, response, strlen(response)) < 0) {
                perror("Gagal mengirim respons ke client");
            }
            return;
        }

        while (fgets(line, sizeof(line), auth_file)) {
            char *token = strtok(line, ",");
            if (token == NULL) continue;
            token = strtok(NULL, ",");
            if (token == NULL) continue;
            if (strcmp(token, client->logged_in_user) == 0) {
                token = strtok(NULL, ",");
                if (strstr(token, "ADMIN") != NULL) {
                    is_admin = true;
                }else if (strstr(token, "ROOT") != NULL){
                    is_root = true;
                }
                break;
            }
        }

        fclose(auth_file);

    if (!is_admin && !is_root) {
        char response[] = "Anda tidak memiliki izin untuk menghapus room";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char path[256];
    snprintf(path, sizeof(path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/%s", channel, room);
    struct stat st;
    if (stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
        char response[] = "Room tidak ditemukan";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    // Delete directory
    delete_directory(path);

    char response[100];
    snprintf(response, sizeof(response), "%s berhasil dihapus", room);
    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }

    char log_message[100];
    if(is_root){
        snprintf(log_message, sizeof(log_message), "ROOT menghapus room %s", room);
    } else {
        snprintf(log_message, sizeof(log_message), "ADMIN menghapus room %s", room);
    }
    log_activity(channel, log_message);
}
```
```c
void delete_all_rooms(const char *channel, client_info *client) {
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

    char line[256];
    bool is_admin = false;
    bool is_root = false;

    while (fgets(line, sizeof(line), auth_file)) {
        char *token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        if (strcmp(token, client->logged_in_user) == 0) {
            token = strtok(NULL, ",");
            if (strstr(token, "ADMIN") != NULL) {
                is_admin = true;
            } else if (strstr(token, "ROOT") != NULL){
                is_root = true;
            }
            break;
        }
    }

    fclose(auth_file);

    if (!is_admin && !is_root) {
        char response[] = "Anda tidak memiliki izin untuk menghapus semua room";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char path[256];
    snprintf(path, sizeof(path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s", channel);
    DIR *dir = opendir(path);
    if (dir == NULL) {
        char response[] = "Gagal membuka direktori channel";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, "admin") != 0 && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char room_path[1024];
            snprintf(room_path, sizeof(room_path), "%s/%s", path, entry->d_name);

            struct stat st;
            if (stat(room_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                delete_directory(room_path);
            }
        }
    }
    closedir(dir);

    char response[] = "Semua room dihapus";
    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }

    char log_message[100];
    if(is_root){
        snprintf(log_message, sizeof(log_message), "ROOT menghapus semua room");
    } else {
        snprintf(log_message, sizeof(log_message), "ADMIN menghapus semua room");
    }
    log_activity(channel, log_message);
}
```
Sedikit tambahan pada delete room, terdapat fungsi tambahan `delete_room_all()` untuk menghapus semua room dalam channel sekaligus

### Format
[user/channel] CREATE ROOM room     
Room room dibuat    
[user/channel] EDIT ROOM old_room TO new_room   
old_room berhasil diubah menjadi new_room   
[user/channel] DEL ROOM room    
room berhasil dihapus   
[user/channel] DEL ROOM ALL     
Semua room dihapus      

### Contoh
[qurbancare/care] CREATE ROOM urban     
Room urban dibuat   
[qurbancare/care] EDIT ROOM urban TO nabru  
urban berhasil diubah menjadi nabru     
[qurbancare/care] DEL ROOM nabru    
nabru berhasil dihapus      
[qurbancare/care] DEL ROOM ALL      
Semua room dihapus      

### Screenshot
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/248817bb-f374-44e5-8ba9-6fc861a8e6c6)

![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/4edfb3d9-12f4-4709-871d-8a40d5c624a6)

![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/a28bf8f5-328a-4d5d-998b-3f79b35f65fb)

![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/e97aa185-22b7-4061-a1f8-16a6a7e611ad)

### Ban dan Unban
Admin dapat melakukan ban pada user yang nakal. Aktivitas ban tercatat pada `users.log`. Ketika di ban, role "user" berubah menjadi "banned". Data tetap tersimpan dan user tidak dapat masuk ke dalam channel.
```c
void ban_user(const char *channel, const char *target_user, client_info *client) {
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
    bool is_root = false;
    char auth_line[256];

    while (fgets(auth_line, sizeof(auth_line), auth_file)) {
        char *token = strtok(auth_line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        if (strcmp(token, client->logged_in_user) == 0) {
            token = strtok(NULL, ",");
            if (strstr(token, "ROOT") != NULL) {
                is_root = true;
            } else if (strstr(token, "ADMIN") != NULL) {
                is_admin = true;
            }
            break;
        }
    }

    fclose(auth_file);

    if (!is_admin && !is_root) {
        char response[] = "Anda tidak memiliki izin untuk melakukan ban user";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    // Open auth.csv to ban the target user
    auth_file = fopen(auth_path, "r+");
    if (!auth_file) {
        char response[] = "Gagal membuka file auth.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/admin/auth_temp.csv", channel);
    FILE *temp_file = fopen(temp_path, "w+");
    if (!temp_file) {
        char response[] = "Gagal membuat file sementara";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        fclose(auth_file);
        return;
    }

    bool found = false;
    bool banable = true;
    char line[256];

    while (fgets(line, sizeof(line), auth_file)) {
        char *user_id = strtok(line, ",");
        char *user_name = strtok(NULL, ",");
        char *role = strtok(NULL, ",");

        if (user_name && strcmp(user_name, target_user) == 0) {
            found = true;
            if (strstr(role, "ROOT") != NULL || strstr(role, "ADMIN") != NULL) {
                banable = false;
            } else {
                fprintf(temp_file, "%s,%s,BANNED", user_id, user_name);
                continue;
            }
        }
        fprintf(temp_file, "%s,%s,%s", user_id, user_name, role);
    }

    fclose(auth_file);
    fclose(temp_file);

    if (!found) {
        remove(temp_path);
        char response[] = "User tidak ditemukan di channel ini";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    if (!banable) {
        remove(temp_path);
        char response[] = "Anda tidak bisa melakukan ban pada ROOT atau ADMIN";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    remove(auth_path);
    rename(temp_path, auth_path);

    char response[100];
    snprintf(response, sizeof(response), "%s diban", target_user);
    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }

    char log_message[100];
    if(is_root){
        snprintf(log_message, sizeof(log_message), "ROOT ban %s", target_user);
    } else {
        snprintf(log_message, sizeof(log_message), "ADMIN ban %s", target_user);
    }
    log_activity(channel, log_message);
}
```

Admin  juga dapat melakukan unban pada user yang sudah berperilaku baik. Aktivitas unban tercatat pada `users.log`. Ketika di unban, role "banned" berubah kembali menjadi "user" dan dapat masuk ke dalam channel.
```c
void unban_user(const char *channel, const char *target_user, client_info *client) {
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
    bool is_root = false;
    char auth_line[256];

    while (fgets(auth_line, sizeof(auth_line), auth_file)) {
        char *token = strtok(auth_line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        if (strcmp(token, client->logged_in_user) == 0) {
            token = strtok(NULL, ",");
            if (strstr(token, "ROOT") != NULL) {
                is_root = true;
            } else if (strstr(token, "ADMIN") != NULL) {
                is_admin = true;
            }
            break;
        }
    }

    fclose(auth_file);

    if (!is_admin && !is_root) {
        char response[] = "Anda tidak memiliki izin untuk melakukan ban user";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    // Open auth.csv to ban the target user
    auth_file = fopen(auth_path, "r+");
    if (!auth_file) {
        char response[] = "Gagal membuka file auth.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/admin/auth_temp.csv", channel);
    FILE *temp_file = fopen(temp_path, "w+");
    if (!temp_file) {
        char response[] = "Gagal membuat file sementara";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        fclose(auth_file);
        return;
    }

    bool found = false;
    char line[256];

    while (fgets(line, sizeof(line), auth_file)) {
        char *user_id = strtok(line, ",");
        char *user_name = strtok(NULL, ",");
        char *role = strtok(NULL, ",");

        if (user_name && strcmp(user_name, target_user) == 0) {
            found = true;
            if(strstr(role, "USER") != NULL || strstr(role, "ADMIN") != NULL || strstr(role, "ROOT") != NULL){
                char response[] = "User tidak dalam status banned";
                if (write(client->socket, response, strlen(response)) < 0) {
                    perror("Gagal mengirim respons ke client");
                }
                return;
            } else {
                fprintf(temp_file, "%s,%s,USER", user_id, user_name);
                continue;
            }
        }
        fprintf(temp_file, "%s,%s,%s", user_id, user_name, role);
    }

    fclose(auth_file);
    fclose(temp_file);

    if (!found) {
        remove(temp_path);
        char response[] = "User tidak ditemukan di channel ini";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    remove(auth_path);
    rename(temp_path, auth_path);

    char response[100];
    snprintf(response, sizeof(response), "%s kembali", target_user);
    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }

    char log_message[100];
    if(is_root){
        snprintf(log_message, sizeof(log_message), "ROOT unban %s", target_user);
    } else {
        snprintf(log_message, sizeof(log_message), "ADMIN unban %s", target_user);
    }
    log_activity(channel, log_message);
}
```
### Format
[user/channel] BAN user1    
user1 diban     
[user/channel] UNBAN user1  
user1 kembali   

### Contoh
[qurbancare/care] BAN pen   
pen diban   
[qurbancare/care] UNBAN pen     
pen kembali     

### Screenshot
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/31608170-05d2-46a6-9564-0c414984a1ea)

![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/ee824e56-1401-4e35-b43e-686d54754fb6)

![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/05ac14cd-c980-475c-80a1-82a374ec9c3f)

### Remove User
Fungsionalitas kali ini mirip dengan remove user yang bisa dilakukan root user hanya saja user hanya diremove aksesnya dari sebuah channel namun masih bisa mengakses program tanpa perlu register ulang.
```c
void remove_user(const char *channel, const char *target_user, client_info *client) {
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
    bool is_root = false;
    char auth_line[256];

    while (fgets(auth_line, sizeof(auth_line), auth_file)) {
        char *token = strtok(auth_line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        if (strcmp(token, client->logged_in_user) == 0) {
            token = strtok(NULL, ",");
            if (strstr(token, "ROOT") != NULL){
                is_root = true;
            } else if (strstr(token, "ADMIN") != NULL) {
                is_admin = true;
            }
            break;
        }
    }

    fclose(auth_file);

    if (!is_admin && !is_root) {
        char response[] = "Anda tidak memiliki izin untuk menghapus user dari channel";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    // Open auth.csv to remove the target user
    auth_file = fopen(auth_path, "r+");
    if (!auth_file) {
        char response[] = "Gagal membuka file auth.csv";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/admin/auth_temp.csv", channel);
    FILE *temp_file = fopen(temp_path, "w+");
    if (!temp_file) {
        char response[] = "Gagal membuat file sementara";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        fclose(auth_file);
        return;
    }

    bool found = false;
    bool removable = true;
    char line[256];

    while (fgets(line, sizeof(line), auth_file)) {
        char *user_id = strtok(line, ",");
        char *user_name = strtok(NULL, ",");
        char *role = strtok(NULL, ",");

        if (user_name && strcmp(user_name, target_user) == 0) {
            found = true;
            if (strstr(role, "ROOT") != NULL || strstr(role, "ADMIN") != NULL) {
                removable = false;
            } else {
                continue;  // Skip writing this user to temp file
            }
        }
        fprintf(temp_file, "%s,%s,%s", user_id, user_name, role);
    }

    fclose(auth_file);
    fclose(temp_file);

    if (!found) {
        remove(temp_path);
        char response[] = "User tidak ditemukan di channel ini";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    if (!removable) {
        remove(temp_path);
        char response[] = "Anda tidak bisa menghapus ROOT atau ADMIN dari channel";
        if (write(client->socket, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
        }
        return;
    }

    remove(auth_path);
    rename(temp_path, auth_path);

    char response[100];
    snprintf(response, sizeof(response), "%s dikick", target_user);
    if (write(client->socket, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
    }

    char log_message[100];
    if(is_root){
        snprintf(log_message, sizeof(log_message), "ROOT kick %s", target_user);
    } else {
        snprintf(log_message, sizeof(log_message), "ADMIN kick %s", target_user);
    }
    log_activity(channel, log_message);
}
```

### Format
[user/channel] REMOVE USER user1    
user1 dikick    

### Contoh
[qurbancare/care] REMOVE USER pen   
pen dikick  

### Screenshot
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/c283d6b7-2739-4a23-ae7b-3bdd3ea33ee8)

### Log Activity
Seperti yang diminta oleh soal, segala aktivitas admin maupun root dalam melakukan pengelolaan channel, room, dan user dalam sebuah channel akan tercatat di dalam sebuah log yang tersimpan di folder admin bersama dengan `auth.csv`
```c
void log_activity(const char *channel, const char *message) {
    char log_path[256];
    snprintf(log_path, sizeof(log_path), "/home/rafaelega24/SISOP/FP/DiscorIT/%s/admin/user.log", channel);

    FILE *log_file = fopen(log_path, "a+");
    if (!log_file) {
        perror("Gagal membuka file user.log");
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char date[30];
    strftime(date, sizeof(date), "%d/%m/%Y %H:%M:%S", t);

    fprintf(log_file, "[%s] %s\n", date, message);
    fclose(log_file);
}
```
Dapat diperhatikan lagi bahwa function ini sudah dipanggil di setiap function terkait setelah berhasil melakukan suatu aksi.

### Screenshot
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/9fbaa983-b1cf-43a1-8a6b-7d332c1f5fef)

## E. User
User dapat mengubah informasi profil mereka, user yang di ban tidak dapat masuk kedalam channel dan dapat keluar dari room, channel, atau keluar sepenuhnya dari DiscorIT.

### Edit User's Username/Password
```c
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
                char salt[SALT_SIZE];
                snprintf(salt, sizeof(salt), "$2y$12$%.22s", "inistringsaltuntukbcrypt");
                char new_hash[BCRYPT_HASHSIZE];
                bcrypt_hashpw(new_value, salt,new_hash);
                
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
```
Penentuan untuk mengubah username/password sudah dilakukan di `handle_client()` sama seperti edit user yang dilakukan oleh root.


### Format
[user] EDIT PROFILE SELF -u new_username    
Profil diupdate     
[new_username]  
[user] EDIT PROFILE SELF -p new_password    
Password diupdate   

### Contoh
[qurbancare] EDIT PROFILE SELF -u pen   
Profil diupdate     
[pen]   
[pen] EDIT PROFILE SELF -p pen123    
Password diupdate   

### Screenshot
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/d1516dfa-e1a8-4ce7-bd11-5fd760c0e995)

![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/49d786d6-b059-4863-b0f6-7c267c0eb3b1)


### Banned User
Perihal ini sudah dijelaskan pada bagian `ban/unban user`

### Format
[user] JOIN channel     
anda telah diban, silahkan menghubungi admin

### Contoh
[qurbancare] JOIN care  
anda telah diban, silahkan menghubungi admin

### Screenshot
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/eb2f7540-dbc7-4ecf-bdbd-d4cc1551cb16)

### Exit
Bagian ini hanya menghandle bagaimana user keluar dari state room ke channel atau channel ke program serta keluar total dari program.
```c
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
```

### Format
[user/channel/room] EXIT    
[user/channel] EXIT     
[user] EXIT     

### Contoh
[qurbancare/care/urban] EXIT    
[qurbancare/care] EXIT  
[qurbancare] EXIT   

### Screenshot
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/ee56a9b0-d54e-45b1-92c7-73f2d409e1d8)

## F. Error Handling
Sudah ada beberapa error handling yang diterapkan di berbagai fungsi seperti input kosong, input tidak sesuai format, akses bagi user root/admin/banned, channel/room tidak ada, belum bergabung ke channel/room, dll.

## G. Monitor
- User dapat menampilkan isi chat secara real-time menggunakan program monitor. Jika ada perubahan pada isi chat, perubahan tersebut akan langsung ditampilkan di terminal.
- Sebelum dapat menggunakan monitor, pengguna harus login terlebih dahulu dengan cara yang mirip seperti login di DiscorIT.
- Untuk keluar dari room dan menghentikan program monitor dengan perintah "EXIT".
- Monitor dapat digunakan untuk menampilkan semua chat pada room, mulai dari chat pertama hingga chat yang akan datang nantinya.
```c
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
```
Pada bagian ini, konsepnya sederhana saja, program akan membaca status file dari `chat.csv` yang diminta dan akan melakukan update setiap kali terdapat perubahan pada status last edited sehingga tidak perlu melakukan print terus menerus dan hanya print ketika file csv benar-benar mengalami perubahan.

### Format
[username] -channel channel_name -room room_name     
sebelumnya  
[dd/mm/yyyy HH:MM:SS][id][username] “text”      
sesudahnya  
[user/channel/room] EXIT    
[user] EXIT     


### Contoh
[qurbancare] -channel care -room urban    
sebelumnya  
[05/06/2024 23:22:12][3][qurbancare] “hallo”    
sesudahnya      
[qurbancare/care/urban] EXIT    
[qurbancare] EXIT   

### Screenshot
![image](https://github.com/krisadityabcde/Sisop-FP-2024-MH-IT21/assets/144150187/1ed2dfa0-698a-46e8-a6d3-c0189e44500f)

## Kendala
Tidak ada kendala, program sudah selesai beberapa hari sebelum deadline, walau mungkin masih luput beberapa error handling ketika asistensi.

## Revisi
Tidak ada revisi dari asisten.
