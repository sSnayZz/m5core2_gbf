/**
 * @file GbfAffichage.cpp
 * @brief Implémentation de l'affichage LCD et de la machine à états de l'IHM du GBF.
 */

#include "GbfAffichage.h"
#include "GbfSignal.h"
#include "icons.h"
#include <stdio.h>

/**************************************************************************
 * @brief Initialise ou restaure la structure des paramètres du GBF aux valeurs par défaut.
 * 
 * @param parametres : Pointeur vers la structure ParametresGbf_t à initialiser
 * @return true si l'initialisation a réussi, false si le pointeur est NULL
 */
bool reinitialiserParametresParDefaut(ParametresGbf_t *parametres) {
    if (parametres == nullptr) {
        return false;
    }

    parametres->formeSignal = SIGNAL_SINUS;
    parametres->frequenceHz = FREQUENCE_DEFAUT_HZ;
    parametres->amplitudeMv = AMPLITUDE_DEFAUT_MV;
    parametres->offsetMv = OFFSET_DEFAUT_MV;
    parametres->etatIhm = ETAT_IHM_CONSULTATION;

    return true;
}

/**************************************************************************
 * @brief Calcule la nouvelle fréquence après incrémentation ou décrémentation par pas adaptatif.
 * 
 * @param frequenceCouranteHz : Fréquence actuelle en Hz [1 .. 1500]
 * @param incrementer : true pour augmenter (+), false pour diminuer (-)
 * @return uint16_t : Nouvelle fréquence saturée dans [1 .. 1500]
 */
uint16_t calculerNouvelleFrequenceParPas(uint16_t frequenceCouranteHz, bool incrementer) {
    int32_t frequence = (int32_t)frequenceCouranteHz;
    int32_t pasHz = (int32_t)PAS_FREQUENCE_FIN_HZ;

    if (incrementer) {
        if (frequence >= (int32_t)SEUIL_FREQ_PAS_10_HZ) {
            pasHz = (int32_t)PAS_FREQUENCE_LARGE_HZ;
        } else if (frequence >= (int32_t)SEUIL_FREQ_PAS_1_HZ) {
            pasHz = (int32_t)PAS_FREQUENCE_MOYEN_HZ;
        }
        frequence += pasHz;
    } else {
        if (frequence > (int32_t)SEUIL_FREQ_PAS_10_HZ) {
            pasHz = (int32_t)PAS_FREQUENCE_LARGE_HZ;
        } else if (frequence > (int32_t)SEUIL_FREQ_PAS_1_HZ) {
            pasHz = (int32_t)PAS_FREQUENCE_MOYEN_HZ;
        }
        frequence -= pasHz;
    }

    if (frequence < (int32_t)FREQUENCE_MIN_HZ) {
        frequence = (int32_t)FREQUENCE_MIN_HZ;
    } else if (frequence > (int32_t)FREQUENCE_MAX_HZ) {
        frequence = (int32_t)FREQUENCE_MAX_HZ;
    }

    return (uint16_t)frequence;
}

/**************************************************************************
 * @brief Fait varier le paramètre actuellement sélectionné dans l'IHM (ou la fréquence en mode consultation).
 * 
 * @param parametres : Pointeur vers les paramètres courants du GBF
 * @param incrementer : true pour action Plus (+), false pour action Moins (-)
 */
