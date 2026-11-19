#include "racestyle.h"

RaceStyle::RaceStyle(QObject *parent)
    : QObject(parent)
{
}

QColor RaceStyle::rgba(double red, double green, double blue, double alpha) const
{
    return QColor::fromRgbF(float(red), float(green), float(blue), float(alpha));
}

QColor RaceStyle::lighter(const QColor &colour, double factor) const
{
    return colour.lighter(qRound(factor * 100.0));
}

QColor RaceStyle::darker(const QColor &colour, double factor) const
{
    return colour.darker(qRound(factor * 100.0));
}

QColor RaceStyle::tint(const QColor &base, const QColor &overlay) const
{
    const QColor overlayRgb = overlay.toRgb();
    const float overlayAlpha = overlayRgb.alphaF();
    if (overlayAlpha >= 1.0f)
        return overlay;
    if (overlayAlpha <= 0.0f)
        return base;
    const QColor baseRgb = base.toRgb();
    const float baseWeight = 1.0f - overlayAlpha;
    return QColor::fromRgbF(overlayRgb.redF() * overlayAlpha + baseRgb.redF() * baseWeight,
                            overlayRgb.greenF() * overlayAlpha + baseRgb.greenF() * baseWeight,
                            overlayRgb.blueF() * overlayAlpha + baseRgb.blueF() * baseWeight,
                            overlayAlpha + baseWeight * baseRgb.alphaF());
}

QColor RaceStyle::mix(const QColor &from, const QColor &to, double t) const
{
    const float weight = float(t);
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * weight,
                            from.greenF() + (to.greenF() - from.greenF()) * weight,
                            from.blueF() + (to.blueF() - from.blueF()) * weight, 1.0f);
}

QString RaceStyle::fixed(double value, int decimals) const
{
    return QString::number(value, 'f', decimals);
}

QString RaceStyle::shortName(const QString &name, int maxLength) const
{
    if (name.length() <= maxLength)
        return name;
    return name.left(maxLength - 1) + QChar(0x2026);
}
