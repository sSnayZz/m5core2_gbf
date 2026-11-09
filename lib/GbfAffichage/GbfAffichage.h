/**
 * @file GbfAffichage.h
 * @brief Module de gestion de l'affichage LCD et de la machine à états de l'IHM du GBF.
 * @details Reprend les conventions du TP INFO5 (§5.3 et Chronomètre) :
 *          - Constantes de couleurs COULEUR_FOND, COULEUR_TEXTE, COULEUR_SELECTION, etc.
 *          - Affichage des 3 icônes (Menu, Moins, Plus) en bas de l'écran.
 *          - Affichage des caractéristiques du signal et d'un aperçu graphique.
 */

#pragma once

#include <M5Unified.h>
#include "GbfConfig.h"

// ============================================================================
// CONSTANTES DE COULEURS DE L'IHM (conformément au §5.3 du TP)
// ============================================================================
#define COULEUR_FOND            TFT_BLACK
#define COULEUR_TEXTE           TFT_YELLOW
#define COULEUR_TEXT_SELECT     TFT_BLACK
#define COULEUR_SELECTION       TFT_YELLOW
#define COULEUR_ICONS           TFT_WHITE
#define COULEUR_TITRE_FOND      TFT_NAVY
#define COULEUR_TITRE_TEXTE     TFT_WHITE
#define COULEUR_CADRE_OSCILLO   TFT_DARKGREY
#define COULEUR_LIGNE_OFFSET    TFT_DARKGREEN
#define COULEUR_COURBE_SIGNAL   TFT_CYAN

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
 * @brief Applique une commande (issue des boutons ou du port série) sur les paramètres du GBF.
 *        Implémente la machine à états de l'IHM et vérifie toutes les bornes (Charte §6).
 * 
 * @param parametres : Pointeur vers l'état courant du GBF à modifier
 * @param commande : Pointeur vers la commande à appliquer
 * @return true si la commande est valide et appliquée, false si hors limites ou pointeur NULL
 */
bool appliquerCommandeSurParametres(ParametresGbf_t *parametres, const CommandeGbf_t *commande);

/**
 * @brief Affiche les 3 icônes (Menu, Moins, Plus) en bas de l'écran en face des boutons tactiles.
 */
void afficherIconesBoutons(void);

/**
 * @brief Dessine l'aperçu graphique de la forme d'onde (mini-oscilloscope) sur la partie droite.
 * 
 * @param parametres : Pointeur vers les paramètres courants du signal
 */
void afficherApercuOscilloscope(const ParametresGbf_t *parametres);

/**
 * @brief Affiche l'écran complet du GBF (bandeau titre, 4 lignes de paramètres et aperçu courbe).
 * 
 * @param parametres : Pointeur vers les paramètres courants du GBF et l'état de sélection IHM
 */
void afficherEcranCompletGbf(const ParametresGbf_t *parametres);

/**
 * @brief Active le vibreur du M5Stack pendant une courte durée (non bloquant pour FreeRTOS).
 * 
 * @param intensite : Intensité de la vibration (0 à 255)
 * @param dureeMs : Durée de la vibration en millisecondes
 */
void emettreVibrationBouton(uint8_t intensite, uint32_t dureeMs);
