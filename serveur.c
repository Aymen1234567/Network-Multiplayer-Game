// serveur.c
//gcc -o serveur serveur.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>
#include "commun.h"
#include <sys/stat.h>
#include <unistd.h> 

#include <pthread.h>

// Définir un mutex global ---> QUAND LES JOUEUR COMMENTE IL YA UNE CONCURENCE
pthread_mutex_t commentaire_mutex = PTHREAD_MUTEX_INITIALIZER;
void log_message(const char *fmt, ...);
// Structure pour passer les infos à chaque thread
typedef struct {
    int joueur_idx;
    int sock;
    int *pdf_demande;
    char (*commentaires)[256];
} ThreadPDFArgs;

// ==================== CONFIGURATION ====================
#define MAX_JOUEURS_POSSIBLE 10   // Maximum autorisé (règle officielle du jeu)
int NB_JOUEURS;                    // Nombre réel de joueurs (défini au lancement)

// ======================================================

Carte paquet[104];// le paquet principal contient 104 carte
int   paquet_melange[104]; //le paquet melanger
int manche_prefixe_affiche = 0;
Carte rangées[NB_RANGÉES][MAX_CARTES_PAR_RANGÉE + 1];// les cartes  par ranger
int   nb_cartes_rangée[NB_RANGÉES] = {0};

int scores[MAX_JOUEURS_POSSIBLE] = {0}; //enregistre pour chaque joueur
Carte main_joueur[MAX_JOUEURS_POSSIBLE][CARTES_PAR_JOUEUR]; // la main du joueur
Carte main_joueur2[MAX_JOUEURS_POSSIBLE][CARTES_PAR_JOUEUR]; // ce que reste de carte pour le joueur aprés qu'il joue
int nb_cartes_main[MAX_JOUEURS_POSSIBLE] = {0};

int sockets_joueurs[MAX_JOUEURS_POSSIBLE];
int nb_joueurs_connectés = 0;// nb de joueur connecté
int manche = 0;// nombre de manche joué

FILE *log_file = NULL;

int carte_jouée_ce_tour[MAX_JOUEURS_POSSIBLE];   // pour affichage (j1) (j2) ...

// Fonction thread pour gérer PDF + commentaire pour un joueur
void* thread_pdf_commentaire(void *arg) {
    ThreadPDFArgs *args = (ThreadPDFArgs*)arg;
    int j = args->joueur_idx;
    int sock = args->sock;

    // Envoyer question PDF
    Message ask_pdf = { MSG_PDF, "" };
    snprintf(ask_pdf.contenu, TAILLE_MSG,
             "Voulez-vous générer un PDF des statistiques du match ? (oui/non)");
    send(sock, &ask_pdf, sizeof(ask_pdf), 0);

    // Réception réponse
    Message rep_pdf;
    recv(sock, &rep_pdf, sizeof(rep_pdf), 0);

    if (strncasecmp(rep_pdf.contenu, "oui", 3) == 0) {
        args->pdf_demande[j] = 1;

        // Demande si commentaire
        Message ask_com = { MSG_CMN, "" };
        snprintf(ask_com.contenu, TAILLE_MSG,
                 "Voulez-vous ajouter un commentaire dans le PDF ? (oui/non)");
        send(sock, &ask_com, sizeof(ask_com), 0);

        Message rep_com;
        recv(sock, &rep_com, sizeof(rep_com), 0);

        if (strncasecmp(rep_com.contenu, "oui", 3) == 0) {
            // Demander le commentaire
            Message ask_text = { MSG_CMN, "" };
            snprintf(ask_text.contenu, TAILLE_MSG, "Entrez votre commentaire :");
            send(sock, &ask_text, sizeof(ask_text), 0);

            // recois du commentaire
            Message com_text;
            memset(&com_text, 0, sizeof(com_text));
            recv(sock, &com_text, sizeof(com_text), 0);

            // Accès protégé pour écrire dans commentaires et log
            pthread_mutex_lock(&commentaire_mutex);
            strncpy(args->commentaires[j], com_text.contenu, 255);
            log_message(" commentaire Joueur %d : %s ", j+1 , args->commentaires[j]);
            pthread_mutex_unlock(&commentaire_mutex);
        }
    } else {
        // Aucun PDF pour ce joueur → informer
        Message done = { MSG_FIN, "" };
        snprintf(done.contenu, TAILLE_MSG, "Fin");
        send(sock, &done, sizeof(done), 0);
    }

    return NULL;
}


