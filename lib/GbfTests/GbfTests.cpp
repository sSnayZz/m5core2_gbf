/**
 * @file GbfTests.cpp
 * @brief Implémentation des tests unitaires couvrant les cas nominaux, limites et d'erreur.
 */

#include "GbfTests.h"
#include "GbfSignal.h"
#include "GbfIhm.h"
#include "GbfMiseEnPage.h"
#include "GbfSerie.h"
#include <math.h>
#include <string.h>

#define TOLERANCE_FLOTTANTE_TEST        0.02f
#define VALEUR_DAC_MI_ECHELLE_ATTENDUE  128U
#define SEUIL_TIMER_1_HZ_ATTENDU        100000ULL
#define SEUIL_TIMER_1000_HZ_ATTENDU     100ULL
#define SEUIL_TIMER_1500_HZ_ATTENDU     67ULL
#define FREQUENCE_TEST_440_HZ           440U
#define FREQUENCE_TEST_HORS_LIMITE_HZ   2500U
#define AMPLITUDE_TEST_800_MV           800U
#define OFFSET_TEST_1650_MV             1650U
#define OFFSET_TEST_ECRETAGE_HAUT_MV    2800U
#define TENSION_ATTENDUE_BASSE_MV       600U
#define TENSION_ATTENDUE_HAUTE_MV       2600U

/**************************************************************************
 * @brief Teste la fonction convertirMillivoltsEnCodeDac (cas nominaux, limites et hors bornes).
 * 
 * @return true si tous les cas de test passent, false sinon
 */
static bool testerConversionTensionDac(void) {
    // Cas limites (0 mV et 3300 mV)
    if (convertirMillivoltsEnCodeDac(TENSION_DAC_MIN_MV) != VALEUR_DAC_MIN) {
        return false;
    }
    if (convertirMillivoltsEnCodeDac(TENSION_DAC_MAX_MV) != VALEUR_DAC_MAX) {
        return false;
    }

    // Cas nominal mi-échelle (1650 mV -> 128)
    if (convertirMillivoltsEnCodeDac(TENSION_DAC_MAX_MV / 2) != VALEUR_DAC_MI_ECHELLE_ATTENDUE) {
        return false;
    }

    // Cas d'erreur / saturation hors bornes (< 0 mV et > 3300 mV)
    if (convertirMillivoltsEnCodeDac(-500) != VALEUR_DAC_MIN) {
        return false;
    }
    if (convertirMillivoltsEnCodeDac(5000) != VALEUR_DAC_MAX) {
        return false;
    }

    return true;
}

/**************************************************************************
 * @brief Teste la fonction calculerValeurNormalisee et genererTableEchantillons.
 * 
 * @return true si tous les cas de test passent, false sinon
 */
