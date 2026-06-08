#include "GameWidget.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QtMath>

static QRectF cellRf(const QPointF &pt, int cs, int offX, int offY, int pad)
{
    return QRectF(offX + pt.x() * cs + pad, offY + pt.y() * cs + pad, cs - pad * 2, cs - pad * 2);
}

static QPointF dirVec(const QVector<QPoint> &body)
{
    if (body.size() < 2) return {1, 0};
    const QPoint d = body[0] - body[1];
    return QPointF(d.x(), d.y());
}

GameWidget::GameWidget(QWidget *parent)
    : QWidget(parent), frameAccum(0), tickInterval(120), animT(1.0), hasPrevHead(false),
      foodType(FoodType::Normal), difficulty(Difficulty::Normal), survivalMs(0), countdownMs(0),
      foodPulseMs(0), obstacleClearMs(0), invincibleMs(0), slowMs(0), superMs(0), revives(0), muted(false), turboHeld(false), showHelp(false), lastSuperWarn(-1), shakeLeft(0), shakeOff(0, 0),
      cellSize(24), rows(22), cols(30), topBarHeight(64), offsetX(0), offsetY(0)
{
    resize(cols * cellSize, rows * cellSize + topBarHeight);
    setMinimumSize(480, 360);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    foodImg.load(assetPath("Food.png"));

    bgMusic.setAudioOutput(&bgAudio);
    bgMusic.setSource(QUrl::fromLocalFile(assetPath("game_bgm.mp3")));
    bgMusic.setLoops(QMediaPlayer::Infinite);

    eatSfx.setAudioOutput(&eatAud);
    eatSfx.setSource(QUrl::fromLocalFile(assetPath("eat_sound.mp3")));

    dieSfx.setAudioOutput(&dieAud);
    dieSfx.setSource(QUrl::fromLocalFile(assetPath("death_sound.mp3")));

    clickSfx.setAudioOutput(&clickAud);
    clickSfx.setSource(QUrl::fromLocalFile(assetPath("click_sound.mp3")));
    applyMutedVolume();
    bgMusic.play();

    connect(&timer, &QTimer::timeout, this, &GameWidget::onFrame);
    timer.start(16);
    elapsed.start();
}

void GameWidget::resizeEvent(QResizeEvent *)
{
    updateLayout();
}

void GameWidget::updateLayout()
{
    topBarHeight = qMax(48, height() / 11);
    cellSize = qMax(10, qMin((width() - 4) / cols, (height() - topBarHeight - 4) / rows));
    offsetX = (width() - cols * cellSize) / 2;
    offsetY = topBarHeight + (height() - topBarHeight - rows * cellSize) / 2;
}

void GameWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (shakeLeft > 0) p.translate(shakeOff);

    drawBackground(p);

    if (controller.state() == GameState::Ready) {
        drawMenu(p);
        if (showHelp) drawHelpPanel(p);
        return;
    }

    drawObstacles(p);
    drawFood(p);
    drawSnake(p);
    drawScore(p);
    drawFloatings(p);
    drawCountdown(p);
    drawOverlay(p);
}

void GameWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_F11) {
        if (isFullScreen()) showNormal();
        else showFullScreen();
        return;
    }
    if (e->key() == Qt::Key_M) {
        muted = !muted;
        applyMutedVolume();
        update();
        return;
    }

    if (controller.state() == GameState::Ready) {
        if (e->key() == Qt::Key_1) setDifficulty(Difficulty::Easy);
        else if (e->key() == Qt::Key_2) setDifficulty(Difficulty::Normal);
        else if (e->key() == Qt::Key_3) setDifficulty(Difficulty::Hard);
        else if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space) startGame();
        else QWidget::keyPressEvent(e);
        update();
        return;
    }

    switch (e->key()) {
    case Qt::Key_Up:    case Qt::Key_W:   snake.setDirection(Snake::Up);    break;
    case Qt::Key_Down:  case Qt::Key_S:   snake.setDirection(Snake::Down);  break;
    case Qt::Key_Left:  case Qt::Key_A:   snake.setDirection(Snake::Left);  break;
    case Qt::Key_Right: case Qt::Key_D:   snake.setDirection(Snake::Right); break;
    case Qt::Key_Shift: turboHeld = true; break;
    case Qt::Key_Space:
        if (controller.isRunning()) controller.pause();
        else if (controller.isPaused()) controller.resume();
        playClick();
        break;
    case Qt::Key_R: resetGame(); break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (controller.isGameOver()) resetGame();
        break;
    default: QWidget::keyPressEvent(e); return;
    }
    update();
}

void GameWidget::keyReleaseEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Shift) {
        turboHeld = false;
    }
    QWidget::keyReleaseEvent(e);
}

void GameWidget::mousePressEvent(QMouseEvent *e)
{
    if (controller.state() != GameState::Ready) return;

    const int pw = qMin(width() * 2 / 3, 540);
    const qreal sc = qreal(pw) / 460.0;

    if (showHelp) {
        const int hpw = qMin(width() - 40, 520);
        const int hph = qMin(height() - 40, 560);
        const int px = (width() - hpw) / 2;
        const int py = (height() - hph) / 2;
        const QRect closeBtn(px + hpw / 2 - 40, py + hph - 38, 80, 28);
        if (closeBtn.contains(e->pos())) {
            showHelp = false;
            update();
        }
        return;
    }

    const int btnW = int(210 * sc);
    const int btnH = int(54 * sc);
    const QRect btn(width() / 2 - btnW / 2, height() / 2 - int(24 * sc), btnW, btnH);
    if (btn.contains(e->pos())) {
        startGame();
        return;
    }

    const int helpBtnW = int(210 * sc);
    const int helpBtnH = int(38 * sc);
    const QRect helpBtn(width() / 2 - helpBtnW / 2, height() / 2 + int(86 * sc), helpBtnW, helpBtnH);
    if (helpBtn.contains(e->pos())) {
        showHelp = true;
        update();
        return;
    }

    const int diffBw = int(120 * sc);
    const int diffGap = int(18 * sc);
    const int diffH = int(38 * sc);
    const int diffTotal = diffBw * 3 + diffGap * 2;
    const int diffX0 = width() / 2 - diffTotal / 2;
    const int diffY = height() / 2 + int(42 * sc);
    for (int i = 0; i < 3; ++i) {
        const QRect r(diffX0 + i * (diffBw + diffGap), diffY, diffBw, diffH);
        if (r.contains(e->pos())) {
            setDifficulty(static_cast<Difficulty>(i));
            update();
            return;
        }
    }
}