void initialiser_paquet();
void melanger_paquet();
void distribuer_cartes();
void envoyer_état_à_tous();
int  trouver_rangée(Carte c);
void traiter_carte_jouée(int joueur_idx, int numero_carte);
void log_message(const char *fmt, ...);
void envoyer_pdf(int sock, const char *filename);

int main() {
    srand(time(NULL));

    log_file = fopen("log_jeu.txt", "w");
    if (!log_file) { perror("log"); exit(1); }

    // ==== DEMANDE DU NOMBRE DE JOUEURS ====
    do {
        printf("Combien de joueurs ? (2 à %d) : ", MAX_JOUEURS_POSSIBLE);
        if (scanf("%d", &NB_JOUEURS) != 1 || NB_JOUEURS < 2 || NB_JOUEURS > MAX_JOUEURS_POSSIBLE) {
            printf("Nombre invalide ! Veuillez entrer un nombre entre 2 et %d.\n", MAX_JOUEURS_POSSIBLE);
            while (getchar() != '\n'); // vider le buffer
            NB_JOUEURS = 0;
        }
    } while (NB_JOUEURS < 2);

    fprintf(log_file, "============= 6 QUI PREND – %d JOUEUR(S) =============\n\n", NB_JOUEURS);
    fflush(log_file);

    initialiser_paquet();

    initialiser_paquet();
    int serveur_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(serveur_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); exit(1); }
    if (listen(serveur_sock, 10) < 0) { perror("listen"); exit(1); }

    printf("Serveur lancé – en attente de %d joueur(s)...\n", NB_JOUEURS);

    // Attente des joueurs
   while (nb_joueurs_connectés < NB_JOUEURS) {
        int client = accept(serveur_sock, NULL, NULL);
        if (client < 0) continue;

        sockets_joueurs[nb_joueurs_connectés] = client;

        char bienv[256];
        snprintf(bienv, sizeof(bienv),
                 "Bienvenue ! Vous êtes le joueur %d\nEn attente des autres... (%d/%d)",
                 nb_joueurs_connectés + 1, nb_joueurs_connectés + 1, NB_JOUEURS);

        Message m = {MSG_BIENVENUE, ""};
        strncpy(m.contenu, bienv, TAILLE_MSG-1);
        send(client, &m, sizeof(m), 0);

        nb_joueurs_connectés++;
        printf("Joueur %d connecté (%d/%d)\n", nb_joueurs_connectés, nb_joueurs_connectés, NB_JOUEURS);
    }

    // Début de la partie
    manche = 1;
    melanger_paquet();
    distribuer_cartes();

    log_message("PARTIE COMMENCÉE – %d joueur(s)", NB_JOUEURS);
    for (int r = 0; r < NB_RANGÉES; r++) {
    log_message("  Rangée %d : %d(%d)", r+1, rangées[r][0].numero,rangées[r][0].tetes_de_boeuf);
}
    for (int i = 0; i < NB_JOUEURS; i++) carte_jouée_ce_tour[i] = -1;
    memset(carte_jouée_ce_tour, -1, sizeof(carte_jouée_ce_tour));
    envoyer_état_à_tous();
    

    while (1) {
        // Reset cartes jouées du manche
        memset(carte_jouée_ce_tour, -1, sizeof(carte_jouée_ce_tour));
        for (int tour = 0; tour < CARTES_PAR_JOUEUR; tour++) {
        // Reset cartes jouées du tour
         memset(carte_jouée_ce_tour, -1, sizeof(carte_jouée_ce_tour));

   
    int cartes[NB_JOUEURS], ordre_joueurs[NB_JOUEURS];
    for (int j = 0; j < NB_JOUEURS; j++) {
    int carte_valide = 0;


        while (!carte_valide) {
            Message msg;
            recv(sockets_joueurs[j], &msg, sizeof(msg), 0);

            int num;
            if (sscanf(msg.contenu, "%d", &num) != 1) {
                Message err = { .type = MSG_RÉSULTAT };
                snprintf(err.contenu, TAILLE_MSG, "Veuillez entrer un nombre valide.\n");
                send(sockets_joueurs[j], &err, sizeof(err), 0);
                continue;
            }

            // Vérifie si la carte est dans la main
            for (int k = 0; k < nb_cartes_main[j]; k++) {
                if (main_joueur2[j][k].numero == num) {
                    carte_valide = 1;
                    cartes[j] = num;
                    carte_jouée_ce_tour[j] = num;
                    ordre_joueurs[j] = j;
                    break;
                }
            }

            if (!carte_valide) {
                Message err = { .type = MSG_RÉSULTAT };
                snprintf(err.contenu, TAILLE_MSG, "Cette carte n'est pas dans votre main. Réessayez.\n");
                send(sockets_joueurs[j], &err, sizeof(err), 0);
            }
        }

    // Quand on sort du while : la carte est valide → on passe au joueur suivant
}

    // Tri par ordre croissant les cartes joué par les joueur
   for (int i = 0; i < NB_JOUEURS-1; i++)
        for (int k = i+1; k < NB_JOUEURS; k++)
            if (cartes[k] < cartes[i]) {
                int tc = cartes[i], tj = ordre_joueurs[i];
                cartes[i] = cartes[k]; ordre_joueurs[i] = ordre_joueurs[k];
                cartes[k] = tc; ordre_joueurs[k] = tj;
            }

    // MAJ de carte_jouée_ce_tour après tri
    for (int i = 0; i < NB_JOUEURS; i++) {
        int joueur = ordre_joueurs[i];
        int carte  = cartes[i];

        carte_jouée_ce_tour[joueur] = carte;  // ← ici
    }

    // Placement des cartes dans l'ordre
   for (int i = 0; i < NB_JOUEURS; i++) {
        int joueur = ordre_joueurs[i];
        int carte  = cartes[i];

        // Retirer de la main
        for (int c = 0; c < nb_cartes_main[joueur]; c++) {
            if (main_joueur2[joueur][c].numero == carte) {
                // On remplace la carte à retirer par la dernière carte de la main
                main_joueur2[joueur][c] = main_joueur2[joueur][nb_cartes_main[joueur] - 1];
                nb_cartes_main[joueur]--;  // on "oublie" la dernière case
                break;
            }
        }
        traiter_carte_jouée(joueur, carte);
    }

    log_message("Tour %d terminé", tour+1);
    if(tour!=CARTES_PAR_JOUEUR-1){
        // ecrit stats dans fichier log
       for (int r = 0; r < NB_RANGÉES; r++) {

    char ligne[256];
    snprintf(ligne, sizeof(ligne), "Rangée %d (%d/5):",
             r + 1, nb_cartes_rangée[r]);

    for (int i = 0; i < nb_cartes_rangée[r]; i++) {

        int n = rangées[r][i].numero;
        int t = rangées[r][i].tetes_de_boeuf;

        char suf[16] = "";
        // Vérifier si un joueur possède cette carte (si besoin)
        for (int j = 0; j < NB_JOUEURS; j++) {
            for (int c = 0; c < CARTES_PAR_JOUEUR; c++) {
                if (main_joueur[j][c].numero == n) {
                    snprintf(suf, sizeof(suf), "(j%d)", j+1);
                    break;
                }
            }
        }

        char tmp[32];
        snprintf(tmp, sizeof(tmp), " %d(tete=%d)%s", n, t, suf);

        strncat(ligne, tmp, sizeof(ligne) - strlen(ligne) - 1);
    }

    // → Une seule ligne écrite dans le fichier
    log_message("%s", ligne);
}
// ecrit stats dans fichier log

    envoyer_état_à_tous();  // État mis à jour + demande PROCHAINE carte
    }
}

        // Fin de manche →
        char msg_manche[512] = "Fin de manche ! Scores = ";
        for (int j = 0; j < NB_JOUEURS; j++) {
            char buf[32];
            snprintf(buf, sizeof(buf), "J%d:%d  ", j+1, scores[j]);
            strcat(msg_manche, buf);
        }
        log_message("%s", msg_manche);

        Message finm = {MSG_FIN_MANCHE, ""};
        strncpy(finm.contenu, msg_manche, TAILLE_MSG-1);
        for (int j = 0; j < NB_JOUEURS; j++)
            send(sockets_joueurs[j], &finm, sizeof(finm), 0);

       // Vérifier fin de partie
        int joueur_perdant = -1;
        int score_max = -1;

        // Trouver le joueur perdant : celui qui dépasse le seuil ET a le score le plus élevé
        for (int j = 0; j < NB_JOUEURS; j++) {
            if (scores[j] >= SEUIL_PERDU) {
                if (scores[j] > score_max) {
                    score_max = scores[j];
                    joueur_perdant = j;
                }
            }
        }

        if (joueur_perdant != -1) {

            // Log dans le fichier
            log_message("FIN DE PARTIE : Joueur %d perd la partie avec %d points",
                        joueur_perdant + 1, score_max);

            // Message pour le joueur perdant
            Message m_perdu = { MSG_FIN_PARTIE, "" };
            snprintf(m_perdu.contenu, TAILLE_MSG,
                    "💀 Vous perdez la partie avec %d points !", score_max);
            send(sockets_joueurs[joueur_perdant], &m_perdu, sizeof(m_perdu), 0);

            // Message pour les autres joueurs
            for (int j = 0; j < NB_JOUEURS; j++) {
                if (j != joueur_perdant) {
                    Message m_gagne = { MSG_FIN_PARTIE, "" };
                    snprintf(m_gagne.contenu, TAILLE_MSG,
                            "🎉 Vous gagnez la partie ! Le joueur %d a perdu avec %d points.",
                            joueur_perdant + 1, score_max);
                    send(sockets_joueurs[j], &m_gagne, sizeof(m_gagne), 0);
                }
            }

            int pdf_demande[MAX_JOUEURS_POSSIBLE] = {0};
            char commentaires[MAX_JOUEURS_POSSIBLE][256] = {{0}};

            pthread_t threads[MAX_JOUEURS_POSSIBLE];
            ThreadPDFArgs args[MAX_JOUEURS_POSSIBLE];

            // Lancer tous les threads
            for (int j = 0; j < NB_JOUEURS; j++) {
                args[j].joueur_idx = j;
                args[j].sock = sockets_joueurs[j];
                args[j].pdf_demande = pdf_demande;
                args[j].commentaires = commentaires;
                pthread_create(&threads[j], NULL, thread_pdf_commentaire, &args[j]);
            }

            // Attendre que tous les threads aient terminé
            for (int j = 0; j < NB_JOUEURS; j++) {
                pthread_join(threads[j], NULL);
            }

            // Vérifier si au moins un joueur veut le PDF
            int generer_pdf = 0;
            for (int j = 0; j < NB_JOUEURS; j++) {
                if (pdf_demande[j]) { generer_pdf = 1;}
            }
            if (!generer_pdf) break;

            // Générer PDF
            //--------------------------------------------------
            //Exécuter le script stats.sh avec commentaire
            char cmd[512];
            snprintf(cmd, sizeof(cmd),
                    "./stats.sh log_jeu.txt");  // Le script génère log_jeu.tex
            system(cmd);
            // Compiler le fichier LaTeX
            snprintf(cmd, sizeof(cmd),
                    "pdflatex -interaction=nonstopmode log_jeu.tex");
            system(cmd);
            system("mv log_jeu.pdf statistiques.pdf");

            // Optionnel : supprimer les fichiers intermédiaires
            system("rm log_jeu.aux log_jeu.log log_jeu.tex");




            // Envoyer le PDF aux joueurs concernés
            for (int j = 0; j < NB_JOUEURS; j++) {
                if (pdf_demande[j]) envoyer_pdf(sockets_joueurs[j], "statistiques.pdf");
            }

            // Informer tous les joueurs
            for (int j = 0; j < NB_JOUEURS; j++) {
                Message done = { MSG_FIN, "" };
                snprintf(done.contenu, TAILLE_MSG,
                        "la fin :📄 PDF généré avec succès : statistiques.pdf");
                send(sockets_joueurs[j], &done, sizeof(done), 0);
            }


            break; // fin de la partie

        }

        // debut d'une nouvelle manche
        manche++;
        melanger_paquet();
        distribuer_cartes();
        manche_prefixe_affiche = 0;
        log_message("Manche %d", manche);
         for (int r = 0; r < NB_RANGÉES; r++) {
        log_message("  Rangée %d : %d(%d)", r+1, rangées[r][0].numero,rangées[r][0].tetes_de_boeuf);
         }
        //for (int i = 0; i < MAX_JOUEURS; i++) carte_jouée_ce_tour[i] = -1;
        memset(carte_jouée_ce_tour, -1, sizeof(carte_jouée_ce_tour));
        envoyer_état_à_tous();
    }

    return 0;
}

