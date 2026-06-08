#ifndef FOOD_H
#define FOOD_H

#include <QPoint>
#include <QVector>

class Food
{
public:
    Food();

    void generate(int cols, int rows, const QVector<QPoint> &snakeBody);
    QPoint position() const;

private:
    QPoint pos;
};

#endif