static bool testerSyntheseSignaux(void) {
    // Cas d'erreur : nombre d'échantillons <= 1
    if (fabsf(calculerValeurNormalisee(SIGNAL_SINUS, 0U, 0U)) > TOLERANCE_FLOTTANTE_TEST) {
        return false;
    }

    // Cas nominal Sinus : sin(0) = 0, sin(pi/2) = +1, sin(3pi/2) = -1
    if (fabsf(calculerValeurNormalisee(SIGNAL_SINUS, 0U, NB_ECHANTILLONS_PAR_PERIODE) - 0.0f) > TOLERANCE_FLOTTANTE_TEST) {
        return false;
    }
    if (fabsf(calculerValeurNormalisee(SIGNAL_SINUS, 25U, NB_ECHANTILLONS_PAR_PERIODE) - 1.0f) > TOLERANCE_FLOTTANTE_TEST) {
        return false;
    }

    // Cas nominal Carré : +1 sur la première demi-période, -1 sur la seconde
    if (calculerValeurNormalisee(SIGNAL_CARRE, 10U, NB_ECHANTILLONS_PAR_PERIODE) != 1.0f) {
        return false;
    }
    if (calculerValeurNormalisee(SIGNAL_CARRE, 60U, NB_ECHANTILLONS_PAR_PERIODE) != -1.0f) {
        return false;
    }

    // Cas nominal Triangle : -1 au début, +1 au milieu
    if (fabsf(calculerValeurNormalisee(SIGNAL_TRIANGLE, 0U, NB_ECHANTILLONS_PAR_PERIODE) + 1.0f) > TOLERANCE_FLOTTANTE_TEST) {
        return false;
    }
    if (fabsf(calculerValeurNormalisee(SIGNAL_TRIANGLE, INDICE_MILIEU_PERIODE, NB_ECHANTILLONS_PAR_PERIODE) - 1.0f) > TOLERANCE_FLOTTANTE_TEST) {
        return false;
    }

    // Cas nominal Dent de scie : -1 au premier point, +1 au dernier point
    if (fabsf(calculerValeurNormalisee(SIGNAL_DENT_DE_SCIE, 0U, NB_ECHANTILLONS_PAR_PERIODE) + 1.0f) > TOLERANCE_FLOTTANTE_TEST) {
        return false;
    }
    if (fabsf(calculerValeurNormalisee(SIGNAL_DENT_DE_SCIE, NB_ECHANTILLONS_PAR_PERIODE - 1U, NB_ECHANTILLONS_PAR_PERIODE) - 1.0f) > TOLERANCE_FLOTTANTE_TEST) {
        return false;
    }

    // Cas d'erreur sur genererTableEchantillons : pointeurs NULL
    ParametresGbf_t paramsTest;
    uint8_t tableTest[NB_ECHANTILLONS_PAR_PERIODE];
    reinitialiserParametresParDefaut(&paramsTest);

    if (genererTableEchantillons(nullptr, tableTest, NB_ECHANTILLONS_PAR_PERIODE)) {
        return false;
    }
    if (genererTableEchantillons(&paramsTest, nullptr, NB_ECHANTILLONS_PAR_PERIODE)) {
        return false;
    }
    if (!genererTableEchantillons(&paramsTest, tableTest, NB_ECHANTILLONS_PAR_PERIODE)) {
        return false;
    }

    return true;
}

/**************************************************************************
 * @brief Teste la fonction calculerSeuilAlarmeTimer (1 Hz, 1000 Hz, 1500 Hz et hors bornes).
 * 
 * @return true si tous les cas de test passent, false sinon
 */
static bool testerCalculTimer(void) {
    if (calculerSeuilAlarmeTimer(FREQUENCE_MIN_HZ) != SEUIL_TIMER_1_HZ_ATTENDU) {
        return false;
    }
    if (calculerSeuilAlarmeTimer(1000U) != SEUIL_TIMER_1000_HZ_ATTENDU) {
        return false;
    }
    if (calculerSeuilAlarmeTimer(FREQUENCE_MAX_HZ) != SEUIL_TIMER_1500_HZ_ATTENDU) {
        return false;
    }
    // Saturation basse (0 Hz -> 1 Hz) et haute (5000 Hz -> 1500 Hz)
    if (calculerSeuilAlarmeTimer(0U) != SEUIL_TIMER_1_HZ_ATTENDU) {
        return false;
    }
    if (calculerSeuilAlarmeTimer(5000U) != SEUIL_TIMER_1500_HZ_ATTENDU) {
        return false;
    }

    return true;
}

/**************************************************************************
 * @brief Teste la machine à états IHM, l'écrêtage, les libellés et la sélection tactile.
 * 
 * @return true si tous les cas de test passent, false sinon
 */
