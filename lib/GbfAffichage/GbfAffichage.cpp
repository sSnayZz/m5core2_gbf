/**
 * @file GbfAffichage.cpp
 * @brief Implémentation du rendu graphique anti-scintillement (M5Canvas) de l'IHM du GBF.
 * @details Respecte la charte de qualité du code INFO5 :
 *          - Fonctions courtes à responsabilité unique.
 *          - Factorisation du dessin des 4 cartes de paramètres (principe DRY).
 *          - Aucune valeur numérique magique (toutes les constantes sont dans GbfAffichage.h et GbfMiseEnPage.h).
 */

#include "GbfAffichage.h"
#include "GbfIhm.h"
#include "GbfSignal.h"
#include <stdio.h>

#define DECALAGE_X_TITRE_BANDEAU        8
#define TAILLE_TEXTE_BANDEAU_MAX        40U
#define DECALAGE_X_BADGE_DROITE         (LARGEUR_ECRAN_PX - LARGEUR_BADGE_PX - MARGE_ECRAN_PX)
#define POS_Y_BADGE_BANDEAU             3
#define RAYON_BADGE_PX                  4
#define RAYON_TOUCHE_PX                 5
#define HAUTEUR_INDICATEUR_SELECTION_PX 3
#define LARGEUR_AXE_TENSION_PX          14
#define TENSION_GRADUATION_MAX_V        3
#define FACTEUR_ECHELLE_MILLIVOLTS      1000
#define DECALAGE_TEXTE_LEGENDE_Y        4

/**
 * @brief Structure interne décrivant le contenu textuel et la jauge d'une carte de paramètre.
 */
typedef struct {
    const char *titre;                      /**< Titre court en haut à gauche de la carte */
    char valeurPrincipale[TAILLE_LIBELLE_MAX]; /**< Valeur principale en gras */
    char infoSecondaire[TAILLE_LIBELLE_MAX];   /**< Information complémentaire à droite */
    uint16_t numProgression;                /**< Numérateur de la barre de progression */
    uint16_t denProgression;                /**< Dénominateur de la barre de progression */
    bool afficherMiniatureForme;            /**< true pour dessiner la miniature d'onde (carte Forme) */
} DescripteurCarte_t;

/**************************************************************************
 * @brief Alloue le sprite plein écran (en PSRAM 16 bits, ou en RAM 8 bits en secours).
 *
 * @param canvas : Pointeur vers le sprite M5Canvas à initialiser
 * @return true si le sprite a été alloué avec succès, false sinon
 */
bool initialiserAffichage(M5Canvas *canvas) {
    if (canvas == nullptr) {
        return false;
    }

    canvas->setPsram(true);
    canvas->setColorDepth(PROFONDEUR_COULEUR_SPRITE);
    void *tampon = canvas->createSprite(LARGEUR_ECRAN_PX, HAUTEUR_ECRAN_PX);

    if (tampon == nullptr) {
        canvas->setPsram(false);
        canvas->setColorDepth(PROFONDEUR_COULEUR_SECOURS);
        tampon = canvas->createSprite(LARGEUR_ECRAN_PX, HAUTEUR_ECRAN_PX);
    }

    return (tampon != nullptr);
}

/**************************************************************************
 * @brief Dessine le bandeau supérieur (titre, état du menu et badge de santé du signal).
 *
 * @param canvas : Sprite de dessin
 * @param parametres : Paramètres courants du GBF
 */
