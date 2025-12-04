# CPUCollector

agent_cpu_mem.c tiene una función donde extrae los siguientes datos y los guarda en un arreglo de chars (string). La cadena tiene el siguiente formato:

<cpu_label>;<logical_ip>;<cpu_usage>;<cpu_user>;<cpu_system>;<cpu_idle>;<mem_used_mb>;<mem_total_mb>;<swap_total_mb>;<swap_free_mb>\n


Ejemplo
cpu;192.168.1.1;18.38;13.94;3.49;81.62;2939.65;3819.66;3995.00;3356.91
