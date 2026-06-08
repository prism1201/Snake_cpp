#include "Food.h"

#include <QRandomGenerator>

Food::Food()
    : pos(0, 0)
{
}

void Food::generate(int cols, int rows, const QVector<QPoint> &snakeBody)
{
    do {
        const int x = QRandomGenerator::global()->bounded(cols);
        const int y = QRandomGenerator::global()->bounded(rows);
        pos = QPoint(x, y);
    } while (snakeBody.contains(pos));
}

QPoint Food::position() const
{
    return pos;
}
