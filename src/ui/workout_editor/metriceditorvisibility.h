#ifndef METRICEDITORVISIBILITY_H
#define METRICEDITORVISIBILITY_H

#include <QWidget>
#include <QPushButton>
#include <QStyle>

// PowerEditor / HrEditor / CadenceEditor share an identical
// None / Flat / Progressive show-hide layout driven by their step-type
// combobox. The widget set is the same shape in every editor — only the
// concrete ui-> pointers differ — so the visibility rule lives here once.
//
// stepTypeIndex: 0 = None (hide everything), 1 = Flat (single target, no
// "to" range), anything else = Progressive (start..end range).
namespace MetricEditorVisibility {

inline void applyStepTypeVisibility(int stepTypeIndex,
                                    QWidget *startSpinBox,
                                    QWidget *toLabel,
                                    QWidget *endSpinBox,
                                    QWidget *unitWidget,
                                    QWidget *acceptedRangeLabel,
                                    QWidget *rangeSpinBox,
                                    QWidget *acceptedUnitLabel)
{
    const bool isNone = (stepTypeIndex == 0);
    const bool isFlat = (stepTypeIndex == 1);

    const bool showCommonControls = !isNone;            // Flat or Progressive
    const bool showEndTarget = !isNone && !isFlat;      // Progressive only

    startSpinBox->setVisible(showCommonControls);
    unitWidget->setVisible(showCommonControls);
    acceptedRangeLabel->setVisible(showCommonControls);
    rangeSpinBox->setVisible(showCommonControls);
    acceptedUnitLabel->setVisible(showCommonControls);

    toLabel->setVisible(showEndTarget);
    endSpinBox->setVisible(showEndTarget);
}

// The editors share two icon-only action buttons. Give them the native OS
// Ok/Cancel icons (matching the Repeat-change editor) and drop any text so they
// stay compact.
inline void applyNativeOkCancelButtons(QPushButton *okButton, QPushButton *cancelButton)
{
    okButton->setText(QString());
    okButton->setIcon(okButton->style()->standardIcon(QStyle::SP_DialogOkButton));
    okButton->setToolTip(QObject::tr("Confirm"));

    cancelButton->setText(QString());
    cancelButton->setIcon(cancelButton->style()->standardIcon(QStyle::SP_DialogCancelButton));
    cancelButton->setToolTip(QObject::tr("Cancel"));
}

} // namespace MetricEditorVisibility

#endif // METRICEDITORVISIBILITY_H
