/**
 * @file GbfConfig.h
 * @brief Constantes globales, types énumérés et structures de données du projet GBF.
 * @details Respecte la charte de qualité du code INFO5 :
 *          - Toutes les constantes sont en MAJUSCULES_AVEC_UNDERSCORE.
 *          - Aucune valeur numérique magique n'est écrite en dur dans le code.
 *          - Les structures permettent de faire transiter l'état via les files FreeRTOS
 *            sans recourir à des variables globales d'état partagées.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// ============================================================================
// CONSTANTES DE COMMUNICATION SERIE
// ============================================================================
#define VITESSE_PORT_SERIE              115200U
#define TAILLE_BUFFER_TRAME_SERIE       40U
#define TAILLE_FILE_RECEPTION_SERIE     64U
#define TAILLE_MAX_MESSAGE_REPONSE      96U
#define DELAI_ATTENTE_FILE_SERIE_TICKS  10U

// ============================================================================
// CONSTANTES MATERIELLES (DAC & TIMER ESP32)
// ============================================================================
#define NUMERO_TIMER_MATERIEL           0U
#define DIVISEUR_HORLOGE_TIMER          8U
#define FREQUENCE_HORLOGE_TIMER_HZ      10000000UL
#define NB_ECHANTILLONS_PAR_PERIODE     100U
#define INDICE_MILIEU_PERIODE           50U

#define VALEUR_DAC_MIN                  0U
#define VALEUR_DAC_MAX                  255U
#define TENSION_DAC_MIN_MV              0
#define TENSION_DAC_MAX_MV              3300

// ============================================================================
// CONSTANTES DES PARAMETRES DU SIGNAL GBF
// ============================================================================
#define NB_FORMES_SIGNAL                4U

#define FREQUENCE_MIN_HZ                1U
#define FREQUENCE_MAX_HZ                1500U
#define FREQUENCE_DEFAUT_HZ             100U
#define SEUIL_FREQ_PAS_1_HZ             10U
#define SEUIL_FREQ_PAS_10_HZ            100U
#define PAS_FREQUENCE_FIN_HZ            1U
#define PAS_FREQUENCE_MOYEN_HZ          10U
#define PAS_FREQUENCE_LARGE_HZ          100U

#define AMPLITUDE_MIN_MV                100U
#define AMPLITUDE_MAX_MV                1600U
#define AMPLITUDE_DEFAUT_MV             1000U
#define PAS_AMPLITUDE_MV                100U

#define OFFSET_MIN_MV                   0U
#define OFFSET_MAX_MV                   3300U
#define OFFSET_DEFAUT_MV                1600U
#define PAS_OFFSET_MV                   100U

#define FACTEUR_MV_PAR_VOLT             1000.0f
#define DEUX_PI                         6.283185307f

// ============================================================================
// CONSTANTES FREERTOS (FILES, TACHES, PRIORITES, COEURS)
// ============================================================================
#define TAILLE_FILE_COMMANDES_GBF       8U
#define TAILLE_FILE_AFFICHAGE_LCD       4U

#define TAILLE_PILE_TACHE_OCTETS        8192U
#define COEUR_EXECUTION_APPLICATIF      1

#define PRIORITE_TACHE_AFFICHAGE_LCD    1U
#define PRIORITE_TACHE_BOUTONS_IHM      2U
#define PRIORITE_TACHE_TRAITE_TRAME     3U
#define PRIORITE_TACHE_CONTROLEUR_GBF   4U

#define PERIODE_SCRUTATION_IHM_MS       20U
#define INTENSITE_VIBRATION_COURTE      180U
#define DUREE_VIBRATION_COURTE_MS       40U
#define DUREE_VIBRATION_LONGUE_MS       150U

// ============================================================================
// CONSTANTES GENERALES DE L'IHM
// (la mise en page détaillée de l'écran est définie dans GbfAffichage.h)
// ============================================================================
#define LARGEUR_ECRAN_PX                320
#define HAUTEUR_ECRAN_PX                240
#define NB_ETATS_IHM                    5U
#define NB_PARAMETRES_REGLABLES         4U
#define TAILLE_LIBELLE_MAX              24U

// ============================================================================
// TYPES ENUMERES
// ============================================================================

/**
 * @brief Formes d'ondes disponibles sur le GBF.
 */
