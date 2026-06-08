#include "ScoreManager.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

ScoreManager::ScoreManager()
    : score(0), bestScore(0)
{
    loadHighScore();
}

void ScoreManager::reset()
{
    score = 0;
}

void ScoreManager::addScore(int value)
{
    score += value;

    if (score > bestScore) {
        bestScore = score;
    }
}

void ScoreManager::loadHighScore()
{
    QFile file("data/score.txt");

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        in >> bestScore;
    }
}

void ScoreManager::saveHighScore()
{
    QDir().mkpath("data");

    QFile file("data/score.txt");

    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << bestScore;
    }
}

int ScoreManager::currentScore() const
{
    return score;
}

int ScoreManager::highScore() const
{
    return bestScore;
}
