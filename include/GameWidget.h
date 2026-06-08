#ifndef GAMEWIDGET_H
#define GAMEWIDGET_H

#include "Food.h"
#include "GameController.h"
#include "ScoreManager.h"
#include "Snake.h"

#include <QAudioOutput>
#include <QElapsedTimer>
#include <QMediaPlayer>
#include <QPixmap>
#include <QTimer>
#include <QUrl>
#include <QVector>
#include <QWidget>

class GameWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GameWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private slots:
    void onFrame();

private:
    enum class Difficulty { Easy, Normal, Hard };
    enum class FoodType { Normal, ScoreBoost, Revive, ClearObstacles, Slow, Super };

    void startGame();
    void resetGame();
    void resetCommonState();
    void stepGame();
    void spawnFood();
    void spawnFloating(const QString &text);
    void spawnFloatingAt(const QString &text, const QPointF &pos);
    void applyObstacleClear();
    void triggerShake();
    void playClick();
    void applyMutedVolume();
    void setDifficulty(Difficulty value);
    int baseTickInterval() const;
    int minimumTickInterval() const;
    int speedCapTickInterval() const;
    int obstacleCount() const;
    QString difficultyText() const;

    void drawBackground(QPainter &p);
    void drawMenu(QPainter &p);
    void drawSnake(QPainter &p);
    void drawSnakeHead(QPainter &p, const QPointF &pxPos);
    void drawFood(QPainter &p);
    void drawObstacles(QPainter &p);
    void drawScore(QPainter &p);
    void drawOverlay(QPainter &p);
    void drawFloatings(QPainter &p);
    void drawCountdown(QPainter &p);
    void drawPanel(QPainter &p, const QRectF &rect, const QColor &fill = QColor(10, 28, 16, 220));
    void drawHelpPanel(QPainter &p);

    void generateObstacles();
    void updateLayout();
    bool isObstacle(const QPoint &pt) const;
    QString assetPath(const QString &name) const;

    QTimer timer;
    QElapsedTimer elapsed;
    int frameAccum;
    int tickInterval;
    qreal animT;
    QPoint prevHeadGrid;
    QVector<QPoint> prevBodyGrid;
    bool hasPrevHead;

    Snake snake;
    Food food;
    ScoreManager scoreManager;
    GameController controller;
    QVector<QPoint> obstacles;
    QVector<QPoint> hiddenObstacles;
    QVector<QPoint> destroyedObstacles;
    QVector<int> destroyedObstacleTimers;
    QPixmap foodImg;
    FoodType foodType;
    Difficulty difficulty;
    int survivalMs;
    int countdownMs;
    int foodPulseMs;
    int obstacleClearMs;
    int invincibleMs;
    int slowMs;
    int superMs;
    int revives;
    bool muted;
    bool turboHeld;
    bool showHelp;
    int lastSuperWarn;

    struct Floating {
        QString text;
        QPointF pos;
        int ageMs;
    };
    QVector<Floating> floatings;
    int shakeLeft;
    QPointF shakeOff;

    QMediaPlayer bgMusic;
    QAudioOutput bgAudio;
    QMediaPlayer eatSfx;
    QAudioOutput eatAud;
    QMediaPlayer dieSfx;
    QAudioOutput dieAud;
    QMediaPlayer clickSfx;
    QAudioOutput clickAud;

    int cellSize;
    int rows;
    int cols;
    int topBarHeight;
    int offsetX;
    int offsetY;
};

#endif
