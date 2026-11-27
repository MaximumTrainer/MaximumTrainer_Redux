#include "sensorstatusstrip.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QToolButton>

SensorStatusStrip::SensorStatusStrip(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("sensorStatusStrip"));
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(6, 6, 6, 6);
    m_layout->setSpacing(6);
    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(5000);
    connect(m_hideTimer, &QTimer::timeout, this, &QWidget::hide);
}

void SensorStatusStrip::setAutoHide(bool autoHide)
{
    m_autoHide = autoHide;
    if (!autoHide) {
        m_hideTimer->stop();
        show();
    }
}

void SensorStatusStrip::clear()
{
    qDeleteAll(m_chips);
    m_chips.clear();
    m_names.clear();
    m_devices.clear();
    m_states.clear();
}

void SensorStatusStrip::updateVisibility()
{
    if (!m_autoHide)
        return;
    const bool allConnected = std::all_of(m_states.cbegin(), m_states.cend(),
                                          [](State current) { return current == State::Connected; });
    if (allConnected) {
        if (!m_hideTimer->isActive() && isVisible())
            m_hideTimer->start();
    } else {
        m_hideTimer->stop();
        show();
    }
}

void SensorStatusStrip::addSensor(int key, const QString &name, const QString &device)
{
    auto *chip = new QToolButton(this);
    chip->setObjectName(QStringLiteral("sensorChip%1").arg(key));
    chip->setFocusPolicy(Qt::NoFocus);
    chip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(chip, &QToolButton::clicked, this, [this, key]() {
        const State current = state(key);
        if (current == State::NotFound || current == State::Lost || current == State::Disconnected)
            emit retryRequested(key);
    });
    chip->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(chip, &QToolButton::customContextMenuRequested, this, [this, key, chip](const QPoint &pos) {
        emit chipMenuRequested(key, chip->mapToGlobal(pos));
    });
    m_layout->addWidget(chip);
    m_chips.insert(key, chip);
    m_names.insert(key, name);
    m_devices.insert(key, device.trimmed());
    m_states.insert(key, State::Searching);
    refreshChip(key);
}

void SensorStatusStrip::setState(int key, State state)
{
    if (!m_chips.contains(key))
        return;
    m_states.insert(key, state);
    refreshChip(key);
    updateVisibility();
}

QString SensorStatusStrip::chipText(int key) const
{
    const QToolButton *chip = m_chips.value(key, nullptr);
    return chip ? chip->text() : QString();
}

void SensorStatusStrip::refreshChip(int key)
{
    QToolButton *chip = m_chips.value(key);
    const State current = m_states.value(key);
    QString status;
    QColor dot;
    QString tip;
    switch (current) {
    case State::Searching:  status = tr("Searching…");  dot = QColor(0x9a, 0x9a, 0xa6); break;
    case State::Connecting: status = tr("Connecting…"); dot = QColor(0xf2, 0xc2, 0x30); break;
    case State::Connected:  status = tr("Connected");   dot = QColor(0x3f, 0xb2, 0x7f); break;
    case State::NotFound:
        status = tr("Not found - click to retry"); dot = QColor(0xe5, 0x53, 0x4b);
        tip = tr("Not found. Check that it is awake and nearby, then click to search again.");
        break;
    case State::Disconnected:
        status = tr("Disconnected"); dot = QColor(0x5a, 0x5a, 0x66);
        tip = tr("Disconnected. Click or right-click to reconnect.");
        break;
    case State::Lost:
        status = tr("Lost - click to retry"); dot = QColor(0xe5, 0x53, 0x4b);
        tip = tr("Connection lost. Click to reconnect.");
        break;
    }

    const qreal dpr = chip->devicePixelRatioF();
    QPixmap pixmap(QSize(10, 10) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(dot);
    painter.drawEllipse(QRectF(1, 1, 8, 8));
    painter.end();

    const bool needsAttention = current == State::NotFound || current == State::Lost;
    chip->setIcon(QIcon(pixmap));
    chip->setIconSize(QSize(10, 10));
    const QString device = m_devices.value(key);
    QString label = m_names.value(key);
    if (!device.isEmpty()) {
        const QString shortDevice = chip->fontMetrics().elidedText(device, Qt::ElideRight, 160);
        label += QStringLiteral(": %1").arg(shortDevice);
    }
    chip->setText(QStringLiteral("%1 · %2").arg(label, status));
    chip->setToolTip(tip.isEmpty() ? (device.isEmpty() ? QString() : device) : device + QStringLiteral("\n") + tip);
    chip->setCursor(needsAttention ? Qt::PointingHandCursor : Qt::ArrowCursor);
    chip->setStyleSheet(QStringLiteral(
        "QToolButton { background-color: rgba(20, 20, 24, 215); color: #e6e6ea; border: 1px solid %1;"
        " border-radius: 15px; padding: 7px 10px; font: 600 9pt 'Inter'; }"
        "QToolButton:hover { border-color: %2; }")
        .arg(needsAttention ? dot.name() : QStringLiteral("#3a3a44"), dot.name()));
}
