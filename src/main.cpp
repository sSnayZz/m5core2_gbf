/**
 * @file main.cpp
 * @brief Programme principal du Générateur Basses Fréquences (GBF) sur M5Stack Core 2.
 * @details Exercice 14 - Module INFO5 (R5.10 Systèmes Embarqués Temps Réel).
 *          Architecture multi-tâches FreeRTOS :
 *          - Génération de signaux Sinus, Carré, Triangle et Dent de scie (1 Hz à 1,5 kHz)
 *            sur la sortie analogique GPIO26 (DAC_CHANNEL_2) via interruption Timer 0.
 *          - Réglage complet (Forme, Fréquence, Amplitude, Offset) par les 3 boutons,
 *            par sélection tactile directe des cartes sur l'écran et par protocole série
 *            à 115200 bauds avec accusé de réception.
 *          - Affichage double-bufferisé sans scintillement via sprite M5Canvas.
 *          - Communication inter-tâches par files (QueueHandle_t) et protection série par Mutex.
 */

#include <M5Unified.h>
#include "GbfConfig.h"
#include "GbfSignal.h"
#include "GbfIhm.h"
#include "GbfAffichage.h"
#include "GbfSerie.h"
#include "GbfTests.h"

// ============================================================================
// HANDLE DE LA FILE DE RECEPTION SERIE POUR LE CALLBACK MATERIEL
// Note Charte §1.1 : `Serial.onReceive(void(*)(void))` n'acceptant pas d'argument
// utilisateur, seul ce handle de file est statique au fichier main.cpp. Toutes
// les tâches reçoivent leur contexte via le paramètre `pvParameters`.
// ============================================================================
static QueueHandle_t fileInterruptSerie = nullptr;

// Déclaration des prototypes des callbacks et tâches FreeRTOS
void serialCallback(void);
void taskTraiteTrame(void *pvParameters);
void taskBoutonsIhm(void *pvParameters);
void taskControleurGbf(void *pvParameters);
void taskAffichageLcd(void *pvParameters);

/**************************************************************************
 * @brief Fonction d'initialisation exécutée une seule fois au démarrage.
 *        Configure le M5Stack Core 2, le port série, le CNA/Timer et lance FreeRTOS.
 */
