// --quick (testing): straight to playing the beach the way tools/run_pcsx2.py's --quick does on the PS2 build, through the game
// controller's next state alone (no input): past the logos, the title's cutscene (New Game, as the main menu asks) and the
// intro movie, and no level title card once playing. Checked once a frame (Platform::Pads::Read)
#include "game/gamecontroller.h"
#include "native.h"

namespace
{
bool g_Quick = false;
}

namespace Native
{
void SetQuickStart(bool quick)
{
    g_Quick = quick;
}

void QuickStartFrame()
{
    GameController* controller = G_GameController;
    if (!g_Quick || controller == nullptr)
    {
        return;
    }

    switch (controller->State())
    {
    case GameController::StateVivendiLogo:
        controller->SetNextState(GameController::StateLoadingTitle);
        break;
    case GameController::StateTitle:
        controller->SetNextState(GameController::StateNewGame);
        break;
    case GameController::StateMovie:
        controller->SetNextState(GameController::StateStartingPlay);
        break;
    case GameController::StatePlaying:
        controller->hudDelay = 0;
        break;
    default:
        break;
    }
}
}
