#include <stdio.h>
#include <stdlib.h>
#include "../../utils/src/utils/utils.h"

void ejecutar_comando(const char* comando){
    printf("Ejecutando: %s\n", comando);
    int resultado = system(comando);
    if(resultado != 0){
        fprintf(stderr, "Error al ejecutar el comando %s\n",comando);
        exit(EXIT_FAILURE);
    }
}
int main(){
    ejecutar_comando("cd ../memoria && make all");
    ejecutar_comando("../memoria/bin/memoria");
    
    ejecutar_comando("cd ../cpu && make all");
    ejecutar_comando("../cpu/bin/cpu");
    
    ejecutar_comando("cd ../kernel && make all");
    ejecutar_comando("../kernel/bin/kernel archivoPseudocodigo.txt 100");

    printf("Ejecucion completada con exito. \n");
    return 0;
}