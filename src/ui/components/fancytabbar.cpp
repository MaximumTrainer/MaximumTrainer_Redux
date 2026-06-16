/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include <QMouseEvent>

//#include <QWindowsStyle>
#include <QPainter>
#include <QColor>
#include <QStackedLayout>
#include <QToolTip>
#include <QDebug>

#include "fancytabbar.h"
#include "stylehelper.h"




const int iconHeight2 = 45;
const int widthBar = 85;
const int FancyTabBar::m_rounding = 30;
const int FancyTabBar::m_textPadding = 4;



FancyTabBar::FancyTabBar(const TabBarPosition position, QWidget *parent)
    : QWidget(parent), mPosition(position)
{
    mHoverIndex = -1;
    mCurrentIndex = -1;

    if(mPosition == TabBarPosition::Above || mPosition == TabBarPosition::Below)
    {
        setMinimumHeight(qMax(2 * m_rounding, 40));
        setMaximumHeight(tabSizeHint(false).height());
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
    else
    {
        setMinimumWidth(qMax(2 * m_rounding, 40));
//        setMaximumWidth(tabSizeHint(false).width());
        setMaximumWidth(widthBar);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    }

    //    setStyle(new QWindowsStyle);
    setAttribute(Qt::WA_Hover, true);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true); // Needed for hover events
    mTimerTriggerChangedSignal.setSingleShot(true);

    // We use a zerotimer to keep the sidebar responsive
    connect(&mTimerTriggerChangedSignal, SIGNAL(timeout()), this, SLOT(emitCurrentIndex()));


//    qDebug() << "FANCY**W" << this->height();
//    qDebug() << "PARENTSIZ" << parent->height();

    //    this->setMinimumHeight(500);
}

FancyTabBar::~FancyTabBar()
{
    //    delete style();
}

QSize FancyTabBar::tabSizeHint(bool minimum) const
{
    QFont boldFont(font());
    boldFont.setPointSizeF(StyleHelper::sidebarFontSize());
    boldFont.setBold(true);
    QFontMetrics fm(boldFont);
    int spacing = 8;
//    int width = 60 + spacing + 2;
    int width = widthBar;
    int maxLabelwidth = 0;
    for (int tab=0 ; tab<count() ;++tab) {
        int width = fm.horizontalAdvance(tabText(tab));
        if (width > maxLabelwidth)
            maxLabelwidth = width;
    }
    int iconHeight = minimum ? 0 : iconHeight2;
    //    int iconHeight = minimum ? 0 : 40;

    //        return QSize(qMax(width, maxLabelwidth + 4), iconHeight + spacing + fm.height() );
    return QSize(qMax(width, maxLabelwidth + 4), iconHeight + spacing + fm.height() );

}

QPoint FancyTabBar::getCorner(const QRect& rect, const Corner corner) const
{
    if(mPosition == TabBarPosition::Above)
    {
        if(corner == Corner::OutsideBeginning) return rect.topLeft();
        if(corner == Corner::OutsideEnd) return rect.topRight();
        if(corner == Corner::InsideBeginning) return rect.bottomLeft();
        if(corner == Corner::InsideEnd) return rect.bottomRight();
    }
    else if(mPosition == TabBarPosition::Below)
    {
        if(corner == Corner::OutsideBeginning) return rect.bottomLeft();
        if(corner == Corner::OutsideEnd) return rect.bottomRight();
        if(corner == Corner::InsideBeginning) return rect.topLeft();
        if(corner == Corner::InsideEnd) return rect.topRight();
    }
    else if(mPosition == TabBarPosition::Left)
    {
        if(corner == Corner::OutsideBeginning) return rect.topLeft();
        if(corner == Corner::OutsideEnd) return rect.bottomLeft();
        if(corner == Corner::InsideBeginning) return rect.topRight();
        if(corner == Corner::InsideEnd) return rect.bottomRight();
    }
    else if(mPosition == TabBarPosition::Right)
    {
        if(corner == Corner::OutsideBeginning) return rect.topRight();
        if(corner == Corner::OutsideEnd) return rect.bottomRight();
        if(corner == Corner::InsideBeginning) return rect.topLeft();
        if(corner == Corner::InsideEnd) return rect.bottomLeft();
    }

    Q_ASSERT("that's impossible!");
    return QPoint();
}

