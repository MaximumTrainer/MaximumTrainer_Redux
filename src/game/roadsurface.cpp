#include "roadsurface.h"

#include <QCoreApplication>
#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGRectangleNode>
#include <QSGRendererInterface>
#include <QSGVertexColorMaterial>
#include <QtQml/qqml.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr QRgb kGrassHaze   = 0xff3c7548;
constexpr QRgb kGrassEven   = 0xff47935b;
constexpr QRgb kGrassOdd    = 0xff3a8050;
constexpr QRgb kAsphaltEven = 0xff5b5b66;
constexpr QRgb kAsphaltOdd  = 0xff55555f;
constexpr QRgb kRumbleEven  = 0xffd83a3a;
constexpr QRgb kRumbleOdd   = 0xfff2f2f2;
constexpr QRgb kEdgeLine    = 0xfff4f4f4;
constexpr QRgb kCentreDash  = 0xfff3f1d6;

struct RoadQuad {
    QRectF rect;
    QRgb colour;
};

// Quads for one slice, back to front. A hidden centre dash is an empty rect so
// every slice always has the same number of quads.
void sliceQuads(int sliceIndex, int sliceCount, double visualDist, double maxHalfWidth,
                double itemWidth, double itemHeight, RoadQuad (&quads)[RoadSurface::kQuadsPerSlice])
{
    const double p = static_cast<double>(sliceIndex + 1) / sliceCount;   // 0 horizon .. 1 near
    const auto segment = static_cast<long long>(std::floor((1.0 / p) * 1.5 + visualDist * 0.22));
    const bool even = ((segment % 2) + 2) % 2 == 0;
    const double halfWidth = maxHalfWidth * p;
    const double rumbleWidth = std::max(2.0, halfWidth * 0.16 + 3.0);
    const double edgeWidth = std::max(1.0, halfWidth * 0.028 + 1.0);
    const double dashWidth = std::max(1.5, halfWidth * 0.032);
    const bool farHaze = p < 0.18;   // far field: no flicker
    const bool dash = even && p > 0.05;
    const double y = (static_cast<double>(sliceIndex) / sliceCount) * itemHeight;
    const double h = itemHeight / sliceCount + 1.4;
    const double centreX = itemWidth / 2.0;

    quads[0] = { QRectF(0.0, y, itemWidth, h), farHaze ? kGrassHaze : (even ? kGrassEven : kGrassOdd) };
    quads[1] = { QRectF(centreX - halfWidth, y, 2.0 * halfWidth, h), even ? kAsphaltEven : kAsphaltOdd };
    quads[2] = { QRectF(centreX - halfWidth - rumbleWidth, y, rumbleWidth, h), even ? kRumbleEven : kRumbleOdd };
    quads[3] = { QRectF(centreX + halfWidth, y, rumbleWidth, h), even ? kRumbleEven : kRumbleOdd };
    quads[4] = { QRectF(centreX - halfWidth, y, edgeWidth, h), kEdgeLine };
    quads[5] = { QRectF(centreX + halfWidth - edgeWidth, y, edgeWidth, h), kEdgeLine };
    quads[6] = { dash ? QRectF(centreX - dashWidth / 2.0, y, dashWidth, h) : QRectF(), kCentreDash };
}

void writeQuad(QSGGeometry::ColoredPoint2D *&vertex, const RoadQuad &quad)
{
    const auto left = static_cast<float>(quad.rect.left());
    const auto right = static_cast<float>(quad.rect.right());
    const auto top = static_cast<float>(quad.rect.top());
    const auto bottom = static_cast<float>(quad.rect.bottom());
    const auto r = static_cast<uchar>(qRed(quad.colour));
    const auto g = static_cast<uchar>(qGreen(quad.colour));
    const auto b = static_cast<uchar>(qBlue(quad.colour));
    (vertex++)->set(left, top, r, g, b, 255);
    (vertex++)->set(right, top, r, g, b, 255);
    (vertex++)->set(left, bottom, r, g, b, 255);
    (vertex++)->set(left, bottom, r, g, b, 255);
    (vertex++)->set(right, top, r, g, b, 255);
    (vertex++)->set(right, bottom, r, g, b, 255);
}

} // namespace

