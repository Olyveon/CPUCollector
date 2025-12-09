#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

int fd;

void handle_sigint(int sig){
    shutdown(fd, SHUT_RDWR);
    printf("\n[AGENTE] Cerrando socket y saliendo...\n");
    close(fd);
    exit(0);
}

char* extract_cpu_mem_info(char* logical_ip) {

    //----------------- LEER MEMORIA -----------------
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) {
        perror("fopen");
        return "ERROR: no se pudo abrir /proc/meminfo";
    }

    char line[256];
    long mem_total_kb = 0;
    long mem_free_kb  = 0;
    long swap_total_kb = 0;
    long swap_free_kb  = 0;

    while (fgets(line, sizeof(line), f)) {
        sscanf(line, "MemTotal: %ld kB", &mem_total_kb);
        sscanf(line, "MemFree: %ld kB", &mem_free_kb);
        sscanf(line, "SwapTotal: %ld kB", &swap_total_kb);
        sscanf(line, "SwapFree: %ld kB", &swap_free_kb);
    }
    fclose(f);

    double mem_total_mb = mem_total_kb / 1024.0;
    double mem_free_mb  = mem_free_kb  / 1024.0;
    double mem_used_mb  = mem_total_mb - mem_free_mb;

    double swap_total_mb = swap_total_kb / 1024.0;
    double swap_free_mb  = swap_free_kb  / 1024.0;



    //----------------- LEER CPU -----------------
    FILE *fcpu = fopen("/proc/stat", "r");
    if (!fcpu) {
        perror("fopen");
        return "ERROR: no se pudo abrir /proc/stat";
    }

    char cpu_label[10];
    unsigned long user, nice, system, idle, iowait, irq, softirq, steal;

    if (fscanf(fcpu, "%9s %lu %lu %lu %lu %lu %lu %lu %lu",
               cpu_label, &user, &nice, &system, &idle,
               &iowait, &irq, &softirq, &steal) != 9) {
        fclose(fcpu);
        return "ERROR: no se pudo leer /proc/stat de cpu";
    }
    fclose(fcpu);

    unsigned long total = user + nice + system + idle + iowait + irq + softirq + steal;

    double cpu_usage  = (double)(total - idle) * 100.0 / total;
    double cpu_user   = (double)user   * 100.0 / total;
    double cpu_system = (double)system * 100.0 / total;
    double cpu_idle   = (double)idle   * 100.0 / total;
    snprintf(cpu_label, sizeof(cpu_label), "%s", "CPU");


    
    //----------------- CREAR STRING RESULTADO -----------------
    char *buffer = malloc(300);
    
    snprintf(buffer, 300,
             "%s;%s;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f\n",
             cpu_label, logical_ip,
             cpu_usage, cpu_user, cpu_system, cpu_idle,
             mem_used_mb, mem_total_mb, swap_total_mb, swap_free_mb
    );
    // <cpu_label>;<logical_ip>;<cpu_usage>;<cpu_user>;<cpu_system>;<cpu_idle>;<mem_used_mb>;<mem_total_mb>;<swap_total_mb>;<swap_free_mb>
    
    return buffer;
}



int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);
    if (argc != 4) {
        printf("Uso: %s <ip_recolector> <puerto> <ip_logica_agente>\n", argv[0]);
        return 1;
    }

    char *ip_recolector = argv[1];
    int puerto = atoi(argv[2]);
    char *ip_logica_agente = argv[3];

    struct sockaddr_in cliente;

    char buffer[1025];

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("Error al crear el socket");
        exit(-1);
    }

    cliente.sin_family = AF_INET;
    cliente.sin_port = htons(puerto);
    cliente.sin_addr.s_addr = inet_addr(ip_recolector);

    memset(&(cliente.sin_zero), 0, 8);

    int r = connect(fd, (struct sockaddr *)&cliente, sizeof(struct sockaddr));
    if (r == -1) {
        perror("Error al conectar con el recolector");
        close(fd);
        exit(-1);
    }

    while (1) {
        char* values=extract_cpu_mem_info(ip_logica_agente);
        send(fd, values, strlen(values), 0);
        printf("[AGENTE] Enviado: %s", values);
        sleep(2); // Espera 2 segundos antes de la siguiente lectura
        
        free(values);
    }

    close(fd);
    return 0;
}