QRect FancyTabBar::adjustRect(const QRect& rect, const qint8 offsetOutside, const qint8 offsetInside, const qint8 offsetBeginning, const qint8 offsetEnd) const
{
    if(mPosition == TabBarPosition::Above) return rect.adjusted(-offsetBeginning, -offsetOutside, offsetEnd, offsetInside);
    else if(mPosition == TabBarPosition::Below) return rect.adjusted(-offsetBeginning, -offsetInside, -offsetBeginning, offsetOutside);
    else if(mPosition == TabBarPosition::Left) return rect.adjusted(-offsetOutside, -offsetBeginning, offsetInside, offsetEnd);
    else if(mPosition == TabBarPosition::Right) return rect.adjusted(-offsetInside, -offsetBeginning, offsetOutside, offsetEnd);

    Q_ASSERT("that's impossible!");
    return QRect();
}

// Same with a point: + means towards Outside/End, - means towards Inside/Beginning
QPoint FancyTabBar::adjustPoint(const QPoint& point, const qint8 offsetInsideOutside, const qint8 offsetBeginningEnd) const
{
    if(mPosition == TabBarPosition::Above) return point + QPoint(offsetBeginningEnd, -offsetInsideOutside);
    else if(mPosition == TabBarPosition::Below) return point + QPoint(offsetBeginningEnd, offsetInsideOutside);
    else if(mPosition == TabBarPosition::Left) return point + QPoint(-offsetInsideOutside, offsetBeginningEnd);
    else if(mPosition == TabBarPosition::Right) return point + QPoint(offsetInsideOutside, offsetBeginningEnd);

    Q_ASSERT("that's impossible!");
    return QPoint();
}

void FancyTabBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);

    // paint background
    QRect rectangle = adjustRect(rect(), 0, -1, 0, 0);
    QLinearGradient lg;

    lg.setStart(getCorner(rectangle, Corner::OutsideBeginning));
    lg.setFinalStop(getCorner(rectangle, Corner::InsideBeginning));
    lg.setColorAt(0.0, QColor(80, 80, 80, 255));
    lg.setColorAt(1.0, QColor(140, 140, 140, 255));
    painter.fillRect(rectangle, lg);

    // draw dark widget bordert on inner inside (e.g. bottom if the widget position is top)
    painter.setPen(StyleHelper::borderColor());
    painter.drawLine(adjustPoint(getCorner(rectangle, Corner::InsideBeginning), -1, 0), adjustPoint(getCorner(rectangle, Corner::InsideEnd), -1, 0));

    // draw bright widget border on outer inside (e.g. bottom if the widget position is top)
    painter.setPen(StyleHelper::sidebarHighlight());
    painter.drawLine(getCorner(rectangle, Corner::InsideBeginning), getCorner(rectangle, Corner::InsideEnd));

    // paint inactive tabs
    for (int i = 0; i < count(); ++i)
        if (i != currentIndex())
            paintTab(&painter, i);

    // paint active tab last, since it overlaps the neighbors
    if (currentIndex() != -1)
        paintTab(&painter, currentIndex());
}

// Handle hover events for mouse fade ins
void FancyTabBar::mouseMoveEvent(QMouseEvent *e)
{

    int newHover = -1;
    for (int i = 0; i < count(); ++i)
    {
        QRect area = tabRect(i);
        if (area.contains(e->pos())) {
            newHover = i;
            break;
        }
    }

//    qDebug() << "Moving mouse at position" << e->pos() << "hover is:" << newHover << "index curr is:" << mHoverIndex;
    if (newHover == mHoverIndex)
        return;


    if (validIndex(mHoverIndex))
        mAttachedTabs[mHoverIndex]->fadeOut();

    mHoverIndex = newHover;

    if (validIndex(mHoverIndex)) {
        mAttachedTabs[mHoverIndex]->fadeIn();
        mHoverRect = tabRect(mHoverIndex);
    }
}

bool FancyTabBar::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        if (validIndex(mHoverIndex)) {
            QString tt = tabToolTip(mHoverIndex);
            if (!tt.isEmpty()) {
                QToolTip::showText(static_cast<QHelpEvent*>(event)->globalPos(), tt, this);
                return true;
            }
        }
    }
    return QWidget::event(event);
}

// Resets hover animation on mouse enter
void FancyTabBar::enterEvent(QEnterEvent *e)
{
    Q_UNUSED(e)
    mHoverRect = QRect();
    mHoverIndex = -1;
}

