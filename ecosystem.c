#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include <stdbool.h>

#define N 50  // Tamaño del ecosistema (NxN)
#define PLANT 1
#define HERBIVORE 2
#define CARNIVORE 3
#define EMPTY 0
#define MAX_TICKS_SIN_COMER 3
#define ENERGIA_REPRODUCCION 2
#define ENERGIA_NUEVO 3
#define EDAD_MAXIMA 10

// Estructura para la celda del ecosistema
typedef struct {
    int tipo;               // Tipo de entidad
    int energia;            // Solo usado por herbívoros/carnívoros
    int ticks_sin_comer;    // Para muerte por comida
    int edad;               // Para muerte por vejez
} Celda;

Celda ecosistema[N][N];

// Locks por celda para proteger escrituras concurrentes en "copia"
omp_lock_t locks[N][N];

int dx[] = {-1, 1, 0, 0};
int dy[] = {0, 0, -1, 1};

static inline void lock_cell(int i, int j) {
    omp_set_lock(&locks[i][j]);
}
static inline void unlock_cell(int i, int j) {
    omp_unset_lock(&locks[i][j]);
}

// Bloquea dos celdas en orden consistente (para evitar deadlocks)
static inline void lock_two(int a1, int b1, int a2, int b2) {
    if (a1 < a2 || (a1 == a2 && b1 <= b2)) {
        omp_set_lock(&locks[a1][b1]);
        omp_set_lock(&locks[a2][b2]);
    } else {
        omp_set_lock(&locks[a2][b2]);
        omp_set_lock(&locks[a1][b1]);
    }
}
static inline void unlock_two(int a1, int b1, int a2, int b2) {
    // Desbloquea en orden inverso al que bloqueaste (no obligatorio, pero ordenado)
    if (a1 < a2 || (a1 == a2 && b1 <= b2)) {
        omp_unset_lock(&locks[a2][b2]);
        omp_unset_lock(&locks[a1][b1]);
    } else {
        omp_unset_lock(&locks[a1][b1]);
        omp_unset_lock(&locks[a2][b2]);
    }
}

int es_valida(int x, int y) {
    return x >= 0 && x < N && y >= 0 && y < N;
}

// Función para inicializar la matriz (capping total a N*N)
void inicializar_ecosistema(int num_plantas, int num_herviboros, int num_carnivoros) {
    // Inicializar todo vacío
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            ecosistema[i][j].tipo = EMPTY;
            ecosistema[i][j].energia = 0;
            ecosistema[i][j].ticks_sin_comer = 0;
            ecosistema[i][j].edad = 0;
        }

    int total = num_plantas + num_herviboros + num_carnivoros;
    if (total > N * N) total = N * N; // evitar bucle infinito

    int colocados = 0;
    while (colocados < total) {
        int i = rand() % N;
        int j = rand() % N;

        if (ecosistema[i][j].tipo == EMPTY) {
            if (colocados < num_plantas) {
                ecosistema[i][j].tipo = PLANT;
                // demás campos no necesarios para planta
            } else if (colocados < num_plantas + num_herviboros) {
                ecosistema[i][j].tipo = HERBIVORE;
                ecosistema[i][j].energia = ENERGIA_NUEVO;
                ecosistema[i][j].edad = 0;
                ecosistema[i][j].ticks_sin_comer = 0;
            } else {
                ecosistema[i][j].tipo = CARNIVORE;
                ecosistema[i][j].energia = ENERGIA_NUEVO;
                ecosistema[i][j].edad = 0;
                ecosistema[i][j].ticks_sin_comer = 0;
            }
            colocados++;
        }
    }
}

// Imprimir el estado del ecosistema (como lo tenías)
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

