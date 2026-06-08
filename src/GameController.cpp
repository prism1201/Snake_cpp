#include "GameController.h"

GameController::GameController()
    : currentState(GameState::Ready)
{
}

void GameController::start()
{
    currentState = GameState::Running;
}

void GameController::pause()
{
    if (currentState == GameState::Running) {
        currentState = GameState::Paused;
    }
}

void GameController::resume()
{
    if (currentState == GameState::Paused) {
        currentState = GameState::Running;
    }
}

void GameController::gameOver()
{
    currentState = GameState::GameOver;
}

void GameController::reset()
{
    currentState = GameState::Ready;
}

GameState GameController::state() const
{
    return currentState;
}

bool GameController::isRunning() const
{
    return currentState == GameState::Running;
}

bool GameController::isPaused() const
{
    return currentState == GameState::Paused;
}

bool GameController::isGameOver() const
{
    return currentState == GameState::GameOver;
}