void setup(void) {
    // Structure de contexte RTOS conservée en mémoire statique locale à setup()
    // pour être transmise par pointeur (`pvParameters`) aux tâches FreeRTOS.
    static ContexteRtosGbf_t contexteRtos;

    auto configM5 = M5.config();
    configM5.serial_baudrate = VITESSE_PORT_SERIE;
    M5.begin(configM5);
    Serial.begin(VITESSE_PORT_SERIE);

    // Création des files (queues) et du Mutex FreeRTOS avec vérification des retours (Charte §6)
    contexteRtos.queueReceptionSerie = xQueueCreate(TAILLE_FILE_RECEPTION_SERIE, sizeof(char));
    contexteRtos.queueCommandesGbf = xQueueCreate(TAILLE_FILE_COMMANDES_GBF, sizeof(CommandeGbf_t));
    contexteRtos.queueAffichageLcd = xQueueCreate(TAILLE_FILE_AFFICHAGE_LCD, sizeof(ParametresGbf_t));
    contexteRtos.mutexPortSerie = xSemaphoreCreateMutex();

    if (contexteRtos.queueReceptionSerie == nullptr ||
        contexteRtos.queueCommandesGbf == nullptr ||
        contexteRtos.queueAffichageLcd == nullptr ||
        contexteRtos.mutexPortSerie == nullptr) {
        Serial.println("ERREUR CRITIQUE : Echec d'allocation des objets FreeRTOS !");
        return;
    }

    fileInterruptSerie = contexteRtos.queueReceptionSerie;

    // Initialisation du générateur de signal matériel et envoi de l'état initial à l'écran
    ParametresGbf_t parametresInitiaux;
    reinitialiserParametresParDefaut(&parametresInitiaux);

    if (!initialiserGenerateurSignal(&parametresInitiaux)) {
        envoyerMessageSerieProtege(contexteRtos.mutexPortSerie, "ERREUR CRITIQUE : Echec init CNA/Timer !");
        return;
    }

    xQueueSendToBack(contexteRtos.queueAffichageLcd, (const void *)&parametresInitiaux, 0U);

    // Exécution des tests unitaires au démarrage (Charte §7)
    executerTousLesTestsUnitairesGbf(contexteRtos.mutexPortSerie);

    // Création des 4 tâches FreeRTOS associées au coeur applicatif (Core 1)
    BaseType_t retourTacheControleur = xTaskCreatePinnedToCore(
        taskControleurGbf,
        "ControleurGbf",
        TAILLE_PILE_TACHE_OCTETS,
        (void *)&contexteRtos,
        PRIORITE_TACHE_CONTROLEUR_GBF,
        nullptr,
        COEUR_EXECUTION_APPLICATIF
    );

    BaseType_t retourTacheSerie = xTaskCreatePinnedToCore(
        taskTraiteTrame,
        "TraiteTrame",
        TAILLE_PILE_TACHE_OCTETS,
        (void *)&contexteRtos,
        PRIORITE_TACHE_TRAITE_TRAME,
        nullptr,
        COEUR_EXECUTION_APPLICATIF
    );

    BaseType_t retourTacheBoutons = xTaskCreatePinnedToCore(
        taskBoutonsIhm,
        "BoutonsIhm",
        TAILLE_PILE_TACHE_OCTETS,
        (void *)&contexteRtos,
        PRIORITE_TACHE_BOUTONS_IHM,
        nullptr,
        COEUR_EXECUTION_APPLICATIF
    );

    BaseType_t retourTacheAffichage = xTaskCreatePinnedToCore(
        taskAffichageLcd,
        "AffichageLcd",
        TAILLE_PILE_TACHE_OCTETS,
        (void *)&contexteRtos,
        PRIORITE_TACHE_AFFICHAGE_LCD,
        nullptr,
        COEUR_EXECUTION_APPLICATIF
    );

    if (retourTacheControleur != pdPASS || retourTacheSerie != pdPASS ||
        retourTacheBoutons != pdPASS || retourTacheAffichage != pdPASS) {
        envoyerMessageSerieProtege(contexteRtos.mutexPortSerie, "ERREUR CRITIQUE : Echec creation taches !");
        return;
    }

    // Affectation de la fonction de callback à l'événement OnReceive du port série
    Serial.onReceive(serialCallback);

    envoyerMessageSerieProtege(contexteRtos.mutexPortSerie, "=== GBF M5Stack Core 2 PRET (115200 bauds) ===");
}

/**************************************************************************
 * @brief Boucle principale Arduino laissée volontairement vide.
 *        L'ensemble des traitements est délégué aux tâches FreeRTOS et aux ISR.
 */
void loop(void) {
    vTaskDelay(portMAX_DELAY);
}

/**************************************************************************
 * @brief Callback appelé automatiquement à la réception d'octets sur le port série.
 *        Lit chaque caractère disponible et l'insère dans la file de réception.
 */
void serialCallback(void) {
    if (fileInterruptSerie == nullptr) {
        return;
    }

    char caractereRecu = '\0';
    while (Serial.available() > 0) {
        if (Serial.read(&caractereRecu, 1) == 1) {
            xQueueSendToBack(fileInterruptSerie, (const void *)&caractereRecu, DELAI_ATTENTE_FILE_SERIE_TICKS);
        }
    }
}

/**************************************************************************
 * @brief Tâche FreeRTOS de reconstitution et de décodage des trames série.
 * 
 * @param pvParameters : Pointeur vers la structure ContexteRtosGbf_t
 */
