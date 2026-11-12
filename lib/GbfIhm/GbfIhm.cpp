/**
 * @file GbfIhm.cpp
 * @brief Implémentation de la logique de l'IHM du GBF (sans aucun accès matériel).
 */

#include "GbfIhm.h"
#include "GbfMiseEnPage.h"
#include "GbfSignal.h"
#include <stdio.h>

#define SEUIL_PERIODE_SECONDE_MS        1000.0f
#define SEUIL_PERIODE_SANS_DECIMALE_MS  100.0f
#define SEUIL_PERIODE_UNE_DECIMALE_MS   10.0f
#define MILLISECONDES_PAR_SECONDE       1000.0f

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
 * @brief Ajoute ou retire un pas à une valeur en mV et la sature dans [valeurMin .. valeurMax].
 *        Factorise le traitement de l'amplitude et de l'offset (Charte §4.2, principe DRY).
 *
 * @param valeurMv : Valeur courante en mV
 * @param pasMv : Pas de réglage en mV
 * @param valeurMin : Borne basse autorisée en mV
 * @param valeurMax : Borne haute autorisée en mV
 * @param incrementer : true pour ajouter le pas, false pour le retirer
 * @return uint16_t : Nouvelle valeur saturée
 */
static uint16_t appliquerPasSature(uint16_t valeurMv, uint16_t pasMv, uint16_t valeurMin, uint16_t valeurMax, bool incrementer) {
    int32_t nouvelleValeur = (int32_t)valeurMv + (incrementer ? (int32_t)pasMv : -(int32_t)pasMv);

    if (nouvelleValeur < (int32_t)valeurMin) {
        nouvelleValeur = (int32_t)valeurMin;
    } else if (nouvelleValeur > (int32_t)valeurMax) {
        nouvelleValeur = (int32_t)valeurMax;
    }

    return (uint16_t)nouvelleValeur;
}

/**************************************************************************
 * @brief Fait varier le paramètre sélectionné dans l'IHM (ou la fréquence en mode consultation).
 *
 * @param parametres : Pointeur vers les paramètres courants du GBF (non NULL)
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
            parametres->amplitudeMv = appliquerPasSature(parametres->amplitudeMv, PAS_AMPLITUDE_MV,
                                                         AMPLITUDE_MIN_MV, AMPLITUDE_MAX_MV, incrementer);
            break;
        }

        case ETAT_IHM_REGLAGE_OFFSET: {
            parametres->offsetMv = appliquerPasSature(parametres->offsetMv, PAS_OFFSET_MV,
                                                      OFFSET_MIN_MV, OFFSET_MAX_MV, incrementer);
            break;
        }

        default:
            break;
    }
}

/**************************************************************************
 * @brief Applique une commande (issue des boutons, du toucher ou du port série) sur les paramètres.
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
            parametres->etatIhm = obtenirEtatIhmSuivant(parametres->etatIhm);
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

        case CMD_IHM_SELECTIONNER: {
            if (commande->valeurParametre >= NB_ETATS_IHM) {
                return false;
            }
            parametres->etatIhm = (EtatIhm_t)commande->valeurParametre;
            return true;
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
 * @brief Indique si le signal est écrêté par le CNA.
 *
 * @param parametres : Pointeur vers les paramètres courants du GBF
 * @return true si une partie du signal sort de [0 mV .. 3300 mV], false sinon
 */
bool estSignalEcrete(const ParametresGbf_t *parametres) {
    if (parametres == nullptr) {
        return false;
    }

    int32_t tensionBasseMv = (int32_t)parametres->offsetMv - (int32_t)parametres->amplitudeMv;
    int32_t tensionHauteMv = (int32_t)parametres->offsetMv + (int32_t)parametres->amplitudeMv;

    return (tensionBasseMv < TENSION_DAC_MIN_MV) || (tensionHauteMv > TENSION_DAC_MAX_MV);
}

/**************************************************************************
 * @brief Calcule les tensions minimale et maximale réellement produites (après écrêtage).
 *
 * @param parametres : Pointeur vers les paramètres courants du GBF
 * @param tensionMinMv : Pointeur de sortie pour la tension minimale en mV
 * @param tensionMaxMv : Pointeur de sortie pour la tension maximale en mV
 * @return true si le calcul a réussi, false si un pointeur est NULL
 */