// ====================== FONCTIONS ======================

void initialiser_paquet() {
    for (int i = 1; i <= 104; i++) {
        paquet[i-1].numero = i;
        paquet[i-1].tetes_de_boeuf = (i%11==0)?7:(i%10==0)?5:(i%5==0)?3:(i%2==0)?2:1;
    }
}

void melanger_paquet() {
    for (int i = 0; i < 104; i++) 
        paquet_melange[i] = i + 1;   // 1..104 au lieu de 0..103

    for (int i = 103; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = paquet_melange[i];
        paquet_melange[i] = paquet_melange[j];
        paquet_melange[j] = t;
    }
}


void distribuer_cartes() {
    int idx = 0;
    memset(nb_cartes_rangée, 0, sizeof(nb_cartes_rangée));

    // Rangées initiales
    for (int r = 0; r < NB_RANGÉES; r++) {
        rangées[r][0] = paquet[paquet_melange[idx++] - 1];
        nb_cartes_rangée[r] = 1;
    }

    // Mains des joueurs
    for (int j = 0; j < NB_JOUEURS; j++) {
        nb_cartes_main[j] = CARTES_PAR_JOUEUR;
        for (int c = 0; c < CARTES_PAR_JOUEUR; c++) {
            int carte_idx = paquet_melange[idx++] - 1;
            main_joueur[j][c] = paquet[carte_idx];
            main_joueur2[j][c] = paquet[carte_idx];
        }
    }
}