void GameWidget::onFrame()
{
    const int dt = elapsed.restart();
    if (dt <= 0) return;

    foodPulseMs = (foodPulseMs + dt) % 2000;

    if (controller.isRunning()) {
        if (countdownMs > 0) {
            countdownMs = qMax(0, countdownMs - dt);
            frameAccum = 0;
        } else {
            survivalMs += dt;
            if (obstacleClearMs > 0) {
                obstacleClearMs = qMax(0, obstacleClearMs - dt);
                if (obstacleClearMs == 0) hiddenObstacles.clear();
            }
            if (invincibleMs > 0) invincibleMs = qMax(0, invincibleMs - dt);
            if (slowMs > 0) {
                slowMs = qMax(0, slowMs - dt);
                if (slowMs == 0) {
                    const int base = baseTickInterval();
                    tickInterval = qMax(speedCapTickInterval(), base - (scoreManager.currentScore() / 50) * 10);
                }
            }
            if (superMs > 0) {
                if (superMs <= 2000) {
                    const int bucket = superMs / 700;
                    if (bucket != lastSuperWarn) {
                        lastSuperWarn = bucket;
                        spawnFloatingAt("无敌即将结束!", QPointF(width() / 2.0, topBarHeight + rows * cellSize * 0.4));
                    }
                }
                superMs = qMax(0, superMs - dt);
                if (superMs == 0) {
                    lastSuperWarn = -1;
                    const int base = baseTickInterval();
                    tickInterval = qMax(speedCapTickInterval(), base - (scoreManager.currentScore() / 50) * 10);
                }
            }
            for (int i = destroyedObstacleTimers.size() - 1; i >= 0; --i) {
                destroyedObstacleTimers[i] -= dt;
                if (destroyedObstacleTimers[i] <= 0) {
                    destroyedObstacles.removeAt(i);
                    destroyedObstacleTimers.removeAt(i);
                }
            }
            frameAccum += dt;

            const int stepInterval = turboHeld ? qMax(minimumTickInterval(), tickInterval * 2 / 3) : tickInterval;
            while (frameAccum >= stepInterval) {
                frameAccum -= stepInterval;
                stepGame();
                if (!controller.isRunning()) break;
            }
        }
    }

    animT = (controller.isRunning() && countdownMs == 0) ? qMin(1.0, qreal(frameAccum) / tickInterval) : 1.0;

    if (shakeLeft > 0) {
        shakeLeft--;
        const qreal r = 4.0 * shakeLeft / 10.0;
        auto &rng = *QRandomGenerator::global();
        const int range = qMax(1, static_cast<int>(r * 2 + 1));
        shakeOff = QPointF(rng.bounded(range) - r, rng.bounded(range) - r);
    } else {
        shakeOff = QPointF(0, 0);
    }

    for (int i = floatings.size() - 1; i >= 0; --i) {
        floatings[i].ageMs += dt;
        floatings[i].pos.ry() -= 0.6;
        if (floatings[i].ageMs > 900) floatings.removeAt(i);
    }

    update();
}

void GameWidget::stepGame()
{
    const QPoint nxt = snake.nextHead();

    if (nxt.x() < 0 || nxt.x() >= cols || nxt.y() < 0 || nxt.y() >= rows
        || snake.wouldHitSelf() || isObstacle(nxt)) {
        if (superMs > 0) {
            if (isObstacle(nxt)) {
                destroyedObstacles.append(nxt);
                destroyedObstacleTimers.append(15000);
                spawnFloatingAt("撞碎!", QPointF(offsetX + nxt.x() * cellSize + cellSize / 2.0,
                                                  offsetY + nxt.y() * cellSize));
            } else {
                static const Snake::Direction dirs[] = {Snake::Up, Snake::Down, Snake::Left, Snake::Right};
                for (auto d : dirs) {
                    Snake testSnake = snake;
                    testSnake.setDirection(d);
                    const QPoint testNxt = testSnake.nextHead();
                    if (testNxt.x() >= 0 && testNxt.x() < cols && testNxt.y() >= 0 && testNxt.y() < rows
                        && !testSnake.wouldHitSelf()) {
                        snake.setDirection(d);
                        spawnFloatingAt("转向", QPointF(width() / 2.0, topBarHeight + rows * cellSize / 3.0));
                        return;
                    }
                }
                return;
            }
        } else if (invincibleMs > 0) {
            static const Snake::Direction dirs[] = {Snake::Up, Snake::Down, Snake::Left, Snake::Right};
            for (auto d : dirs) {
                Snake testSnake = snake;
                testSnake.setDirection(d);
                const QPoint testNxt = testSnake.nextHead();
                if (testNxt.x() >= 0 && testNxt.x() < cols && testNxt.y() >= 0 && testNxt.y() < rows
                    && !isObstacle(testNxt) && !testSnake.wouldHitSelf()) {
                    snake.setDirection(d);
                    spawnFloatingAt("转向", QPointF(width() / 2.0, topBarHeight + rows * cellSize / 3.0));
                    return;
                }
            }
            return;
        } else {
            triggerShake();
            if (revives > 0) {
                --revives;
                invincibleMs = 2500;
                countdownMs = 1200;
                frameAccum = 0;
                spawnFloatingAt("复活成功", QPointF(width() / 2.0, topBarHeight + rows * cellSize / 2.0));
                return;
            }
            controller.gameOver();
            dieSfx.stop();
            dieSfx.play();
            scoreManager.saveHighScore();
            return;
        }
    }

    const bool ate = (nxt == food.position());
    prevBodyGrid = snake.getBody();
    prevHeadGrid = snake.head();
    hasPrevHead = true;
    snake.move(ate);

    if (ate) {
        const FoodType eatenType = foodType;
        int pts = 10;
        QString text = "+10";
        if (eatenType == FoodType::ScoreBoost) {
            pts = 50;
            text = "+50分";
        } else if (eatenType == FoodType::Revive) {
            revives = qMin(3, revives + 1);
            text = "复活 +1";
        } else if (eatenType == FoodType::ClearObstacles) {
            text = "清障 15 秒";
            applyObstacleClear();
        } else if (eatenType == FoodType::Slow) {
            text = "+10 冰冻减速";
        } else if (eatenType == FoodType::Super) {
            pts = 30;
            text = "+30 炫彩!";
        }

        scoreManager.addScore(pts);
        eatSfx.stop();
        eatSfx.play();
        spawnFloating(text);
        spawnFood();

        const int base = baseTickInterval();
        tickInterval = qMax(speedCapTickInterval(), base - (scoreManager.currentScore() / 50) * 10);
        if (eatenType == FoodType::Slow) slowMs = 10000;
        if (eatenType == FoodType::Super) {
            superMs = 8000;
        }
        if (slowMs > 0) tickInterval = qMin(base + 45, tickInterval + 40);
    }
}