bool calculerTensionsExtremes(const ParametresGbf_t *parametres, uint16_t *tensionMinMv, uint16_t *tensionMaxMv) {
    if (parametres == nullptr || tensionMinMv == nullptr || tensionMaxMv == nullptr) {
        return false;
    }

    int32_t tensionBasseMv = (int32_t)parametres->offsetMv - (int32_t)parametres->amplitudeMv;
    int32_t tensionHauteMv = (int32_t)parametres->offsetMv + (int32_t)parametres->amplitudeMv;

    if (tensionBasseMv < TENSION_DAC_MIN_MV) {
        tensionBasseMv = TENSION_DAC_MIN_MV;
    }
    if (tensionHauteMv > TENSION_DAC_MAX_MV) {
        tensionHauteMv = TENSION_DAC_MAX_MV;
    }

    *tensionMinMv = (uint16_t)tensionBasseMv;
    *tensionMaxMv = (uint16_t)tensionHauteMv;
    return true;
}

/**************************************************************************
 * @brief Retourne le nom d'un état de l'IHM.
 *
 * @param etatIhm : État de l'IHM
 * @param versionCourte : true pour le libellé court, false pour le libellé long
 * @return const char* : Chaîne constante décrivant l'état
 */
const char *obtenirNomEtatIhm(EtatIhm_t etatIhm, bool versionCourte) {
    switch (etatIhm) {
        case ETAT_IHM_CONSULTATION:
            return versionCourte ? "VUE" : "CONSULTATION";
        case ETAT_IHM_REGLAGE_FORME:
            return versionCourte ? "FORME" : "REGLAGE FORME";
        case ETAT_IHM_REGLAGE_FREQUENCE:
            return versionCourte ? "FREQ" : "REGLAGE FREQUENCE";
        case ETAT_IHM_REGLAGE_AMPLITUDE:
            return versionCourte ? "AMPL" : "REGLAGE AMPLITUDE";
        case ETAT_IHM_REGLAGE_OFFSET:
            return versionCourte ? "OFFSET" : "REGLAGE OFFSET";
        default:
            return "?";
    }
}

/**************************************************************************
 * @brief Retourne l'état IHM suivant dans le cycle du bouton Menu.
 *
 * @param etatIhm : État courant de l'IHM
 * @return EtatIhm_t : État suivant
 */
EtatIhm_t obtenirEtatIhmSuivant(EtatIhm_t etatIhm) {
    uint8_t indiceSuivant = (uint8_t)(((uint8_t)etatIhm + 1U) % NB_ETATS_IHM);
    return (EtatIhm_t)indiceSuivant;
}

/**************************************************************************
 * @brief Formate le sous-libellé d'une touche Moins/Plus selon le paramètre réglé.
 *        La variation est obtenue en simulant l'action sur une copie des paramètres,
 *        ce qui garantit que le libellé affiché correspond exactement au comportement réel.
 *
 * @param parametres : Pointeur vers les paramètres courants du GBF
 * @param incrementer : true pour la touche Plus, false pour la touche Moins
 * @param bufferLibelle : Tampon de sortie
 * @param tailleBuffer : Taille du tampon de sortie en octets
 * @return true si le formatage a réussi, false sinon
 */
bool formaterLibelleTouche(const ParametresGbf_t *parametres, bool incrementer, char *bufferLibelle, size_t tailleBuffer) {
    if (parametres == nullptr || bufferLibelle == nullptr || tailleBuffer == 0U) {
        return false;
    }

    ParametresGbf_t simulation = *parametres;
    modifierParametreSelectionne(&simulation, incrementer);

    const char *butee = incrementer ? "MAX" : "MIN";
    char signe = incrementer ? '+' : '-';
    int nbCaracteres = 0;

    switch (parametres->etatIhm) {
        case ETAT_IHM_REGLAGE_FORME: {
            nbCaracteres = snprintf(bufferLibelle, tailleBuffer, "%s", obtenirNomFormeSignal(simulation.formeSignal));
            break;
        }

        case ETAT_IHM_CONSULTATION:
        case ETAT_IHM_REGLAGE_FREQUENCE: {
            int32_t ecartHz = (int32_t)simulation.frequenceHz - (int32_t)parametres->frequenceHz;
            if (ecartHz == 0) {
                nbCaracteres = snprintf(bufferLibelle, tailleBuffer, "%s", butee);
            } else {
                nbCaracteres = snprintf(bufferLibelle, tailleBuffer, "%c%ld Hz", signe, (long)(ecartHz < 0 ? -ecartHz : ecartHz));
            }
            break;
        }

        case ETAT_IHM_REGLAGE_AMPLITUDE:
        case ETAT_IHM_REGLAGE_OFFSET: {
            bool reglageAmplitude = (parametres->etatIhm == ETAT_IHM_REGLAGE_AMPLITUDE);
            int32_t avantMv = reglageAmplitude ? (int32_t)parametres->amplitudeMv : (int32_t)parametres->offsetMv;
            int32_t apresMv = reglageAmplitude ? (int32_t)simulation.amplitudeMv : (int32_t)simulation.offsetMv;
            int32_t ecartMv = (apresMv > avantMv) ? (apresMv - avantMv) : (avantMv - apresMv);
            if (ecartMv == 0) {
                nbCaracteres = snprintf(bufferLibelle, tailleBuffer, "%s", butee);
            } else {
                nbCaracteres = snprintf(bufferLibelle, tailleBuffer, "%c%.2f V", signe, (float)ecartMv / FACTEUR_MV_PAR_VOLT);
            }
            break;
        }

        default: {
            nbCaracteres = snprintf(bufferLibelle, tailleBuffer, "%c", signe);
            break;
        }
    }

    return (nbCaracteres > 0 && (size_t)nbCaracteres < tailleBuffer);
}