void envoyer_état_à_tous() {
    char lignes[NB_RANGÉES][512] = {{0}};
    char msg[TAILLE_MSG];
    for (int r = 0; r < NB_RANGÉES; r++) {
        snprintf(lignes[r], sizeof(lignes[r]), "Rangée %d (%d/5):", r + 1, nb_cartes_rangée[r]);
        for (int i = 0; i < nb_cartes_rangée[r]; i++) {
            int n = rangées[r][i].numero;
            int t = rangées[r][i].tetes_de_boeuf;
            char suf[16] = "";
            for (int j = 0; j < NB_JOUEURS; j++) {
                for (int c = 0; c < CARTES_PAR_JOUEUR; c++) {
                    if (main_joueur[j][c].numero == n) {
                        snprintf(suf, sizeof(suf), "(j%d)", j+1);
                        break;
                    }
                }
            }
            char tmp[32];
            snprintf(tmp, sizeof(tmp), " %d(tete= %d)%s", n,t, suf);
            strncat(lignes[r], tmp, sizeof(lignes[r]) - strlen(lignes[r]) - 1);
        }
    }

    for (int j = 0; j < NB_JOUEURS; j++) {  // ← MAX_JOUEURS (avec S)
        msg[0] = '\0';
        // Rangées avec SAUVAGE nouvelle ligne !!!!
        for (int r = 0; r < NB_RANGÉES; r++) {
            strncat(msg, lignes[r], TAILLE_MSG - strlen(msg) - 1);
            strncat(msg, "\n", TAILLE_MSG - strlen(msg) - 1);  // ← ÇA !
        }

        int played = 0;
        for (int p = 0; p < NB_JOUEURS; p++) if (carte_jouée_ce_tour[p] != -1) played = 1;
        if (played) {
            strncat(msg, "Cartes jouées :", TAILLE_MSG - strlen(msg) - 1);
            for (int p = 0; p < NB_JOUEURS; p++)
                if (carte_jouée_ce_tour[p] != -1) {
                    char t[32];
                    snprintf(t, sizeof(t), " %d(j%d)", carte_jouée_ce_tour[p], p+1);
                    strncat(msg, t, TAILLE_MSG - strlen(msg) - 1);
                }
            strncat(msg, "\n\n", TAILLE_MSG - strlen(msg) - 1);
        }
       
            strncat(msg, "Votre main : ", TAILLE_MSG - strlen(msg) - 1);
            for (int c = 0; c < nb_cartes_main[j]; c++) {
                char b[16];
                snprintf(b, sizeof(b), "%d(%d) ", main_joueur2[j][c].numero,main_joueur2[j][c].tetes_de_boeuf);
                strncat(msg, b, TAILLE_MSG - strlen(msg) - 1);
            }
            
        Message m = {MSG_RÉSULTAT, ""};
        strncpy(m.contenu, msg, TAILLE_MSG-1);
        send(sockets_joueurs[j], &m, sizeof(m), 0);
    }
}
int trouver_rangée(Carte c) {
    int meilleure = -1, diff_min = 9999;
    for (int r = 0; r < NB_RANGÉES; r++) {
        if (nb_cartes_rangée[r] == 0) continue;
        int derniere = rangées[r][nb_cartes_rangée[r]-1].numero;
        if (c.numero > derniere) {
            int diff = c.numero - derniere;
            if (diff < diff_min) { diff_min = diff; meilleure = r; }
        }
    }
    return meilleure;
}
int demander_rangée_au_joueur(int joueur_idx, Carte carte) {

    // ---- ENVOI DU MESSAGE ----
    char texte[256];
    snprintf(texte, sizeof(texte),
        "Votre carte (%d) est plus petite que toutes les rangées.\n"
        "Choisissez une rangée à ramasser (1-4) : ",
        carte.numero);

    Message msg_envoi = { MSG_CHOIX, "" };
    strncpy(msg_envoi.contenu, texte, sizeof(msg_envoi.contenu)-1);

    send(sockets_joueurs[joueur_idx], &msg_envoi, sizeof(msg_envoi), 0);

    // ---- RÉCEPTION DE LA RÉPONSE ----
    Message msg_recu;
    recv(sockets_joueurs[joueur_idx], &msg_recu, sizeof(msg_recu), 0);

    int choix = -1;
    sscanf(msg_recu.contenu, "%d", &choix);

    // on convertit de 1-4 vers 0-3
    return choix - 1;
}

