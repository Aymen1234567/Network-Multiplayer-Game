// joueur_robot.c – Robot intelligent pour 6 qui prend
// gcc -o joueur_robot joueur_robot.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "commun.h"

#define MAX_MAIN 10

// --- VARIABLES GLOBALES ---
int ma_main[MAX_MAIN];
int nb_cartes = 0;

int rangées[NB_RANGÉES][MAX_CARTES_PAR_RANGÉE];
int nb_cartes_rangée[NB_RANGÉES];

int mon_numero = -1;

// --- PROTOTYPES ---
void lire_rangées(const char *msg);
int choisir_carte_intelligente(void);
int peut_placer(int carte, int *rangée_cible);
int calculer_tetes_rangée(int r);
void afficher_main(void);
int tetes_de_boeuf(int i);


// ============================================================================
//                                MAIN
// ============================================================================
int main() {
srand(time(NULL) ^ getpid());


int sock = socket(AF_INET, SOCK_STREAM, 0);
if (sock < 0) { perror("socket"); return 1; }

struct sockaddr_in serv_addr = {
    .sin_family = AF_INET,
    .sin_port = htons(PORT)
};
inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("connect"); 
    return 1;
}

memset(nb_cartes_rangée, 0, sizeof(nb_cartes_rangée));

Message msg;
while (1) {
    if (recv(sock, &msg, sizeof(msg), 0) <= 0) break;

    switch (msg.type) {

        case MSG_BIENVENUE:
            sscanf(msg.contenu, "Bienvenue ! Vous êtes le joueur %d", &mon_numero);
            printf("[ROBOT J%d] Connecté.\n", mon_numero);
            break;

        case MSG_RÉSULTAT: {
            // Lecture des rangées et de la main
             printf("\n[ROBOT J%d] État du jeu :\n%s\n", mon_numero, msg.contenu);
            lire_rangées(msg.contenu);

            // Afficher l'état et la main du robot
            //printf("\n[ROBOT J%d] État du jeu :\n%s\n", mon_numero, msg.contenu);
           // afficher_main();

            // Choisir la carte à jouer
            if (nb_cartes > 0) {
                int carte = choisir_carte_intelligente();

                Message joue = {MSG_JOUE, ""};
                sprintf(joue.contenu, "%d", carte);
                send(sock, &joue, sizeof(joue), 0);

                // Retirer la carte de la main
                for (int i = 0; i < nb_cartes; i++) {
                    if (ma_main[i] == carte) {
                        ma_main[i] = ma_main[nb_cartes-1];
                        nb_cartes--;
                        break;
                    }
                }

                printf("[ROBOT J%d] Je joue %d\n", mon_numero, carte);
            }
            break;
        }

        case MSG_CHOIX:{
            int min = 9999, cible = 0;
                for (int i = 0; i < NB_RANGÉES; i++) {
                    int tot = 0;
                    for (int k = 0; k < nb_cartes_rangée[i]; k++) tot += tetes_de_boeuf(rangées[i][k]);
                    if (tot < min) { min = tot; cible = i; }
                } 

           
            Message joue = { MSG_JOUE, "" };
            snprintf(joue.contenu, TAILLE_MSG, "%d", cible);
            send(sock, &joue, sizeof(joue), 0);
            break;
        }    

        case MSG_PDF: {
            printf("%s\n", msg.contenu);

            // --- Le robot répond automatiquement "non" ---
            Message rep = { MSG_CMN, "" };
            snprintf(rep.contenu, TAILLE_MSG, "non");

            send(sock, &rep, sizeof(rep), 0);
            break;
        }

        case MSG_FIN:
                printf("%s\n", msg.contenu);
                if (msg.type == MSG_FIN) goto fin;
                break;
                
        case MSG_FIN_MANCHE:  
        case MSG_FIN_PARTIE:
            printf("[ROBOT] %s\n", msg.contenu);
            break;
    }
}


