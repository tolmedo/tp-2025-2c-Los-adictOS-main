/* El query interpreter sería algo así:


void ejecutar_query(char* path_query, int program_counter) {
    FILE* archivo = fopen(path_query, "r");
    char* linea = NULL;
    size_t len = 0;
    int pc_actual = 0;
    
    // Saltar hasta el Program Counter indicado
    while (pc_actual < program_counter && getline(&linea, &len, archivo) != -1) {
        pc_actual++;
    }
    
    // Ejecutar desde el PC
    while (getline(&linea, &len, archivo) != -1) {
        t_instruccion* instr = parsear_instruccion(linea);
        
        if (instr) {
            ejecutar_instruccion(instr);
            destruir_instruccion(instr);
        }
        
        pc_actual++;
    }
    
    free(linea);
    fclose(archivo);
}

void ejecutar_instruccion(t_instruccion* instr) {
    switch (instr->tipo) {
        case INSTRUCCION_CREATE:
            log obligatorio: ## Query <QUERY_ID>: - Instrucción realizada: <INSTRUCCIÓN>
            storage_create_file(instr->argumentos.create.nombre_file, instr->argumentos.create.tag); // Envía al storage
            break;
            
        case INSTRUCCION_WRITE:
            log obligatorio: ## Query <QUERY_ID>: - Instrucción realizada: <INSTRUCCIÓN>
            // Primero escribir en memoria interna, luego al storage si es necesario
            escribir_memoria_interna(instr->argumentos.write.direccion_base, instr->argumentos.write.contenido);
            break;
            
        // ... resto de casos
    }
}

*/