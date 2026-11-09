/**
 * @file GbfSignal.cpp
 * @brief Implémentation de la génération des signaux GBF par CNA (GPIO26) et interruption Timer.
 */

#include "GbfSignal.h"
#include <driver/dac.h>
#include <math.h>

// ============================================================================
// CONTEXTE INTERNE AU MODULE POUR L'ISR DU TIMER MATERIEL
// Note Charte §1.1 : La signature matérielle d'interruption Arduino-ESP32
// `void IRAM_ATTR onTimer(void)` n'acceptant aucun paramètre, ce double-buffer
// statique encapsulé dans ce seul fichier est l'unique moyen de transmettre les
// échantillons à la routine d'interruption en mémoire IRAM.
// ============================================================================
#define NB_BUFFERS_ISR  2U

static volatile uint8_t tableEchantillonsIsr[NB_BUFFERS_ISR][NB_ECHANTILLONS_PAR_PERIODE];
static volatile uint8_t indiceBufferActifIsr = 0U;
static volatile uint8_t indiceEchantillonCourantIsr = 0U;
static hw_timer_t *timerMaterielGbf = nullptr;

/**************************************************************************
 * @brief Routine d'interruption matérielle (ISR) du Timer 0 placée en IRAM.
 *        Émet l'échantillon suivant sur le CNA (GPIO26 / DAC_CHANNEL_2).
 */
static void IRAM_ATTR routineInterruptionTimerGbf(void) {
    uint8_t bufferActif = indiceBufferActifIsr;
    uint8_t indice = indiceEchantillonCourantIsr;

    dac_output_voltage(DAC_CHANNEL_2, tableEchantillonsIsr[bufferActif][indice]);

    indice++;
    if (indice >= NB_ECHANTILLONS_PAR_PERIODE) {
        indice = 0U;
    }
    indiceEchantillonCourantIsr = indice;
}

/**************************************************************************
 * @brief Convertit une tension en millivolts en code numérique 8 bits pour le CNA.
 *        Sature automatiquement la tension dans la plage [0 mV .. 3300 mV].
 * 
 * @param tensionMv : Tension instantanée souhaitée en millivolts
 * @return uint8_t : Code numérique sur 8 bits entre 0 et 255
 */
uint8_t convertirMillivoltsEnCodeDac(int32_t tensionMv) {
    int32_t tensionSatureeMv = tensionMv;

    if (tensionSatureeMv < TENSION_DAC_MIN_MV) {
        tensionSatureeMv = TENSION_DAC_MIN_MV;
    } else if (tensionSatureeMv > TENSION_DAC_MAX_MV) {
        tensionSatureeMv = TENSION_DAC_MAX_MV;
    }

    // Conversion proportionnelle avec arrondi à l'entier le plus proche
    int32_t codeDac = (tensionSatureeMv * (int32_t)VALEUR_DAC_MAX + (TENSION_DAC_MAX_MV / 2)) / TENSION_DAC_MAX_MV;

    if (codeDac < (int32_t)VALEUR_DAC_MIN) {
        codeDac = (int32_t)VALEUR_DAC_MIN;
    } else if (codeDac > (int32_t)VALEUR_DAC_MAX) {
        codeDac = (int32_t)VALEUR_DAC_MAX;
    }

    return (uint8_t)codeDac;
}

/**************************************************************************
 * @brief Calcule l'échantillon normalisé dans [-1.0 ; +1.0] selon la forme d'onde.
 * 
 * @param formeSignal : Forme du signal à synthétiser
 * @param indiceEchantillon : Indice courant dans la période
 * @param nbTotalEchantillons : Nombre total d'échantillons par période
 * @return float : Valeur normalisée comprise entre -1.0f et +1.0f
 */