static void dessinerBandeauSuperieur(M5Canvas *canvas, const ParametresGbf_t *parametres) {
    canvas->fillRect(0, 0, LARGEUR_ECRAN_PX, HAUTEUR_BANDEAU_PX, COULEUR_BANDEAU);

    // Titre et mode actif à gauche
    char texteBandeau[TAILLE_TEXTE_BANDEAU_MAX];
    snprintf(texteBandeau, sizeof(texteBandeau), "GBF GPIO26 | %s",
             obtenirNomEtatIhm(parametres->etatIhm, false));

    canvas->setFont(POLICE_PETITE);
    canvas->setTextDatum(textdatum_t::middle_left);
    canvas->setTextColor(COULEUR_TEXTE_PRINCIPAL);
    canvas->drawString(texteBandeau, DECALAGE_X_TITRE_BANDEAU, HAUTEUR_BANDEAU_PX / 2);

    // Badge d'état de sortie à droite (SORTIE OK ou ECRETAGE !)
    bool ecretage = estSignalEcrete(parametres);
    uint16_t couleurBadge = ecretage ? COULEUR_ALERTE : COULEUR_SORTIE_OK;
    const char *texteBadge = ecretage ? "ECRETAGE !" : "SORTIE OK";

    canvas->fillRoundRect(DECALAGE_X_BADGE_DROITE, POS_Y_BADGE_BANDEAU,
                          LARGEUR_BADGE_PX, HAUTEUR_BADGE_PX, RAYON_BADGE_PX, couleurBadge);
    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor(COULEUR_TEXTE_PRINCIPAL);
    canvas->drawString(texteBadge, DECALAGE_X_BADGE_DROITE + (LARGEUR_BADGE_PX / 2),
                       POS_Y_BADGE_BANDEAU + (HAUTEUR_BADGE_PX / 2));
}

/**************************************************************************
 * @brief Dessine une miniature de la forme d'onde active à droite de la carte Forme.
 *
 * @param canvas : Sprite de dessin
 * @param formeSignal : Forme d'onde à représenter
 * @param posX : Abscisse du coin supérieur gauche de la miniature
 * @param posY : Ordonnée du coin supérieur gauche de la miniature
 * @param couleurTrait : Couleur du tracé
 */
static void dessinerMiniatureForme(M5Canvas *canvas, FormeSignal_t formeSignal,
                                   int32_t posX, int32_t posY, uint16_t couleurTrait) {
    int32_t milieuY = posY + (HAUTEUR_MINIATURE_FORME_PX / 2);
    int32_t demiAmplitudeY = (HAUTEUR_MINIATURE_FORME_PX / 2) - 2;

    int32_t precX = posX;
    float u0 = calculerValeurNormalisee(formeSignal, 0U, (uint8_t)LARGEUR_MINIATURE_FORME_PX);
    int32_t precY = milieuY - (int32_t)(u0 * (float)demiAmplitudeY);

    for (uint8_t i = 1U; i < (uint8_t)LARGEUR_MINIATURE_FORME_PX; i++) {
        float u = calculerValeurNormalisee(formeSignal, i, (uint8_t)LARGEUR_MINIATURE_FORME_PX);
        int32_t courX = posX + (int32_t)i;
        int32_t courY = milieuY - (int32_t)(u * (float)demiAmplitudeY);
        canvas->drawLine(precX, precY, courX, courY, couleurTrait);
        precX = courX;
        precY = courY;
    }
}

/**************************************************************************
 * @brief Dessine la jauge de progression en bas d'une carte de paramètre.
 *
 * @param canvas : Sprite de dessin
 * @param posX : Abscisse de la carte
 * @param posY : Ordonnée de la carte
 * @param descripteur : Descripteur contenant la progression
 * @param estSelectionnee : true si la carte est sélectionnée
 */