void GameWidget::startGame()
{
    controller.start();
    resetCommonState();
    generateObstacles();
    spawnFood();
    countdownMs = 1500;
    update();
}

void GameWidget::resetGame()
{
    controller.reset();
    resetCommonState();
    update();
}

void GameWidget::resetCommonState()
{
    snake.reset();
    scoreManager.reset();
    frameAccum = 0;
    tickInterval = baseTickInterval();
    animT = 1.0;
    hasPrevHead = false;
    prevBodyGrid.clear();
    survivalMs = 0;
    countdownMs = 0;
    floatings.clear();
    hiddenObstacles.clear();
    destroyedObstacles.clear();
    destroyedObstacleTimers.clear();
    obstacleClearMs = 0;
    invincibleMs = 0;
    slowMs = 0;
    superMs = 0;
    revives = 0;
    turboHeld = false;
    lastSuperWarn = -1;
    shakeLeft = 0;
    playClick();
    bgMusic.play();
    elapsed.restart();
}

void GameWidget::spawnFood()
{
    do { food.generate(cols, rows, snake.getBody()); }
    while (obstacles.contains(food.position()) && !destroyedObstacles.contains(food.position()));

    const int roll = QRandomGenerator::global()->bounded(100);
    if (roll < 5) foodType = FoodType::Super;
    else if (roll < 15) foodType = FoodType::ScoreBoost;
    else if (roll < 23) foodType = FoodType::Revive;
    else if (roll < 33) foodType = FoodType::ClearObstacles;
    else if (roll < 43) foodType = FoodType::Slow;
    else foodType = FoodType::Normal;
}

void GameWidget::spawnFloating(const QString &text)
{
    const QPoint fp = food.position();
    spawnFloatingAt(text, QPointF(offsetX + fp.x() * cellSize + cellSize / 2.0, offsetY + fp.y() * cellSize));
}

void GameWidget::spawnFloatingAt(const QString &text, const QPointF &pos)
{
    Floating ft;
    ft.text = text;
    ft.pos = pos;
    ft.ageMs = 0;
    floatings.append(ft);
}

void GameWidget::applyObstacleClear()
{
    QVector<QPoint> visible;
    for (const QPoint &pt : obstacles) {
        if (!hiddenObstacles.contains(pt)) visible.append(pt);
    }

    const int removeCount = qMax(1, visible.size() / 2);
    for (int i = 0; i < removeCount && !visible.isEmpty(); ++i) {
        const int idx = QRandomGenerator::global()->bounded(visible.size());
        hiddenObstacles.append(visible.takeAt(idx));
    }
    obstacleClearMs = 15000;
}

void GameWidget::triggerShake()
{
    shakeLeft = 10;
}

void GameWidget::playClick()
{
    clickSfx.stop();
    clickSfx.play();
}

void GameWidget::applyMutedVolume()
{
    bgAudio.setVolume(muted ? 0.0 : 0.25);
    eatAud.setVolume(muted ? 0.0 : 0.55);
    dieAud.setVolume(muted ? 0.0 : 0.7);
    clickAud.setVolume(muted ? 0.0 : 0.45);
}

void GameWidget::setDifficulty(Difficulty value)
{
    difficulty = value;
    tickInterval = baseTickInterval();
    playClick();
}

int GameWidget::baseTickInterval() const
{
    if (difficulty == Difficulty::Easy) return 145;
    if (difficulty == Difficulty::Hard) return 95;
    return 120;
}

int GameWidget::minimumTickInterval() const
{
    if (difficulty == Difficulty::Easy) return 70;
    if (difficulty == Difficulty::Hard) return 42;
    return 52;
}

int GameWidget::speedCapTickInterval() const
{
    if (difficulty == Difficulty::Easy) return 90;
    if (difficulty == Difficulty::Hard) return 50;
    return 68;
}

int GameWidget::obstacleCount() const
{
    if (difficulty == Difficulty::Easy) return 10;
    if (difficulty == Difficulty::Hard) return 28;
    return 18;
}

