#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
    if (argc != 4) {
        printf("Uso: %s <ip_recolector> <puerto> <ip_logica_agente>\n", argv[0]);
        return 1;
    }

    char *ip_recolector = argv[1];
    int puerto = atoi(argv[2]);
    char *ip_logica_agente = argv[3];


    char* values=extract_cpu_mem_info(ip_logica_agente);
    printf("values: %s\n", values);


    
    

    return 0;
}