static void dessinerJaugeCarte(M5Canvas *canvas, int32_t posX, int32_t posY,
                               const DescripteurCarte_t *descripteur, bool estSelectionnee) {
    int32_t jaugeX = posX + MARGE_INTERNE_CARTE_PX;
    int32_t jaugeY = posY + HAUTEUR_CARTE_PX - DECALAGE_JAUGE_BAS_PX;
    int32_t largeurTotale = LARGEUR_CARTE_PX - (2 * MARGE_INTERNE_CARTE_PX);

    uint16_t couleurPiste = estSelectionnee ? COULEUR_PISTE_SELECTION : COULEUR_GRILLE;
    uint16_t couleurActive = estSelectionnee ? COULEUR_TEXTE_SELECTION : COULEUR_ACCENT;

    if (descripteur->afficherMiniatureForme) {
        // Jauge segmentée en 4 crans pour les 4 formes d'ondes
        int32_t largeurSegment = (largeurTotale - ((int32_t)(NB_FORMES_SIGNAL - 1U) * ESPACE_SEGMENTS_JAUGE_PX))
                                 / (int32_t)NB_FORMES_SIGNAL;
        for (uint8_t s = 0U; s < NB_FORMES_SIGNAL; s++) {
            int32_t segX = jaugeX + (int32_t)s * (largeurSegment + ESPACE_SEGMENTS_JAUGE_PX);
            uint16_t couleurSeg = (s == descripteur->numProgression) ? couleurActive : couleurPiste;
            canvas->fillRect(segX, jaugeY, largeurSegment, HAUTEUR_JAUGE_PX, couleurSeg);
        }
    } else if (descripteur->denProgression > 0U) {
        canvas->fillRect(jaugeX, jaugeY, largeurTotale, HAUTEUR_JAUGE_PX, couleurPiste);
        int32_t largeurRemplie = ((int32_t)descripteur->numProgression * largeurTotale)
                                 / (int32_t)descripteur->denProgression;
        if (largeurRemplie > largeurTotale) {
            largeurRemplie = largeurTotale;
        }
        if (largeurRemplie > 0) {
            canvas->fillRect(jaugeX, jaugeY, largeurRemplie, HAUTEUR_JAUGE_PX, couleurActive);
        }
    }
}

/**************************************************************************
 * @brief Dessine une carte de paramètre (fond arrondi, titre, valeur, info secondaire et jauge).
 *
 * @param canvas : Sprite de dessin
 * @param indiceCarte : Indice de la carte (0 à 3)
 * @param descripteur : Contenu à afficher dans la carte
 * @param formeSignal : Forme d'onde courante (utilisée pour la miniature de la carte 0)
 * @param estSelectionnee : true si cette carte est actuellement sélectionnée
 */
static void dessinerCarteParametre(M5Canvas *canvas, uint8_t indiceCarte,
                                   const DescripteurCarte_t *descripteur,
                                   FormeSignal_t formeSignal, bool estSelectionnee) {
    int32_t posY = POS_Y_PREMIERE_CARTE + (int32_t)indiceCarte * PAS_VERTICAL_CARTES_PX;
    uint16_t couleurFond = estSelectionnee ? COULEUR_SELECTION : COULEUR_PANNEAU;
    uint16_t couleurBordure = estSelectionnee ? COULEUR_TEXTE_PRINCIPAL : COULEUR_BORDURE;
    uint16_t couleurTitre = estSelectionnee ? COULEUR_TEXTE_SELECTION : COULEUR_TEXTE_SECONDAIRE;
    uint16_t couleurValeur = estSelectionnee ? COULEUR_TEXTE_SELECTION : COULEUR_TEXTE_PRINCIPAL;
    uint16_t couleurInfo = estSelectionnee ? COULEUR_TEXTE_SELECTION : COULEUR_ACCENT;

    canvas->fillRoundRect(POS_X_CARTES, posY, LARGEUR_CARTE_PX, HAUTEUR_CARTE_PX, RAYON_ARRONDI_PX, couleurFond);
    canvas->drawRoundRect(POS_X_CARTES, posY, LARGEUR_CARTE_PX, HAUTEUR_CARTE_PX, RAYON_ARRONDI_PX, couleurBordure);

    // Titre en haut à gauche
    canvas->setFont(POLICE_PETITE);
    canvas->setTextDatum(textdatum_t::top_left);
    canvas->setTextColor(couleurTitre);
    canvas->drawString(descripteur->titre, POS_X_CARTES + MARGE_INTERNE_CARTE_PX, posY + POS_Y_LIBELLE_CARTE);

    // Information secondaire ou miniature en haut à droite
    int32_t droiteX = POS_X_CARTES + LARGEUR_CARTE_PX - MARGE_INTERNE_CARTE_PX;
    if (descripteur->afficherMiniatureForme) {
        dessinerMiniatureForme(canvas, formeSignal,
                               droiteX - LARGEUR_MINIATURE_FORME_PX,
                               posY + POS_Y_LIBELLE_CARTE, couleurInfo);
    } else {
        canvas->setTextDatum(textdatum_t::top_right);
        canvas->setTextColor(couleurInfo);
        canvas->drawString(descripteur->infoSecondaire, droiteX, posY + POS_Y_LIBELLE_CARTE);
    }

    // Valeur principale en gras
    canvas->setFont(POLICE_VALEUR);
    canvas->setTextDatum(textdatum_t::middle_left);
    canvas->setTextColor(couleurValeur);
    canvas->drawString(descripteur->valeurPrincipale, POS_X_CARTES + MARGE_INTERNE_CARTE_PX, posY + POS_Y_VALEUR_CARTE);

    // Barre ou segments de progression en bas de la carte
    dessinerJaugeCarte(canvas, POS_X_CARTES, posY, descripteur, estSelectionnee);
}

