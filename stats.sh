#!/bin/bash

LOG="$1"
COMMENTAIRE="$2"
# Extraire le nom de base sans extension
BASE="${LOG%.txt}"

# Nom du fichier LaTeX
OUTPUT="${BASE}.tex"

if [ ! -f "$LOG" ]; then
    echo "Fichier $LOG introuvable."
    exit 1
fi

# Début du document LaTeX
cat > "$OUTPUT" <<EOF
\documentclass[a4paper,12pt]{article}
\usepackage[utf8]{inputenc}
\usepackage{geometry}
\usepackage{longtable}
\geometry{margin=2cm}
\title{Rapport du Match}
\date{}
\begin{document}
\maketitle
\section*{Statistiques du match}
EOF

# Nombre de manches
manches=$(grep -c "^\[Manche" "$LOG")
echo "\\textbf{Nombre de manches jouées :} $manches\\\\\n" >> "$OUTPUT"

# Nombre total de tours
tours=$(grep -c "^Tour" "$LOG")
echo "\\textbf{Nombre total de tours :} $tours\\\\\n" >> "$OUTPUT"

# Têtes ramassées par joueur
echo "\\subsection*{Têtes ramassées}" >> "$OUTPUT"
echo "\\begin{itemize}" >> "$OUTPUT"
awk '
/Joueur [0-9]+ (choisit|ramasse) la rangee/ {
    joueur = $2
    for(i=1;i<=NF;i++){
        if($i ~ /^\(\+[0-9]+/){
            gsub(/\(\+/, "", $i)
            gsub(/\)/, "", $i)
            ramasse[joueur] += $i
        }
    }
}
END {
    for(j=1;j<=10;j++){
        if(ramasse[j] != "") {
            print "  \\item Joueur " j " : " ramasse[j] " têtes"
        }
    }
}
' "$LOG" >> "$OUTPUT"
echo "\\end{itemize}\n" >> "$OUTPUT"

# Scores par manche
echo "\\subsection*{Scores par manche}" >> "$OUTPUT"
echo "\\begin{itemize}" >> "$OUTPUT"
grep "Fin de manche" "$LOG" | sed 's/Fin de manche ! //' | while read line; do
    echo "  \\item $line" >> "$OUTPUT"
done
echo "\\end{itemize}\n" >> "$OUTPUT"

# Score final
echo "\\subsection*{Score final}" >> "$OUTPUT"
final_score=$(grep "Fin de manche" "$LOG" | tail -n 1 | sed 's/Fin de manche ! //')
echo "$final_score\\\\\n" >> "$OUTPUT"

# Commentaires des joueurs
echo "\\subsection*{Commentaires des joueurs}" >> "$OUTPUT"
player_comments=$(grep "^ commentaire Joueur" "$LOG")
if [ -z "$player_comments" ]; then
    echo "Aucun commentaire.\\\\\n" >> "$OUTPUT"
else
    echo "\\begin{itemize}" >> "$OUTPUT"
    echo "$player_comments" | while read line; do
        echo "  \\item $line" >> "$OUTPUT"
    done
    echo "\\end{itemize}\n" >> "$OUTPUT"
fi

# Commentaire optionnel
if [ ! -z "$COMMENTAIRE" ]; then
    echo "\\subsection*{Commentaire ajouté}" >> "$OUTPUT"
    echo "$COMMENTAIRE\\\\\n" >> "$OUTPUT"
fi

# Fin du document
echo "\\end{document}" >> "$OUTPUT"

echo "Fichier LaTeX généré : $OUTPUT"
