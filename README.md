6 qui prend — Network Multiplayer Game (C)

Implementation of the card game “6 qui prend” (6 Nimmt! / Take 5) as a multiplayer client–server application using TCP sockets in C.
This project was developed as part of the Systèmes et Réseaux course (L3 Informatique) at Université Bourgogne Europe.

📌 Project Overview

The goal of this project is to apply systems and network programming concepts through a real multiplayer game:

TCP socket communication

Client–server architecture

Thread synchronization (mutex)

Process coordination

Log analysis and statistics generation

Optional PDF report generation

Players can be human clients or controlled robots, all interacting through a central server.

🧠 Game Description

6 qui prend is a strategic card game created by Wolfgang Kramer.

104 cards numbered from 1 to 104

Each card has 1 to 7 penalty points (bull heads)

Players place cards into rows

Placing the 6th card of a row forces the player to collect penalty points

The objective is to finish with the lowest score

🏗️ System Architecture

The project follows a client–server model:

Server
Handles:

Game logic

Card distribution

Turn synchronization

Score calculation

Logging

Optional PDF generation

Clients

joueur_humain.c → human player (terminal interface)

joueur_robot.c → AI player (automatic decisions)

Statistics Tool

stats.sh → analyzes game logs and generates statistics (terminal or PDF)

All communications use TCP sockets to ensure reliability.


📂 Project Structure
.
├── serveur.c          # Game server (core logic)
├── joueur_humain.c    # Human player client
├── joueur_robot.c     # AI player client
├── commun.h           # Shared constants & message protocol
├── stats.sh           # Log analysis & statistics generation
├── log_jeu.txt        # Game log (generated at runtime)
└── README.md

⚙️ Compilation

Use gcc on Linux / Unix systems:

gcc serveur.c -o serveur -lpthread
gcc joueur_humain.c -o joueur_humain
gcc joueur_robot.c -o joueur_robot


▶️ How to Run
1️⃣ Start the server
./serveur


You will be prompted to choose the number of players (2–4).

2️⃣ Start clients (in separate terminals)

Human player:

./joueur_humain


Robot player:

./joueur_robot


You can mix human and robot players freely.