/**************************************************************************
 * @brief Prépare et dessine les 4 cartes de paramètres (Forme, Fréquence, Amplitude, Offset).
 *
 * @param canvas : Sprite de dessin
 * @param parametres : Paramètres courants du GBF
 */
static void dessinerToutesLesCartes(M5Canvas *canvas, const ParametresGbf_t *parametres) {
    DescripteurCarte_t cartes[NB_PARAMETRES_REGLABLES];

    // Carte 0 : Forme d'onde
    cartes[0].titre = "FORME D'ONDE";
    snprintf(cartes[0].valeurPrincipale, sizeof(cartes[0].valeurPrincipale), "%s",
             obtenirNomFormeSignal(parametres->formeSignal));
    cartes[0].infoSecondaire[0] = '\0';
    cartes[0].numProgression = (uint16_t)parametres->formeSignal;
    cartes[0].denProgression = NB_FORMES_SIGNAL;
    cartes[0].afficherMiniatureForme = true;

    // Carte 1 : Fréquence et période
    cartes[1].titre = "FREQUENCE";
    snprintf(cartes[1].valeurPrincipale, sizeof(cartes[1].valeurPrincipale), "%u Hz", parametres->frequenceHz);
    formaterPeriode(parametres->frequenceHz, cartes[1].infoSecondaire, sizeof(cartes[1].infoSecondaire));
    cartes[1].numProgression = parametres->frequenceHz - FREQUENCE_MIN_HZ;
    cartes[1].denProgression = FREQUENCE_MAX_HZ - FREQUENCE_MIN_HZ;
    cartes[1].afficherMiniatureForme = false;

    // Carte 2 : Amplitude crête et crête-à-crête
    cartes[2].titre = "AMPLITUDE";
    snprintf(cartes[2].valeurPrincipale, sizeof(cartes[2].valeurPrincipale), "%.2f V",
             (float)parametres->amplitudeMv / FACTEUR_MV_PAR_VOLT);
    snprintf(cartes[2].infoSecondaire, sizeof(cartes[2].infoSecondaire), "%u mV (%.1fVpp)",
             parametres->amplitudeMv, (2.0f * (float)parametres->amplitudeMv) / FACTEUR_MV_PAR_VOLT);
    cartes[2].numProgression = parametres->amplitudeMv - AMPLITUDE_MIN_MV;
    cartes[2].denProgression = AMPLITUDE_MAX_MV - AMPLITUDE_MIN_MV;
    cartes[2].afficherMiniatureForme = false;

    // Carte 3 : Offset continu
    cartes[3].titre = "OFFSET DC";
    snprintf(cartes[3].valeurPrincipale, sizeof(cartes[3].valeurPrincipale), "%.2f V",
             (float)parametres->offsetMv / FACTEUR_MV_PAR_VOLT);
    snprintf(cartes[3].infoSecondaire, sizeof(cartes[3].infoSecondaire), "%u mV", parametres->offsetMv);
    cartes[3].numProgression = parametres->offsetMv - OFFSET_MIN_MV;
    cartes[3].denProgression = OFFSET_MAX_MV - OFFSET_MIN_MV;
    cartes[3].afficherMiniatureForme = false;

    for (uint8_t i = 0U; i < NB_PARAMETRES_REGLABLES; i++) {
        bool estSelectionnee = ((uint8_t)parametres->etatIhm == (i + 1U));
        dessinerCarteParametre(canvas, i, &cartes[i], parametres->formeSignal, estSelectionnee);
    }
}

