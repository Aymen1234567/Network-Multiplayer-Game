// commun.h
#ifndef COMMUN_H
#define COMMUN_H

#define PORT 8081
#define MAX_JOUEURS 1
#define CARTES_PAR_JOUEUR 10
#define NB_RANGÉES 4
#define MAX_CARTES_PAR_RANGÉE 5
#define SEUIL_PERDU 10
#define TAILLE_MSG 2048

#define MSG_BIENVENUE    1
#define MSG_MAIN         2
#define MSG_JOUE         3
#define MSG_RÉSULTAT     4
#define MSG_FIN_MANCHE   5
#define MSG_FIN_PARTIE   6
#define MSG_CHOIX        7
#define MSG_PDF          8
#define MSG_CMN          9
#ifndef MSG_FIN
  #define MSG_FIN 10
#endif

#define MSG_PDF_FILE     11

typedef struct {
    int numero;
    int tetes_de_boeuf;
} Carte;

typedef struct {
    int type;
    char contenu[TAILLE_MSG];
} Message;

#endif