QString GameWidget::difficultyText() const
{
    if (difficulty == Difficulty::Easy) return "简单";
    if (difficulty == Difficulty::Hard) return "困难";
    return "普通";
}

void GameWidget::drawPanel(QPainter &p, const QRectF &rect, const QColor &fill)
{
    p.setPen(QPen(QColor(110, 235, 125, 150), 1.6));
    p.setBrush(fill);
    p.drawRoundedRect(rect, 18, 18);
    p.setPen(QColor(255, 255, 255, 30));
    p.drawRoundedRect(rect.adjusted(4, 4, -4, -4), 14, 14);
}

void GameWidget::drawBackground(QPainter &p)
{
    QLinearGradient g(0, 0, width(), height());
    g.setColorAt(0.00, QColor(10, 20, 17));
    g.setColorAt(0.48, QColor(18, 54, 34));
    g.setColorAt(1.00, QColor(7, 18, 12));
    p.fillRect(rect(), g);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(90, 220, 120, 22));
    p.drawEllipse(QPointF(width() * 0.18, height() * 0.24), 120, 80);
    p.setBrush(QColor(40, 145, 230, 18));
    p.drawEllipse(QPointF(width() * 0.78, height() * 0.72), 140, 90);

    QLinearGradient bar(0, 0, width(), 0);
    bar.setColorAt(0, QColor(8, 36, 20, 245));
    bar.setColorAt(1, QColor(23, 83, 44, 245));
    p.fillRect(0, 0, width(), topBarHeight, bar);

    const int gw = cols * cellSize;
    const int gh = rows * cellSize;
    p.fillRect(offsetX, offsetY, gw, gh, QColor(8, 22, 12, 90));

    p.setPen(QColor(70, 115, 72, 55));
    for (int x = 0; x <= cols; ++x) {
        const int lx = offsetX + x * cellSize;
        p.drawLine(lx, offsetY, lx, offsetY + gh);
    }
    for (int y = 0; y <= rows; ++y) {
        const int ly = offsetY + y * cellSize;
        p.drawLine(offsetX, ly, offsetX + gw, ly);
    }

    p.setPen(QPen(QColor(95, 235, 110, 130), 2));
    p.drawRect(QRect(offsetX + 1, offsetY + 1, gw - 2, gh - 2));
}

void GameWidget::drawMenu(QPainter &p)
{
    p.fillRect(rect(), QColor(0, 0, 0, 140));

    const int pw = qMin(width() * 2 / 3, 540);
    const int ph = qMin(height() * 2 / 3, 500);
    const int px = (width() - pw) / 2;
    const int py = (height() - ph) / 2;
    const QRectF panel(px, py, pw, ph);
    drawPanel(p, panel, QColor(9, 30, 18, 225));
    const qreal sc = qreal(pw) / 460.0;

    const int titleFontSz = qMax(18, int(42 * sc));
    p.setPen(QColor(86, 255, 110));
    p.setFont(QFont("Microsoft YaHei", titleFontSz, QFont::Bold));
    p.drawText(QRectF(0, panel.top() + 8, width(), titleFontSz * 1.6), Qt::AlignCenter, "贪吃蛇大作战");

    const int descFontSz = qMax(9, int(11 * sc));
    p.setPen(QColor(190, 220, 190));
    p.setFont(QFont("Microsoft YaHei", descFontSz));
    p.drawText(QRectF(panel.left() + 16, panel.top() + titleFontSz * 1.6 + 16, panel.width() - 32, 24), Qt::AlignCenter,
               "选择难度后开始，吃到特殊水果会触发额外效果");

    const int btnW = int(210 * sc);
    const int btnH = int(54 * sc);
    const int btnX = width() / 2 - btnW / 2;
    const int btnY = height() / 2 - int(24 * sc);
    const QRect btn(btnX, btnY, btnW, btnH);
    QLinearGradient bg(btn.topLeft(), btn.bottomRight());
    bg.setColorAt(0, QColor(62, 220, 90));
    bg.setColorAt(1, QColor(20, 135, 45));
    p.setPen(QPen(QColor(155, 255, 165), 2));
    p.setBrush(bg);
    p.drawRoundedRect(btn, int(16 * sc), int(16 * sc));
    p.setPen(Qt::white);
    const int startFontSz = qMax(12, int(17 * sc));
    p.setFont(QFont("Microsoft YaHei", startFontSz, QFont::Bold));
    p.drawText(btn, Qt::AlignCenter, "开始游戏");

    const int diffBw = int(120 * sc);
    const int diffGap = int(18 * sc);
    const int diffH = int(38 * sc);
    const int diffTotal = diffBw * 3 + diffGap * 2;
    const int diffX0 = width() / 2 - diffTotal / 2;
    const int diffY = height() / 2 + int(42 * sc);
    const QStringList labels = {"1 简单", "2 普通", "3 困难"};
    for (int i = 0; i < 3; ++i) {
        const QRect r(diffX0 + i * (diffBw + diffGap), diffY, diffBw, diffH);
        const bool selected = static_cast<int>(difficulty) == i;
        p.setPen(QPen(selected ? QColor(255, 230, 80) : QColor(105, 180, 115), selected ? 2.2 : 1.2));
        p.setBrush(selected ? QColor(78, 104, 33, 230) : QColor(18, 58, 28, 210));
        p.drawRoundedRect(r, int(12 * sc), int(12 * sc));
        p.setPen(selected ? QColor(255, 245, 165) : QColor(205, 225, 205));
        const int diffFontSz = qMax(9, int(11 * sc));
        p.setFont(QFont("Microsoft YaHei", diffFontSz, selected ? QFont::Bold : QFont::Normal));
        p.drawText(r, Qt::AlignCenter, labels[i]);
    }

    const int helpBtnW = int(210 * sc);
    const int helpBtnH = int(38 * sc);
    const int helpBtnY = height() / 2 + int(86 * sc);
    const QRect helpBtn(width() / 2 - helpBtnW / 2, helpBtnY, helpBtnW, helpBtnH);
    p.setPen(QPen(QColor(180, 200, 255), 1.4));
    p.setBrush(QColor(28, 48, 78, 210));
    p.drawRoundedRect(helpBtn, int(12 * sc), int(12 * sc));
    p.setPen(QColor(200, 220, 255));
    const int helpFontSz = qMax(9, int(11 * sc));
    p.setFont(QFont("Microsoft YaHei", helpFontSz, QFont::Bold));
    p.drawText(helpBtn, Qt::AlignCenter, "玩法介绍");

    const int tipFontSz = qMax(8, int(10 * sc));
    p.setPen(QColor(176, 204, 176));
    p.setFont(QFont("Microsoft YaHei", tipFontSz));
    p.drawText(QRect(0, height() / 2 + int(136 * sc), width(), 22), Qt::AlignCenter,
               "按 F11 切换全屏    M 键静音    回车/点击开始游戏");
}