// Resets hover animation on mouse leave
void FancyTabBar::leaveEvent(QEvent *e)
{
    Q_UNUSED(e)
    mHoverIndex = -1;
    mHoverRect = QRect();
    for (int i = 0 ; i < mAttachedTabs.count() ; ++i) {
        mAttachedTabs[i]->fadeOut();
    }
}

QSize FancyTabBar::sizeHint() const
{
    QSize sh = tabSizeHint();
    //    return QSize(sh.width(), sh.height() * mAttachedTabs.count());

    if(mPosition == TabBarPosition::Above || mPosition == TabBarPosition::Below)
        return QSize(sh.width() * mAttachedTabs.count(), sh.height());
    else
        return QSize(sh.width(), 2500);
    //        return QSize(sh.width(), sh.height() * mAttachedTabs.count());
}

QSize FancyTabBar::minimumSizeHint() const
{
    QSize sh = tabSizeHint(true);
    //    return QSize(sh.width(), sh.height() * mAttachedTabs.count());

    if(mPosition == TabBarPosition::Above || mPosition == TabBarPosition::Below)
        return QSize(sh.width() * mAttachedTabs.count(), sh.height());
    else
        return QSize(sh.width(), sh.height() * mAttachedTabs.count());
}

QRect FancyTabBar::tabRect(int index) const
{
    QSize sh = tabSizeHint();

    if(mPosition == TabBarPosition::Above || mPosition == TabBarPosition::Below)
    {
        if (sh.width() * mAttachedTabs.count() > width())
            sh.setWidth(width() / mAttachedTabs.count());

        return QRect(index * sh.width(), 0, sh.width(), sh.height());
    }
    else
    {
        if (sh.height() * mAttachedTabs.count() > height())
            sh.setHeight(height() / mAttachedTabs.count());

        return QRect(0, index * sh.height(), sh.width(), sh.height());
    }

}

// This keeps the sidebar responsive since
// we get a repaint before loading the
// mode itself
void FancyTabBar::emitCurrentIndex()
{
    emit currentChanged(mCurrentIndex);
}

void FancyTabBar::mousePressEvent(QMouseEvent *e)
{
    e->accept();
    for (int index = 0; index < mAttachedTabs.count(); ++index)
    {
        if (tabRect(index).contains(e->pos()))
        {
            if (isTabEnabled(index))
            {
                mCurrentIndex = index;
                update();
                mTimerTriggerChangedSignal.start(0);
            }
            break;
        }
    }
}