// Imprimir tamaño de cada población
void imprimir_resumen() {
    int count_plant = 0;
    int count_herbivore = 0;
    int count_carnivore = 0;
    int count_empty = 0;

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            switch (ecosistema[i][j].tipo) {
                case PLANT: count_plant++; break;
                case HERBIVORE: count_herbivore++; break;
                case CARNIVORE: count_carnivore++; break;
                default: count_empty++;
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

void plant_update() {
    Celda copia[N][N];

    // Copiar estado actual a copia (single-thread)
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            copia[i][j] = ecosistema[i][j];

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N; i++) {
        // semilla por fila/hilo
        unsigned int seed = (unsigned int) time(NULL) ^ (unsigned) omp_get_thread_num() ^ (i * 73856093u);

        for (int j = 0; j < N; j++) {
            if (ecosistema[i][j].tipo != PLANT) continue;

            // Buscar vecinos vacíos leyendo SIEMPRE del estado viejo
            int vecinos_V[4][2];
            int count_V = 0;

            for (int d = 0; d < 4; d++) {
                int ni = i + dx[d];
                int nj = j + dy[d];
                if (ni >= 0 && ni < N && nj >= 0 && nj < N) {
                    if (ecosistema[ni][nj].tipo == EMPTY) {
                        vecinos_V[count_V][0] = ni;
                        vecinos_V[count_V][1] = nj;
                        count_V++;
                    }
                }
            }

            // Muerte si no hay espacio
            if (count_V == 0) {
                // bloquear la celda (escritura en copia)
                lock_cell(i, j);
                copia[i][j].tipo = EMPTY;
                unlock_cell(i, j);
                continue;
            }

            // Reproducción/expansión: escribir en copia (cada destino protegido)
            for (int d = 0; d < count_V; d++) {
                if (rand() % 100 < 30) { // 30%
                    int ni = vecinos_V[d][0];
                    int nj = vecinos_V[d][1];
                    // bloquear destino y origen para escribir consistentemente
                    lock_two(ni, nj, i, j);
                    // Escribir en destino (puede ser sobrescrito por otro hilo, lock lo serializa)
                    copia[ni][nj].tipo = PLANT;
                    // Nota: no toco energia/ticks/edad para plantas
                    unlock_two(ni, nj, i, j);
                }
            }
        }
    }

    // Aplicar cambios (single-thread)
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            ecosistema[i][j] = copia[i][j];
}

void carnivore_update() {
    Celda copia[N][N];

    // Copiar estado actual (single-thread)
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            copia[i][j] = ecosistema[i][j];

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if (ecosistema[i][j].tipo != CARNIVORE) continue;

            // 1. Muerte al inicio (lee ecosistema)
            if (ecosistema[i][j].energia <= 0 ||
                ecosistema[i][j].ticks_sin_comer >= MAX_TICKS_SIN_COMER ||
                ecosistema[i][j].edad >= EDAD_MAXIMA) {
                lock_cell(i, j);
                copia[i][j].tipo = EMPTY;
                unlock_cell(i, j);
                continue;
            }

            int hizo_algo = 0;
            int dest_i = i, dest_j = j;

            // 2. Comer (prioridad)
            for (int d = 0; d < 4 && !hizo_algo; d++) {
                int ni = i + dx[d];
                int nj = j + dy[d];
                if (es_valida(ni, nj) && ecosistema[ni][nj].tipo == HERBIVORE) {
                    // bloquear destino y origen
                    lock_two(ni, nj, i, j);
                    copia[ni][nj].tipo = CARNIVORE;
                    copia[ni][nj].energia = ecosistema[i][j].energia + 2;
                    copia[ni][nj].ticks_sin_comer = 0;
                    copia[ni][nj].edad = ecosistema[i][j].edad + 1;
                    copia[i][j].tipo = EMPTY;
                    unlock_two(ni, nj, i, j);

                    dest_i = ni; dest_j = nj;
                    hizo_algo = 1;
                    break;
                }
            }

            // 3. Reproducirse (si no comió)
            if (!hizo_algo && ecosistema[i][j].energia >= ENERGIA_REPRODUCCION) {
                for (int d = 0; d < 4 && !hizo_algo; d++) {
                    int ni = i + dx[d];
                    int nj = j + dy[d];
                    if (es_valida(ni, nj) && ecosistema[ni][nj].tipo == EMPTY) {
                        lock_two(ni, nj, i, j);
                        copia[ni][nj].tipo = CARNIVORE;
                        copia[ni][nj].energia = ENERGIA_NUEVO;
                        copia[ni][nj].ticks_sin_comer = 0;
                        copia[ni][nj].edad = 0;

                        // reducir energía en la posición original (en copia)
                        copia[i][j].energia -= 2;
                        unlock_two(ni, nj, i, j);

                        hizo_algo = 1;
                        break;
                    }
                }
            }

            // 4. Moverse (si no comió ni se reprodujo)
            if (!hizo_algo) {
                for (int d = 0; d < 4 && !hizo_algo; d++) {
                    int ni = i + dx[d];
                    int nj = j + dy[d];
                    if (es_valida(ni, nj) && ecosistema[ni][nj].tipo == EMPTY) {
                        lock_two(ni, nj, i, j);
                        copia[ni][nj].tipo = CARNIVORE;
                        copia[ni][nj].energia = ecosistema[i][j].energia;
                        copia[ni][nj].ticks_sin_comer = ecosistema[i][j].ticks_sin_comer + 1;
                        copia[ni][nj].edad = ecosistema[i][j].edad + 1;
                        copia[i][j].tipo = EMPTY;
                        unlock_two(ni, nj, i, j);

                        dest_i = ni; dest_j = nj;
                        hizo_algo = 1;
                        break;
                    }
                }
            }

            // 5. Si no hizo nada, permanece y envejece (modificar copia en celda i,j)
            if (!hizo_algo) {
                lock_cell(i, j);
                copia[i][j].ticks_sin_comer++;
                copia[i][j].edad++;
                unlock_cell(i, j);
            }

            // 6. Pierde energía: decrementar en la celda final si sigue siendo carnívoro
            // (aseguramos hacerlo con lock)
            lock_cell(dest_i, dest_j);
            if (copia[dest_i][dest_j].tipo == CARNIVORE) copia[dest_i][dest_j].energia--;
            unlock_cell(dest_i, dest_j);
        }
    }

    // Aplicar cambios (single-thread)
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

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if (ecosistema[i][j].tipo != HERBIVORE) continue;

            // 1. Muerte al inicio
            if (ecosistema[i][j].energia <= 0 ||
                ecosistema[i][j].ticks_sin_comer >= MAX_TICKS_SIN_COMER ||
                ecosistema[i][j].edad >= EDAD_MAXIMA) {
                lock_cell(i, j);
                copia[i][j].tipo = EMPTY;
                unlock_cell(i, j);
                continue;
            }

            int hizo_algo = 0;
            int dest_i = i, dest_j = j;

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
                for (int d = 0; d < 4 && !hizo_algo; d++) {
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
                            lock_two(ni, nj, i, j);
                            copia[ni][nj].tipo = HERBIVORE;
                            copia[ni][nj].energia = ecosistema[i][j].energia;
                            copia[ni][nj].ticks_sin_comer = ecosistema[i][j].ticks_sin_comer + 1;
                            copia[ni][nj].edad = ecosistema[i][j].edad + 1;
                            copia[i][j].tipo = EMPTY;
                            unlock_two(ni, nj, i, j);

                            dest_i = ni; dest_j = nj;
                            hizo_algo = 1;
                            break;
                        }
                    }
                }
            }

            // 4. Comer plantas (si no huyó)
            if (!hizo_algo) {
                for (int d = 0; d < 4 && !hizo_algo; d++) {
                    int ni = i + dx[d];
                    int nj = j + dy[d];
                    if (es_valida(ni, nj) && ecosistema[ni][nj].tipo == PLANT) {
                        lock_two(ni, nj, i, j);
                        copia[ni][nj].tipo = HERBIVORE;
                        copia[ni][nj].energia = ecosistema[i][j].energia + 1; // Gana 1 energía
                        copia[ni][nj].ticks_sin_comer = 0; // Resetea hambre
                        copia[ni][nj].edad = ecosistema[i][j].edad + 1;
                        copia[i][j].tipo = EMPTY;
                        unlock_two(ni, nj, i, j);

                        dest_i = ni; dest_j = nj;
                        hizo_algo = 1;
                        break;
                    }
                }
            }

            // 5. Reproducirse (si no huyó ni comió, y tiene energía suficiente)
            if (!hizo_algo && ecosistema[i][j].energia >= ENERGIA_REPRODUCCION) {
                for (int d = 0; d < 4 && !hizo_algo; d++) {
                    int ni = i + dx[d];
                    int nj = j + dy[d];
                    if (es_valida(ni, nj) && ecosistema[ni][nj].tipo == EMPTY) {
                        lock_two(ni, nj, i, j);
                        copia[ni][nj].tipo = HERBIVORE;
                        copia[ni][nj].energia = ENERGIA_NUEVO;
                        copia[ni][nj].ticks_sin_comer = 0;
                        copia[ni][nj].edad = 0;

                        copia[i][j].energia -= 2; // Costo de reproducción
                        copia[i][j].edad++; // Envejece
                        unlock_two(ni, nj, i, j);

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
                        int cerca_plant = 0;
                        for (int d2 = 0; d2 < 4; d2++) {
                            int ni2 = ni + dx[d2];
                            int nj2 = nj + dy[d2];
                            if (es_valida(ni2, nj2) && ecosistema[ni2][nj2].tipo == PLANT) {
                                cerca_plant = 1;
                                break;
                            }
                        }
                        if (cerca_plant) {
                            mejor_x = ni;
                            mejor_y = nj;
                            break;
                        }
                    }
                }

                // Si encontró una buena posición, moverse ahí
                if (mejor_x != -1) {
                    lock_two(mejor_x, mejor_y, i, j);
                    copia[mejor_x][mejor_y].tipo = HERBIVORE;
                    copia[mejor_x][mejor_y].energia = ecosistema[i][j].energia;
                    copia[mejor_x][mejor_y].ticks_sin_comer = ecosistema[i][j].ticks_sin_comer + 1;
                    copia[mejor_x][mejor_y].edad = ecosistema[i][j].edad + 1;
                    copia[i][j].tipo = EMPTY;
                    unlock_two(mejor_x, mejor_y, i, j);

                    dest_i = mejor_x; dest_j = mejor_y;
                    hizo_algo = 1;
                } else {
                    // Movimiento aleatorio si no hay plantas cerca
                    for (int d = 0; d < 4 && !hizo_algo; d++) {
                        int ni = i + dx[d];
                        int nj = j + dy[d];
                        if (es_valida(ni, nj) && ecosistema[ni][nj].tipo == EMPTY) {
                            lock_two(ni, nj, i, j);
                            copia[ni][nj].tipo = HERBIVORE;
                            copia[ni][nj].energia = ecosistema[i][j].energia;
                            copia[ni][nj].ticks_sin_comer = ecosistema[i][j].ticks_sin_comer + 1;
                            copia[ni][nj].edad = ecosistema[i][j].edad + 1;
                            copia[i][j].tipo = EMPTY;
                            unlock_two(ni, nj, i, j);

                            dest_i = ni; dest_j = nj;
                            hizo_algo = 1;
                            break;
                        }
                    }
                }
            }

            // 7. Si no pudo hacer nada, permanece y envejece
            if (!hizo_algo) {
                lock_cell(i, j);
                copia[i][j].ticks_sin_comer++;
                copia[i][j].edad++;
                unlock_cell(i, j);
            }

            // 8. Pierde energía (si sigue vivo) -> decrementar en celda final
            lock_cell(dest_i, dest_j);
            if (copia[dest_i][dest_j].tipo == HERBIVORE) copia[dest_i][dest_j].energia--;
            unlock_cell(dest_i, dest_j);
        }
    }

    // Aplicar cambios (single-thread)
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            ecosistema[i][j] = copia[i][j];
}

// ---------------------------------- MAIN ----------------------------------

int main() {
    srand(time(NULL));

    // Inicializar locks
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            omp_init_lock(&locks[i][j]);

    // Ajusta estos números (serán caps a N*N)
    int num_plantas = 300;
    int num_herviboros = 200;
    int num_carnivoros = 75;

    int num_TICKS = 10;

    inicializar_ecosistema(num_plantas, num_herviboros, num_carnivoros);
    printf("Ecosistema Inicial:\n");
    printf("Celdas disponibles: %d\n", N * N);
    imprimir_resumen();
    
    for (int t = 1; t <= num_TICKS; t++) {
        printf("--------------------------\n");
        printf("**** TICK: %d ****\n", t);
        plant_update();
        carnivore_update();
        herbivore_update();
        imprimir_resumen();
        imprimir_ecosistema();
    }

    // Destruir locks
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            omp_destroy_lock(&locks[i][j]);

    return 0;
}