static void modifierParametreSelectionne(ParametresGbf_t *parametres, bool incrementer) {
    switch (parametres->etatIhm) {
        case ETAT_IHM_REGLAGE_FORME: {
            int32_t indiceForme = (int32_t)parametres->formeSignal;
            if (incrementer) {
                indiceForme = (indiceForme + 1) % (int32_t)NB_FORMES_SIGNAL;
            } else {
                indiceForme = (indiceForme + (int32_t)NB_FORMES_SIGNAL - 1) % (int32_t)NB_FORMES_SIGNAL;
            }
            parametres->formeSignal = (FormeSignal_t)indiceForme;
            break;
        }

        case ETAT_IHM_CONSULTATION:
        case ETAT_IHM_REGLAGE_FREQUENCE: {
            parametres->frequenceHz = calculerNouvelleFrequenceParPas(parametres->frequenceHz, incrementer);
            break;
        }

        case ETAT_IHM_REGLAGE_AMPLITUDE: {
            int32_t nouvelleAmplitude = (int32_t)parametres->amplitudeMv +
                                        (incrementer ? (int32_t)PAS_AMPLITUDE_MV : -(int32_t)PAS_AMPLITUDE_MV);
            if (nouvelleAmplitude < (int32_t)AMPLITUDE_MIN_MV) {
                nouvelleAmplitude = (int32_t)AMPLITUDE_MIN_MV;
            } else if (nouvelleAmplitude > (int32_t)AMPLITUDE_MAX_MV) {
                nouvelleAmplitude = (int32_t)AMPLITUDE_MAX_MV;
            }
            parametres->amplitudeMv = (uint16_t)nouvelleAmplitude;
            break;
        }

        case ETAT_IHM_REGLAGE_OFFSET: {
            int32_t nouvelOffset = (int32_t)parametres->offsetMv +
                                   (incrementer ? (int32_t)PAS_OFFSET_MV : -(int32_t)PAS_OFFSET_MV);
            if (nouvelOffset < (int32_t)OFFSET_MIN_MV) {
                nouvelOffset = (int32_t)OFFSET_MIN_MV;
            } else if (nouvelOffset > (int32_t)OFFSET_MAX_MV) {
                nouvelOffset = (int32_t)OFFSET_MAX_MV;
            }
            parametres->offsetMv = (uint16_t)nouvelOffset;
            break;
        }

        default:
            break;
    }
}

/**************************************************************************
 * @brief Applique une commande (issue des boutons ou du port série) sur les paramètres du GBF.
 * 
 * @param parametres : Pointeur vers l'état courant du GBF à modifier
 * @param commande : Pointeur vers la commande à appliquer
 * @return true si la commande est valide et appliquée, false sinon
 */
bool appliquerCommandeSurParametres(ParametresGbf_t *parametres, const CommandeGbf_t *commande) {
    if (parametres == nullptr || commande == nullptr) {
        return false;
    }

    switch (commande->typeCommande) {
        case CMD_IHM_BOUTON_MENU: {
            if (parametres->etatIhm == ETAT_IHM_REGLAGE_OFFSET) {
                parametres->etatIhm = ETAT_IHM_CONSULTATION;
            } else {
                parametres->etatIhm = (EtatIhm_t)((uint8_t)parametres->etatIhm + 1U);
            }
            return true;
        }

        case CMD_IHM_BOUTON_MOINS: {
            modifierParametreSelectionne(parametres, false);
            return true;
        }

        case CMD_IHM_BOUTON_PLUS: {
            modifierParametreSelectionne(parametres, true);
            return true;
        }

        case CMD_IHM_REINITIALISER: {
            return reinitialiserParametresParDefaut(parametres);
        }

        case CMD_SERIE_FIXER_FORME: {
            if (commande->valeurParametre >= NB_FORMES_SIGNAL) {
                return false;
            }
            parametres->formeSignal = (FormeSignal_t)commande->valeurParametre;
            return true;
        }

        case CMD_SERIE_FIXER_FREQUENCE: {
            if (commande->valeurParametre < FREQUENCE_MIN_HZ || commande->valeurParametre > FREQUENCE_MAX_HZ) {
                return false;
            }
            parametres->frequenceHz = commande->valeurParametre;
            return true;
        }

        case CMD_SERIE_FIXER_AMPLITUDE: {
            if (commande->valeurParametre < AMPLITUDE_MIN_MV || commande->valeurParametre > AMPLITUDE_MAX_MV) {
                return false;
            }
            parametres->amplitudeMv = commande->valeurParametre;
            return true;
        }

        case CMD_SERIE_FIXER_OFFSET: {
            if (commande->valeurParametre > OFFSET_MAX_MV) {
                return false;
            }
            parametres->offsetMv = commande->valeurParametre;
            return true;
        }

        case CMD_SERIE_DEMANDER_ETAT: {
            return true;
        }

        default:
            return false;
    }
}

/**************************************************************************
 * @brief Affiche une ligne de paramètre dans la colonne de gauche avec gestion de la surbrillance.
 *        Factorise l'affichage des 4 lignes pour respecter le principe DRY (Charte §4.2).
 * 
 * @param indiceLigne : Indice de la ligne (0 à 3)
 * @param libelleTexte : Chaîne de caractères à afficher sur la ligne
 * @param estSelectionnee : true si cette ligne est actuellement sélectionnée par le menu
 */
