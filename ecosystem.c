#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

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
            else if (colocados < num_plantas + num_herviboros)
                ecosistema[i][j].tipo = HERBIVORE;
            else
                ecosistema[i][j].tipo = CARNIVORE;
                ecosistema[i][j].energia = ENERGIA_NUEVO_CARNIVORO;  
                ecosistema[i][j].edad = 0;
                ecosistema[i][j].ticks_sin_comer = 0;

            colocados++;
        }
    }
}

// Imprimir el estado del ecosistema
void imprimir_ecosistema() {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            char simbolo;
            switch (ecosistema[i][j].tipo) {
                case PLANT: simbolo = 'P'; break;
                case HERBIVORE: simbolo = 'H'; break;
                case CARNIVORE: simbolo = 'C'; break;
                default: simbolo = 'V'; // Vacío
            }
            printf("%c ", simbolo);
        }
        printf("\n");
    }
}

int es_valida(int x, int y) {
    return x >= 0 && x < N && y >= 0 && y < N;
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




int main() {
    srand(time(NULL));
    int num_plantas = 30;
    int num_herviboros = 25;
    int num_carnivoros = 5;

    inicializar_ecosistema(num_plantas, num_herviboros, num_carnivoros);
    printf("Ecosistema Inicial:\n");
    imprimir_ecosistema();
    for(int i = 1; i <= 10; i++){
        carnivore_update();
        printf("--------------------------\n");
        printf("**** TICK: %d ****\n", i);
        imprimir_ecosistema();
    }
    return 0;
}
