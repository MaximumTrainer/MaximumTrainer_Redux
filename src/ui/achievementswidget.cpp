#include "achievementswidget.h"

#include <QApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QScrollArea>
#include <QVBoxLayout>

#include "achievementcatalog.h"
#include "achievementstore.h"
#include "managerachievement.h"

using namespace AchievementCatalog;

namespace {
constexpr int kColumns = 3;

QPixmap greyed(const QPixmap &source)
{
    QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const int grey = qGray(line[x]);
            line[x] = qRgba(grey, grey, grey, qAlpha(line[x]) * 2 / 5);
        }
    }
    return QPixmap::fromImage(image);
}

QString progressText(const Definition &definition, double value)
{
    const auto number = [&](double v) {
        if (definition.kind == Kind::TotalHours || definition.kind == Kind::WeekHours
            || definition.kind == Kind::WattsPerKg)
            return QString::number(v, 'f', 1);
        return QLocale().toString(qRound(v));
    };
    QString unit;
    switch (definition.kind) {
    case Kind::TotalHours:
    case Kind::WeekHours:   unit = QStringLiteral(" h");   break;
    case Kind::WattsPerKg:  unit = QStringLiteral(" W/kg"); break;
    case Kind::TotalKm:
    case Kind::RideKm:      unit = QStringLiteral(" km");  break;
    case Kind::RideMinutes: unit = QStringLiteral(" min"); break;
    case Kind::RideTss:
    case Kind::WeekTss:     unit = QStringLiteral(" TSS"); break;
    case Kind::RideKcal:    unit = QStringLiteral(" kcal"); break;
    case Kind::StreakDays:  unit = QStringLiteral(" days"); break;
    default: break;
    }
    return number(qMin(value, definition.target)) + QStringLiteral(" / ") + number(definition.target) + unit;
}
} // namespace

AchievementsWidget::AchievementsWidget(QWidget *parent) : QWidget(parent)
{
    m_manager = qApp->property("ManagerAchievement").value<ManagerAchievement*>();
    m_trophy = QPixmap(QStringLiteral(":/image/icon/trophy"))
                   .scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_trophyLocked = greyed(m_trophy);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    auto *header = new QHBoxLayout();
    auto *icon = new QLabel(this);
    icon->setPixmap(m_trophy.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    header->addWidget(icon);
    auto *titles = new QVBoxLayout();
    titles->setSpacing(2);
    auto *title = new QLabel(tr("Achievements"), this);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 4);
    titleFont.setBold(true);
    title->setFont(titleFont);
    m_summary = new QLabel(this);
    titles->addWidget(title);
    titles->addWidget(m_summary);
    header->addLayout(titles);
    header->addStretch();
    m_overall = new QProgressBar(this);
    m_overall->setTextVisible(false);
    m_overall->setFixedSize(220, 10);
    m_overall->setStyleSheet(QStringLiteral(
        "QProgressBar { border: none; border-radius: 5px; background: rgba(140,140,150,70); }"
        "QProgressBar::chunk { border-radius: 5px; background: #f2b830; }"));
    header->addWidget(m_overall);
    mainLayout->addLayout(header);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_content = new QWidget(scroll);
    m_sections = new QVBoxLayout(m_content);
    m_sections->setContentsMargins(0, 0, 0, 0);
    m_sections->setSpacing(14);
    scroll->setWidget(m_content);
    mainLayout->addWidget(scroll, 1);

    if (m_manager)
        connect(m_manager, &ManagerAchievement::achievementsChanged, this, &AchievementsWidget::render);
    render();
}

void AchievementsWidget::reload()
{
    if (m_manager)
        m_manager->evaluateHistory(false);   // emits achievementsChanged -> render()
    else
        render();
}