void GameWidget::drawSnake(QPainter &p)
{
    const QVector<QPoint> &b = snake.getBody();
    if (b.isEmpty()) return;

    auto segmentPos = [&](int i) {
        QPointF pos(b[i].x(), b[i].y());
        if (hasPrevHead && animT < 1.0 && i < prevBodyGrid.size()) {
            const QPoint old = prevBodyGrid[i];
            pos = QPointF(old.x() + (b[i].x() - old.x()) * animT,
                          old.y() + (b[i].y() - old.y()) * animT);
        }
        return pos;
    };

    const bool showFreeze = slowMs > 0 && (slowMs >= 2000 || (slowMs / 250) % 2 == 1);

    for (int i = b.size() - 1; i > 0; --i) {
        const QRectF r = cellRf(segmentPos(i), cellSize, offsetX, offsetY,2);

        if (superMs > 0) {
            const int hue = (superMs * 3 + i * 28) % 360;
            QLinearGradient g(r.topLeft(), r.bottomRight());
            g.setColorAt(0, QColor::fromHsv(hue, 220, 220));
            g.setColorAt(1, QColor::fromHsv((hue + 30) % 360, 230, 160));
            p.setPen(QPen(QColor::fromHsv(hue, 200, 120), 1.2));
            p.setBrush(g);
        } else {
            const int bright = 145 + (i % 2) * 25;
            QLinearGradient g(r.topLeft(), r.bottomRight());
            g.setColorAt(0, QColor(35, bright + 35, 80));
            g.setColorAt(1, QColor(15, bright - 30, 55));
            p.setPen(QPen(QColor(12, 70, 28), 1));
            p.setBrush(g);
        }
        p.drawRoundedRect(r, 7, 7);

        p.setPen(Qt::NoPen);
        if (superMs > 0) {
            const int hue = (superMs * 3 + i * 28) % 360;
            p.setBrush(QColor::fromHsv(hue, 150, 255, 80));
        } else {
            p.setBrush(QColor(145, 255, 165, 70));
        }
        p.drawRoundedRect(r.adjusted(4, 4, -4, -r.height() / 2), 4, 4);

        if (showFreeze) {
            p.setPen(QPen(QColor(100, 180, 255, 150), 1.5));
            p.setBrush(QColor(60, 140, 240, 70));
            p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 6, 6);
        }
    }

    if (invincibleMs > 0 && (invincibleMs / 200) % 2 == 0)
        p.setOpacity(0.35);

    drawSnakeHead(p, segmentPos(0));

    if (showFreeze) {
        const QRectF hr = cellRf(segmentPos(0), cellSize, offsetX, offsetY, 1).adjusted(-1, -1, 1, 1);
        p.setPen(QPen(QColor(100, 180, 255, 150), 1.5));
        p.setBrush(QColor(60, 140, 240, 70));
        p.drawRoundedRect(hr.adjusted(2, 2, -2, -2), 6, 6);
    }

    if (invincibleMs > 0)
        p.setOpacity(1.0);
}

void GameWidget::drawSnakeHead(QPainter &p, const QPointF &pxPos)
{
    const QRectF r = cellRf(pxPos, cellSize, offsetX, offsetY, 1).adjusted(-1, -1, 1, 1);
    const QPointF c = r.center();
    const QPointF dir = dirVec(snake.getBody());
    const QPointF side(-dir.y(), dir.x());

    if (superMs > 0) {
        const int hue = (superMs * 3) % 360;
        QLinearGradient g(r.topLeft(), r.bottomRight());
        g.setColorAt(0, QColor::fromHsv(hue, 230, 255));
        g.setColorAt(1, QColor::fromHsv((hue + 40) % 360, 240, 180));
        p.setPen(QPen(QColor::fromHsv(hue, 200, 120), 1.8));
        p.setBrush(g);
    } else {
        QLinearGradient g(r.topLeft(), r.bottomRight());
        g.setColorAt(0, QColor(120, 255, 105));
        g.setColorAt(1, QColor(22, 155, 48));
        p.setPen(QPen(QColor(8, 82, 28), 1.5));
        p.setBrush(g);
    }
    p.drawRoundedRect(r, 8, 8);

    p.setPen(Qt::NoPen);
    if (superMs > 0) {
        const int hue = (superMs * 3) % 360;
        p.setBrush(QColor::fromHsv(hue, 150, 255, 100));
    } else {
        p.setBrush(QColor(185, 255, 170, 90));
    }
    p.drawRoundedRect(r.adjusted(4, 4, -4, -r.height() / 2), 5, 5);

    const QPointF leftEye = c + dir * 4 - side * 5;
    const QPointF rightEye = c + dir * 4 + side * 5;
    p.setBrush(Qt::white);
    p.drawEllipse(leftEye, 3.4, 3.4);
    p.drawEllipse(rightEye, 3.4, 3.4);

    p.setBrush(QColor(20, 35, 18));
    p.drawEllipse(leftEye + dir * 1.2, 1.5, 1.5);
    p.drawEllipse(rightEye + dir * 1.2, 1.5, 1.5);
}