fin:
close(sock);
return 0;
}

// ============================================================================
//                         LECTURE DES RANGÉES
// ============================================================================
void lire_rangées(const char *msg) {
char temp[TAILLE_MSG];
strcpy(temp, msg);


char *line = strtok(temp, "\n");
int r = 0;

while (line && r < NB_RANGÉES) {
    if (strstr(line, "Rangée")) {

        int num_cartes;
        sscanf(line, "Rangée %*d (%d/5):", &num_cartes);
        nb_cartes_rangée[r] = num_cartes;

        char *ptr = strchr(line, ':');
        if (ptr) ptr += 2;

        char *tok = strtok(ptr, " ");
        int c = 0;

        while (tok && c < MAX_CARTES_PAR_RANGÉE) {
            char *paren = strchr(tok, '(');
            if (paren) *paren = '\0'; 
            rangées[r][c++] = atoi(tok);
            tok = strtok(NULL, " ");
        }
        r++;
    }
    line = strtok(NULL, "\n");
}

// Mise à jour de la main du robot
char *main_ptr = strstr(msg, "Votre main :");
nb_cartes = 0;
if (main_ptr) {
    main_ptr += strlen("Votre main :");
    char *tok = strtok(main_ptr, " ");
    while (tok && nb_cartes < MAX_MAIN) {
        char *paren = strchr(tok, '(');
        if (paren) *paren = '\0';
        int n = atoi(tok);
        if (n > 0) ma_main[nb_cartes++] = n;
        tok = strtok(NULL, " ");
    }
}


}


// ============================================================================
//                        CHOIX INTELLIGENT DU ROBOT
// ============================================================================
int choisir_carte_intelligente(void) {
int meilleure = ma_main[0];
int meilleur_score = 9999;


for (int i = 0; i < nb_cartes; i++) {
    int carte = ma_main[i];
    int r;

    if (peut_placer(carte, &r)) {
        if (carte < meilleure) meilleure = carte;
        continue;
    }
}

return meilleure;


}

// ============================================================================
//                 TEST SI UNE CARTE PEUT ÊTRE POSÉE
// ============================================================================
int peut_placer(int carte, int *rangée_cible) {
*rangée_cible = -1;
int diff_min = 1000;


for (int r = 0; r < NB_RANGÉES; r++) {
    if (nb_cartes_rangée[r] == 0) continue;
    if (nb_cartes_rangée[r] >= MAX_CARTES_PAR_RANGÉE) continue;

    int derniere = rangées[r][nb_cartes_rangée[r] - 1];
    if (carte > derniere) {
        int diff = carte - derniere;
        if (diff < diff_min) {
            diff_min = diff;
            *rangée_cible = r;
        }
    }
}

return (*rangée_cible != -1);


}

// ============================================================================
//                 CALCUL DU SCORE (TÊTES DE BŒUF)
// ============================================================================
int calculer_tetes_rangée(int r) {
int total = 0;


for (int i = 0; i < nb_cartes_rangée[r]; i++) {
    int n = rangées[r][i];

    if (n % 11 == 0) total += 7;
    else if (n % 10 == 0) total += 5;
    else if (n % 5 == 0) total += 3;
    else if (n % 2 == 0) total += 2;
    else total += 1;
}

return total;


}

// ============================================================================
//                     AFFICHER LA MAIN DU ROBOT
// ============================================================================
void afficher_main(void) {
printf("[ROBOT J%d] Ma main : ", mon_numero);
for (int i = 0; i < nb_cartes; i++) {
printf("%d ", ma_main[i]);
}
//printf("\n");
}


// ============================================================================
//                     calculer tete boeuf total d une rangeé
// ============================================================================
int tetes_de_boeuf(int numero) {
    if (numero % 11 == 0) return 7;
    else if (numero % 10 == 0) return 5;
    else if (numero % 5 == 0) return 3;
    else if (numero % 2 == 0) return 2;
    else return 1;
}

