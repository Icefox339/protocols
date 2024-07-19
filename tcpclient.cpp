#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

int init();
void deinit();
int sock_err(const char* function, int s);
void s_close(int s);
unsigned int get_host_ipn(const char* name);
int send_to_server(int s, int8_t day, int8_t month, unsigned short year, int16_t aa, const char* tel_num, const char* message_text);
int send_request(int s, FILE* file);
int netdat_int32(int s, int32_t value);
int netdat_uint16(int s, unsigned short value);
int netdat_uint8(int s, int8_t value);
int parse_netdat(int, const char*);
int netdat_to_server(int s, const void* buf, size_t len);
int recv_response(int s, FILE* f);
int netdat_int16(int s, int16_t value);
int message_number = 0;

int init() {
    return 1; // Для других ОС действий не требуется
}

void deinit() {
    // Для других ОС действий не требуется
}

int sock_err(const char* function, int s) {
    int err;
    err = errno;
    fprintf(stderr, "%s: socket error: %d\n", function, err);
    return -1;
}

void s_close(int s) {
    close(s);
}

unsigned int get_host_ipn(const char* name) {
    struct addrinfo* addr = 0;
    unsigned int ip4addr = 0;
    // Функция возвращает все адреса указанного хоста
    // в виде динамического однонаправленного списка
    if (0 == getaddrinfo(name, 0, 0, &addr)) {
        struct addrinfo* cur = addr;
        while (cur) {
            // Интересует только IPv4 адрес, если их несколько - то первый
            if (cur->ai_family == AF_INET) {
                ip4addr = ((struct sockaddr_in*) cur->ai_addr)->sin_addr.s_addr;
                break;
            }
            cur = cur->ai_next;
        }
        freeaddrinfo(addr);
    }
    return ip4addr;
}

int send_to_server(int s,int8_t day, int8_t month, unsigned short year, int16_t aa, const char* tel_num, const char* message_text) {
    if (netdat_int32(s, message_number) != 0) {
        return -1;
    }

    if (netdat_uint8(s, day) != 0) {
        return -1; 
    }
    if (netdat_uint8(s, month) != 0) {
        return -1; 
    }
    if (netdat_uint16(s, year) != 0) {
        return -1; 
    }

    if (netdat_int16(s, aa) != 0) {
        return -1; 
    }
    if (netdat_to_server(s, tel_num, strlen(tel_num)) != 0) {
        return -1; 
    }
    if (netdat_to_server(s, message_text, strlen(message_text)) != 0) {
        return -1; 
    }
    if (netdat_uint8(s, 0) != 0) {
        return -1;
    }

    return 0;
}

int parse_netdat(int s, const char* message) {

    int day, month;
    unsigned short year;
    char tel_num[13];
    int aa;
    char kakashka[strlen(message) + 1];
    memset(kakashka, 0, sizeof(kakashka));
    char message_text[strlen(message) + 1];
    memset(message_text, 0, sizeof(message_text));

    int govno = sscanf(message, "%d.%d.%hu %d %[^\n]", &day, &month, &year, &aa, kakashka);
    int index_parse = 0 ;
    for (; kakashka[index_parse] != ' '; index_parse++){
        tel_num[index_parse] = kakashka[index_parse];
    }
    index_parse++;
    int ia = 0;
    for  (; kakashka[index_parse] != 0; index_parse++, ia++) {
        message_text[ia] = kakashka[index_parse];
    }
    tel_num[12] = '\0';

    if (send_to_server(s, day, month, year, aa, tel_num, message_text) != 0) {
        return -1; // error
    }

    char okMessage[2];
    ssize_t bytesRead = -1;
    bytesRead = recv(s, okMessage, 2, 0);
    printf("%li", bytesRead);
    if (bytesRead == -1) {
        perror("recv");
    } else if (bytesRead == 0) {
        std::cerr << "Server closed connection unexpectedly" << std::endl;
    } else if (bytesRead != 2 || okMessage[0] != 'o' || okMessage[1] != 'k') {
        std::cerr << "Unexpected response from server: " << okMessage[0] << okMessage[1] << std::endl;
    }
    return 0;
}