void GameWidget::drawFood(QPainter &p)
{
    const QPoint pt = food.position();
    const int x = offsetX + pt.x() * cellSize;
    const int y = offsetY + pt.y() * cellSize;
    const qreal pulse = 1.0 + 0.08 * qSin(foodPulseMs / 1000.0 * M_PI * 2.0);
    const int drawSize = qBound(18, int(cellSize * pulse), cellSize + 5);
    const QRect target(x + (cellSize - drawSize) / 2, y + (cellSize - drawSize) / 2, drawSize, drawSize);
    const QPointF c = target.center();

    if (foodType == FoodType::Normal && !foodImg.isNull()) {
        QPixmap sc = foodImg.scaled(target.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        p.drawPixmap(target.center().x() - sc.width() / 2, target.center().y() - sc.height() / 2, sc);
        return;
    }

    if (foodType == FoodType::ScoreBoost) {
        QPainterPath star;
        for (int i = 0; i < 10; ++i) {
            const qreal radius = (i % 2 == 0) ? drawSize * 0.48 : drawSize * 0.22;
            const qreal a = -M_PI / 2 + i * M_PI / 5;
            const QPointF pt(c.x() + qCos(a) * radius, c.y() + qSin(a) * radius);
            if (i == 0) star.moveTo(pt); else star.lineTo(pt);
        }
        star.closeSubpath();
        QRadialGradient rg(c, drawSize * 0.55);
        rg.setColorAt(0, QColor(255, 252, 155));
        rg.setColorAt(1, QColor(230, 145, 18));
        p.setPen(QPen(QColor(145, 88, 0), 1.2));
        p.setBrush(rg);
        p.drawPath(star);
        p.setPen(QColor(125, 75, 0));
        p.setFont(QFont("Microsoft YaHei", 6, QFont::Bold));
        p.drawText(target, Qt::AlignCenter, "50");
        return;
    }

    if (foodType == FoodType::Revive) {
        QPainterPath heart;
        heart.moveTo(c.x(), c.y() + drawSize * 0.35);
        heart.cubicTo(c.x() - drawSize * 0.55, c.y() - drawSize * 0.02,
                      c.x() - drawSize * 0.42, c.y() - drawSize * 0.48,
                      c.x(), c.y() - drawSize * 0.20);
        heart.cubicTo(c.x() + drawSize * 0.42, c.y() - drawSize * 0.48,
                      c.x() + drawSize * 0.55, c.y() - drawSize * 0.02,
                      c.x(), c.y() + drawSize * 0.35);
        QLinearGradient hg(target.topLeft(), target.bottomRight());
        hg.setColorAt(0, QColor(255, 110, 135));
        hg.setColorAt(1, QColor(196, 18, 58));
        p.setPen(QPen(QColor(120, 12, 38), 1.1));
        p.setBrush(hg);
        p.drawPath(heart);
        p.setPen(QPen(QColor(255, 230, 235, 180), 1.4));
        p.drawLine(c + QPointF(-4, -3), c + QPointF(-1, -6));
        return;
    }

    if (foodType == FoodType::ClearObstacles) {
        QRadialGradient orb(c, drawSize * 0.55);
        orb.setColorAt(0, QColor(235, 255, 255));
        orb.setColorAt(0.45, QColor(73, 220, 245));
        orb.setColorAt(1, QColor(80, 70, 210));
        p.setPen(QPen(QColor(180, 245, 255), 1.3));
        p.setBrush(orb);
        p.drawEllipse(target.adjusted(2, 2, -2, -2));
        p.setPen(QPen(QColor(255, 255, 255, 180), 1.2));
        p.drawArc(target.adjusted(4, 4, -4, -4), 30 * 16, 125 * 16);
        p.drawLine(c + QPointF(-8, 0), c + QPointF(8, 0));
        p.drawLine(c + QPointF(0, -8), c + QPointF(0, 8));
        return;
    }

    if (foodType == FoodType::Slow) {
        QRadialGradient berry(c, drawSize * 0.48);
        berry.setColorAt(0, QColor(220, 250, 255));
        berry.setColorAt(1, QColor(55, 125, 235));
        p.setPen(QPen(QColor(20, 70, 155), 1.1));
        p.setBrush(berry);
        p.drawEllipse(target.adjusted(3, 4, -3, -2));
        p.setPen(QPen(QColor(235, 255, 255), 1.2));
        for (int i = 0; i < 6; ++i) {
            const qreal a = i * M_PI / 3;
            p.drawLine(c, c + QPointF(qCos(a), qSin(a)) * (drawSize * 0.34));
        }
        p.setBrush(QColor(80, 210, 130));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(c.x() + 2, c.y() - drawSize * 0.55, 8, 5));
        return;
    }

    if (foodType == FoodType::Super) {
        QPainterPath diamond;
        diamond.moveTo(c.x(), c.y() - drawSize * 0.48);
        diamond.lineTo(c.x() + drawSize * 0.42, c.y());
        diamond.lineTo(c.x(), c.y() + drawSize * 0.48);
        diamond.lineTo(c.x() - drawSize * 0.42, c.y());
        diamond.closeSubpath();
        QConicalGradient cg(c, 90);
        cg.setColorAt(0.0, QColor(255, 80, 80));
        cg.setColorAt(0.2, QColor(255, 220, 50));
        cg.setColorAt(0.4, QColor(80, 255, 80));
        cg.setColorAt(0.6, QColor(50, 200, 255));
        cg.setColorAt(0.8, QColor(200, 80, 255));
        cg.setColorAt(1.0, QColor(255, 80, 80));
        p.setPen(QPen(QColor(255, 255, 200), 1.5));
        p.setBrush(cg);
        p.drawPath(diamond);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 120));
        p.drawEllipse(QRectF(c.x() - 3, c.y() - 3, 6, 6));
        return;
    }

    QPainterPath ap;
    ap.addEllipse(QRectF(c.x() - 8, c.y() - 6, 10, 13));
    ap.addEllipse(QRectF(c.x() - 2, c.y() - 6, 10, 13));
    p.setPen(QPen(QColor(125, 12, 22), 1));
    p.setBrush(QColor(225, 40, 50));
    p.drawPath(ap);
    p.setPen(QPen(QColor(85, 48, 20), 2));
    p.drawLine(QPointF(c.x(), c.y() - 8), QPointF(c.x() + 3, c.y() - 13));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(50, 175, 70));
    p.drawEllipse(QRectF(c.x() + 3, c.y() - 15, 8, 5));
}