void taskTraiteTrame(void *pvParameters) {
    if (pvParameters == nullptr) {
        vTaskDelete(nullptr);
        return;
    }

    ContexteRtosGbf_t *contexte = (ContexteRtosGbf_t *)pvParameters;
    char bufferTrame[TAILLE_BUFFER_TRAME_SERIE];
    uint8_t indiceCaractere = 0U;

    for (;;) {
        if (xQueueReceive(contexte->queueReceptionSerie, (void *)&bufferTrame[indiceCaractere], portMAX_DELAY) == pdTRUE) {
            char caractere = bufferTrame[indiceCaractere];

            if (caractere == '\r' || caractere == '\n') {
                bufferTrame[indiceCaractere] = '\0';

                // Ignorer les fins de lignes vides (ex: séquence \r\n)
                if (indiceCaractere > 0U) {
                    if ((bufferTrame[0] == 'T' || bufferTrame[0] == 't') && indiceCaractere == 1U) {
                        executerTousLesTestsUnitairesGbf(contexte->mutexPortSerie);
                    } else {
                        CommandeGbf_t commandeDecodee;
                        if (decoderTrameSerie(bufferTrame, &commandeDecodee)) {
                            xQueueSendToBack(contexte->queueCommandesGbf, (const void *)&commandeDecodee, portMAX_DELAY);
                        } else {
                            envoyerMessageSerieProtege(contexte->mutexPortSerie, "ERROR: Commande ou valeur hors limites !");
                        }
                    }
                }
                indiceCaractere = 0U;
            } else {
                indiceCaractere++;
                if (indiceCaractere >= (TAILLE_BUFFER_TRAME_SERIE - 1U)) {
                    // Protection contre le débordement de tampon (Charte §6 & §7)
                    indiceCaractere = 0U;
                    envoyerMessageSerieProtege(contexte->mutexPortSerie, "ERROR: Trame trop longue !");
                }
            }
        }
    }
}

/**************************************************************************
 * @brief Tâche FreeRTOS de scrutation des 3 boutons tactiles (BtnA, BtnB, BtnC)
 *        et de la sélection directe par toucher sur les cartes de l'écran.
 * 
 * @param pvParameters : Pointeur vers la structure ContexteRtosGbf_t
 */
void taskBoutonsIhm(void *pvParameters) {
    if (pvParameters == nullptr) {
        vTaskDelete(nullptr);
        return;
    }

    ContexteRtosGbf_t *contexte = (ContexteRtosGbf_t *)pvParameters;
    EtatIhm_t dernierEtatTactile = ETAT_IHM_CONSULTATION;

    for (;;) {
        M5.update();

        CommandeGbf_t commandeBouton;
        commandeBouton.valeurParametre = 0U;
        commandeBouton.depuisPortSerie = false;
        bool actionDetectee = false;
        uint32_t dureeVibrationMs = DUREE_VIBRATION_COURTE_MS;

        if (M5.BtnA.wasHold()) {
            commandeBouton.typeCommande = CMD_IHM_REINITIALISER;
            dernierEtatTactile = ETAT_IHM_CONSULTATION;
            dureeVibrationMs = DUREE_VIBRATION_LONGUE_MS;
            actionDetectee = true;
        } else if (M5.BtnA.wasClicked()) {
            commandeBouton.typeCommande = CMD_IHM_BOUTON_MENU;
            dernierEtatTactile = obtenirEtatIhmSuivant(dernierEtatTactile);
            actionDetectee = true;
        } else if (M5.BtnB.wasClicked()) {
            commandeBouton.typeCommande = CMD_IHM_BOUTON_MOINS;
            actionDetectee = true;
        } else if (M5.BtnC.wasClicked()) {
            commandeBouton.typeCommande = CMD_IHM_BOUTON_PLUS;
            actionDetectee = true;
        } else if (M5.Touch.getCount() > 0U) {
            const auto &detailToucher = M5.Touch.getDetail();
            if (detailToucher.wasPressed() && detailToucher.y < POS_Y_TOUCHES) {
                EtatIhm_t etatCible = ETAT_IHM_CONSULTATION;
                if (determinerEtatDepuisToucher(detailToucher.x, detailToucher.y, dernierEtatTactile, &etatCible)) {
                    commandeBouton.typeCommande = CMD_IHM_SELECTIONNER;
                    commandeBouton.valeurParametre = (uint16_t)etatCible;
                    dernierEtatTactile = etatCible;
                    actionDetectee = true;
                }
            }
        }

        if (actionDetectee) {
            xQueueSendToBack(contexte->queueCommandesGbf, (const void *)&commandeBouton, portMAX_DELAY);
            emettreVibrationBouton(INTENSITE_VIBRATION_COURTE, dureeVibrationMs);
        }

        vTaskDelay(pdMS_TO_TICKS(PERIODE_SCRUTATION_IHM_MS));
    }
}

