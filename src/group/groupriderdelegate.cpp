#include "groupriderdelegate.h"

#include "groupridersmodel.h"

#include <QDateTime>
#include <QPainter>

using RowState = GroupRidersModel::RowState;

namespace {

const QColor kOnTargetColor(0x5f, 0xd0, 0x8a);
const QColor kOffTargetColor(0xff, 0xd1, 0x66);
const QColor kFinishedColor(0x6c, 0xa8, 0xff);

QColor stripeColor(RowState state, const QPalette &palette) {
    switch (state) {
    case RowState::OnTarget:  return kOnTargetColor;
    case RowState::OffTarget: return kOffTargetColor;
    case RowState::Finished:  return kFinishedColor;
    case RowState::Waiting:   return palette.color(QPalette::Mid);
    default:                  return palette.color(QPalette::PlaceholderText);
    }
}

} // namespace

GroupRiderDelegate::GroupRiderDelegate(const GroupRidersModel *model, QObject *parent)
    : QStyledItemDelegate(parent), m_model(model) {}

QSize GroupRiderDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const {
    return QSize(option.rect.width(), kRowHeight);
}

void GroupRiderDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                               const QModelIndex &index) const {
    const GroupRidersModel::RiderRow &row = m_model->rowAt(index.row());
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const RowState state = m_model->stateOf(row, nowMs);
    const QPalette &palette = option.palette;
    const QRect rect = option.rect;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    if (row.isYou) painter->fillRect(rect, palette.color(QPalette::AlternateBase));
    painter->setPen(palette.color(QPalette::Mid));
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());

    const QRect stripe(rect.left() + 4, rect.top() + 6, 4, rect.height() - 12);
    painter->setPen(Qt::NoPen);
    painter->setBrush(stripeColor(state, palette));
    painter->drawRoundedRect(stripe, 2, 2);

    const bool dimmed = state == RowState::Connecting || state == RowState::Stale || state == RowState::Paused;
    const QColor primaryText = dimmed ? palette.color(QPalette::PlaceholderText) : palette.color(QPalette::Text);
    const QColor secondaryText = palette.color(QPalette::PlaceholderText);

    const int rightColumnWidth = 58;
    const QRect textRect(stripe.right() + 8, rect.top() + 2,
                         rect.width() - (stripe.right() + 8 - rect.left()) - rightColumnWidth - 6, rect.height() - 4);
    const QRect valueRect(rect.right() - rightColumnWidth - 4, rect.top() + 2, rightColumnWidth, rect.height() - 4);

    QFont nameFont = option.font;
    nameFont.setBold(true);
    QFont detailFont = option.font;
    detailFont.setPointSizeF(option.font.pointSizeF() * 0.85);
    QFont powerFont = option.font;
    powerFont.setBold(true);
    powerFont.setPointSizeF(option.font.pointSizeF() * 1.25);

    QString name = row.isYou ? QObject::tr("You") : row.name;
    if (row.isHost) name += QObject::tr(" · host");
    const QRect nameRect(textRect.left(), textRect.top(), textRect.width(), textRect.height() / 2);
    const QRect detailRect(textRect.left(), textRect.top() + textRect.height() / 2, textRect.width(), textRect.height() / 2);

    painter->setFont(nameFont);
    painter->setPen(primaryText);
    painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                      QFontMetrics(nameFont).elidedText(name, Qt::ElideRight, nameRect.width()));

    painter->setFont(detailFont);
    painter->setPen(secondaryText);
    const QString detail = m_model->detailLine(row, nowMs);
    painter->drawText(detailRect, Qt::AlignLeft | Qt::AlignVCenter,
                      QFontMetrics(detailFont).elidedText(detail, Qt::ElideRight, detailRect.width()));

    if (row.telemetry && state != RowState::Connecting && state != RowState::Stale) {
        const QRect powerRect(valueRect.left(), valueRect.top(), valueRect.width(), valueRect.height() / 2 + 2);
        const QRect hrRect(valueRect.left(), valueRect.top() + valueRect.height() / 2, valueRect.width(), valueRect.height() / 2);
        painter->setFont(powerFont);
        painter->setPen(primaryText);
        const QString power = row.telemetry->powerWatts ? QString::number(*row.telemetry->powerWatts) : QStringLiteral("-");
        painter->drawText(powerRect, Qt::AlignRight | Qt::AlignVCenter, power);
        if (row.telemetry->heartRateBpm) {
            painter->setFont(detailFont);
            painter->setPen(secondaryText);
            painter->drawText(hrRect, Qt::AlignRight | Qt::AlignVCenter,
                              QObject::tr("HR %1").arg(*row.telemetry->heartRateBpm));
        }
    }

    painter->restore();
}