static void afficherLigneParametre(uint8_t indiceLigne, const char *libelleTexte, bool estSelectionnee) {
    if (libelleTexte == nullptr) {
        return;
    }

    int32_t posY = POS_Y_DEBUT_LIGNES_PARAMS + (int32_t)indiceLigne * HAUTEUR_LIGNE_PARAM_PX;
    uint16_t couleurFondLigne = estSelectionnee ? COULEUR_SELECTION : COULEUR_FOND;
    uint16_t couleurTexteLigne = estSelectionnee ? COULEUR_TEXT_SELECT : COULEUR_TEXTE;

    M5.Lcd.fillRect(0, posY, LARGEUR_COLONNE_PARAMS_PX, HAUTEUR_LIGNE_PARAM_PX - 2, couleurFondLigne);
    M5.Lcd.setTextColor(couleurTexteLigne);
    M5.Lcd.setTextDatum(ML_DATUM);
    M5.Lcd.drawString(libelleTexte, POS_X_COLONNE_PARAMS, posY + DECALAGE_Y_TEXTE_LIGNE_PX, NUMERO_POLICE_LCD);
}

/**************************************************************************
 * @brief Affiche les 3 icônes (Menu, Moins, Plus) en bas de l'écran.
 */
void afficherIconesBoutons(void) {
    int32_t posYIcones = HAUTEUR_ECRAN_PX - HAUTEUR_ICONES_PX;

    M5.Lcd.fillRect(0, posYIcones, LARGEUR_ECRAN_PX, HAUTEUR_ICONES_PX, COULEUR_FOND);
    M5.Lcd.drawXBitmap(POS_X_ICONE_MENU, posYIcones, menu_bits, LARGEUR_ICONES_PX, HAUTEUR_ICONES_PX, COULEUR_ICONS);
    M5.Lcd.drawXBitmap(POS_X_ICONE_MOINS, posYIcones, down_bits, LARGEUR_ICONES_PX, HAUTEUR_ICONES_PX, COULEUR_ICONS);
    M5.Lcd.drawXBitmap(POS_X_ICONE_PLUS, posYIcones, up_bits, LARGEUR_ICONES_PX, HAUTEUR_ICONES_PX, COULEUR_ICONS);
}

/**************************************************************************
 * @brief Dessine l'aperçu graphique de la forme d'onde (mini-oscilloscope) sur la partie droite.
 * 
 * @param parametres : Pointeur vers les paramètres courants du signal
 */
void afficherApercuOscilloscope(const ParametresGbf_t *parametres) {
    if (parametres == nullptr) {
        return;
    }

    // Effacement et tracé du cadre de l'aperçu oscilloscope (0 V en bas, 3,3 V en haut)
    M5.Lcd.fillRect(POS_X_CADRE_OSCILLO, POS_Y_CADRE_OSCILLO, LARGEUR_CADRE_OSCILLO_PX, HAUTEUR_CADRE_OSCILLO_PX, COULEUR_FOND);
    M5.Lcd.drawRect(POS_X_CADRE_OSCILLO, POS_Y_CADRE_OSCILLO, LARGEUR_CADRE_OSCILLO_PX, HAUTEUR_CADRE_OSCILLO_PX, COULEUR_CADRE_OSCILLO);

    int32_t largeurUtile = LARGEUR_CADRE_OSCILLO_PX - (2 * MARGE_INTERNE_OSCILLO_PX);
    int32_t hauteurUtile = HAUTEUR_CADRE_OSCILLO_PX - (2 * MARGE_INTERNE_OSCILLO_PX);
    int32_t origineX = POS_X_CADRE_OSCILLO + MARGE_INTERNE_OSCILLO_PX;
    int32_t basY = POS_Y_CADRE_OSCILLO + HAUTEUR_CADRE_OSCILLO_PX - MARGE_INTERNE_OSCILLO_PX;

    // Ligne horizontale représentant la tension d'offset
    int32_t offsetY = basY - ((int32_t)parametres->offsetMv * hauteurUtile) / TENSION_DAC_MAX_MV;
    M5.Lcd.drawFastHLine(origineX, offsetY, largeurUtile, COULEUR_LIGNE_OFFSET);

    // Génération d'une table d'échantillons à la largeur du graphe pour tracer la courbe réelle
    uint8_t tableGraphe[LARGEUR_CADRE_OSCILLO_PX];
    uint8_t nbPoints = (uint8_t)largeurUtile;
    if (!genererTableEchantillons(parametres, tableGraphe, nbPoints)) {
        return;
    }

    int32_t precedentX = origineX;
    int32_t precedentY = basY - ((int32_t)tableGraphe[0] * hauteurUtile) / (int32_t)VALEUR_DAC_MAX;

    for (uint8_t x = 1U; x < nbPoints; x++) {
        int32_t courantX = origineX + (int32_t)x;
        int32_t courantY = basY - ((int32_t)tableGraphe[x] * hauteurUtile) / (int32_t)VALEUR_DAC_MAX;
        M5.Lcd.drawLine(precedentX, precedentY, courantX, courantY, COULEUR_COURBE_SIGNAL);
        precedentX = courantX;
        precedentY = courantY;
    }
}