/**************************************************************************
 * @brief Dessine l'oscilloscope temps réel (grille graduée, offset, 2 périodes et tensions Vmin/Vmax).
 *
 * @param canvas : Sprite de dessin
 * @param parametres : Paramètres courants du GBF
 */
static void dessinerOscilloscope(M5Canvas *canvas, const ParametresGbf_t *parametres) {
    canvas->fillRoundRect(POS_X_OSCILLO, POS_Y_OSCILLO, LARGEUR_OSCILLO_PX, HAUTEUR_OSCILLO_PX,
                          RAYON_ARRONDI_PX, COULEUR_PANNEAU);
    canvas->drawRoundRect(POS_X_OSCILLO, POS_Y_OSCILLO, LARGEUR_OSCILLO_PX, HAUTEUR_OSCILLO_PX,
                          RAYON_ARRONDI_PX, COULEUR_BORDURE);

    int32_t grapheX = POS_X_OSCILLO + MARGE_GAUCHE_OSCILLO_PX;
    int32_t grapheY = POS_Y_OSCILLO + MARGE_HAUT_OSCILLO_PX;
    int32_t largeurGraphe = LARGEUR_OSCILLO_PX - MARGE_GAUCHE_OSCILLO_PX - MARGE_DROITE_OSCILLO_PX;
    int32_t hauteurGraphe = HAUTEUR_OSCILLO_PX - MARGE_HAUT_OSCILLO_PX - HAUTEUR_LEGENDE_OSCILLO_PX;
    int32_t basGrapheY = grapheY + hauteurGraphe;

    canvas->fillRect(grapheX, grapheY, largeurGraphe, hauteurGraphe, COULEUR_FOND);
    canvas->drawRect(grapheX, grapheY, largeurGraphe, hauteurGraphe, COULEUR_GRILLE);

    // Graduations horizontales de tension (0 V, 1 V, 2 V, 3 V)
    canvas->setFont(POLICE_PETITE);
    canvas->setTextDatum(textdatum_t::middle_right);
    canvas->setTextColor(COULEUR_TEXTE_SECONDAIRE);

    for (int32_t volt = 0; volt <= TENSION_GRADUATION_MAX_V; volt++) {
        int32_t tensionMv = volt * FACTEUR_ECHELLE_MILLIVOLTS;
        int32_t ligneY = basGrapheY - (tensionMv * hauteurGraphe) / TENSION_DAC_MAX_MV;

        for (int32_t x = grapheX; x < grapheX + largeurGraphe; x += PAS_POINTILLES_PX) {
            canvas->drawPixel(x, ligneY, COULEUR_GRILLE);
        }

        char etiquetteVolt[8];
        snprintf(etiquetteVolt, sizeof(etiquetteVolt), "%ldV", (long)volt);
        canvas->drawString(etiquetteVolt, grapheX - 2, ligneY);
    }

    // Divisions temporelles verticales en pointillés
    for (int32_t div = 1; div < NB_DIVISIONS_TEMPS_OSCILLO; div++) {
        int32_t colX = grapheX + (div * largeurGraphe) / NB_DIVISIONS_TEMPS_OSCILLO;
        for (int32_t y = grapheY; y < basGrapheY; y += PAS_POINTILLES_PX) {
            canvas->drawPixel(colX, y, COULEUR_GRILLE);
        }
    }

    // Ligne horizontale d'offset continu en tirets verts
    int32_t offsetY = basGrapheY - ((int32_t)parametres->offsetMv * hauteurGraphe) / TENSION_DAC_MAX_MV;
    for (int32_t x = grapheX + 1; x < grapheX + largeurGraphe - 2; x += PAS_TIRETS_PX) {
        canvas->drawFastHLine(x, offsetY, PAS_TIRETS_PX / 2, COULEUR_OFFSET);
    }

    // Tracé de 2 périodes complètes du signal avec coloration rouge des zones écrêtées
    int32_t nbPixelsLargeur = largeurGraphe - 2;
    int32_t precX = grapheX + 1;
    int32_t precY = offsetY;

    for (int32_t px = 0; px < nbPixelsLargeur; px++) {
        uint8_t indiceOnde = (uint8_t)(((uint32_t)px * NB_PERIODES_OSCILLO * NB_ECHANTILLONS_PAR_PERIODE)
                                       / (uint32_t)nbPixelsLargeur) % NB_ECHANTILLONS_PAR_PERIODE;
        float u = calculerValeurNormalisee(parametres->formeSignal, indiceOnde, NB_ECHANTILLONS_PAR_PERIODE);
        int32_t tensionBruteMv = (int32_t)parametres->offsetMv + (int32_t)(u * (float)parametres->amplitudeMv);
        bool pointEcrete = (tensionBruteMv < TENSION_DAC_MIN_MV) || (tensionBruteMv > TENSION_DAC_MAX_MV);

        uint8_t codeDac = convertirMillivoltsEnCodeDac(tensionBruteMv);
        int32_t courX = grapheX + 1 + px;
        int32_t courY = basGrapheY - ((int32_t)codeDac * (hauteurGraphe - 2)) / (int32_t)VALEUR_DAC_MAX - 1;

        if (px > 0) {
            uint16_t couleurSegment = pointEcrete ? COULEUR_ALERTE : COULEUR_ACCENT;
            canvas->drawWideLine(precX, precY, courX, courY, EPAISSEUR_COURBE_PX, couleurSegment);
        }
        precX = courX;
        precY = courY;
    }

    // Légende inférieure affichant l'excursion réelle [Vmin .. Vmax] sur GPIO26
    uint16_t vMinMv = 0U;
    uint16_t vMaxMv = 0U;
    calculerTensionsExtremes(parametres, &vMinMv, &vMaxMv);

    char texteExcursion[TAILLE_LIBELLE_MAX];
    snprintf(texteExcursion, sizeof(texteExcursion), "%.2fV .. %.2fV",
             (float)vMinMv / FACTEUR_MV_PAR_VOLT, (float)vMaxMv / FACTEUR_MV_PAR_VOLT);

    canvas->setFont(POLICE_PETITE);
    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor(estSignalEcrete(parametres) ? COULEUR_ALERTE : COULEUR_TEXTE_SECONDAIRE);
    canvas->drawString(texteExcursion, POS_X_OSCILLO + (LARGEUR_OSCILLO_PX / 2),
                       basGrapheY + (HAUTEUR_LEGENDE_OSCILLO_PX / 2) + 1);
}

