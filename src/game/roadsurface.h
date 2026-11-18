#ifndef ROADSURFACE_H
#define ROADSURFACE_H

#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

/// The retro race's pseudo-3D road: horizontal slices of grass, asphalt,
/// rumble strips, edge lines and centre dashes whose colours alternate with a
/// world-segment parity that scrolls with visualDist. Drawn as one
/// vertex-coloured geometry node, so a frame rewrites a few thousand vertex
/// colours instead of re-evaluating and re-batching ~840 QML Rectangles. Under
/// the software renderer it falls back to built-in rectangle nodes.
class RoadSurface : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(double visualDist READ visualDist WRITE setVisualDist NOTIFY visualDistChanged)
    Q_PROPERTY(double maxHalfWidth READ maxHalfWidth WRITE setMaxHalfWidth NOTIFY maxHalfWidthChanged)
    Q_PROPERTY(int slices READ slices WRITE setSlices NOTIFY slicesChanged)

public:
    explicit RoadSurface(QQuickItem *parent = nullptr);

    double visualDist() const { return m_visualDist; }
    void setVisualDist(double visualDist);
    double maxHalfWidth() const { return m_maxHalfWidth; }
    void setMaxHalfWidth(double maxHalfWidth);
    int slices() const { return m_slices; }
    void setSlices(int slices);

    static constexpr int kQuadsPerSlice = 7;
    static constexpr int kVerticesPerQuad = 6;

signals:
    void visualDistChanged();
    void maxHalfWidthChanged();
    void slicesChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    QSGNode *updateGeometryNode(QSGNode *oldNode);
    QSGNode *updateRectangleNodes(QSGNode *oldNode);

    double m_visualDist = 0.0;
    double m_maxHalfWidth = 0.0;
    int m_slices = 120;
};

#endif // ROADSURFACE_H
