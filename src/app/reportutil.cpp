#include "reportutil.h"

#include <QtPrintSupport>
#include <QApplication>

#include "qwt_plot_renderer.h"


ReportUtil::ReportUtil()
{

}



//---------------------------------------------------------------------------------------------
void ReportUtil::printWorkoutToPdf(Workout workout, QwtPlot *plot, QString filename) {


    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filename);
    printer.setPageOrientation(QPageLayout::Landscape);

    // QPrinter::width()/height() were removed in Qt6; derive the paintable
    // pixel size from the page layout at the printer's resolution instead.
    const QRect paintRect = printer.pageLayout().paintRectPixels(printer.resolution());
    int width  = paintRect.width();
    int height = paintRect.height();
    qDebug() << "printer width is" << width << "height is:" << height;
    QPainter painter;
    painter.begin(&printer);



    /// --------- Draw Labels (bold) ---------------
    QFont font("Times", 9, QFont::Bold);
    font.setBold(true);
    painter.setFont(font);


    QString name = QApplication::translate("ReportUtil: ", "Name: ");
    QString type = QApplication::translate("ReportUtil: ", "Type: ");
    QString plan = QApplication::translate("ReportUtil: ", "Plan: ");
    QString creator = QApplication::translate("ReportUtil: ", "Creator: ");
    QString description = QApplication::translate("ReportUtil: ", "Description: ");
    QString signature = QApplication::translate("ReportUtil: ", "Workout created with MaximumTrainer.com");
    int sizeName = painter.fontMetrics().horizontalAdvance(name);
    int sizeType = painter.fontMetrics().horizontalAdvance(type);
    int sizePlan = painter.fontMetrics().horizontalAdvance(plan);
    int sizeCreator = painter.fontMetrics().horizontalAdvance(creator);
    int sizeDescription = painter.fontMetrics().horizontalAdvance(description);


    QRectF recNameLabel = QRectF(QPointF(0, 0), QPointF(width/2, 200));
    QRectF recTypeLabel = QRectF(QPointF(0, 200), QPointF(width/2, 400));
    QRectF recPlanLabel = QRectF(QPointF(0, 400), QPointF(width/2, 600));
    QRectF recCreatorLabel = QRectF(QPointF(0, 600), QPointF(width/2, 800));
    QRectF recDescriptionLabel = QRectF(QPointF(width/2 - sizeDescription, 0), QPointF(width/2, 200));

    painter.drawText(recNameLabel, Qt::AlignLeft, name);
    painter.drawText(recTypeLabel, Qt::AlignLeft, type);
    painter.drawText(recPlanLabel, Qt::AlignLeft, plan);
    painter.drawText(recCreatorLabel, Qt::AlignLeft, creator);
    painter.drawText(recDescriptionLabel, Qt::AlignLeft, description);

    font.setBold(false);
    painter.setFont(font);


    /// ------------------- Draw content -----------------
    QRectF recName = QRectF(QPointF(sizeName, 0), QPointF(width/2, 200));
    QRectF recType = QRectF(QPointF(sizeType, 200), QPointF(width/2, 400));
    QRectF recPlan = QRectF(QPointF(sizePlan, 400), QPointF(width/2, 600));
    QRectF recCreator = QRectF(QPointF(sizeCreator, 600), QPointF(width/2, 800));
    QRectF recDescription = QRectF(QPointF(width/2, 0), QPointF(width, 800));
    QRectF recSignature = QRectF(QPointF(0, height-200), QPointF(width, height));


    painter.drawText(recName, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, workout.getName());
    painter.drawText(recType, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, workout.getTypeToString());
    painter.drawText(recPlan, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, workout.getPlan());
    painter.drawText(recCreator, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, workout.getCreatedBy());
    painter.drawText(recDescription, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, workout.getDescription());
    painter.drawText(recSignature, Qt::AlignTop | Qt::AlignRight | Qt::TextWordWrap, signature);


    // draw QwtPlot
    QwtPlotRenderer renderer;
    renderer.setDiscardFlag(QwtPlotRenderer::DiscardBackground, true);
    renderer.setDiscardFlag(QwtPlotRenderer::DiscardCanvasBackground, true);
    renderer.setDiscardFlag(QwtPlotRenderer::DiscardCanvasFrame, true);
    renderer.render(plot, &painter, QRectF(QPointF(0,1000), QPointF(width,height-200)));


    painter.end();
}