void GameWidget::drawObstacles(QPainter &p)
{
    for (const QPoint &pt : obstacles) {
        if (hiddenObstacles.contains(pt) || destroyedObstacles.contains(pt)) continue;

        const int x = offsetX + pt.x() * cellSize + 2;
        const int y = offsetY + pt.y() * cellSize + 2;
        const int sz = cellSize - 4;

        QLinearGradient g(x, y, x + sz, y + sz);
        g.setColorAt(0, QColor(150, 150, 150));
        g.setColorAt(1, QColor(58, 58, 58));
        p.setPen(QPen(QColor(35, 35, 35), 1.5));
        p.setBrush(g);
        p.drawRoundedRect(x, y, sz, sz, 5, 5);
        p.setPen(QPen(QColor(210, 210, 210, 90), 1));
        p.drawLine(x + 4, y + 4, x + sz / 2, y + sz / 2);
        p.setPen(QPen(QColor(25, 25, 25, 80), 1));
        p.drawLine(x + sz - 4, y + 4, x + 5, y + sz - 5);
    }
}

void GameWidget::drawScore(QPainter &p)
{
    const int speedLvl = qMax(1, (baseTickInterval() - tickInterval) / 10 + 1);
    const int sec = survivalMs / 1000;
    const int clearSec = (obstacleClearMs + 999) / 1000;
    const int invSec = (invincibleMs + 999) / 1000;
    const int slSec = (slowMs + 999) / 1000;
    const int superSec = (superMs + 999) / 1000;
    const int fontSize = qMax(8, qMin(10, topBarHeight / 3));

    p.setPen(Qt::white);
    p.setFont(QFont("Microsoft YaHei", fontSize, QFont::Bold));
    p.drawText(12, topBarHeight / 2 - 4,
               QString("分数：%1  最高：%2  长度：%3  时间：%4秒  速度：%5  难度：%6")
                   .arg(scoreManager.currentScore())
                   .arg(scoreManager.highScore())
                   .arg(snake.getBody().size())
                   .arg(sec)
                   .arg(speedLvl)
                   .arg(difficultyText()));

    QString line2;
    if (revives > 0) line2 += QString("复活×%1  ").arg(revives);
    if (obstacleClearMs > 0) line2 += QString("清障%1秒  ").arg(clearSec);
    if (slowMs > 0) line2 += QString("缓速%1秒  ").arg(slSec);
    if (superMs > 0) line2 += QString("无敌%1秒  ").arg(superSec);
    if (invincibleMs > 0 && superMs == 0) line2 += QString("无敌%1秒  ").arg(invSec);
    if (muted) line2 += "静音";
    if (line2.isEmpty()) line2 = "音效开";
    p.drawText(12, topBarHeight / 2 + fontSize + 2, line2);
}

void GameWidget::drawOverlay(QPainter &p)
{
    if (!controller.isPaused() && !controller.isGameOver()) return;

    p.fillRect(rect(), QColor(0, 0, 0, 145));
    const QRectF panel(width() / 2 - 190, height() / 2 - 110, 380, controller.isGameOver() ? 220 : 120);
    drawPanel(p, panel, QColor(8, 25, 16, 235));

    p.setPen(Qt::white);
    p.setFont(QFont("Microsoft YaHei", 21, QFont::Bold));
    p.drawText(QRectF(panel.left(), panel.top() + 20, panel.width(), 42), Qt::AlignCenter,
               controller.isPaused() ? "暂停中" : "游戏结束");

    p.setFont(QFont("Microsoft YaHei", 11));
    if (controller.isPaused()) {
        p.setPen(QColor(205, 230, 205));
        p.drawText(QRectF(panel.left(), panel.top() + 70, panel.width(), 30), Qt::AlignCenter,
                   "按空格键继续，按 R 键返回菜单");
    } else {
        const QStringList stats = {
            QString("得分：%1    最高分：%2").arg(scoreManager.currentScore()).arg(scoreManager.highScore()),
            QString("长度：%1    生存时间：%2 秒").arg(snake.getBody().size()).arg(survivalMs / 1000),
            QString("难度：%1    剩余复活：%2").arg(difficultyText()).arg(revives),
            "按 R 键 / 回车键返回初始界面"
        };
        p.setPen(QColor(215, 235, 215));
        for (int i = 0; i < stats.size(); ++i)
            p.drawText(QRectF(panel.left(), panel.top() + 70 + i * 30, panel.width(), 26), Qt::AlignCenter, stats[i]);
    }
}

