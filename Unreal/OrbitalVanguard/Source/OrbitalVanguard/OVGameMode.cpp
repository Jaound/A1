#include "OVGameMode.h"
#include "OVController.h"
#include "OVHUD.h"
AOVGameMode::AOVGameMode() { PlayerControllerClass=AOVController::StaticClass(); HUDClass=AOVHUD::StaticClass(); DefaultPawnClass=nullptr; }