QWidget *AchievementsWidget::buildCard(const QString &key, QWidget *parent)
{
    const Definition *definition = AchievementCatalog::find(key);
    const QMap<QString, QDateTime> earned = AchievementStore::unlocked();
    const bool isEarned = earned.contains(key);

    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("achievementCard"));
    card->setStyleSheet(isEarned
        ? QStringLiteral("QFrame#achievementCard { border: 2px solid #f2b830; border-radius: 10px; background: palette(base); }")
        : QStringLiteral("QFrame#achievementCard { border: 1px solid palette(mid); border-radius: 10px; background: palette(base); }"));
    card->setMinimumHeight(96);

    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(12);
    auto *icon = new QLabel(card);
    icon->setPixmap(isEarned ? m_trophy : m_trophyLocked);
    icon->setAlignment(Qt::AlignTop);
    layout->addWidget(icon);

    auto *text = new QVBoxLayout();
    text->setSpacing(3);
    auto *name = new QLabel(definition->name, card);
    QFont nameFont = name->font();
    nameFont.setBold(true);
    nameFont.setPointSizeF(nameFont.pointSizeF() + 1);
    name->setFont(nameFont);
    if (!isEarned)
        name->setStyleSheet(QStringLiteral("color: #a0a0aa;"));
    auto *description = new QLabel(definition->description, card);
    description->setWordWrap(true);
    description->setStyleSheet(QStringLiteral("color: #8a8a94;"));
    text->addWidget(name);
    text->addWidget(description);

    if (isEarned) {
        auto *date = new QLabel(tr("✓ Earned %1").arg(QLocale().toString(earned.value(key).date(), QStringLiteral("d MMM yyyy"))), card);
        date->setStyleSheet(QStringLiteral("color: #d99a1c; font-weight: bold;"));
        text->addWidget(date);
    } else if (!isYesNo(*definition) && m_manager) {
        const double value = progress(*definition, m_manager->historyStats());
        auto *row = new QHBoxLayout();
        auto *bar = new QProgressBar(card);
        bar->setTextVisible(false);
        bar->setFixedHeight(6);
        bar->setRange(0, 1000);
        bar->setValue(qRound(qMin(1.0, value / definition->target) * 1000));
        bar->setStyleSheet(QStringLiteral(
            "QProgressBar { border: none; border-radius: 3px; background: rgba(140,140,150,70); }"
            "QProgressBar::chunk { border-radius: 3px; background: #8a8a94; }"));
        auto *count = new QLabel(progressText(*definition, value), card);
        count->setStyleSheet(QStringLiteral("color: #8a8a94; font-size: 9pt;"));
        row->addWidget(bar, 1);
        row->addWidget(count);
        text->addLayout(row);
    } else {
        auto *locked = new QLabel(tr("Not earned yet"), card);
        locked->setStyleSheet(QStringLiteral("color: #8a8a94; font-size: 9pt;"));
        text->addWidget(locked);
    }
    text->addStretch();
    layout->addLayout(text, 1);
    return card;
}

void AchievementsWidget::render()
{
    // Rebuild the sections from scratch: the list is small.
    while (QLayoutItem *item = m_sections->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    const QMap<QString, QDateTime> earned = AchievementStore::unlocked();
    int earnedCount = 0;
    for (const Definition &definition : all())
        if (earned.contains(definition.key))
            ++earnedCount;
    m_summary->setText(tr("%1 of %2 earned").arg(earnedCount).arg(all().size()));
    m_overall->setRange(0, all().size());
    m_overall->setValue(earnedCount);

    for (Category category : {Category::Milestones, Category::Endurance, Category::Consistency,
                              Category::Fitness, Category::Power, Category::Special}) {
        auto *section = new QWidget(m_content);
        auto *sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
        sectionLayout->setSpacing(8);
        auto *heading = new QLabel(categoryName(category), section);
        QFont headingFont = heading->font();
        headingFont.setBold(true);
        headingFont.setPointSizeF(headingFont.pointSizeF() + 1);
        heading->setFont(headingFont);
        sectionLayout->addWidget(heading);

        auto *grid = new QGridLayout();
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(10);
        int index = 0;
        for (const Definition &definition : all()) {
            if (definition.category != category)
                continue;
            grid->addWidget(buildCard(definition.key, section), index / kColumns, index % kColumns);
            ++index;
        }
        for (int column = 0; column < kColumns; ++column)
            grid->setColumnStretch(column, 1);
        sectionLayout->addLayout(grid);
        m_sections->addWidget(section);
    }
    m_sections->addStretch();
}
