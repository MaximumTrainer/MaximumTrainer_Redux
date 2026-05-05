/********************************************************************************
** Form generated from reading UI file 'repeatwidget.ui'
**
** Created by: Qt User Interface Compiler version 5.15.13
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_REPEATWIDGET_H
#define UI_REPEATWIDGET_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_RepeatWidget
{
public:
    QHBoxLayout *horizontalLayout;
    QWidget *widget_left;
    QHBoxLayout *horizontalLayout_2;
    QFrame *frame_border_left;
    QHBoxLayout *horizontalLayout_3;
    QSpacerItem *horizontalSpacer;
    QWidget *widget_right;
    QHBoxLayout *horizontalLayout_4;
    QFrame *frame_border_right;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout_5;
    QComboBox *comboBox_repeat;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *pushButton_delete;
    QSpacerItem *verticalSpacer;

    void setupUi(QWidget *RepeatWidget)
    {
        if (RepeatWidget->objectName().isEmpty())
            RepeatWidget->setObjectName(QString::fromUtf8("RepeatWidget"));
        RepeatWidget->resize(682, 80);
        QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(RepeatWidget->sizePolicy().hasHeightForWidth());
        RepeatWidget->setSizePolicy(sizePolicy);
        RepeatWidget->setMinimumSize(QSize(0, 0));
        RepeatWidget->setWindowOpacity(1.000000000000000);
        RepeatWidget->setAutoFillBackground(false);
        RepeatWidget->setStyleSheet(QString::fromUtf8("\n"
"#frame_border_left {\n"
" 	border-left : 3px solid qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 rgba(7,0,62,255), stop:1 rgba(13, 0, 158, 255));\n"
"	border-top: 3px solid qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 rgba(7,0,62,255), stop:1 rgba(13, 0, 158, 255));\n"
"	border-bottom: 3px solid qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 rgba(7,0,62,255), stop:1 rgba(13, 0, 158, 255));\n"
"	border-right : 0px;\n"
"}\n"
"\n"
"\n"
"\n"
"#frame_border_right {\n"
"	border-left : 0px;\n"
" 	border-right : 3px solid qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 rgba(7,0,62,255), stop:1 rgba(13, 0, 158, 255));\n"
"	border-top: 3px solid qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 rgba(7,0,62,255), stop:1 rgba(13, 0, 158, 255));\n"
"	border-bottom: 3px solid qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 rgba(7,0,62,255), stop:1 rgba(13, 0, 158, 255));\n"
"	\n"
"	background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0,"
                        " stop:0 rgba(229, 229, 229, 255), stop:1 rgba(255, 255, 255, 255));\n"
"}\n"
"\n"
"QLabel#label_repeat{\n"
"	image: url(:/image/icon/repeat);\n"
"}\n"
"\n"
"\n"
"/*\n"
"QPushButton#pushButton_delete:hover{\n"
"	border:1px black;\n"
"*/\n"
"\n"
""));
        horizontalLayout = new QHBoxLayout(RepeatWidget);
        horizontalLayout->setSpacing(0);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        widget_left = new QWidget(RepeatWidget);
        widget_left->setObjectName(QString::fromUtf8("widget_left"));
        horizontalLayout_2 = new QHBoxLayout(widget_left);
        horizontalLayout_2->setSpacing(0);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        horizontalLayout_2->setContentsMargins(0, 0, 0, 0);
        frame_border_left = new QFrame(widget_left);
        frame_border_left->setObjectName(QString::fromUtf8("frame_border_left"));
        frame_border_left->setFrameShape(QFrame::StyledPanel);
        frame_border_left->setFrameShadow(QFrame::Raised);
        frame_border_left->setLineWidth(0);
        horizontalLayout_3 = new QHBoxLayout(frame_border_left);
        horizontalLayout_3->setSpacing(0);
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        horizontalLayout_3->setContentsMargins(0, 0, 0, 0);
        horizontalSpacer = new QSpacerItem(512, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer);


        horizontalLayout_2->addWidget(frame_border_left);


        horizontalLayout->addWidget(widget_left);

        widget_right = new QWidget(RepeatWidget);
        widget_right->setObjectName(QString::fromUtf8("widget_right"));
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(widget_right->sizePolicy().hasHeightForWidth());
        widget_right->setSizePolicy(sizePolicy1);
        widget_right->setMinimumSize(QSize(0, 0));
        widget_right->setMaximumSize(QSize(16777215, 16777215));
        horizontalLayout_4 = new QHBoxLayout(widget_right);
        horizontalLayout_4->setSpacing(0);
        horizontalLayout_4->setObjectName(QString::fromUtf8("horizontalLayout_4"));
        horizontalLayout_4->setContentsMargins(0, 0, 0, 0);
        frame_border_right = new QFrame(widget_right);
        frame_border_right->setObjectName(QString::fromUtf8("frame_border_right"));
        frame_border_right->setFrameShape(QFrame::StyledPanel);
        frame_border_right->setFrameShadow(QFrame::Raised);
        frame_border_right->setLineWidth(0);
        verticalLayout = new QVBoxLayout(frame_border_right);
        verticalLayout->setSpacing(0);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(6, 0, 0, 0);
        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setSpacing(0);
        horizontalLayout_5->setObjectName(QString::fromUtf8("horizontalLayout_5"));
        comboBox_repeat = new QComboBox(frame_border_right);
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->addItem(QString());
        comboBox_repeat->setObjectName(QString::fromUtf8("comboBox_repeat"));
        QSizePolicy sizePolicy2(QSizePolicy::Minimum, QSizePolicy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(comboBox_repeat->sizePolicy().hasHeightForWidth());
        comboBox_repeat->setSizePolicy(sizePolicy2);
        comboBox_repeat->setMinimumSize(QSize(70, 0));
        comboBox_repeat->setMaximumSize(QSize(70, 16777215));

        horizontalLayout_5->addWidget(comboBox_repeat);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_5->addItem(horizontalSpacer_2);

        pushButton_delete = new QPushButton(frame_border_right);
        pushButton_delete->setObjectName(QString::fromUtf8("pushButton_delete"));
        QSizePolicy sizePolicy3(QSizePolicy::Minimum, QSizePolicy::Minimum);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(pushButton_delete->sizePolicy().hasHeightForWidth());
        pushButton_delete->setSizePolicy(sizePolicy3);
        pushButton_delete->setMinimumSize(QSize(25, 25));
        pushButton_delete->setMaximumSize(QSize(25, 25));
        pushButton_delete->setCursor(QCursor(Qt::PointingHandCursor));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/image/icon/delete"), QSize(), QIcon::Normal, QIcon::Off);
        pushButton_delete->setIcon(icon);

        horizontalLayout_5->addWidget(pushButton_delete);


        verticalLayout->addLayout(horizontalLayout_5);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);


        horizontalLayout_4->addWidget(frame_border_right);


        horizontalLayout->addWidget(widget_right);


        retranslateUi(RepeatWidget);

        QMetaObject::connectSlotsByName(RepeatWidget);
    } // setupUi

    void retranslateUi(QWidget *RepeatWidget)
    {
        RepeatWidget->setWindowTitle(QCoreApplication::translate("RepeatWidget", "Form", nullptr));
        comboBox_repeat->setItemText(0, QCoreApplication::translate("RepeatWidget", "2", nullptr));
        comboBox_repeat->setItemText(1, QCoreApplication::translate("RepeatWidget", "3", nullptr));
        comboBox_repeat->setItemText(2, QCoreApplication::translate("RepeatWidget", "4", nullptr));
        comboBox_repeat->setItemText(3, QCoreApplication::translate("RepeatWidget", "5", nullptr));
        comboBox_repeat->setItemText(4, QCoreApplication::translate("RepeatWidget", "6", nullptr));
        comboBox_repeat->setItemText(5, QCoreApplication::translate("RepeatWidget", "7", nullptr));
        comboBox_repeat->setItemText(6, QCoreApplication::translate("RepeatWidget", "8", nullptr));
        comboBox_repeat->setItemText(7, QCoreApplication::translate("RepeatWidget", "9", nullptr));
        comboBox_repeat->setItemText(8, QCoreApplication::translate("RepeatWidget", "10", nullptr));
        comboBox_repeat->setItemText(9, QCoreApplication::translate("RepeatWidget", "11", nullptr));
        comboBox_repeat->setItemText(10, QCoreApplication::translate("RepeatWidget", "12", nullptr));
        comboBox_repeat->setItemText(11, QCoreApplication::translate("RepeatWidget", "13", nullptr));
        comboBox_repeat->setItemText(12, QCoreApplication::translate("RepeatWidget", "14", nullptr));
        comboBox_repeat->setItemText(13, QCoreApplication::translate("RepeatWidget", "15", nullptr));
        comboBox_repeat->setItemText(14, QCoreApplication::translate("RepeatWidget", "16", nullptr));
        comboBox_repeat->setItemText(15, QCoreApplication::translate("RepeatWidget", "17", nullptr));
        comboBox_repeat->setItemText(16, QCoreApplication::translate("RepeatWidget", "18", nullptr));
        comboBox_repeat->setItemText(17, QCoreApplication::translate("RepeatWidget", "19", nullptr));
        comboBox_repeat->setItemText(18, QCoreApplication::translate("RepeatWidget", "20", nullptr));

        pushButton_delete->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class RepeatWidget: public Ui_RepeatWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_REPEATWIDGET_H