float calculerValeurNormalisee(FormeSignal_t formeSignal, uint8_t indiceEchantillon, uint8_t nbTotalEchantillons) {
    if (nbTotalEchantillons <= 1U) {
        return 0.0f;
    }

    uint8_t indiceSecurise = indiceEchantillon % nbTotalEchantillons;
    uint8_t demiPeriode = nbTotalEchantillons / 2U;
    float valeurNormalisee = 0.0f;

    switch (formeSignal) {
        case SIGNAL_SINUS: {
            float angleRadians = (DEUX_PI * (float)indiceSecurise) / (float)nbTotalEchantillons;
            valeurNormalisee = sinf(angleRadians);
            break;
        }

        case SIGNAL_CARRE: {
            valeurNormalisee = (indiceSecurise < demiPeriode) ? 1.0f : -1.0f;
            break;
        }

        case SIGNAL_TRIANGLE: {
            if (indiceSecurise < demiPeriode) {
                valeurNormalisee = -1.0f + (2.0f * (float)indiceSecurise) / (float)demiPeriode;
            } else {
                uint8_t indiceDescendant = indiceSecurise - demiPeriode;
                valeurNormalisee = 1.0f - (2.0f * (float)indiceDescendant) / (float)demiPeriode;
            }
            break;
        }

        case SIGNAL_DENT_DE_SCIE: {
            valeurNormalisee = -1.0f + (2.0f * (float)indiceSecurise) / (float)(nbTotalEchantillons - 1U);
            break;
        }

        default: {
            valeurNormalisee = 0.0f;
            break;
        }
    }

    return valeurNormalisee;
}

/**************************************************************************
 * @brief Remplit une table d'échantillons 8 bits pour le CNA en fonction des paramètres du GBF.
 * 
 * @param parametres : Pointeur vers la structure des paramètres
 * @param tableSortie : Pointeur vers le tableau d'octets à remplir
 * @param tailleTable : Taille du tableau d'échantillons
 * @return true si la table a été générée avec succès, false sinon
 */
bool genererTableEchantillons(const ParametresGbf_t *parametres, uint8_t *tableSortie, uint8_t tailleTable) {
    // Vérification des pointeurs NULL et de la taille (Charte §6)
    if (parametres == nullptr || tableSortie == nullptr || tailleTable < 2U) {
        return false;
    }

    for (uint8_t indice = 0U; indice < tailleTable; indice++) {
        float ondeNormalisee = calculerValeurNormalisee(parametres->formeSignal, indice, tailleTable);
        int32_t variationMv = (int32_t)lroundf(ondeNormalisee * (float)parametres->amplitudeMv);
        int32_t tensionInstantaneeMv = (int32_t)parametres->offsetMv + variationMv;

        tableSortie[indice] = convertirMillivoltsEnCodeDac(tensionInstantaneeMv);
    }

    return true;
}

/**************************************************************************
 * @brief Calcule le seuil d'alarme du Timer matériel pour une fréquence donnée.
 * 
 * @param frequenceHz : Fréquence du signal souhaitée en Hz
 * @return uint64_t : Nombre de ticks du Timer entre deux échantillons
 */
uint64_t calculerSeuilAlarmeTimer(uint16_t frequenceHz) {
    uint16_t frequenceSaturee = frequenceHz;

    if (frequenceSaturee < FREQUENCE_MIN_HZ) {
        frequenceSaturee = FREQUENCE_MIN_HZ;
    } else if (frequenceSaturee > FREQUENCE_MAX_HZ) {
        frequenceSaturee = FREQUENCE_MAX_HZ;
    }

    uint64_t frequenceEchantillonnage = (uint64_t)frequenceSaturee * (uint64_t)NB_ECHANTILLONS_PAR_PERIODE;
    uint64_t seuilTicks = ((uint64_t)FREQUENCE_HORLOGE_TIMER_HZ + (frequenceEchantillonnage / 2ULL)) / frequenceEchantillonnage;

    if (seuilTicks == 0ULL) {
        seuilTicks = 1ULL;
    }

    return seuilTicks;
}

/**************************************************************************
 * @brief Initialise le CNA (GPIO26) et le Timer matériel 0 pour démarrer la génération.
 * 
 * @param parametresInitiaux : Pointeur vers les paramètres initiaux du signal
 * @return true si l'initialisation matérielle a réussi, false sinon
 */