void GameWidget::drawFloatings(QPainter &p)
{
    for (const auto &ft : floatings) {
        const int alpha = qMax(0, 255 - ft.ageMs * 255 / 900);
        p.setPen(QColor(255, 220, 60, alpha));
        p.setFont(QFont("Microsoft YaHei", 14, QFont::Bold));
        p.drawText(QRectF(ft.pos.x() - 55, ft.pos.y() - 20, 110, 30), Qt::AlignCenter, ft.text);
    }
}

void GameWidget::drawCountdown(QPainter &p)
{
    if (!controller.isRunning() || countdownMs <= 0) return;

    QString text = "开始";
    if (countdownMs > 1000) text = "3";
    else if (countdownMs > 500) text = "2";
    else if (countdownMs > 150) text = "1";

    p.fillRect(rect(), QColor(0, 0, 0, 70));
    p.setPen(QColor(255, 245, 135));
    p.setFont(QFont("Microsoft YaHei", 54, QFont::Bold));
    p.drawText(rect(), Qt::AlignCenter, text);
}

void GameWidget::drawHelpPanel(QPainter &p)
{
    p.fillRect(rect(), QColor(0, 0, 0, 160));

    const int pw = qMin(width() - 40, 520);
    const int ph = qMin(height() - 40, 560);
    const QRectF panel((width() - pw) / 2.0, (height() - ph) / 2.0, pw, ph);
    drawPanel(p, panel, QColor(9, 28, 22, 240));

    p.setPen(QColor(86, 255, 110));
    p.setFont(QFont("Microsoft YaHei", 16, QFont::Bold));
    p.drawText(QRectF(panel.left(), panel.top() + 10, panel.width(), 32), Qt::AlignCenter, "玩法介绍");

    const int lx = panel.left() + 20;
    int y = panel.top() + 50;
    const int lw = panel.width() - 40;
    const int lh = 18;

    auto section = [&](const QString &title) {
        p.setPen(QColor(120, 255, 140));
        p.setFont(QFont("Microsoft YaHei", 11, QFont::Bold));
        p.drawText(QRectF(lx, y, lw, lh + 4), Qt::AlignLeft, title);
        y += lh + 6;
    };
    auto line = [&](const QString &text) {
        p.setPen(QColor(195, 218, 195));
        p.setFont(QFont("Microsoft YaHei", 9));
        p.drawText(QRectF(lx, y, lw, lh), Qt::AlignLeft, text);
        y += lh;
    };

    section("操作方式");
    line("方向键 / WASD — 移动    空格 — 暂停/继续    R — 返回菜单");
    line("Shift — 按住加速    M — 静音    F11 — 全屏切换");
    line("1/2/3 — 菜单界面选难度    回车 / 点击按钮 — 开始游戏");
    y += 4;

    section("速度成长机制");
    line("每吃到一个食物，蛇的移动速度会逐渐加快，但有上限。");
    line("难度越高，速度上限越高（简单 90ms → 普通 68ms → 困难 50ms）");
    line("按住 Shift 可临时加速（约 1.5 倍），松开即恢复。");
    y += 4;

    section("特殊道具");
    line("苹果：+10分，普通食物");
    line("金星：+50分，高分奖励");
    line("爱心：获得1次复活机会（最多3次），死亡后自动复活并获得2.5秒无敌时间");
    line("法球：随机清除一半障碍物，持续15秒");
    line("冰莓：蛇进入冰冻减速状态10秒，快结束时蓝光闪烁");
    line("彩钻：获得8秒炫彩无敌状态，可撞碎障碍物（15秒后刷新）");
    line("              无敌期间碰墙/碰自己会自动转向，快结束时有文字提醒");
    y += 4;

    section("难度设置");
    line("简单 — 初始速度慢、障碍较少（10个）、速度上限低");
    line("普通 — 初始速度均衡、初始障碍适中（18个）、速度上限适中");
    line("困难 — 初始速度快、初始障碍较多（28个）、速度上限高");

    const QRect closeBtn(panel.center().x() - 40, panel.bottom() - 38, 80, 28);
    p.setPen(QPen(QColor(220, 120, 120), 1.3));
    p.setBrush(QColor(80, 28, 28, 200));
    p.drawRoundedRect(closeBtn, 10, 10);
    p.setPen(Qt::white);
    p.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    p.drawText(closeBtn, Qt::AlignCenter, "关闭");
}

void GameWidget::generateObstacles()
{
    obstacles.clear();
    const QVector<QPoint> &sb = snake.getBody();
    const int count = obstacleCount();

    while (obstacles.size() < count) {
        const QPoint pt(QRandomGenerator::global()->bounded(cols), QRandomGenerator::global()->bounded(rows));
        const bool nearStart = (pt.x() >= 2 && pt.x() <= 8 && pt.y() >= 8 && pt.y() <= 13);
        const bool nearCenter = (qAbs(pt.x() - cols / 2) <= 1 && qAbs(pt.y() - rows / 2) <= 1);
        if (nearStart || nearCenter || sb.contains(pt) || obstacles.contains(pt)) continue;
        obstacles.append(pt);
    }
}

bool GameWidget::isObstacle(const QPoint &pt) const
{
    return obstacles.contains(pt) && !hiddenObstacles.contains(pt) && !destroyedObstacles.contains(pt);
}

QString GameWidget::assetPath(const QString &name) const
{
    const QString ad = QCoreApplication::applicationDirPath();
    const QStringList roots = {
        QDir::cleanPath(ad + "/../../.."),
        QDir::cleanPath(ad + "/../../../.."),
        QDir::currentPath(),
        QDir::cleanPath(QDir::currentPath() + "/.."),
        QDir::cleanPath(QDir::currentPath() + "/assets")
    };
    for (const auto &r : roots) {
        QDirIterator it(r, {name}, QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext()) return it.next();
    }
    return {};
}
