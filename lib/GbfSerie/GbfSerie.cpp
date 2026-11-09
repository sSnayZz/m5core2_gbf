/**
 * @file GbfSerie.cpp
 * @brief Implémentation de l'automate de décodage de trames série et des réponses de confirmation.
 */

#include "GbfSerie.h"
#include "GbfSignal.h"
#include <stdio.h>
#include <ctype.h>

#define VALEUR_MAX_ACCUMULATEUR_SERIE   10000UL
#define BASE_DECIMALE                   10UL

/**************************************************************************
 * @brief Vérifie si le préfixe et la valeur numérique extraite respectent les bornes du GBF,
 *        et construit la commande correspondante.
 * 
 * @param codeCommande : Lettre de commande ('S', 'F', 'A', 'O') en majuscule
 * @param valeurNumerique : Valeur entière positive extraite de la trame
 * @param commandeSortie : Pointeur vers la structure CommandeGbf_t à remplir
 * @return true si la valeur est dans les bornes autorisées, false sinon
 */
static bool validerEtAffecterCommandeParametree(char codeCommande, uint32_t valeurNumerique, CommandeGbf_t *commandeSortie) {
    commandeSortie->depuisPortSerie = true;

    switch (codeCommande) {
        case 'S': {
            if (valeurNumerique >= NB_FORMES_SIGNAL) {
                return false;
            }
            commandeSortie->typeCommande = CMD_SERIE_FIXER_FORME;
            commandeSortie->valeurParametre = (uint16_t)valeurNumerique;
            return true;
        }

        case 'F': {
            if (valeurNumerique < FREQUENCE_MIN_HZ || valeurNumerique > FREQUENCE_MAX_HZ) {
                return false;
            }
            commandeSortie->typeCommande = CMD_SERIE_FIXER_FREQUENCE;
            commandeSortie->valeurParametre = (uint16_t)valeurNumerique;
            return true;
        }

        case 'A': {
            if (valeurNumerique < AMPLITUDE_MIN_MV || valeurNumerique > AMPLITUDE_MAX_MV) {
                return false;
            }
            commandeSortie->typeCommande = CMD_SERIE_FIXER_AMPLITUDE;
            commandeSortie->valeurParametre = (uint16_t)valeurNumerique;
            return true;
        }

        case 'O': {
            if (valeurNumerique > OFFSET_MAX_MV) {
                return false;
            }
            commandeSortie->typeCommande = CMD_SERIE_FIXER_OFFSET;
            commandeSortie->valeurParametre = (uint16_t)valeurNumerique;
            return true;
        }

        default:
            return false;
    }
}

/**************************************************************************
 * @brief Analyse une trame série complète à l'aide d'un diagramme d'état.
 * 
 * @param trame : Chaîne de caractères reçue (terminée par '\0')
 * @param commandeSortie : Pointeur vers la structure CommandeGbf_t à renseigner
 * @return true si la trame est valide et dans les plages autorisées, false sinon
 */
