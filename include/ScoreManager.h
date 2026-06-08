#ifndef SCOREMANAGER_H
#define SCOREMANAGER_H

class ScoreManager
{
public:
    ScoreManager();

    void reset();
    void addScore(int value);
    void loadHighScore();
    void saveHighScore();

    int currentScore() const;
    int highScore() const;

private:
    int score;
    int bestScore;
};

#endif
