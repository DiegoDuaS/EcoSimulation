#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include <stdbool.h>

#define N 10  // Tamaño del ecosistema (NxN)
#define PLANT 1
#define HERBIVORE 2
#define CARNIVORE 3
#define EMPTY 0
#define MAX_TICKS_SIN_COMER 3
#define ENERGIA_REPRODUCCION 3
#define ENERGIA_NUEVO_CARNIVORO 2
#define EDAD_MAXIMA 10

// Estructura para la celda del ecosistema
typedef struct {
    int tipo;               // Tipo de entidad
    int energia;            // Solo usado por herbívoros/carnívoros
    int ticks_sin_comer;    // Para muerte por comida
    int edad;               // Para muerte por vejez
} Celda;

Celda ecosistema[N][N];

int dx[] = {-1, 1, 0, 0};
int dy[] = {0, 0, -1, 1};

// Función para inicializar la matriz
void inicializar_ecosistema(int num_plantas, int num_herviboros, int num_carnivoros) {
    // Inicializar todo vacío
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            ecosistema[i][j].tipo = EMPTY;

    int total = num_plantas + num_herviboros + num_carnivoros;

    int colocados = 0;
    while (colocados < total) {
        int i = rand() % N;
        int j = rand() % N;

        if (ecosistema[i][j].tipo == EMPTY) {
            if (colocados < num_plantas)
                ecosistema[i][j].tipo = PLANT;
            else if (colocados < num_plantas + num_herviboros){
                ecosistema[i][j].tipo = HERBIVORE;
                ecosistema[i][j].energia = ENERGIA_NUEVO_CARNIVORO;  
                ecosistema[i][j].edad = 0;
                ecosistema[i][j].ticks_sin_comer = 0;
            }
            else{
                ecosistema[i][j].tipo = CARNIVORE;
                ecosistema[i][j].energia = ENERGIA_NUEVO_CARNIVORO;  
                ecosistema[i][j].edad = 0;
                ecosistema[i][j].ticks_sin_comer = 0;
            }
            
            colocados++;
        }
    }
}

// Imprimir el estado del ecosistema
void imprimir_ecosistema() {
    // Imprimir encabezado de columnas
    printf("    ");
    for (int j = 0; j < N; j++)
        printf("%2d ", j);
    printf("\n");

    // Línea superior
    printf("   +");
    for (int j = 0; j < N; j++)
        printf("---");
    printf("+\n");

    for (int i = 0; i < N; i++) {
        printf("%2d |", i); // Índice de fila

        for (int j = 0; j < N; j++) {
            char simbolo;
            switch (ecosistema[i][j].tipo) {
                case PLANT:     simbolo = 'P'; break;
                case HERBIVORE: simbolo = 'H'; break;
                case CARNIVORE: simbolo = 'C'; break;
                default:        simbolo = ' '; break;
            }
            printf(" %c ", simbolo);
        }

        printf("|\n");
    }

    // Línea inferior
    printf("   +");
    for (int j = 0; j < N; j++)
        printf("---");
    printf("+\n");
}

void imprimir_resumen() {
    int count_plant = 0;
    int count_herbivore = 0;
    int count_carnivore = 0;
    int count_empty = 0;

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            char simbolo;
            switch (ecosistema[i][j].tipo) {
                case PLANT: count_plant++; break;
                case HERBIVORE: count_herbivore++; break;
                case CARNIVORE: count_carnivore++; break;
                default:count_empty++;
            }
        }
    }

    // Mostrar resumen
    printf("\nResumen:\n");
    printf("Plantas:     %d\n", count_plant);
    printf("Herbívoros:  %d\n", count_herbivore);
    printf("Carnívoros:  %d\n", count_carnivore);
    printf("Vacíos:      %d\n", count_empty);
}

int es_valida(int x, int y) {
    return x >= 0 && x < N && y >= 0 && y < N;
}