/**************************************************************************
 * @brief Dessine une touche logicielle en bas de l'écran (titre principal et sous-texte contextuel).
 *
 * @param canvas : Sprite de dessin
 * @param centreX : Abscisse du centre de la touche
 * @param textePrincipal : Libellé principal de la touche
 * @param sousTexte : Sous-libellé dynamique expliquant l'action immédiate
 * @param accentuer : true pour souligner la touche avec la couleur d'accent
 */
static void dessinerToucheLogicielle(M5Canvas *canvas, int32_t centreX,
                                     const char *textePrincipal, const char *sousTexte, bool accentuer) {
    int32_t coinX = centreX - (LARGEUR_TOUCHE_PX / 2);
    uint16_t couleurBord = accentuer ? COULEUR_SELECTION : COULEUR_BORDURE;

    canvas->fillRoundRect(coinX, POS_Y_TOUCHES, LARGEUR_TOUCHE_PX, HAUTEUR_TOUCHE_PX, RAYON_TOUCHE_PX, COULEUR_TOUCHE);
    canvas->drawRoundRect(coinX, POS_Y_TOUCHES, LARGEUR_TOUCHE_PX, HAUTEUR_TOUCHE_PX, RAYON_TOUCHE_PX, couleurBord);

    canvas->setFont(POLICE_NORMALE);
    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor(COULEUR_TEXTE_PRINCIPAL);
    canvas->drawString(textePrincipal, centreX, POS_Y_TOUCHES + POS_Y_TEXTE_TOUCHE);

    canvas->setFont(POLICE_PETITE);
    canvas->setTextColor(accentuer ? COULEUR_SELECTION : COULEUR_ACCENT);
    canvas->drawString(sousTexte, centreX, POS_Y_TOUCHES + POS_Y_SOUS_TEXTE_TOUCHE);
}