static bool testerMachineEtatsIhm(void) {
    ParametresGbf_t params;
    CommandeGbf_t cmd;

    if (reinitialiserParametresParDefaut(nullptr)) {
        return false;
    }
    if (!reinitialiserParametresParDefaut(&params)) {
        return false;
    }

    // Cycle complet des 5 états du Menu (0 -> 1 -> 2 -> 3 -> 4 -> 0)
    cmd.typeCommande = CMD_IHM_BOUTON_MENU;
    cmd.valeurParametre = 0U;
    cmd.depuisPortSerie = false;

    for (uint8_t attendu = 1U; attendu <= 4U; attendu++) {
        if (!appliquerCommandeSurParametres(&params, &cmd)) {
            return false;
        }
        if ((uint8_t)params.etatIhm != attendu) {
            return false;
        }
    }
    if (!appliquerCommandeSurParametres(&params, &cmd) || params.etatIhm != ETAT_IHM_CONSULTATION) {
        return false;
    }

    // Vérification du pas adaptatif de fréquence aux frontières (1 Hz, 10 Hz, 100 Hz, 1500 Hz)
    if (calculerNouvelleFrequenceParPas(FREQUENCE_MIN_HZ, false) != FREQUENCE_MIN_HZ) {
        return false;
    }
    if (calculerNouvelleFrequenceParPas(FREQUENCE_MAX_HZ, true) != FREQUENCE_MAX_HZ) {
        return false;
    }
    if (calculerNouvelleFrequenceParPas(10U, true) != 20U) {
        return false;
    }
    if (calculerNouvelleFrequenceParPas(10U, false) != 9U) {
        return false;
    }

    // Détection d'écrêtage et calcul des tensions extrêmes Vmin / Vmax
    uint16_t vMin = 0U;
    uint16_t vMax = 0U;
    if (estSignalEcrete(&params)) {
        return false;
    }
    if (!calculerTensionsExtremes(&params, &vMin, &vMax) ||
        vMin != TENSION_ATTENDUE_BASSE_MV || vMax != TENSION_ATTENDUE_HAUTE_MV) {
        return false;
    }

    params.offsetMv = OFFSET_TEST_ECRETAGE_HAUT_MV;
    if (!estSignalEcrete(&params)) {
        return false;
    }
    if (!calculerTensionsExtremes(&params, &vMin, &vMax) || vMax != (uint16_t)TENSION_DAC_MAX_MV) {
        return false;
    }

    // Formatage de la période et des libellés de touches contextuelles
    char tamponTexte[TAILLE_LIBELLE_MAX];
    if (!formaterPeriode(100U, tamponTexte, sizeof(tamponTexte)) || formaterPeriode(0U, tamponTexte, sizeof(tamponTexte))) {
        return false;
    }
    if (!formaterLibelleTouche(&params, true, tamponTexte, sizeof(tamponTexte))) {
        return false;
    }

    // Sélection tactile d'une carte et bascule en mode consultation au second toucher
    EtatIhm_t nouvelEtat = ETAT_IHM_CONSULTATION;
    if (!determinerEtatDepuisToucher(POS_X_CARTES + 10, POS_Y_PREMIERE_CARTE + 10, ETAT_IHM_CONSULTATION, &nouvelEtat) ||
        nouvelEtat != ETAT_IHM_REGLAGE_FORME) {
        return false;
    }
    if (!determinerEtatDepuisToucher(POS_X_CARTES + 10, POS_Y_PREMIERE_CARTE + 10, ETAT_IHM_REGLAGE_FORME, &nouvelEtat) ||
        nouvelEtat != ETAT_IHM_CONSULTATION) {
        return false;
    }

    return true;
}

/**************************************************************************
 * @brief Teste l'automate de décodage des trames série (cas nominaux, limites et erreurs).
 * 
 * @return true si tous les cas de test passent, false sinon
 */
