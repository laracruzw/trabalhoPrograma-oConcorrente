#include <stdlib.h>
#include "config.h"
#include "chieftain.h"
#include "valhalla.h"
#include <math.h>

void chieftain_init(chieftain_t *self, valhalla_t *valhalla)
{
    /* TODO: Adicionar código aqui se necessário! */
    pthread_mutex_init(&self->escolhaMesaMutex, NULL);
    pthread_cond_init(&self->liberouLugarCond, NULL);

    pthread_mutex_init(&self->comeramMutex, NULL);
    pthread_cond_init(&self->comeramCond, NULL);
    self->comeramCont = 0;

    if (config.table_size < 2) { // tem q ter no min 2 pratos
        fprintf(stderr, "Erro: a mesa precisa de ao menos 2 cadeiras (c precisa ser no mínimo 2).\n");
        exit(1); // exit vai encerra o programa
    }
    
    self->cadeira_ocupada = (int*) calloc(config.table_size, sizeof(int)); // tamanho da mesa, tem q iniciar em 0 pra nao dar errado
    self->cadeira_tipo = (int*) malloc(sizeof(int)*config.table_size); // se é berserker
    self->prato1 = (int*) malloc(sizeof(int)*config.table_size); // "prato na mao esquerda"
    self->prato2 = (int*) malloc(sizeof(int)*config.table_size); // "prato na mao direita"
    self->prato_ocupado = (int*) calloc(config.table_size, sizeof(int)); // tamanho da mesa, tem q iniciar em 0 pra nao dar errado


    pthread_mutex_init(&self->escolhaGodMutex, NULL);
    for (int i = 0; i < NUMBER_OF_GODS; i++) { // pra calcular pros super deuses
        self->rezasNormais[i] = 0;
    } 

    self->valhalla = valhalla;
    plog("[chieftain] Initialized\n");
}

int chieftain_acquire_seat_plates(chieftain_t *self, int berserker)
{
    pthread_mutex_lock(&(self->escolhaMesaMutex));
    int size = config.table_size;
    while (1) {
        for (int i = 0; i < size; i++) {
            if (self->cadeira_ocupada[i]) {
                continue; // cadeira ocupada
            }
            if (i > 0 && self->cadeira_ocupada[i-1] && (self->cadeira_tipo[i-1] != berserker)) {
                continue; // vizinho da esquerda é de outro tipo (como vao n conta, o i = 0 n pega)
            }
            if (i < (size - 1) && self->cadeira_ocupada[i+1] && (self->cadeira_tipo[i+1] != berserker)) {
                continue; // vizinho da direita é de outro tipo (como vao n conta, o ultimo i n pega)
            }
            int prato_esq = (i - 1 + size) % size; // + size pq pode ser o 0 e n ao pode ficar negativo
            int prato_dir = (i + 1) % size; // o mod é pq vai de 0 a size - 1
            int pratos_disp[] = {0, 0, 0}; // lista local pr averificacao previa de qual pegar
            if (self->prato_ocupado[prato_esq] == 0) {
                pratos_disp[0] = 1;
            }
            if (self->prato_ocupado[i] == 0) {
                pratos_disp[1] = 1;
            }
            if (self->prato_ocupado[prato_dir] == 0) {
                pratos_disp[2] = 1;
            }

            if (prato_dir == prato_esq) pratos_disp[2] = 0; 
            // se houver só 2 pratos: o dir e o esq sao o mesmo (há apenas 2 pratos), 
            // entao tenho que remover a duplicação ao transformar 
            // o de indice 0 ou 2 (o mesmo) em 0

            int soma = 0; // contador de pratos disponiveis

            for (int j = 0; j < 3; j++) {
                soma += pratos_disp[j];
            }

            if (soma < 2) {
                continue; // nao ha pratos suficientes disponiveis
            }
            
            int peguei = 0;
            if (pratos_disp[0] == 1) {
                self->prato1[i] = prato_esq;
                peguei++;
            }
            if (pratos_disp[1] == 1) {
                if (peguei == 0) { // ainda n ocupei a "mao" esq
                    self->prato1[i] = i;
                } else { // ja ocupei
                    self->prato2[i] = i; 
                }
                peguei++;
            }
            if (pratos_disp[2] == 1 && peguei < 2) { // ainda nao peguei 2 e verificacao de q o terceiro esta msm livre
                self->prato2[i] = prato_dir;
                peguei++;
            }

            self->prato_ocupado[self->prato1[i]] = 1; // indice da lista prato ocupado: pegado pela mao esquerda pelo lugar i
            self->prato_ocupado[self->prato2[i]] = 1; // indice da lista prato ocupado: pegado pela mao direita pelo lugar i
            self->cadeira_ocupada[i] = 1; // ocupa a cadeira
            self->cadeira_tipo[i] = berserker; // bota se é berserker
            pthread_mutex_unlock(&self->escolhaMesaMutex);
            return i;
        }
        // nao pegou nenhuma cadeira ent dorme até alguém liberar
        pthread_cond_wait(&self->liberouLugarCond, &self->escolhaMesaMutex); // barreira, quando dorme nao usa o lock, quando acorda, segue com o lock (segue 1 por vez no while)
    }
}

