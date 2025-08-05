Convertisseur PNG vers BMP en C

Ce projet est un utilitaire en ligne de commande écrit en C pur qui convertit des images au format PNG vers le format BMP 24-bits.

    Analyse PNG manuelle : Lit et interprète la structure d'un fichier PNG sans dépendre de bibliothèques externes comme libpng.

    Gestion des Chunks : Identifie et traite les chunks critiques comme IHDR (en-tête), IDAT (données d'image) et IEND (fin de l'image).

    Décompression zlib : Concatène les données des chunks IDAT et les décompresse en utilisant la bibliothèque zlib pour obtenir les données de pixels brutes (filtrées).

    Défiltrage PNG : Applique les algorithmes de défiltrage (None, Sub, Up, Average, Paeth) pour reconstruire les pixels originaux.

    Création de fichiers BMP : Génère un fichier BMP 24-bits valide à partir des données de pixels défiltrées, en construisant les en-têtes BITMAPFILEHEADER et BITMAPINFOHEADER.

    Journalisation structurée : Intègre un module de journalisation (logger) qui enregistre chaque étape du processus, les informations lues et les erreurs potentielles dans un fichier conversion.log.

Prérequis

Pour compiler et exécuter ce projet, vous aurez besoin de :

    La bibliothèque de développement zlib.

Utilisation

L'exécutable prend deux arguments en ligne de commande : le chemin vers le fichier source PNG et le chemin vers le fichier de destination BMP.
Syntaxe


./converter mon_image.png image_convertie.bmp

Après l'exécution, deux fichiers seront créés (ou mis à jour) :

    image_convertie.bmp : L'image finale au format BMP.

    conversion.log : Un fichier de log détaillant toutes les étapes de la conversion, y compris les informations de l'en-tête PNG, le statut de la décompression, et les éventuelles erreurs.

Structure du Projet

Le code est organisé en plusieurs modules pour une meilleure séparation des préoccupations.

    main.c : Point d'entrée du programme. Gère les arguments de la ligne de commande et orchestre le flux de travail (lecture, traitement, écriture).

    logger.c / logger.h : Module de journalisation. Gère la création et l'écriture dans le fichier de log. Il est conçu pour être réutilisable et ne dépend pas d'un état global.

    png.c / png.h : Cœur de la logique PNG. Responsable de la lecture du fichier, de l'analyse des chunks, de la décompression et du défiltrage des données d'image.

    png_to_bmp.c / png_to_bmp.h : Module de conversion BMP. Construit les en-têtes et écrit les données de pixels dans un fichier au format BMP.
