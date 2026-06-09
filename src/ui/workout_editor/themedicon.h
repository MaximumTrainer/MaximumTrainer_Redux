#ifndef THEMEDICON_H
#define THEMEDICON_H

#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QColor>

// Recolour a monochrome glyph (e.g. resources/images/repeat.png) to `color`,
// using the source alpha as a mask. The bundled icons are dark and vanish on a
// dark background; tinting them to the palette text colour keeps them visible
// under both light and dark themes.
inline QPixmap tintedPixmap(const QString &resourcePath, const QColor &color)
{
    QPixmap src(resourcePath);
    if (src.isNull())
        return src;

    QPixmap out(src.size());
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.drawPixmap(0, 0, src);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(out.rect(), color);
    p.end();
    return out;
}

inline QIcon tintedIcon(const QString &resourcePath, const QColor &color)
{
    const QPixmap pm = tintedPixmap(resourcePath, color);
    return pm.isNull() ? QIcon(resourcePath) : QIcon(pm);
}

#endif // THEMEDICON_H