int send_request(int s, FILE* file) {
    char* buffer = NULL;
    unsigned long bufsize = 0;

    ssize_t reading;

    while ((reading = getline(&buffer, &bufsize, file)) != -1) {
        if (buffer[0] == '\n' || strlen(buffer) <= 1) {
            continue; // skip invalid row
        }
        // send stuff
        if (parse_netdat(s, buffer) != 0) {
            free(buffer);
            return -1; // error (
        }
        message_number++;
    }

    free(buffer);
    return 0;
}


bool sendData(int socket, const char *data, size_t length) {
    size_t totalSent = 0;
    while (totalSent < length) {
        ssize_t current_sent = send(socket, data + totalSent, length - totalSent, 0);
        if (current_sent == -1) {
            perror("send");
            return false;
        }
        totalSent += current_sent;
    }
    return true;
}

int netdat_int32(int s, int32_t value) {
    value = htonl(value); 
    return netdat_to_server(s, &value, sizeof(value));
}

int netdat_uint16(int s, unsigned short value) {
    value = htons(value); 
    return netdat_to_server(s, &value, sizeof(value));
}
int netdat_int16(int s, int16_t value){
    value = htons(value); 
    return netdat_to_server(s, &value, sizeof(value));
}
int netdat_uint8 (int s, int8_t value) {
    return netdat_to_server(s, &value, sizeof(value));
}

int netdat_to_server(int s, const void* buf, size_t len) {
    size_t server_received = 0;
    while (server_received < len) {
        int current_sent = send(s, (const char*)buf + server_received, len - server_received, 0);
        if (current_sent < 0) {
            return sock_err("send", s);
        }
        server_received += current_sent;
    }
    return 0;
}

int recv_response(int s, FILE* f) {
    char buffer[256];
    int res;
    // Принятие очередного блока данных.
    // Если соединение будет разорвано удаленным узлом recv вернет 0
    while ((res = recv(s, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, res, f);
        printf(" %d bytes received\n", res);
    }
    if (res < 0)
        return sock_err("recv", s);
    return 0;
}

int main(int argc, char* argv[]) {
    int port;
    if (argc == 3) {
        
    }
    else {
        fprintf(stderr, "Bad input\n", argv[0]);
    }
    char* ip_port = strtok(argv[1], ":");
    char* port_str = strtok(NULL, ":");
    if (ip_port == NULL || port_str == NULL) {
        fprintf(stderr, "Bad IP or Port");
        return 1;
    }
    const char* server_address = ip_port;
    port = atoi(port_str);
    const char* file_name = argv[2];
    // const char* server_address = "127.0.0.1";
    // port = 9000;
    // const char* file_name = "file1.txt";
    
    
    FILE* file = fopen(file_name, "r");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    int s;
    struct sockaddr_in addr;

    // Инициализация сетевой библиотеки
    if (!init()) {
        fprintf(stderr, "Error initializing network library\n");
        return 1;
    }

    // Создание TCP-сокета
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("Error creating socket");
        return 1;
    }

    // Заполнение структуры с адресом удаленного узла
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = get_host_ipn(server_address);

    // Установка соединения с удаленным хостом
    if (connect(s, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
        perror("Error connecting to server");
        s_close(s);
        return 1;
    }

    const char startMessage[] = {'p', 'u', 't'};
    if (!sendData(s, startMessage, sizeof(startMessage))) {
        close(s);
        return 1;
    }

    // Отправка запроса на удаленный сервер
    if (send_request(s, file) != 0) {
        fprintf(stderr, "Error sending request\n");
        s_close(s);
        fclose(file);
        return 1;
    }

    fclose(file);

    // Дожидаемся финального подтверждения от сервера
    
    // Закрытие соединения
    s_close(s);
    deinit();

    return 0;
}