void plant_update() {
    Celda copia[N][N];

    // Copiar estado actual
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            copia[i][j] = ecosistema[i][j];

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            // Reconocer su espacio al rededor
            Celda* vecinos_V[4];
            int count_V = 0;

            for (int d = 0; d < 4; d++) {
                int ni = i + dx[d];
                int nj = j + dy[d];

                if (ni >= 0 && ni < N && nj >= 0 && nj < N) {
                    Celda* vecina = &copia[ni][nj];

                    if (vecina->tipo == EMPTY) {
                        vecinos_V[count_V++] = vecina;
                    }
                }
            }

            if (copia[i][j].tipo == PLANT) {
                // Muerte al inicio (si no tiene espacio para crecer)
                if (count_V == 0) {
                    copia[i][j].tipo = EMPTY;
                    // printf("Murio en (%d,%d)\n", i, j); debug
                    continue;  // No hace más nada, pasa al siguiente carnívoro
                }

                // Reproducción/Expansión
                for (int d = 0; d < count_V; d++) {
                    int probabilidad = 30; // 30%
                    if (rand() % 100 < probabilidad) {
                        vecinos_V[d]->tipo = PLANT;
                    }
                }
            }
        }
        
    }
    // Aplicar cambios
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            ecosistema[i][j] = copia[i][j];
}

void carnivore_update() {
    Celda copia[N][N];

    // Copiar estado actual
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            copia[i][j] = ecosistema[i][j];

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if (ecosistema[i][j].tipo == CARNIVORE) {
                // 1. Muerte al inicio
                if (ecosistema[i][j].energia <= 0 ||
                    ecosistema[i][j].ticks_sin_comer >= MAX_TICKS_SIN_COMER ||
                    ecosistema[i][j].edad >= EDAD_MAXIMA) {
                    copia[i][j].tipo = EMPTY;
                    // printf("Murio en (%d,%d)\n", i, j); debug
                    continue;  // No hace más nada, pasa al siguiente carnívoro
                }

                int hizo_algo = 0;

                // 2. Comer
                for (int d = 0; d < 4; d++) {
                    int ni = i + dx[d];
                    int nj = j + dy[d];
                    if (es_valida(ni, nj) && ecosistema[ni][nj].tipo == HERBIVORE) {
                        copia[ni][nj].tipo = CARNIVORE;
                        copia[ni][nj].energia = ecosistema[i][j].energia + 2;
                        copia[ni][nj].ticks_sin_comer = 0;
                        copia[ni][nj].edad = ecosistema[i][j].edad + 1;
                        copia[i][j].tipo = EMPTY;
                        // printf("Comio en (%d,%d)\n", ni, nj); debug
                        hizo_algo = 1;
                        break;
                    }
                }

                // 3. Reproducirse (si no comió)
                if (!hizo_algo && ecosistema[i][j].energia >= ENERGIA_REPRODUCCION) {
                    for (int d = 0; d < 4; d++) {
                        int ni = i + dx[d];
                        int nj = j + dy[d];
                        if (es_valida(ni, nj) && ecosistema[ni][nj].tipo == EMPTY) {
                            copia[ni][nj].tipo = CARNIVORE;
                            copia[ni][nj].energia = ENERGIA_NUEVO_CARNIVORO;
                            copia[ni][nj].ticks_sin_comer = 0;
                            copia[ni][nj].edad = 0;

                            copia[i][j].energia -= 2;
                            // printf("Se reprodujo en (%d,%d)\n", ni, nj); debug
                            hizo_algo = 1;
                            break;
                        }
                    }
                }

                // 4. Moverse (si no comió ni se reprodujo)
                if (!hizo_algo) {
                    for (int d = 0; d < 4; d++) {
                        int ni = i + dx[d];
                        int nj = j + dy[d];
                        if (es_valida(ni, nj) && ecosistema[ni][nj].tipo == EMPTY) {
                            copia[ni][nj].tipo = CARNIVORE;
                            copia[ni][nj].energia = ecosistema[i][j].energia;
                            copia[ni][nj].ticks_sin_comer = ecosistema[i][j].ticks_sin_comer + 1;
                            copia[ni][nj].edad = ecosistema[i][j].edad + 1;
                            copia[i][j].tipo = EMPTY;
                            // printf("Se movio a (%d,%d)\n", ni, nj); debug
                            hizo_algo = 1;
                            break;
                        }
                    }
                }

                // 5. Si no hizo nada, permanece y envejece
                if (!hizo_algo) {
                    copia[i][j].ticks_sin_comer++;
                    copia[i][j].edad++;
                    // printf("No hizo nada en (%d,%d)\n", i, j); debug
                }

                // 6. Pierde energía
                if (copia[i][j].tipo == CARNIVORE)
                    copia[i][j].energia--;
            }
        }
    }

    // Aplicar cambios
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            ecosistema[i][j] = copia[i][j];
}

