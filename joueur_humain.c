// joueur_humain.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "commun.h"

//gcc -o joueur_humain joueur_humain.c

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT)
    };
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connexion");
        return 1;
    }

    Message msg;

    while (1) {
        if (recv(sock, &msg, sizeof(msg), 0) <= 0) break;

        switch (msg.type) {

            case MSG_BIENVENUE:
                printf("%s\n", msg.contenu);
                break;

            case MSG_RÉSULTAT: {
                printf("%s\n", msg.contenu);

                int carte_valide = 0;
                int carte;

                while (!carte_valide) {
                    printf("Votre carte ? ");
                    if (scanf("%d", &carte) != 1) {
                        printf("❌ Entrée invalide.\n");
                        int c; while ((c = getchar()) != '\n' && c != EOF);
                    } else {
                        carte_valide = 1;
                    }
                }

                Message joue = { MSG_JOUE, "" };
                snprintf(joue.contenu, TAILLE_MSG, "%d", carte);
                send(sock, &joue, sizeof(joue), 0);
                break;
            }

            case MSG_CHOIX: {
                int choix;
                int ok = 0;

                printf("%s\n", msg.contenu);

                while (!ok) {
                    printf("Votre choix (1-4) : ");

                    if (scanf("%d", &choix) != 1) {
                        printf("❌ Vous devez entrer un nombre.\n");
                        int c; while ((c = getchar()) != '\n' && c != EOF);
                        continue;
                    }

                    if (choix < 1 || choix > 4) {
                        printf("❌ Le nombre doit être entre 1 et 4.\n");
                        continue;
                    }

                    ok = 1;
                }

                Message joue = { MSG_JOUE, "" };
                snprintf(joue.contenu, TAILLE_MSG, "%d", choix);
                send(sock, &joue, sizeof(joue), 0);
                break;
            }
            case MSG_PDF: {
                printf("%s\n", msg.contenu);

                char reponse[32];

                while (1) {
                    printf("> ");
                    if (scanf("%31s", reponse) != 1) {
                        int c; while ((c = getchar()) != '\n' && c != EOF);
                        continue;
                    }

                    // Validation oui/non
                    if (strcasecmp(reponse, "oui") == 0 || strcasecmp(reponse, "non") == 0) { 
                        break;
                    }

                    printf("❌ Réponse invalide. Veuillez taper \"oui\" ou \"non\".\n");
                }

                Message rep = { MSG_CMN, "" };
                snprintf(rep.contenu, TAILLE_MSG, "%s", reponse);

                send(sock, &rep, sizeof(rep), 0);
                break;
            }

            case MSG_CMN: {
                printf("%s\n", msg.contenu);

                char texte[256];

                // 🟦 Vérifier si la question est de type oui/non
                int demande_oui_non = 
                    (strstr(msg.contenu, "(oui/non)") != NULL);

                if (demande_oui_non) {
                    // ------- Réponse OBLIGATOIREMENT oui/non -------
                    while (1) {
                        printf("> ");
                        if (scanf("%255s", texte) != 1) {
                            int c; while ((c = getchar()) != '\n' && c != EOF);
                            continue;
                        }

                        if (strcasecmp(texte, "oui") == 0 || strcasecmp(texte, "non") == 0) {
                            break;  // OK
                        }

                        printf("❌ Réponse invalide. Tape \"oui\" ou \"non\".\n");
                    }

                } else {
                    // ------- Ici c'est le commentaire texte -------
                    getchar(); // consomme le \n restant
                    fgets(texte, sizeof(texte), stdin);
                    texte[strcspn(texte, "\n")] = 0; // retirer \n
                }

                Message rep = { MSG_CMN, "" };
                snprintf(rep.contenu, TAILLE_MSG, "%s", texte);

                send(sock, &rep, sizeof(rep), 0);
                break;
            }


            case MSG_FIN:
                printf("%s\n", msg.contenu);
                goto fin;
                break;
            case MSG_PDF_FILE:{
                long taille = atol(msg.contenu); // lire la taille du fichier
                FILE *f = fopen("mon_pdf.pdf", "wb");
                if (!f) { perror("fopen"); break; }

                long recu = 0;
                while (recu < taille) {
                    Message m_pdf;
                    recv(sock, &m_pdf, sizeof(m_pdf), 0);
                    long n = ((taille - recu) > TAILLE_MSG) ? TAILLE_MSG : (taille - recu);
                    fwrite(m_pdf.contenu, 1, n, f);
                    recu += n;
                }

                fclose(f);
                printf("📄 PDF reçu et sauvegardé sous mon_pdf.pdf\n");
                break;
            }
            case MSG_FIN_MANCHE:    
            case MSG_FIN_PARTIE:
                printf("%s\n", msg.contenu);
                break;
        }
    }

fin:   // ✔ le label est ici, AVANT la fin de main()
    close(sock);
    return 0;
}

