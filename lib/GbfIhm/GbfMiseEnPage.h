/**
 * @file GbfMiseEnPage.h
 * @brief Géométrie de l'écran LCD 320x240 du GBF (zones, cartes, oscilloscope, touches).
 * @details Ce fichier ne dépend d'aucune bibliothèque matérielle : il est partagé entre
 *          le module de rendu (GbfAffichage) et la logique IHM (GbfIhm, détection du
 *          toucher), ce qui garantit que la zone tactile correspond exactement au dessin.
 */

#pragma once

#include "GbfConfig.h"

// ============================================================================
// BANDEAU SUPERIEUR
// ============================================================================
#define HAUTEUR_BANDEAU_PX              22
#define MARGE_ECRAN_PX                  4

// ============================================================================
// CARTES DES PARAMETRES (colonne de gauche)
// ============================================================================
#define POS_X_CARTES                    MARGE_ECRAN_PX
#define POS_Y_PREMIERE_CARTE            26
#define LARGEUR_CARTE_PX                194
#define HAUTEUR_CARTE_PX                40
#define ESPACE_ENTRE_CARTES_PX          2
#define PAS_VERTICAL_CARTES_PX          (HAUTEUR_CARTE_PX + ESPACE_ENTRE_CARTES_PX)
#define POS_Y_FIN_CARTES                (POS_Y_PREMIERE_CARTE + (int32_t)NB_PARAMETRES_REGLABLES * PAS_VERTICAL_CARTES_PX)

// ============================================================================
// OSCILLOSCOPE (colonne de droite)
// ============================================================================
#define POS_X_OSCILLO                   202
#define POS_Y_OSCILLO                   POS_Y_PREMIERE_CARTE
#define LARGEUR_OSCILLO_PX              (LARGEUR_ECRAN_PX - POS_X_OSCILLO - MARGE_ECRAN_PX)
#define HAUTEUR_OSCILLO_PX              (POS_Y_FIN_CARTES - POS_Y_OSCILLO - ESPACE_ENTRE_CARTES_PX)

// ============================================================================
// TOUCHES LOGICIELLES (en face des boutons BtnA, BtnB, BtnC)
// ============================================================================
#define POS_Y_TOUCHES                   196
#define HAUTEUR_TOUCHE_PX               40
#define LARGEUR_TOUCHE_PX               98
#define POS_X_CENTRE_TOUCHE_A           53
#define POS_X_CENTRE_TOUCHE_B           160
#define POS_X_CENTRE_TOUCHE_C           267