void traiter_carte_jouée(int joueur_idx, int numero_carte) {
    Carte carte = {0};
    for (int i = 0; i < 104; i++) if (paquet[i].numero == numero_carte) { carte = paquet[i]; break; }

    int r = trouver_rangée(carte);

    if (r != -1 && nb_cartes_rangée[r] < MAX_CARTES_PAR_RANGÉE) {
        rangées[r][nb_cartes_rangée[r]++] = carte;
    }
    else if (r != -1 && nb_cartes_rangée[r] == MAX_CARTES_PAR_RANGÉE) {
        int total = 0;
        for (int i = 0; i < MAX_CARTES_PAR_RANGÉE; i++) total += rangées[r][i].tetes_de_boeuf;
        scores[joueur_idx] += total;
        log_message("Joueur %d ramasse la rangee %d (+%d têtes)", joueur_idx+1, r+1, total);
        nb_cartes_rangée[r] = 1;
        rangées[r][0] = carte;
    }
    else {
        int choix = demander_rangée_au_joueur(joueur_idx,carte); // une fonction à écrire
    
         int total = 0;
            for (int i = 0; i < nb_cartes_rangée[choix]; i++)
                total += rangées[choix][i].tetes_de_boeuf;

            scores[joueur_idx] += total;

            log_message("Joueur %d choisit la rangee %d (+%d têtes)", 
                        joueur_idx+1, choix+1, total);

            nb_cartes_rangée[choix] = 1;
            rangées[choix][0] = carte;
    }
}

