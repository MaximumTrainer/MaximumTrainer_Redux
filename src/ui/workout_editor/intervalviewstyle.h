#ifndef INTERVALVIEWSTYLE_H
#define INTERVALVIEWSTYLE_H

#include <QProxyStyle>
#include <QStyle>
#include <QStyleOption>
#include <QPainter>

class IntervalViewStyle : public QProxyStyle
{

public:
    IntervalViewStyle(QStyle* style = nullptr);
    void drawPrimitive(PrimitiveElement element, const QStyleOption * option, QPainter * painter, const QWidget * widget = nullptr) const override;
};

#endif // INTERVALVIEWSTYLE_H