/**************************************************************************
 * @brief Tâche FreeRTOS contrôleur central du GBF.
 *        Possède l'état courant `parametresCourants` en variable locale (sans globale),
 *        pilote la synthèse CNA/Timer et notifie la tâche d'affichage LCD.
 * 
 * @param pvParameters : Pointeur vers la structure ContexteRtosGbf_t
 */
void taskControleurGbf(void *pvParameters) {
    if (pvParameters == nullptr) {
        vTaskDelete(nullptr);
        return;
    }

    ContexteRtosGbf_t *contexte = (ContexteRtosGbf_t *)pvParameters;
    ParametresGbf_t parametresCourants;
    reinitialiserParametresParDefaut(&parametresCourants);

    CommandeGbf_t commandeRecue;
    char messageConfirmation[TAILLE_MAX_MESSAGE_REPONSE];

    for (;;) {
        if (xQueueReceive(contexte->queueCommandesGbf, (void *)&commandeRecue, portMAX_DELAY) == pdTRUE) {
            if (appliquerCommandeSurParametres(&parametresCourants, &commandeRecue)) {
                // Mise à jour du signal analogique sur GPIO26
                appliquerParametresSignal(&parametresCourants);

                // Demande de rafraîchissement de l'écran LCD via la file dédiée
                xQueueSendToBack(contexte->queueAffichageLcd, (const void *)&parametresCourants, portMAX_DELAY);

                // Envoi de la réponse de confirmation si la commande provient du port série
                if (commandeRecue.depuisPortSerie) {
                    if (formaterMessageConfirmation(&parametresCourants, messageConfirmation, sizeof(messageConfirmation))) {
                        envoyerMessageSerieProtege(contexte->mutexPortSerie, messageConfirmation);
                    }
                }
            } else if (commandeRecue.depuisPortSerie) {
                envoyerMessageSerieProtege(contexte->mutexPortSerie, "ERROR: Parametre hors bornes !");
            }
        }
    }
}

/**************************************************************************
 * @brief Tâche FreeRTOS dédiée au rafraîchissement de l'écran LCD 320x240 via M5Canvas.
 * 
 * @param pvParameters : Pointeur vers la structure ContexteRtosGbf_t
 */
void taskAffichageLcd(void *pvParameters) {
    if (pvParameters == nullptr) {
        vTaskDelete(nullptr);
        return;
    }

    ContexteRtosGbf_t *contexte = (ContexteRtosGbf_t *)pvParameters;
    M5Canvas canvasEcran(&M5.Display);

    if (!initialiserAffichage(&canvasEcran)) {
        envoyerMessageSerieProtege(contexte->mutexPortSerie, "ERREUR CRITIQUE : Echec allocation M5Canvas !");
        vTaskDelete(nullptr);
        return;
    }

    ParametresGbf_t parametresAAfficher;

    for (;;) {
        if (xQueueReceive(contexte->queueAffichageLcd, (void *)&parametresAAfficher, portMAX_DELAY) == pdTRUE) {
            afficherEcranCompletGbf(&canvasEcran, &parametresAAfficher);
        }
    }
}