void log_message(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    if (!manche_prefixe_affiche) {
        fprintf(log_file, "\n[Manche %d] ", manche);
        manche_prefixe_affiche = 1; // on ne le fait plus pour ce bloc
    } else {
        fprintf(log_file, ""); // pas de préfixe
    }

    vfprintf(log_file, fmt, args);
    fprintf(log_file, "\n");
    fflush(log_file);
    va_end(args);
}

int carte_valide(int joueur_idx, int numero_carte) {
    for (int c = 0; c < nb_cartes_main[joueur_idx]; c++) {
        if (main_joueur[joueur_idx][c].numero == numero_carte) {
            return 1; // valide
        }
    }
    return 0; // pas valide
}

void envoyer_pdf(int sock, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("Erreur ouverture PDF");
        return;
    }

    // Calculer la taille
    fseek(f, 0, SEEK_END);
    long taille = ftell(f);
    fseek(f, 0, SEEK_SET);

    //  Envoyer un message pour dire qu'on envoie un PDF
    Message m = { MSG_PDF_FILE, "" };
    snprintf(m.contenu, TAILLE_MSG, "%ld", taille); // taille du fichier
    send(sock, &m, sizeof(m), 0);

    //  Envoyer le contenu en morceaux via le même message
    char buffer[TAILLE_MSG];
    long lu = 0;
    while (lu < taille) {
        size_t n = fread(buffer, 1, sizeof(buffer), f);
        if (n <= 0) break;

        Message m_data = { MSG_PDF_FILE, "" };
        memcpy(m_data.contenu, buffer, n); // copier les octets lus
        send(sock, &m_data, sizeof(m_data), 0);

        lu += n;
    }

    fclose(f);
}

