#ifndef GAMECONTROLLER_H
#define GAMECONTROLLER_H

enum class GameState {
    Ready,
    Running,
    Paused,
    GameOver
};

class GameController
{
public:
    GameController();

    void start();
    void pause();
    void resume();
    void gameOver();
    void reset();

    GameState state() const;
    bool isRunning() const;
    bool isPaused() const;
    bool isGameOver() const;

private:
    GameState currentState;
};

#endif