void chieftain_release_seat_plates(chieftain_t *self, int pos)
{
    pthread_mutex_lock(&(self->escolhaMesaMutex));

    self->prato_ocupado[self->prato1[pos]] = 0;
    self->prato_ocupado[self->prato2[pos]] = 0;
    self->cadeira_ocupada[pos] = 0;

    pthread_cond_broadcast(&self->liberouLugarCond); // acorda todo mundo pra tentar de novo
    pthread_mutex_unlock(&self->escolhaMesaMutex);
    
    // a barreira pra esperar comerem
    pthread_mutex_lock(&self->comeramMutex);
    self->comeramCont++;
    if (self->comeramCont == config.horde_size) { // todos comeram
        pthread_cond_broadcast(&self->comeramCond);
    } else {
        while (self->comeramCont < config.horde_size) { // ainda tem q esperar
            pthread_cond_wait(&self->comeramCond, &self->comeramMutex);
        }
    }
    pthread_mutex_unlock(&self->comeramMutex);
}

god_t chieftain_get_god(chieftain_t *self) // aqui que o calculo de deuses tem q estar
{
    /* TODO: Implementar! O código abaixo deve ser modificado! */
    
    pthread_mutex_lock(&self->escolhaGodMutex);

    int total_normais = 0;
    for (int i = BALDR; i <= JORD; i++) // soma todos os 6 deuses normais
        total_normais += self->rezasNormais[i];

    god_t god;
    int g = rand() % NUMBER_OF_GODS; // de 0 a 7 sao os deuses possiveis
    int coube = 0; // falso
    if (valhalla_is_super(g)) { // é super deus
        int tetoSuper = (int) ceil(total_normais * (1.0 + SUPER_GOD_TOLERANCE_RATE));
        if (self->rezasNormais[g] + 1 <= tetoSuper) { //. coube
            god = g;
            coube = 1;            
        } else { // era super deus mas n cabia
            g = rand() % 6; // pega um entre 0 e 5 (nao é super deus)
        }
    } 
    if (!coube) { // ou nao coube o super deus ou pegor deus normal desde o inicio
        god_t rival = valhalla_get_rival(g);
        int tetoRival = (int) ceil(self->rezasNormais[rival] * (1.0 + RIVAL_TOLERANCE_RATE));
        if (tetoRival < 1) { // caso o rival tenha 0 preces ainda
            tetoRival = 1; // pode rezar
        }            
        if (self->rezasNormais[g] + 1 <= tetoRival) { 
            god = g; // rezo p o g
        } else {
            god = rival; // rezo pro rival
        }
    }

    self->rezasNormais[god]++;    
    pthread_mutex_unlock(&self->escolhaGodMutex);
    return god;
}

void chieftain_finalize(chieftain_t *self)
{
    /* TODO: Adicionar código aqui se necessário! */
    pthread_mutex_destroy(&self->escolhaMesaMutex);
    pthread_cond_destroy(&self->liberouLugarCond);

    pthread_mutex_destroy(&self->comeramMutex);
    pthread_cond_destroy(&self->comeramCond);

    free(self->cadeira_ocupada);
    free(self->cadeira_tipo);   
    free(self->prato1); 
    free(self->prato2); 
    free(self->prato_ocupado); 

    pthread_mutex_destroy(&self->escolhaGodMutex);

    plog("[chieftain] Finalized\n");
}