static bool testerDecodeurSerie(void) {
    CommandeGbf_t cmd;

    // Cas nominaux
    if (!decoderTrameSerie("S2", &cmd) || cmd.typeCommande != CMD_SERIE_FIXER_FORME || cmd.valeurParametre != SIGNAL_TRIANGLE) {
        return false;
    }
    if (!decoderTrameSerie("F=440", &cmd) || cmd.typeCommande != CMD_SERIE_FIXER_FREQUENCE || cmd.valeurParametre != FREQUENCE_TEST_440_HZ) {
        return false;
    }
    if (!decoderTrameSerie("A800", &cmd) || cmd.typeCommande != CMD_SERIE_FIXER_AMPLITUDE || cmd.valeurParametre != AMPLITUDE_TEST_800_MV) {
        return false;
    }
    if (!decoderTrameSerie("O1650", &cmd) || cmd.typeCommande != CMD_SERIE_FIXER_OFFSET || cmd.valeurParametre != OFFSET_TEST_1650_MV) {
        return false;
    }
    if (!decoderTrameSerie("+", &cmd) || cmd.typeCommande != CMD_IHM_BOUTON_PLUS) {
        return false;
    }
    if (!decoderTrameSerie("-", &cmd) || cmd.typeCommande != CMD_IHM_BOUTON_MOINS) {
        return false;
    }
    if (!decoderTrameSerie("M", &cmd) || cmd.typeCommande != CMD_IHM_BOUTON_MENU) {
        return false;
    }
    if (!decoderTrameSerie("R", &cmd) || cmd.typeCommande != CMD_IHM_REINITIALISER) {
        return false;
    }

    // Cas limites exacts
    if (!decoderTrameSerie("F1", &cmd) || !decoderTrameSerie("F1500", &cmd)) {
        return false;
    }
    if (!decoderTrameSerie("A100", &cmd) || !decoderTrameSerie("A1600", &cmd)) {
        return false;
    }
    if (!decoderTrameSerie("O0", &cmd) || !decoderTrameSerie("O3300", &cmd)) {
        return false;
    }

    // Cas d'erreur (valeurs hors bornes, commandes malformées, pointeurs NULL)
    if (decoderTrameSerie(nullptr, &cmd) || decoderTrameSerie("F100", nullptr) || decoderTrameSerie("", &cmd)) {
        return false;
    }
    if (decoderTrameSerie("S4", &cmd) || decoderTrameSerie("F0", &cmd) || decoderTrameSerie("F1501", &cmd)) {
        return false;
    }
    if (decoderTrameSerie("A99", &cmd) || decoderTrameSerie("A1601", &cmd) || decoderTrameSerie("O3301", &cmd)) {
        return false;
    }
    if (decoderTrameSerie("++", &cmd) || decoderTrameSerie("F12A", &cmd) || decoderTrameSerie("ERREUR", &cmd)) {
        return false;
    }

    return true;
}

/**************************************************************************
 * @brief Exécute la suite complète de tests unitaires et affiche le rapport sur le port série.
 * 
 * @param mutexPortSerie : Mutex protégeant l'écriture sur le port série
 * @return true si 100 % des tests unitaires réussissent, false sinon
 */
bool executerTousLesTestsUnitairesGbf(SemaphoreHandle_t mutexPortSerie) {
    bool okDac = testerConversionTensionDac();
    bool okSynthese = testerSyntheseSignaux();
    bool okTimer = testerCalculTimer();
    bool okIhm = testerMachineEtatsIhm();
    bool okSerie = testerDecodeurSerie();

    bool bilanGlobal = okDac && okSynthese && okTimer && okIhm && okSerie;

    envoyerMessageSerieProtege(mutexPortSerie, "=== RAPPORT DE TESTS UNITAIRES GBF ===");
    envoyerMessageSerieProtege(mutexPortSerie, okDac ? "[PASS] TU1 : Conversion mV -> CNA 8 bits" : "[FAIL] TU1 : Conversion mV -> CNA 8 bits");
    envoyerMessageSerieProtege(mutexPortSerie, okSynthese ? "[PASS] TU2 : Synthese des 4 formes d'ondes" : "[FAIL] TU2 : Synthese des 4 formes d'ondes");
    envoyerMessageSerieProtege(mutexPortSerie, okTimer ? "[PASS] TU3 : Calcul seuil Timer (1 Hz - 1,5 kHz)" : "[FAIL] TU3 : Calcul seuil Timer");
    envoyerMessageSerieProtege(mutexPortSerie, okIhm ? "[PASS] TU4 : Machine a etats IHM et bornes" : "[FAIL] TU4 : Machine a etats IHM");
    envoyerMessageSerieProtege(mutexPortSerie, okSerie ? "[PASS] TU5 : Automate decodeur protocole serie" : "[FAIL] TU5 : Automate decodeur protocole serie");
    envoyerMessageSerieProtege(mutexPortSerie, bilanGlobal ? "=== BILAN : 5/5 TESTS REUSSIS (100%) ===" : "=== BILAN : ECHEC D'AU MOINS UN TEST ===");

    return bilanGlobal;
}