void FancyTabBar::paintTab(QPainter *painter, int tabIndex) const
{
    if (!validIndex(tabIndex))
    {
        qWarning("invalid index");
        return;
    }
    painter->save();

    QRect rect = tabRect(tabIndex);
    bool selected = (tabIndex == mCurrentIndex);
    bool enabled = isTabEnabled(tabIndex);

    if(selected)
    {
        // background
        painter->save();
        QLinearGradient grad(getCorner(rect, Corner::OutsideBeginning), getCorner(rect, Corner::InsideBeginning));
        grad.setColorAt(0, QColor(255, 255, 255, 170));
        grad.setColorAt(1, QColor(255, 255, 255, 240));
//        grad.setColorAt(0, QColor(255, 255, 255, 120));
//        grad.setColorAt(1, QColor(255, 255, 255, 160));
        painter->fillRect(adjustRect(rect, 0, 0, 0, -1), grad);
        painter->restore();

        // shadows (the black lines immediately before/after (active && selected)-backgrounds)
        painter->setPen(QColor(0, 0, 0, 110));
        painter->drawLine(adjustPoint(getCorner(rect, Corner::OutsideBeginning), 0, -1), adjustPoint(getCorner(rect, Corner::InsideBeginning), 0, -1));
        painter->drawLine(getCorner(rect, Corner::OutsideEnd), getCorner(rect, Corner::InsideEnd));

        // thin shadow on the outside of active tab
        painter->setPen(QColor(0, 0, 0, 40));
        painter->drawLine(getCorner(rect, Corner::OutsideBeginning), getCorner(rect, Corner::OutsideEnd));

        // highlights
        painter->setPen(QColor(255, 255, 255, 50));
        painter->drawLine(adjustPoint(getCorner(rect, Corner::OutsideBeginning), 0, -2), adjustPoint(getCorner(rect, Corner::InsideBeginning), 0, -2));
        painter->drawLine(adjustPoint(getCorner(rect, Corner::OutsideEnd), 0, 1), adjustPoint(getCorner(rect, Corner::InsideEnd), 0, 1));

        painter->setPen(QColor(255, 255, 255, 40));
        // thin white line towards beginning
        painter->drawLine(adjustPoint(getCorner(rect, Corner::OutsideBeginning), 0, 0), adjustPoint(getCorner(rect, Corner::InsideBeginning), 0, 0));
        // thin white line on inside border
        painter->drawLine(adjustPoint(getCorner(rect, Corner::InsideBeginning), 0, 1), adjustPoint(getCorner(rect, Corner::InsideEnd), 0, -1));
        // thin white line towards end
        painter->drawLine(adjustPoint(getCorner(rect, Corner::OutsideEnd), 0, -1), adjustPoint(getCorner(rect, Corner::InsideEnd), 0, -1));
    }

    QString tabText(this->tabText(tabIndex));
    QRect tabTextRect(rect);
    const bool drawIcon = rect.height() > 36;
    QRect tabIconRect(tabTextRect);

    //        tabTextRect.translate(0, drawIcon ? -2 : 1);  //TODO: ici
    tabTextRect.translate(0, drawIcon ? -2 : 1);
    QFont boldFont(painter->font());
    boldFont.setPointSizeF(StyleHelper::sidebarFontSize());
    boldFont.setBold(true);
    painter->setFont(boldFont);
    //Font color selected vs not selected here
    painter->setPen(selected ? QColor(255, 255, 255, 160) : QColor(0, 0, 0, 110));
    const int textFlags = Qt::AlignCenter | (drawIcon ? Qt::AlignBottom : Qt::AlignVCenter) | Qt::TextWordWrap;
    if (enabled) {
        painter->drawText(tabTextRect, textFlags, tabText);
        painter->setPen(selected ? QColor(60, 60, 60) : StyleHelper::panelTextColor());
    } else {
        painter->setPen(selected ? StyleHelper::panelTextColor() : QColor(255, 255, 255, 120));
    }

#if defined(Q_OS_MAC)
    bool isMac=true;
#else
    bool isMac = false;
#endif

    // hover
    if(!isMac && !selected && enabled)
    {
        painter->save();
        int fader = int(mAttachedTabs[tabIndex]->fader());
        QLinearGradient grad(getCorner(rect, Corner::OutsideBeginning), getCorner(rect, Corner::InsideBeginning));

        grad.setColorAt(0, Qt::transparent);
        grad.setColorAt(0.5, QColor(255, 255, 255, fader));
        grad.setColorAt(1, Qt::transparent);
        painter->fillRect(rect, grad);
        painter->setPen(QPen(grad, 1.0));

        if(mPosition == TabBarPosition::Above || mPosition == TabBarPosition::Below)
        {
            painter->drawLine(rect.topLeft(), rect.bottomLeft());
            painter->drawLine(rect.topRight(), rect.bottomRight());
        }
        else
        {
            painter->drawLine(rect.topLeft(), rect.topRight());
            painter->drawLine(rect.bottomLeft(), rect.bottomRight());
        }

        painter->restore();
    }

    if (!enabled)
        painter->setOpacity(0.7);

    if (drawIcon) {
        int textHeight = painter->fontMetrics().boundingRect(QRect(0, 0, width(), height()), Qt::TextWordWrap, tabText).height();
        tabIconRect.adjust(0, 4, 0, -textHeight);
        StyleHelper::drawIconWithShadow(tabIcon(tabIndex), tabIconRect, painter, enabled ? QIcon::Normal : QIcon::Disabled);
    }

    painter->translate(0, -1);
    painter->drawText(tabTextRect, textFlags, tabText);
    painter->restore();
}

void FancyTabBar::setCurrentIndex(int index) {
    if (isTabEnabled(index)) {
        mCurrentIndex = index;
        update();
        emit currentChanged(mCurrentIndex);
    }
}

void FancyTabBar::setTabEnabled(int index, bool enable)
{
    Q_ASSERT(index < mAttachedTabs.size());
    Q_ASSERT(index >= 0);

    if (index < mAttachedTabs.size() && index >= 0) {
        mAttachedTabs[index]->enabled = enable;
        update(tabRect(index));
    }
}

bool FancyTabBar::isTabEnabled(int index) const
{
    Q_ASSERT(index < mAttachedTabs.size());
    Q_ASSERT(index >= 0);

    if (index < mAttachedTabs.size() && index >= 0)
        return mAttachedTabs[index]->enabled;

    return false;
}
