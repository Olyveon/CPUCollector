#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

#define PORT 8080
#define BACKLOG 10
#define MAX_CLIENTS 4
#define TIMEOUT_SECONDS 5  // Timeout after 5 seconds without data



int server_fd;
struct host_info {
    char ip[32];
    float cpu_usage;
    float cpu_user;
    float cpu_system;
    float cpu_idle;
    float mem_used_mb;
    float mem_free_mb;
    float swap_total_mb;
    float swap_free_mb;
    time_t last_update;  // Track when data was last received
    int is_active;       // Flag to mark if client has data
};



// Crea hosts para almacenar la información de los clientes, definido por MAX_CLIENTS
struct host_info clients[MAX_CLIENTS];



// Necesitamos un mutex para coordinar
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
// Contador de clientes activos



int active_clients = 0;



// Manejo de señal para cerrar el socket limpiamente
void handle_sigint(int sig){
    printf("\n[RECOLECTOR] Cerrando socket y saliendo...\n");
    close(server_fd);
    exit(0);
}



// Parsea el mensaje recibido usando sscanf
int parse_cpu_message(const char *msg, struct host_info *h) {
    int n = sscanf(msg, "CPU;%31[^;];%f;%f;%f;%f;%f;%f;%f;%f",
                   h->ip,
                   &h->cpu_usage,
                   &h->cpu_user,
                   &h->cpu_system,
                   &h->cpu_idle,
                   &h->mem_used_mb,
                   &h->mem_free_mb,
                   &h->swap_total_mb,
                   &h->swap_free_mb);
    if (n == 9) {
        return 0;
    } else {
        return -1;
    }
}



void print_table() {
    // Limpia la pantalla
    system("clear");
    // Imprime la tabla de clientes
    printf("%-20s %-12s %-12s %-12s %-12s %-16s %-16s %-16s %-16s\n", "IP", "CPU%", "User%", "System%", "Idle%", "Mem Used(MB)", "Mem Free(MB)", "Swap Total(MB)", "Swap Free(MB)");
    printf("%-20s %-12s %-12s %-12s %-12s %-16s %-16s %-16s %-16s\n", "--------------------", "----------", "----------", "----------", "----------", "----------------", "----------------", "-----------------", "----------------");
    
    time_t current_time = time(NULL);
    
    // Revisa todos los clientes en la lista de clientes 
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (strlen(clients[i].ip) > 0) {
            // Check if client data is stale
            if (clients[i].is_active && (current_time - clients[i].last_update) > TIMEOUT_SECONDS) {
                printf("%-20s %-12s\n", clients[i].ip, "without data");
            } else if (clients[i].is_active) {
                printf("%-20s %-12.2f %-12.2f %-12.2f %-12.2f %-16.2f %-16.2f %-16.2f %-16.2f\n", 
                       clients[i].ip, clients[i].cpu_usage, clients[i].cpu_user, 
                       clients[i].cpu_system, clients[i].cpu_idle, clients[i].mem_used_mb, 
                       clients[i].mem_free_mb, clients[i].swap_total_mb, clients[i].swap_free_mb);
            } else {
                printf("%-20s %-12s %-12s %-12s %-12s %-16s %-16s %-16s %-16s\n", "----", "----", "----", "----", "----", "----", "----", "----", "----");
            }
        }
    }
    printf("\n[Active clients: %d/%d]\n", active_clients, MAX_CLIENTS);
}



void* handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);
    
    char buffer[1025];
    struct host_info host;
    int client_index = -1;

    // Set socket receive timeout
    struct timeval tv;
    tv.tv_sec = TIMEOUT_SECONDS;
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);


    while(1){
        int bytes = recv(client_fd, buffer, sizeof(buffer)-1, 0);
        
        if (bytes < 0) {
            // Timeout or error
            if (errno == EAGAIN || errno == EWOULDBLOCK) {// Timeout occurred - client hasn't sent data
                
                pthread_mutex_lock(&clients_mutex); // ---------------------------- critical zone INIT
                if (client_index >= 0) {
                    clients[client_index].is_active = 0;
                    print_table();
                }
                pthread_mutex_unlock(&clients_mutex); // ---------------------------- critical zone END
                
                // Continue waiting for data
                continue; 

            } else { 
                // Real error - client disconnected naturally witch ctrl + c for example!!
                printf("[RECOLECTOR] Cliente desconectado\n");

                pthread_mutex_lock(&clients_mutex); // ---------------------------- critical zone INIT
                if (client_index>=0){
                    memset(&clients[client_index], 0, sizeof(struct host_info));
                    active_clients--;
                    print_table();
                }
                pthread_mutex_unlock(&clients_mutex); // ---------------------------- critical zone END

                close(client_fd);
                break;
            }
        }
        
        if (bytes == 0){
            printf("[RECOLECTOR] Cliente desconectado\n");
            
            // Remueve el cliente de la tabla en caso de desconexion y para evitar errores de sincronizacion usa mutex

            pthread_mutex_lock(&clients_mutex); // ---------------------------- critical zone INIT

            if (client_index >= 0) {

                memset(&clients[client_index], 0, sizeof(struct host_info));
                if (active_clients>0){
                    active_clients--;
                }
            }

            pthread_mutex_unlock(&clients_mutex); // --------------------------- critical zone END
            close(client_fd);
            break;
        }

        buffer[bytes] = '\0';
        memset(&host, 0, sizeof(host));

        if (parse_cpu_message(buffer, &host) == 0) { // successful case

            pthread_mutex_lock(&clients_mutex); // ---------------------------- critical zone INIT
            // Find or assign slot for this client
            if (client_index == -1) { 
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (strlen(clients[i].ip) == 0) {
                        client_index = i;
                        active_clients++;
                        break;
                    }
                }
            }
            
            // Update client data if we have an available slot
            if (client_index >= 0) { 
                memcpy(&clients[client_index], &host, sizeof(struct host_info));
                clients[client_index].last_update = time(NULL);
                clients[client_index].is_active = 1;
                printf("[RECOLECTOR] Datos recibidos de %s\n", host.ip);
                print_table();

            } else {
                printf("[RECOLECTOR] Max clients (%d) reached. Ignoring connection from %s\n", MAX_CLIENTS, host.ip);
            }
            
            pthread_mutex_unlock(&clients_mutex); //-------------------------- critical zone END


        } else {
            printf("[RECOLECTOR] Formato de mensaje inválido\n");
        }
        
        // Echo response
        send(client_fd, buffer, strlen(buffer), 0);
    }

    pthread_exit(NULL);
}



int main(int argc, char *argv[]){
    
    struct sockaddr_in server, client;
    signal(SIGINT, handle_sigint);
    
    // Parse port from command line
    int port = PORT;  // Default port
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Error: Port must be between 1 and 65535\n");
            exit(-1);
        }
    }
    
    // Initialize clients array
    memset(clients, 0, sizeof(clients));

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd == -1){
        perror("Error al crear el socket");
        exit(-1);
    } 
    
    int opt = 2;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);  // Use the parsed port
    server.sin_addr.s_addr = INADDR_ANY; 
    memset(&(server.sin_zero), 0, 8);

    int r = bind(server_fd, (struct sockaddr *)&server, sizeof(struct sockaddr)); 
    if (r == -1){
        perror("Error al hacer el bind");
        close(server_fd);
        exit(-1);
    }

    r = listen(server_fd, BACKLOG);
    if (r == -1){
        perror("Error al hacer Listen");
        close(server_fd);
        exit(-1);
    }

    printf("[RECOLECTOR] Escuchando en el puerto %d . . . \n", port);
    
    while(1){
        socklen_t addrlen = sizeof(struct sockaddr_in);
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, (struct sockaddr*)&client, &addrlen);
        
        if (*client_fd == -1){
            perror("Error al aceptar la conexion");
            free(client_fd);
            continue;
        }

        printf("[RECOLECTOR] Cliente conectado\n");
        
        pthread_t thread_id;
        // Crea un thread para manejar al cliente y maneja el error 
        if (pthread_create(&thread_id, NULL, handle_client, client_fd) != 0) {
            perror("Error al crear thread");
            close(*client_fd);
            free(client_fd);
        }
        pthread_detach(thread_id); // Let OS clean up thread
    }

    close(server_fd);
    return 0;
}