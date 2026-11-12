/**
 * @file GbfAffichage.h
 * @brief Module de rendu graphique de l'IHM du GBF sur l'écran LCD 320x240 du M5Stack Core 2.
 * @details L'écran complet est dessiné dans un sprite M5Canvas (double tampon, cf. TP Chronomètre)
 *          puis transféré en une seule opération : aucun scintillement lors des rafraîchissements.
 *          Organisation de l'écran :
 *          - Bandeau : mode courant et indicateur d'écrêtage de la sortie GPIO26.
 *          - 4 cartes de paramètres (Forme, Fréquence, Amplitude, Offset) avec jauge de position.
 *          - Oscilloscope : 2 périodes du signal réel, grille 0-3 V, offset et zone d'écrêtage.
 *          - 3 touches logicielles contextuelles en face des boutons BtnA, BtnB et BtnC.
 */

#pragma once

#include <M5Unified.h>
#include "GbfConfig.h"
#include "GbfMiseEnPage.h"

// ============================================================================
// PALETTE DE COULEURS (format RGB565)
// ============================================================================
#define COULEUR_FOND                0x0000U     /**< Noir */
#define COULEUR_BANDEAU             0x10A6U     /**< Bleu nuit */
#define COULEUR_PANNEAU             0x18E3U     /**< Gris anthracite (cartes, oscilloscope) */
#define COULEUR_BORDURE             0x4208U     /**< Gris moyen */
#define COULEUR_TEXTE_PRINCIPAL     0xFFFFU     /**< Blanc */
#define COULEUR_TEXTE_SECONDAIRE    0x9CF3U     /**< Gris clair */
#define COULEUR_SELECTION           0xFFE0U     /**< Jaune (carte sélectionnée, convention du TP) */
#define COULEUR_TEXTE_SELECTION     0x0000U     /**< Noir sur fond jaune */
#define COULEUR_PISTE_SELECTION     0xC600U     /**< Jaune foncé (piste de jauge sur carte sélectionnée) */
#define COULEUR_ACCENT              0x07FFU     /**< Cyan (courbe, jauges) */
#define COULEUR_GRILLE              0x2945U     /**< Gris foncé (grille, pistes de jauges) */
#define COULEUR_OFFSET              0x07E0U     /**< Vert (ligne d'offset) */
#define COULEUR_ALERTE              0xF800U     /**< Rouge (écrêtage) */
#define COULEUR_SORTIE_OK           0x0600U     /**< Vert foncé (badge sortie correcte) */
#define COULEUR_TOUCHE              0x2104U     /**< Gris très foncé (touches logicielles) */

// ============================================================================
// POLICES DE CARACTERES (polices intégrées à M5GFX)
// ============================================================================
#define POLICE_PETITE               (&fonts::Font0)
#define POLICE_NORMALE              (&fonts::Font2)
#define POLICE_VALEUR               (&fonts::FreeSansBold9pt7b)
#define POLICE_TOUCHE               (&fonts::FreeSansBold12pt7b)

// ============================================================================
// DETAILS DE MISE EN FORME
// ============================================================================
#define PROFONDEUR_COULEUR_SPRITE   16
#define PROFONDEUR_COULEUR_SECOURS  8
#define RAYON_ARRONDI_PX            6
#define MARGE_INTERNE_CARTE_PX      6
#define POS_Y_LIBELLE_CARTE         4
#define POS_Y_VALEUR_CARTE          22
#define HAUTEUR_JAUGE_PX            3
#define DECALAGE_JAUGE_BAS_PX       6
#define ESPACE_SEGMENTS_JAUGE_PX    2
#define LARGEUR_MINIATURE_FORME_PX  40
#define HAUTEUR_MINIATURE_FORME_PX  16
#define LARGEUR_BADGE_PX            72
#define HAUTEUR_BADGE_PX            16
#define MARGE_GAUCHE_OSCILLO_PX     16
#define MARGE_DROITE_OSCILLO_PX     4
#define MARGE_HAUT_OSCILLO_PX       6
#define HAUTEUR_LEGENDE_OSCILLO_PX  18
#define NB_PERIODES_OSCILLO         2U
#define NB_DIVISIONS_TEMPS_OSCILLO  4
#define PAS_GRADUATION_MV           1000
#define PAS_POINTILLES_PX           3
#define PAS_TIRETS_PX               6
#define EPAISSEUR_COURBE_PX         1.0f
#define POS_Y_TEXTE_TOUCHE          14
#define POS_Y_SOUS_TEXTE_TOUCHE     32

/**
 * @brief Alloue le sprite plein écran (en PSRAM, ou en RAM interne 8 bits en secours).
 *
 * @param canvas : Pointeur vers le sprite M5Canvas à initialiser (associé à M5.Display)
 * @return true si le sprite a été alloué, false en cas de mémoire insuffisante ou pointeur NULL
 */
bool initialiserAffichage(M5Canvas *canvas);

/**
 * @brief Dessine l'écran complet du GBF dans le sprite puis le transfère sur l'écran LCD.
 *
 * @param canvas : Pointeur vers le sprite initialisé par initialiserAffichage()
 * @param parametres : Pointeur vers les paramètres courants du GBF et l'état de sélection IHM
 */
void afficherEcranCompletGbf(M5Canvas *canvas, const ParametresGbf_t *parametres);

/**
 * @brief Active le vibreur du M5Stack pendant une courte durée (non bloquant pour FreeRTOS).
 *
 * @param intensite : Intensité de la vibration (0 à 255)
 * @param dureeMs : Durée de la vibration en millisecondes
 */
void emettreVibrationBouton(uint8_t intensite, uint32_t dureeMs);
