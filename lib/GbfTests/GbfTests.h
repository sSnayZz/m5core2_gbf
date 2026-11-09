/**
 * @file GbfTests.h
 * @brief Module de tests unitaires automatisés du projet GBF (Charte Qualité §7).
 * @details Couvre les cas nominaux, les cas limites et les cas d'erreur/robustesse
 *          de l'ensemble des fonctions de calcul, d'IHM et de décodage série.
 */

#pragma once

#include "GbfConfig.h"

/**
 * @brief Exécute la suite complète de tests unitaires et affiche le rapport sur le port série.
 * 
 * @param mutexPortSerie : Mutex protégeant l'écriture sur le port série (peut être NULL)
 * @return true si 100 % des tests unitaires réussissent, false si au moins un test échoue
 */
bool executerTousLesTestsUnitairesGbf(SemaphoreHandle_t mutexPortSerie);