/**************************************************************************
 * @brief Affiche l'écran complet du GBF (bandeau titre, 4 lignes de paramètres et aperçu courbe).
 * 
 * @param parametres : Pointeur vers les paramètres courants du GBF
 */
void afficherEcranCompletGbf(const ParametresGbf_t *parametres) {
    if (parametres == nullptr) {
        return;
    }

    char texteLigne[TAILLE_TEXTE_LIGNE_MAX];

    float sauvegardeTailleX = M5.Lcd.getTextSizeX();
    float sauvegardeTailleY = M5.Lcd.getTextSizeY();

    // Bandeau supérieur de titre
    M5.Lcd.fillRect(0, 0, LARGEUR_ECRAN_PX, HAUTEUR_BANDEAU_TITRE_PX, COULEUR_TITRE_FOND);
    M5.Lcd.setTextSize(TAILLE_POLICE_NORMALE);
    M5.Lcd.setTextColor(COULEUR_TITRE_TEXTE);
    M5.Lcd.setTextDatum(MC_DATUM);
    M5.Lcd.drawString("GBF M5Core2 - GPIO26 (DAC)", POS_X_CENTRE_ECRAN, POS_Y_TEXTE_TITRE, NUMERO_POLICE_LCD);

    // Zone centrale des paramètres
    M5.Lcd.fillRect(0, HAUTEUR_BANDEAU_TITRE_PX, LARGEUR_COLONNE_PARAMS_PX,
                    HAUTEUR_ECRAN_PX - HAUTEUR_ICONES_PX - HAUTEUR_BANDEAU_TITRE_PX, COULEUR_FOND);

    // Ligne 0 : Forme du signal
    snprintf(texteLigne, sizeof(texteLigne), "Forme : %s", obtenirNomFormeSignal(parametres->formeSignal));
    afficherLigneParametre(0U, texteLigne, (parametres->etatIhm == ETAT_IHM_REGLAGE_FORME));

    // Ligne 1 : Fréquence en Hz
    snprintf(texteLigne, sizeof(texteLigne), "Freq  : %u Hz", parametres->frequenceHz);
    afficherLigneParametre(1U, texteLigne, (parametres->etatIhm == ETAT_IHM_REGLAGE_FREQUENCE));

    // Ligne 2 : Amplitude crête en Volts et mV
    snprintf(texteLigne, sizeof(texteLigne), "Ampl  : %.2f V (%u mV)",
             (float)parametres->amplitudeMv / FACTEUR_MV_PAR_VOLT, parametres->amplitudeMv);
    afficherLigneParametre(2U, texteLigne, (parametres->etatIhm == ETAT_IHM_REGLAGE_AMPLITUDE));

    // Ligne 3 : Offset continu en Volts et mV
    snprintf(texteLigne, sizeof(texteLigne), "Offset: %.2f V (%u mV)",
             (float)parametres->offsetMv / FACTEUR_MV_PAR_VOLT, parametres->offsetMv);
    afficherLigneParametre(3U, texteLigne, (parametres->etatIhm == ETAT_IHM_REGLAGE_OFFSET));

    // Mini-oscilloscope à droite
    afficherApercuOscilloscope(parametres);

    M5.Lcd.setTextSize(sauvegardeTailleX, sauvegardeTailleY);
}

/**************************************************************************
 * @brief Active le vibreur du M5Stack pendant une courte durée (non bloquant pour FreeRTOS).
 * 
 * @param intensite : Intensité de la vibration (0 à 255)
 * @param dureeMs : Durée de la vibration en millisecondes
 */
void emettreVibrationBouton(uint8_t intensite, uint32_t dureeMs) {
    M5.Power.setVibration(intensite);
    vTaskDelay(pdMS_TO_TICKS(dureeMs));
    M5.Power.setVibration(0U);
}
