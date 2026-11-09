/**
 * @file GbfSerie.h
 * @brief Module de gestion du protocole de communication série du GBF.
 * @details Décode les trames reçues par automate à états finis (conformément au §10.1 du TP),
 *          vérifie la validité des paramètres et renvoie un message de confirmation protégé par Mutex.
 */

#pragma once

#include <Arduino.h>
#include "GbfConfig.h"

/**
 * @brief États de l'automate d'analyse syntaxique d'une trame série.
 */
typedef enum {
    ETAT_PARSE_DEBUT = 0,           /**< Lecture du premier caractère d'identification de commande */
    ETAT_PARSE_FIN_COMMANDE_SIMPLE, /**< Vérification qu'une commande à 1 caractère est bien terminée */
    ETAT_PARSE_SEPARATEUR_OPTIONNEL,/**< Lecture du séparateur '=' éventuel ou du premier chiffre */
    ETAT_PARSE_CHIFFRES_VALEUR,     /**< Accumulation des chiffres décimaux du paramètre */
    ETAT_PARSE_VALIDE,              /**< Trame entièrement analysée et syntaxiquement correcte */
    ETAT_PARSE_ERREUR               /**< Caractère inattendu ou dépassement de capacité */
} EtatParseurSerie_t;

/**
 * @brief Analyse une trame série complète (terminée par '\0') à l'aide d'un diagramme d'état
 *        et remplit la structure de commande correspondante.
 * 
 * @param trame : Chaîne de caractères reçue (sans '\r' ni '\n')
 * @param commandeSortie : Pointeur vers la structure CommandeGbf_t à renseigner
 * @return true si la trame est valide et dans les plages autorisées, false sinon
 */
bool decoderTrameSerie(const char *trame, CommandeGbf_t *commandeSortie);

/**
 * @brief Génère la chaîne de confirmation décrivant l'état courant du GBF après une commande.
 * 
 * @param parametres : Pointeur vers les paramètres courants du GBF
 * @param bufferMessage : Tampon de sortie où écrire la chaîne formatée
 * @param tailleBuffer : Taille maximale du tampon de sortie en octets
 * @return true si le formatage a réussi, false si un pointeur est NULL ou tampon trop court
 */
bool formaterMessageConfirmation(const ParametresGbf_t *parametres, char *bufferMessage, size_t tailleBuffer);

/**
 * @brief Envoie un message sur le port série en protégeant la ressource partagée par un Mutex FreeRTOS.
 * 
 * @param mutexPortSerie : Handle du Mutex protégeant le port série (peut être NULL si non utilisé)
 * @param message : Chaîne de caractères à envoyer avec saut de ligne
 */
void envoyerMessageSerieProtege(SemaphoreHandle_t mutexPortSerie, const char *message);
