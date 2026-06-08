#ifndef SNAKE_H
#define SNAKE_H

#include <QPoint>
#include <QVector>

class Snake
{
public:
    enum Direction {
        Up,
        Down,
        Left,
        Right
    };

    Snake();

    void reset();
    void move(bool grow);
    void setDirection(Direction newDirection);
    void markDirectionApplied();

    const QVector<QPoint> &getBody() const;
    QPoint head() const;
    QPoint nextHead() const;
    bool hitsSelf() const;
    bool wouldHitSelf() const;

private:
    QVector<QPoint> body;
    Direction direction;
    Direction moveDirection;
};

#endif