bool decoderTrameSerie(const char *trame, CommandeGbf_t *commandeSortie) {
    if (trame == nullptr || commandeSortie == nullptr || trame[0] == '\0') {
        return false;
    }

    EtatParseurSerie_t etatCourant = ETAT_PARSE_DEBUT;
    uint8_t pos = 0U;
    char lettreCommande = '\0';
    uint32_t valeurAccumulee = 0UL;
    bool auMoinsUnChiffreLu = false;

    while (etatCourant != ETAT_PARSE_VALIDE && etatCourant != ETAT_PARSE_ERREUR) {
        char caractereCourant = trame[pos];

        switch (etatCourant) {
            case ETAT_PARSE_DEBUT: {
                char majuscule = (char)toupper((unsigned char)caractereCourant);
                if (majuscule == '+' || majuscule == '-' || majuscule == 'M' ||
                    majuscule == 'R' || majuscule == '?') {
                    lettreCommande = majuscule;
                    pos++;
                    etatCourant = ETAT_PARSE_FIN_COMMANDE_SIMPLE;
                } else if (majuscule == 'S' || majuscule == 'F' ||
                           majuscule == 'A' || majuscule == 'O') {
                    lettreCommande = majuscule;
                    pos++;
                    etatCourant = ETAT_PARSE_SEPARATEUR_OPTIONNEL;
                } else {
                    etatCourant = ETAT_PARSE_ERREUR;
                }
                break;
            }

            case ETAT_PARSE_FIN_COMMANDE_SIMPLE: {
                if (caractereCourant == '\0') {
                    etatCourant = ETAT_PARSE_VALIDE;
                } else {
                    etatCourant = ETAT_PARSE_ERREUR;
                }
                break;
            }

            case ETAT_PARSE_SEPARATEUR_OPTIONNEL: {
                if (caractereCourant == '=') {
                    pos++;
                    etatCourant = ETAT_PARSE_CHIFFRES_VALEUR;
                } else if (caractereCourant >= '0' && caractereCourant <= '9') {
                    etatCourant = ETAT_PARSE_CHIFFRES_VALEUR;
                } else {
                    etatCourant = ETAT_PARSE_ERREUR;
                }
                break;
            }

            case ETAT_PARSE_CHIFFRES_VALEUR: {
                if (caractereCourant >= '0' && caractereCourant <= '9') {
                    valeurAccumulee = valeurAccumulee * BASE_DECIMALE + (uint32_t)(caractereCourant - '0');
                    auMoinsUnChiffreLu = true;
                    pos++;
                    if (valeurAccumulee > VALEUR_MAX_ACCUMULATEUR_SERIE) {
                        etatCourant = ETAT_PARSE_ERREUR;
                    }
                } else if (caractereCourant == '\0' && auMoinsUnChiffreLu) {
                    etatCourant = ETAT_PARSE_VALIDE;
                } else {
                    etatCourant = ETAT_PARSE_ERREUR;
                }
                break;
            }

            default:
                etatCourant = ETAT_PARSE_ERREUR;
                break;
        }
    }

    if (etatCourant != ETAT_PARSE_VALIDE) {
        return false;
    }

    // Affectation de la commande simple ou paramétrée après validation syntaxique
    commandeSortie->depuisPortSerie = true;
    commandeSortie->valeurParametre = 0U;

    switch (lettreCommande) {
        case '+':
            commandeSortie->typeCommande = CMD_IHM_BOUTON_PLUS;
            return true;
        case '-':
            commandeSortie->typeCommande = CMD_IHM_BOUTON_MOINS;
            return true;
        case 'M':
            commandeSortie->typeCommande = CMD_IHM_BOUTON_MENU;
            return true;
        case 'R':
            commandeSortie->typeCommande = CMD_IHM_REINITIALISER;
            return true;
        case '?':
            commandeSortie->typeCommande = CMD_SERIE_DEMANDER_ETAT;
            return true;
        default:
            return validerEtAffecterCommandeParametree(lettreCommande, valeurAccumulee, commandeSortie);
    }
}

/**************************************************************************
 * @brief Génère la chaîne de confirmation décrivant l'état courant du GBF.
 * 
 * @param parametres : Pointeur vers les paramètres courants du GBF
 * @param bufferMessage : Tampon de sortie où écrire la chaîne formatée
 * @param tailleBuffer : Taille maximale du tampon de sortie en octets
 * @return true si le formatage a réussi, false sinon
 */
bool formaterMessageConfirmation(const ParametresGbf_t *parametres, char *bufferMessage, size_t tailleBuffer) {
    if (parametres == nullptr || bufferMessage == nullptr || tailleBuffer == 0U) {
        return false;
    }

    int nbCaracteresEcrits = snprintf(
        bufferMessage,
        tailleBuffer,
        "OK: SIGNAL=%s | FREQ=%u Hz | AMPL=%u mV | OFFSET=%u mV | MENU=%u",
        obtenirNomFormeSignal(parametres->formeSignal),
        parametres->frequenceHz,
        parametres->amplitudeMv,
        parametres->offsetMv,
        (unsigned int)parametres->etatIhm
    );

    return (nbCaracteresEcrits > 0 && (size_t)nbCaracteresEcrits < tailleBuffer);
}

/**************************************************************************
 * @brief Envoie un message sur le port série en protégeant la ressource partagée par un Mutex.
 * 
 * @param mutexPortSerie : Handle du Mutex protégeant le port série
 * @param message : Chaîne de caractères à envoyer
 */
void envoyerMessageSerieProtege(SemaphoreHandle_t mutexPortSerie, const char *message) {
    if (message == nullptr) {
        return;
    }

    if (mutexPortSerie != nullptr) {
        if (xSemaphoreTake(mutexPortSerie, portMAX_DELAY) == pdTRUE) {
            Serial.println(message);
            xSemaphoreGive(mutexPortSerie);
        }
    } else {
        Serial.println(message);
    }
}