/**************************************************************************
 * @brief Formate la période du signal avec une unité et une précision adaptées.
 *
 * @param frequenceHz : Fréquence du signal en Hz (> 0)
 * @param bufferPeriode : Tampon de sortie
 * @param tailleBuffer : Taille du tampon de sortie en octets
 * @return true si le formatage a réussi, false sinon
 */
bool formaterPeriode(uint16_t frequenceHz, char *bufferPeriode, size_t tailleBuffer) {
    if (frequenceHz == 0U || bufferPeriode == nullptr || tailleBuffer == 0U) {
        return false;
    }

    float periodeMs = MILLISECONDES_PAR_SECONDE / (float)frequenceHz;
    int nbCaracteres = 0;

    if (periodeMs >= SEUIL_PERIODE_SECONDE_MS) {
        nbCaracteres = snprintf(bufferPeriode, tailleBuffer, "T=%.2f s", periodeMs / MILLISECONDES_PAR_SECONDE);
    } else if (periodeMs >= SEUIL_PERIODE_SANS_DECIMALE_MS) {
        nbCaracteres = snprintf(bufferPeriode, tailleBuffer, "T=%.0f ms", periodeMs);
    } else if (periodeMs >= SEUIL_PERIODE_UNE_DECIMALE_MS) {
        nbCaracteres = snprintf(bufferPeriode, tailleBuffer, "T=%.1f ms", periodeMs);
    } else {
        nbCaracteres = snprintf(bufferPeriode, tailleBuffer, "T=%.2f ms", periodeMs);
    }

    return (nbCaracteres > 0 && (size_t)nbCaracteres < tailleBuffer);
}

/**************************************************************************
 * @brief Détermine l'état IHM à sélectionner suite à un toucher sur l'écran.
 *
 * @param posX : Abscisse du toucher en pixels
 * @param posY : Ordonnée du toucher en pixels
 * @param etatCourant : État courant de l'IHM
 * @param nouvelEtat : Pointeur de sortie vers le nouvel état à appliquer
 * @return true si le toucher correspond à une zone active, false sinon
 */
bool determinerEtatDepuisToucher(int32_t posX, int32_t posY, EtatIhm_t etatCourant, EtatIhm_t *nouvelEtat) {
    if (nouvelEtat == nullptr) {
        return false;
    }

    bool dansColonneCartes = (posX >= POS_X_CARTES) && (posX < POS_X_CARTES + LARGEUR_CARTE_PX);
    bool dansHauteurCartes = (posY >= POS_Y_PREMIERE_CARTE) && (posY < POS_Y_FIN_CARTES);

    if (dansColonneCartes && dansHauteurCartes) {
        int32_t positionRelative = posY - POS_Y_PREMIERE_CARTE;

        // Toucher dans l'espace entre deux cartes : ignoré
        if ((positionRelative % PAS_VERTICAL_CARTES_PX) >= HAUTEUR_CARTE_PX) {
            return false;
        }

        EtatIhm_t etatCarte = (EtatIhm_t)(1 + positionRelative / PAS_VERTICAL_CARTES_PX);
        *nouvelEtat = (etatCarte == etatCourant) ? ETAT_IHM_CONSULTATION : etatCarte;
        return true;
    }

    bool dansOscilloscope = (posX >= POS_X_OSCILLO) && (posX < POS_X_OSCILLO + LARGEUR_OSCILLO_PX) &&
                            (posY >= POS_Y_OSCILLO) && (posY < POS_Y_OSCILLO + HAUTEUR_OSCILLO_PX);
    if (dansOscilloscope) {
        *nouvelEtat = ETAT_IHM_CONSULTATION;
        return true;
    }

    return false;
}
