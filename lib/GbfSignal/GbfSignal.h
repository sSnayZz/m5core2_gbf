/**
 * @file GbfSignal.h
 * @brief Module de synthèse numérique et de génération matérielle (CNA + Timer) du GBF.
 * @details Génère les quatre formes d'ondes (Sinus, Carré, Triangle, Dent de scie)
 *          sur la broche GPIO26 (DAC_CHANNEL_2) cadencée par le Timer matériel 0.
 */

#pragma once

#include <Arduino.h>
#include "GbfConfig.h"

/**
 * @brief Convertit une tension en millivolts en code numérique 8 bits pour le CNA.
 *        Sature automatiquement la tension dans la plage [0 mV .. 3300 mV].
 * 
 * @param tensionMv : Tension instantanée souhaitée en millivolts (peut être hors bornes)
 * @return uint8_t : Code numérique sur 8 bits entre 0 (0,0 V) et 255 (3,3 V)
 */
uint8_t convertirMillivoltsEnCodeDac(int32_t tensionMv);

/**
 * @brief Calcule l'échantillon normalisé dans [-1.0 ; +1.0] selon la forme d'onde.
 * 
 * @param formeSignal : Forme du signal (SIGNAL_SINUS, SIGNAL_CARRE, SIGNAL_TRIANGLE, SIGNAL_DENT_DE_SCIE)
 * @param indiceEchantillon : Indice courant dans la période [0 .. nbTotalEchantillons - 1]
 * @param nbTotalEchantillons : Nombre total d'échantillons par période (> 1)
 * @return float : Valeur normalisée comprise entre -1.0f et +1.0f
 */
float calculerValeurNormalisee(FormeSignal_t formeSignal, uint8_t indiceEchantillon, uint8_t nbTotalEchantillons);

/**
 * @brief Remplit une table d'échantillons 8 bits pour le CNA en fonction des paramètres du GBF.
 * 
 * @param parametres : Pointeur vers la structure des paramètres (forme, amplitude, offset)
 * @param tableSortie : Pointeur vers le tableau d'octets à remplir
 * @param tailleTable : Taille du tableau d'échantillons (doit être >= 2)
 * @return true si la table a été générée avec succès, false si un pointeur est NULL ou taille invalide
 */
bool genererTableEchantillons(const ParametresGbf_t *parametres, uint8_t *tableSortie, uint8_t tailleTable);

/**
 * @brief Calcule le seuil d'alarme (en ticks de 0,1 µs) du Timer matériel pour une fréquence donnée.
 * 
 * @param frequenceHz : Fréquence du signal souhaitée en Hz (saturée entre 1 Hz et 1500 Hz)
 * @return uint64_t : Nombre de ticks du Timer entre deux échantillons successifs
 */
uint64_t calculerSeuilAlarmeTimer(uint16_t frequenceHz);

/**
 * @brief Initialise le CNA (GPIO26) et le Timer matériel 0 pour démarrer la génération du signal.
 * 
 * @param parametresInitiaux : Pointeur vers les paramètres initiaux du signal
 * @return true si le CNA et le Timer matériel ont été correctement initialisés, false sinon
 */
bool initialiserGenerateurSignal(const ParametresGbf_t *parametresInitiaux);

/**
 * @brief Met à jour en temps réel la table d'onde et la fréquence du Timer matériel.
 * 
 * @param nouveauxParametres : Pointeur vers les nouveaux paramètres du GBF
 * @return true si la mise à jour a réussi, false en cas d'erreur de pointeur ou de Timer
 */
bool appliquerParametresSignal(const ParametresGbf_t *nouveauxParametres);

/**
 * @brief Retourne le libellé texte correspondant à une forme de signal.
 * 
 * @param formeSignal : Valeur de l'énumération FormeSignal_t
 * @return const char* : Chaîne constante ("SINUS", "CARRE", "TRIANGLE", "DENT_DE_SCIE")
 */
const char *obtenirNomFormeSignal(FormeSignal_t formeSignal);
