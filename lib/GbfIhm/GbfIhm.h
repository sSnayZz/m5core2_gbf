/**
 * @file GbfIhm.h
 * @brief Logique de l'IHM du GBF, indépendante du matériel (testable sur PC).
 * @details Regroupe la machine à états de l'IHM, le calcul des pas de réglage,
 *          la détection d'écrêtage, le formatage des libellés et l'interprétation
 *          du toucher sur l'écran. Aucune fonction de ce module ne dessine à l'écran.
 */

#pragma once

#include <stddef.h>
#include "GbfConfig.h"

/**
 * @brief Initialise ou restaure la structure des paramètres du GBF aux valeurs par défaut.
 *
 * @param parametres : Pointeur vers la structure ParametresGbf_t à initialiser
 * @return true si l'initialisation a réussi, false si le pointeur est NULL
 */
bool reinitialiserParametresParDefaut(ParametresGbf_t *parametres);

/**
 * @brief Calcule la nouvelle fréquence après incrémentation ou décrémentation par pas adaptatif.
 *        Pas de 1 Hz entre 1 et 10 Hz, 10 Hz entre 10 et 100 Hz, 100 Hz entre 100 et 1500 Hz.
 *
 * @param frequenceCouranteHz : Fréquence actuelle en Hz [1 .. 1500]
 * @param incrementer : true pour augmenter (+), false pour diminuer (-)
 * @return uint16_t : Nouvelle fréquence saturée dans [FREQUENCE_MIN_HZ .. FREQUENCE_MAX_HZ]
 */
uint16_t calculerNouvelleFrequenceParPas(uint16_t frequenceCouranteHz, bool incrementer);

/**
 * @brief Applique une commande (issue des boutons, du toucher ou du port série) sur les paramètres.
 *        Implémente la machine à états de l'IHM et vérifie toutes les bornes (Charte §6).
 *
 * @param parametres : Pointeur vers l'état courant du GBF à modifier
 * @param commande : Pointeur vers la commande à appliquer
 * @return true si la commande est valide et appliquée, false si hors limites ou pointeur NULL
 */
bool appliquerCommandeSurParametres(ParametresGbf_t *parametres, const CommandeGbf_t *commande);

/**
 * @brief Indique si le signal est écrêté par le CNA (Offset - Amplitude < 0 V ou Offset + Amplitude > 3,3 V).
 *
 * @param parametres : Pointeur vers les paramètres courants du GBF
 * @return true si une partie du signal sort de la plage [0 mV .. 3300 mV], false sinon (ou pointeur NULL)
 */
bool estSignalEcrete(const ParametresGbf_t *parametres);

/**
 * @brief Calcule les tensions minimale et maximale réellement produites sur GPIO26 (après écrêtage).
 *
 * @param parametres : Pointeur vers les paramètres courants du GBF
 * @param tensionMinMv : Pointeur de sortie pour la tension minimale en mV
 * @param tensionMaxMv : Pointeur de sortie pour la tension maximale en mV
 * @return true si le calcul a réussi, false si un pointeur est NULL
 */
bool calculerTensionsExtremes(const ParametresGbf_t *parametres, uint16_t *tensionMinMv, uint16_t *tensionMaxMv);

/**
 * @brief Retourne le nom d'un état de l'IHM.
 *
 * @param etatIhm : État de l'IHM
 * @param versionCourte : true pour le libellé court des touches ("FREQ"), false pour le bandeau ("REGLAGE FREQUENCE")
 * @return const char* : Chaîne constante décrivant l'état
 */
const char *obtenirNomEtatIhm(EtatIhm_t etatIhm, bool versionCourte);

/**
 * @brief Retourne l'état IHM suivant dans le cycle du bouton Menu (0 -> 1 -> 2 -> 3 -> 4 -> 0).
 *
 * @param etatIhm : État courant de l'IHM
 * @return EtatIhm_t : État suivant
 */
EtatIhm_t obtenirEtatIhmSuivant(EtatIhm_t etatIhm);

/**
 * @brief Formate le sous-libellé d'une touche Moins/Plus selon le paramètre réglé
 *        (ex : "+10 Hz", "-0.10 V", "CARRE", "MAX").
 *
 * @param parametres : Pointeur vers les paramètres courants du GBF
 * @param incrementer : true pour la touche Plus, false pour la touche Moins
 * @param bufferLibelle : Tampon de sortie
 * @param tailleBuffer : Taille du tampon de sortie en octets
 * @return true si le formatage a réussi, false si un pointeur est NULL ou le tampon trop court
 */
bool formaterLibelleTouche(const ParametresGbf_t *parametres, bool incrementer, char *bufferLibelle, size_t tailleBuffer);

/**
 * @brief Formate la période du signal (ex : "T=1.00 s", "T=500 ms", "T=1.00 ms").
 *
 * @param frequenceHz : Fréquence du signal en Hz (> 0)
 * @param bufferPeriode : Tampon de sortie
 * @param tailleBuffer : Taille du tampon de sortie en octets
 * @return true si le formatage a réussi, false si fréquence nulle, pointeur NULL ou tampon trop court
 */
bool formaterPeriode(uint16_t frequenceHz, char *bufferPeriode, size_t tailleBuffer);

/**
 * @brief Détermine l'état IHM à sélectionner suite à un toucher sur l'écran.
 *        - Toucher une carte non sélectionnée : sélectionne ce paramètre.
 *        - Toucher la carte déjà sélectionnée ou l'oscilloscope : retour en consultation.
 *
 * @param posX : Abscisse du toucher en pixels
 * @param posY : Ordonnée du toucher en pixels
 * @param etatCourant : État courant de l'IHM
 * @param nouvelEtat : Pointeur de sortie vers le nouvel état à appliquer
 * @return true si le toucher correspond à une zone active, false sinon (ou pointeur NULL)
 */
bool determinerEtatDepuisToucher(int32_t posX, int32_t posY, EtatIhm_t etatCourant, EtatIhm_t *nouvelEtat);