/**************************************************************************
 * @brief Dessine la barre des 3 touches logicielles en face des boutons BtnA, BtnB et BtnC.
 *
 * @param canvas : Sprite de dessin
 * @param parametres : Paramètres courants du GBF
 */
static void dessinerBarreTouches(M5Canvas *canvas, const ParametresGbf_t *parametres) {
    char sousTexteMenu[TAILLE_LIBELLE_MAX];
    char sousTexteMoins[TAILLE_LIBELLE_MAX];
    char sousTextePlus[TAILLE_LIBELLE_MAX];

    EtatIhm_t prochainEtat = obtenirEtatIhmSuivant(parametres->etatIhm);
    snprintf(sousTexteMenu, sizeof(sousTexteMenu), "-> %s (Hold:Rst)", obtenirNomEtatIhm(prochainEtat, true));

    formaterLibelleTouche(parametres, false, sousTexteMoins, sizeof(sousTexteMoins));
    formaterLibelleTouche(parametres, true, sousTextePlus, sizeof(sousTextePlus));

    bool enEdition = (parametres->etatIhm != ETAT_IHM_CONSULTATION);
    dessinerToucheLogicielle(canvas, POS_X_CENTRE_TOUCHE_A, "MENU", sousTexteMenu, enEdition);
    dessinerToucheLogicielle(canvas, POS_X_CENTRE_TOUCHE_B, "MOINS (-)", sousTexteMoins, false);
    dessinerToucheLogicielle(canvas, POS_X_CENTRE_TOUCHE_C, "PLUS (+)", sousTextePlus, false);
}

/**************************************************************************
 * @brief Dessine l'écran complet du GBF dans le sprite puis le transfère sur l'écran LCD.
 *
 * @param canvas : Pointeur vers le sprite initialisé par initialiserAffichage()
 * @param parametres : Pointeur vers les paramètres courants du GBF
 */
void afficherEcranCompletGbf(M5Canvas *canvas, const ParametresGbf_t *parametres) {
    if (canvas == nullptr || parametres == nullptr) {
        return;
    }

    canvas->fillSprite(COULEUR_FOND);

    dessinerBandeauSuperieur(canvas, parametres);
    dessinerToutesLesCartes(canvas, parametres);
    dessinerOscilloscope(canvas, parametres);
    dessinerBarreTouches(canvas, parametres);

    canvas->pushSprite(0, 0);
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
