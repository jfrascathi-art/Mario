#pragma once
#include "Level.h"
#include "Level0.h"
#include "Level1.h"
#include "Level2.h"
#include "Level3.h"
#include "Level4.h"

// ─────────────────────────────────────────────────────────────────────────────
// LevelManager.h  —  Fabrique (factory) de niveaux
//
// RÔLE :
//   Donner un numéro de niveau → retourner l'objet Level correspondant.
//   Le code appelant n'a pas besoin de savoir quelle classe concrète est utilisée.
//
// UTILISATION dans le .ino :
//
//   // Au lieu de :
//   loadLevelPlatforms(scr);
//   loadLevelGoombas(scr);
//
//   // On fait :
//   Level* lvl = LevelManager::create(currentLevel);
//   if (lvl) {
//       lvl->load(scr);
//       delete lvl;
//   }
//
// AJOUTER UN NIVEAU :
//   1. Créer Level5.h qui hérite de Level
//   2. #include "Level5.h" ici
//   3. Ajouter un case 5: dans le switch ci-dessous
//   C'est tout — le reste du code ne change pas.
// ─────────────────────────────────────────────────────────────────────────────

class LevelManager
{
public:
    // Retourne un pointeur vers un nouveau Level (alloué sur le tas).
    // L'appelant est responsable de le supprimer avec delete.
    // Retourne nullptr si l'index est inconnu.
    static Level *create(int levelIndex)
    {
        switch (levelIndex)
        {
        case 0:
            return new Level0();
        case 1:
            return new Level1();
        case 2:
            return new Level2();
        case 3:
            return new Level3();
        case 4:
            return new Level4();
        default:
            return nullptr;
        }
    }
};