typedef enum {
    SIGNAL_SINUS = 0,       /**< Signal sinusoïdal */
    SIGNAL_CARRE = 1,       /**< Signal carré (rapport cyclique 50 %) */
    SIGNAL_TRIANGLE = 2,    /**< Signal triangulaire symétrique */
    SIGNAL_DENT_DE_SCIE = 3 /**< Signal en dent de scie (rampe montante) */
} FormeSignal_t;

/**
 * @brief États de la machine à états de l'IHM (navigation via le bouton Menu).
 */
typedef enum {
    ETAT_IHM_CONSULTATION = 0,      /**< Aucun paramètre en édition (vue globale) */
    ETAT_IHM_REGLAGE_FORME = 1,     /**< Réglage de la forme du signal */
    ETAT_IHM_REGLAGE_FREQUENCE = 2, /**< Réglage de la fréquence (1 Hz à 1500 Hz) */
    ETAT_IHM_REGLAGE_AMPLITUDE = 3, /**< Réglage de l'amplitude crête (mV) */
    ETAT_IHM_REGLAGE_OFFSET = 4     /**< Réglage de la tension d'offset (mV) */
} EtatIhm_t;

/**
 * @brief Types d'événements/commandes transmis au contrôleur GBF via la file FreeRTOS.
 */
typedef enum {
    CMD_IHM_BOUTON_MENU = 0,        /**< Appui court sur le bouton Menu (BtnA) */
    CMD_IHM_BOUTON_MOINS = 1,       /**< Appui court sur le bouton Moins (BtnB) */
    CMD_IHM_BOUTON_PLUS = 2,        /**< Appui court sur le bouton Plus (BtnC) */
    CMD_IHM_REINITIALISER = 3,      /**< Appui long sur BtnA ou commande série 'R' */
    CMD_SERIE_FIXER_FORME = 4,      /**< Fixe directement la forme d'onde */
    CMD_SERIE_FIXER_FREQUENCE = 5,  /**< Fixe directement la fréquence en Hz */
    CMD_SERIE_FIXER_AMPLITUDE = 6,  /**< Fixe directement l'amplitude en mV */
    CMD_SERIE_FIXER_OFFSET = 7,     /**< Fixe directement l'offset en mV */
    CMD_SERIE_DEMANDER_ETAT = 8,    /**< Demande l'envoi de l'état courant sur le port série */
    CMD_IHM_SELECTIONNER = 9        /**< Sélection directe d'un état IHM (toucher sur l'écran) */
} TypeCommandeGbf_t;

// ============================================================================
// STRUCTURES DE DONNEES
// ============================================================================

/**
 * @brief Structure regroupant l'ensemble des paramètres courants du GBF et de l'IHM.
 */
typedef struct {
    FormeSignal_t formeSignal;  /**< Forme d'onde active */
    uint16_t frequenceHz;       /**< Fréquence du signal en Hz [1..1500] */
    uint16_t amplitudeMv;       /**< Amplitude crête en mV [100..1600] */
    uint16_t offsetMv;          /**< Offset continu en mV [0..3300] */
    EtatIhm_t etatIhm;          /**< État courant de sélection sur l'écran LCD */
} ParametresGbf_t;

/**
 * @brief Message de commande envoyé par l'IHM boutons ou la tâche série vers le contrôleur.
 */
typedef struct {
    TypeCommandeGbf_t typeCommande; /**< Nature de la commande à exécuter */
    uint16_t valeurParametre;       /**< Valeur associée (pour les commandes directes série) */
    bool depuisPortSerie;           /**< Indique si une confirmation série doit être émise */
} CommandeGbf_t;

/**
 * @brief Contexte partagé passé en paramètre (`pvParameters`) aux tâches FreeRTOS.
 * @details Permet de respecter strictement l'interdiction des variables globales applicatives.
 */
typedef struct {
    QueueHandle_t queueReceptionSerie;  /**< File des octets bruts reçus sur l'UART */
    QueueHandle_t queueCommandesGbf;    /**< File des commandes vers le contrôleur GBF */
    QueueHandle_t queueAffichageLcd;    /**< File des états à afficher sur l'écran LCD */
    SemaphoreHandle_t mutexPortSerie;   /**< Mutex protégeant l'accès concurrent à Serial */
} ContexteRtosGbf_t;