bool initialiserGenerateurSignal(const ParametresGbf_t *parametresInitiaux) {
    if (parametresInitiaux == nullptr) {
        return false;
    }

    // Activation du CNA sur GPIO26 (DAC_CHANNEL_2) avec vérification du code de retour ESP-IDF
    esp_err_t erreurDac = dac_output_enable(DAC_CHANNEL_2);
    if (erreurDac != ESP_OK) {
        return false;
    }

    // Calcul de la table initiale dans le premier tampon
    uint8_t tableLocale[NB_ECHANTILLONS_PAR_PERIODE];
    if (!genererTableEchantillons(parametresInitiaux, tableLocale, NB_ECHANTILLONS_PAR_PERIODE)) {
        return false;
    }

    for (uint8_t i = 0U; i < NB_ECHANTILLONS_PAR_PERIODE; i++) {
        tableEchantillonsIsr[0][i] = tableLocale[i];
        tableEchantillonsIsr[1][i] = tableLocale[i];
    }
    indiceBufferActifIsr = 0U;
    indiceEchantillonCourantIsr = 0U;

    // Initialisation du Timer 0 (horloge 80 MHz / 8 = 10 MHz)
    timerMaterielGbf = timerBegin(NUMERO_TIMER_MATERIEL, DIVISEUR_HORLOGE_TIMER, true);
    if (timerMaterielGbf == nullptr) {
        return false;
    }

    uint64_t seuilInitial = calculerSeuilAlarmeTimer(parametresInitiaux->frequenceHz);
    timerAttachInterrupt(timerMaterielGbf, &routineInterruptionTimerGbf, true);
    timerAlarmWrite(timerMaterielGbf, seuilInitial, true);
    timerAlarmEnable(timerMaterielGbf);

    return true;
}

/**************************************************************************
 * @brief Met à jour en temps réel la table d'onde et la fréquence du Timer matériel.
 * 
 * @param nouveauxParametres : Pointeur vers les nouveaux paramètres du GBF
 * @return true si la mise à jour a réussi, false sinon
 */
bool appliquerParametresSignal(const ParametresGbf_t *nouveauxParametres) {
    if (nouveauxParametres == nullptr) {
        return false;
    }

    uint8_t tableLocale[NB_ECHANTILLONS_PAR_PERIODE];
    if (!genererTableEchantillons(nouveauxParametres, tableLocale, NB_ECHANTILLONS_PAR_PERIODE)) {
        return false;
    }

    // Écriture dans le tampon inactif puis bascule atomique de l'indice de tampon
    uint8_t prochainBuffer = (indiceBufferActifIsr == 0U) ? 1U : 0U;
    for (uint8_t i = 0U; i < NB_ECHANTILLONS_PAR_PERIODE; i++) {
        tableEchantillonsIsr[prochainBuffer][i] = tableLocale[i];
    }
    indiceBufferActifIsr = prochainBuffer;

    // Mise à jour de la période d'alarme du Timer matériel si initialisé
    if (timerMaterielGbf != nullptr) {
        uint64_t nouveauSeuil = calculerSeuilAlarmeTimer(nouveauxParametres->frequenceHz);
        timerAlarmWrite(timerMaterielGbf, nouveauSeuil, true);
    }

    return true;
}

/**************************************************************************
 * @brief Retourne le libellé texte correspondant à une forme de signal.
 * 
 * @param formeSignal : Valeur de l'énumération FormeSignal_t
 * @return const char* : Chaîne constante décrivant la forme
 */
const char *obtenirNomFormeSignal(FormeSignal_t formeSignal) {
    switch (formeSignal) {
        case SIGNAL_SINUS:
            return "SINUS";
        case SIGNAL_CARRE:
            return "CARRE";
        case SIGNAL_TRIANGLE:
            return "TRIANGLE";
        case SIGNAL_DENT_DE_SCIE:
            return "DENT_DE_SCIE";
        default:
            return "INCONNU";
    }
}