void herbivore_update() {
    Celda copia[N][N];

    // Copiar estado actual
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            copia[i][j] = ecosistema[i][j];

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if (ecosistema[i][j].tipo == HERBIVORE) {
                // 1. Muerte al inicio
                if (ecosistema[i][j].energia <= 0 ||
                    ecosistema[i][j].ticks_sin_comer >= MAX_TICKS_SIN_COMER ||
                    ecosistema[i][j].edad >= EDAD_MAXIMA) {
                    copia[i][j].tipo = EMPTY;
                    // printf("Herbívoro murió en (%d,%d)\n", i, j); // debug
                    continue;  // No hace más nada, pasa al siguiente herbívoro
                }

                int hizo_algo = 0;

                // 2. Verificar si hay carnívoros cerca (huir tiene prioridad)
                int hay_carnivoro_cerca = 0;
                for (int d = 0; d < 4; d++) {
                    int ni = i + dx[d];
                    int nj = j + dy[d];
                    if (es_valida(ni, nj) && ecosistema[ni][nj].tipo == CARNIVORE) {
                        hay_carnivoro_cerca = 1;
                        break;
                    }
                }

                // 3. Huir de carnívoros (prioridad máxima)
                if (hay_carnivoro_cerca) {
                    for (int d = 0; d < 4; d++) {
                        int ni = i + dx[d];
                        int nj = j + dy[d];
                        if (es_valida(ni, nj) && ecosistema[ni][nj].tipo == EMPTY) {
                            // Verificar que la celda de escape no tenga carnívoros cerca
                            int escape_seguro = 1;
                            for (int d2 = 0; d2 < 4; d2++) {
                                int ni2 = ni + dx[d2];
                                int nj2 = nj + dy[d2];
                                if (es_valida(ni2, nj2) && ecosistema[ni2][nj2].tipo == CARNIVORE) {
                                    escape_seguro = 0;
                                    break;
                                }
                            }
                            
                            if (escape_seguro) {
                                copia[ni][nj].tipo = HERBIVORE;
                                copia[ni][nj].energia = ecosistema[i][j].energia;
                                copia[ni][nj].ticks_sin_comer = ecosistema[i][j].ticks_sin_comer + 1;
                                copia[ni][nj].edad = ecosistema[i][j].edad + 1;
                                copia[i][j].tipo = EMPTY;
                                // printf("Herbívoro huyó a (%d,%d)\n", ni, nj); // debug
                                hizo_algo = 1;
                                break;
                            }
                        }
                    }
                }

                // 4. Comer plantas (si no huyó)
                if (!hizo_algo) {
                    for (int d = 0; d < 4; d++) {
                        int ni = i + dx[d];
                        int nj = j + dy[d];
                        if (es_valida(ni, nj) && ecosistema[ni][nj].tipo == PLANT) {
                            copia[ni][nj].tipo = HERBIVORE;
                            copia[ni][nj].energia = ecosistema[i][j].energia + 1; // Gana 1 energía
                            copia[ni][nj].ticks_sin_comer = 0; // Resetea hambre
                            copia[ni][nj].edad = ecosistema[i][j].edad + 1;
                            copia[i][j].tipo = EMPTY;
                            // printf("Herbívoro comió en (%d,%d)\n", ni, nj); // debug
                            hizo_algo = 1;
                            break;
                        }
                    }
                }

                // 5. Reproducirse (si no huyó ni comió, y tiene energía suficiente)
                if (!hizo_algo && ecosistema[i][j].energia >= ENERGIA_REPRODUCCION) {
                    for (int d = 0; d < 4; d++) {
                        int ni = i + dx[d];
                        int nj = j + dy[d];
                        if (es_valida(ni, nj) && ecosistema[ni][nj].tipo == EMPTY) {
                            copia[ni][nj].tipo = HERBIVORE;
                            copia[ni][nj].energia = ENERGIA_NUEVO_CARNIVORO; // Misma energía inicial
                            copia[ni][nj].ticks_sin_comer = 0;
                            copia[ni][nj].edad = 0;

                            copia[i][j].energia -= 2; // Costo de reproducción
                            copia[i][j].edad++; // Envejece
                            // printf("Herbívoro se reprodujo en (%d,%d)\n", ni, nj); // debug
                            hizo_algo = 1;
                            break;
                        }
                    }
                }

                // 6. Moverse hacia plantas (si no hizo nada más)
                if (!hizo_algo) {
                    // Buscar plantas cercanas primero
                    int mejor_x = -1, mejor_y = -1;
                    for (int d = 0; d < 4; d++) {
                        int ni = i + dx[d];
                        int nj = j + dy[d];
                        if (es_valida(ni, nj) && ecosistema[ni][nj].tipo == EMPTY) {
                            // Verificar si hay plantas cerca de esta posición
                            for (int d2 = 0; d2 < 4; d2++) {
                                int ni2 = ni + dx[d2];
                                int nj2 = nj + dy[d2];
                                if (es_valida(ni2, nj2) && ecosistema[ni2][nj2].tipo == PLANT) {
                                    mejor_x = ni;
                                    mejor_y = nj;
                                    break;
                                }
                            }
                            if (mejor_x != -1) break;
                        }
                    }

                    // Si encontró una buena posición, moverse ahí
                    if (mejor_x != -1) {
                        copia[mejor_x][mejor_y].tipo = HERBIVORE;
                        copia[mejor_x][mejor_y].energia = ecosistema[i][j].energia;
                        copia[mejor_x][mejor_y].ticks_sin_comer = ecosistema[i][j].ticks_sin_comer + 1;
                        copia[mejor_x][mejor_y].edad = ecosistema[i][j].edad + 1;
                        copia[i][j].tipo = EMPTY;
                        hizo_algo = 1;
                        // printf("Herbívoro se movió hacia plantas a (%d,%d)\n", mejor_x, mejor_y); // debug
                    } else {
                        // Movimiento aleatorio si no hay plantas cerca
                        for (int d = 0; d < 4; d++) {
                            int ni = i + dx[d];
                            int nj = j + dy[d];
                            if (es_valida(ni, nj) && ecosistema[ni][nj].tipo == EMPTY) {
                                copia[ni][nj].tipo = HERBIVORE;
                                copia[ni][nj].energia = ecosistema[i][j].energia;
                                copia[ni][nj].ticks_sin_comer = ecosistema[i][j].ticks_sin_comer + 1;
                                copia[ni][nj].edad = ecosistema[i][j].edad + 1;
                                copia[i][j].tipo = EMPTY;
                                // printf("Herbívoro se movió aleatoriamente a (%d,%d)\n", ni, nj); // debug
                                hizo_algo = 1;
                                break;
                            }
                        }
                    }
                }

                // 7. Si no pudo hacer nada, permanece y envejece
                if (!hizo_algo) {
                    copia[i][j].ticks_sin_comer++;
                    copia[i][j].edad++;
                    // printf("Herbívoro no hizo nada en (%d,%d)\n", i, j); // debug
                }

                // 8. Pierde energía (si sigue vivo)
                if (copia[i][j].tipo == HERBIVORE)
                    copia[i][j].energia--;
            }
        }
    }

    // Aplicar cambios
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            ecosistema[i][j] = copia[i][j];
}

int main() {
    srand(time(NULL));
    int num_plantas = 30;
    int num_herviboros = 25;
    int num_carnivoros = 5;

    inicializar_ecosistema(num_plantas, num_herviboros, num_carnivoros);
    printf("Ecosistema Inicial:\n");
    printf("Celdas disponibles: %d", N*N);
    imprimir_resumen();
    for(int i = 1; i <= 10; i++){
        printf("--------------------------\n");
        printf("**** TICK: %d ****\n", i);
        printf("**** CAMBIO PLANTA ****\n");
        plant_update();
        imprimir_resumen();
        printf("--------------------------\n");
        printf("**** CAMBIO CARNIVORO ****\n");
        carnivore_update();
        imprimir_resumen();
        printf("--------------------------\n");
        printf("**** ACTUALIZANDO HERBIVOROS ****\n");
        herbivore_update();
        imprimir_resumen();
        
    }
    printf("\nEcosistema Final:\n");
    imprimir_ecosistema();
    return 0;
}