RoadSurface::RoadSurface(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void RoadSurface::setVisualDist(double visualDist)
{
    if (m_visualDist == visualDist)
        return;
    m_visualDist = visualDist;
    emit visualDistChanged();
    update();
}

void RoadSurface::setMaxHalfWidth(double maxHalfWidth)
{
    if (m_maxHalfWidth == maxHalfWidth)
        return;
    m_maxHalfWidth = maxHalfWidth;
    emit maxHalfWidthChanged();
    update();
}

void RoadSurface::setSlices(int slices)
{
    slices = std::max(1, slices);
    if (m_slices == slices)
        return;
    m_slices = slices;
    emit slicesChanged();
    update();
}

void RoadSurface::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    update();
}

QSGNode *RoadSurface::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    // The software renderer draws only built-in node types, so it gets one
    // rectangle node per quad; hardware renderers get a single geometry node.
    const bool software = window()
        && window()->rendererInterface()->graphicsApi() == QSGRendererInterface::Software;
    return software ? updateRectangleNodes(oldNode) : updateGeometryNode(oldNode);
}

QSGNode *RoadSurface::updateGeometryNode(QSGNode *oldNode)
{
    const int vertexCount = m_slices * kQuadsPerSlice * kVerticesPerQuad;
    auto *node = static_cast<QSGGeometryNode *>(oldNode);
    if (!node) {
        node = new QSGGeometryNode;
        auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), vertexCount);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        // Always blended, so the overlapping quads draw strictly in order.
        node->setMaterial(new QSGVertexColorMaterial);
        node->setFlag(QSGNode::OwnsMaterial);
    } else if (node->geometry()->vertexCount() != vertexCount) {
        node->geometry()->allocate(vertexCount);
    }

    auto *vertex = node->geometry()->vertexDataAsColoredPoint2D();
    RoadQuad quads[kQuadsPerSlice];
    for (int sliceIndex = 0; sliceIndex < m_slices; ++sliceIndex) {
        sliceQuads(sliceIndex, m_slices, m_visualDist, m_maxHalfWidth, width(), height(), quads);
        for (const RoadQuad &quad : quads)
            writeQuad(vertex, quad);
    }
    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}

QSGNode *RoadSurface::updateRectangleNodes(QSGNode *oldNode)
{
    const int quadCount = m_slices * kQuadsPerSlice;
    QSGNode *root = oldNode;
    if (!root)
        root = new QSGNode;
    if (root->childCount() > quadCount) {
        while (QSGNode *extra = root->lastChild()) {
            root->removeChildNode(extra);
            delete extra;
        }
    }
    while (root->childCount() < quadCount) {
        QSGRectangleNode *rectangle = window()->createRectangleNode();
        rectangle->setFlag(QSGNode::OwnedByParent);
        root->appendChildNode(rectangle);
    }

    auto *child = root->firstChild();
    RoadQuad quads[kQuadsPerSlice];
    for (int sliceIndex = 0; sliceIndex < m_slices; ++sliceIndex) {
        sliceQuads(sliceIndex, m_slices, m_visualDist, m_maxHalfWidth, width(), height(), quads);
        for (const RoadQuad &quad : quads) {
            auto *rectangle = static_cast<QSGRectangleNode *>(child);
            if (rectangle->rect() != quad.rect)
                rectangle->setRect(quad.rect);
            if (rectangle->color().rgba() != quad.colour)
                rectangle->setColor(QColor::fromRgba(quad.colour));
            child = child->nextSibling();
        }
    }
    return root;
}

static void registerRoadSurfaceType()
{
    qmlRegisterType<RoadSurface>("MaximumTrainer.Game", 1, 0, "RoadSurface");
}
Q_COREAPP_STARTUP_FUNCTION(registerRoadSurfaceType)
