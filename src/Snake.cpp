#include "Snake.h"

Snake::Snake()
{
    reset();
}

void Snake::reset()
{
    body.clear();
    body.append(QPoint(6, 10));
    body.append(QPoint(5, 10));
    body.append(QPoint(4, 10));
    direction = Right;
    moveDirection = Right;
}

void Snake::move(bool grow)
{
    body.prepend(nextHead());

    if (!grow) {
        body.removeLast();
    }

    markDirectionApplied();
}

void Snake::setDirection(Direction newDirection)
{
    if ((moveDirection == Up && newDirection == Down) ||
        (moveDirection == Down && newDirection == Up) ||
        (moveDirection == Left && newDirection == Right) ||
        (moveDirection == Right && newDirection == Left)) {
        return;
    }

    direction = newDirection;
}

void Snake::markDirectionApplied()
{
    moveDirection = direction;
}

const QVector<QPoint> &Snake::getBody() const
{
    return body;
}

QPoint Snake::head() const
{
    return body.first();
}

QPoint Snake::nextHead() const
{
    QPoint newHead = head();

    switch (direction) {
    case Up:
        newHead.setY(newHead.y() - 1);
        break;
    case Down:
        newHead.setY(newHead.y() + 1);
        break;
    case Left:
        newHead.setX(newHead.x() - 1);
        break;
    case Right:
        newHead.setX(newHead.x() + 1);
        break;
    }

    return newHead;
}

bool Snake::hitsSelf() const
{
    const QPoint currentHead = head();

    for (int i = 1; i < body.size(); ++i) {
        if (body[i] == currentHead) {
            return true;
        }
    }

    return false;
}

bool Snake::wouldHitSelf() const
{
    const QPoint next = nextHead();

    for (int i = 0; i < body.size() - 1; ++i) {
        if (body[i] == next) {
            return true;
        }
    }

    return false;
}
