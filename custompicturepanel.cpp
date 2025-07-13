#include "custompicturepanel.h"
#include <QVBoxLayout>
#include <QInputDialog>
#include <QHeaderView>
#include <QDebug>

CustomPicturePanel::CustomPicturePanel(QWidget *parent)
    : QWidget(parent), currentMode(CustomPictureBox::Test)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    pictureBox = new CustomPictureBox(this);
    tableWidget = new QTableWidget(this);

    layout->addWidget(pictureBox, 1); // Give more space to the picturebox
    layout->addWidget(tableWidget, 0);
    layout->setSpacing(4);
    layout->setContentsMargins(0,0,0,0);
    setLayout(layout);

    setupTable();

    // Connect table selection to picturebox
    connect(tableWidget, &QTableWidget::itemClicked, this, &CustomPicturePanel::onTableItemClicked);

    // Connect picturebox changes to update the table
    connect(pictureBox, &CustomPictureBox::templatesChanged, this, &CustomPicturePanel::onTemplatesChanged);
    connect(pictureBox, &CustomPictureBox::roisChanged, this, &CustomPicturePanel::onRoisChanged);
    connect(pictureBox, &CustomPictureBox::measurementChanged, this, &CustomPicturePanel::onMeasurementChanged);
    connect(pictureBox, &CustomPictureBox::requestInputRealLength, this, &CustomPicturePanel::onRequestInputRealLength);

    // Forward signals for external use
    connect(pictureBox, &CustomPictureBox::templatesChanged, this, &CustomPicturePanel::templatesChanged);
    connect(pictureBox, &CustomPictureBox::roisChanged, this, &CustomPicturePanel::roisChanged);
}

void CustomPicturePanel::setupTable()
{
    tableWidget->setRowCount(5);
    tableWidget->setColumnCount(2);

    tableWidget->setHorizontalHeaderLabels({"Item", "Value"});
    tableWidget->horizontalHeader()->setVisible(false);
    tableWidget->verticalHeader()->setVisible(false);

    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    tableWidget->setFocusPolicy(Qt::NoFocus);

    QStringList rowLabels = {"Template 1", "Template 2", "ROI 1", "ROI 2", "Distance"};
    for (int i = 0; i < rowLabels.size(); ++i) {
        QTableWidgetItem* item = new QTableWidgetItem(rowLabels[i]);
        tableWidget->setItem(i, 0, item);
        tableWidget->setItem(i, 1, new QTableWidgetItem("N/A"));
    }

    tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    // Set fixed height for 5 rows
    int height = tableWidget->horizontalHeader()->height();
    for(int i = 0; i < 5; ++i) {
        height += tableWidget->rowHeight(i);
    }
    tableWidget->setFixedHeight(height + 5); // Add a small margin
}


void CustomPicturePanel::setImage(const QPixmap &pixmap) {
    pictureBox->setImage(pixmap);
    updateTableContents();
}

void CustomPicturePanel::setMode(CustomPictureBox::Mode mode) {
    currentMode = mode;
    pictureBox->setMode(mode);
    qDebug() << "CustomPicturePanel::setMode" << mode;
    if (mode == CustomPictureBox::Test) {
        tableWidget->setVisible(false);
        this->update();
        return;
    } else {
        tableWidget->setVisible(true);
        this->update();
    }
    updateTableContents();

    // Highlight relevant rows for the current mode
    for (int i = 0; i < tableWidget->rowCount(); ++i) {
        bool isRelevant = false;
        if (currentMode == CustomPictureBox::Training && (i == 0 || i == 1)) isRelevant = true;
        if (currentMode == CustomPictureBox::Setting && (i == 2 || i == 3)) isRelevant = true;
        if (currentMode == CustomPictureBox::Measure && i == 4) isRelevant = true;

        QColor color = isRelevant ? QColor(230, 240, 255) : QColor(Qt::white);
        tableWidget->item(i, 0)->setBackground(color);
        tableWidget->item(i, 1)->setBackground(color);
    }
}

CustomPictureBox* CustomPicturePanel::getPictureBox() {
    return pictureBox;
}

void CustomPicturePanel::updateTableContents() {
    const auto& bboxes = pictureBox->getBoundingBoxes();
    const auto& rois = pictureBox->getRois();

    // Update Template 1
    auto itemT1 = tableWidget->item(0, 1);
    if (bboxes.contains(0)) {
        const auto& data = bboxes[0];
        itemT1->setText(QString("(%1, %2, %3, %4)").arg(data.roi.x()).arg(data.roi.y()).arg(data.roi.width()).arg(data.roi.height()));
    } else {
        itemT1->setText("N/A");
    }

    // Update Template 2
    auto itemT2 = tableWidget->item(1, 1);
    if (bboxes.contains(1)) {
        const auto& data = bboxes[1];
        itemT2->setText(QString("(%1, %2, %3, %4)").arg(data.roi.x()).arg(data.roi.y()).arg(data.roi.width()).arg(data.roi.height()));
    } else {
        itemT2->setText("N/A");
    }

    // Update ROI 1
    auto itemR1 = tableWidget->item(2, 1);
    if (rois.contains(0)) {
        const auto& data = rois[0];
        itemR1->setText(QString("(%1, %2, %3, %4)").arg(data.roi.x()).arg(data.roi.y()).arg(data.roi.width()).arg(data.roi.height()));
    } else {
        itemR1->setText("N/A");
    }

    // Update ROI 2
    auto itemR2 = tableWidget->item(3, 1);
    if (rois.contains(1)) {
        const auto& data = rois[1];
        itemR2->setText(QString("(%1, %2, %3, %4)").arg(data.roi.x()).arg(data.roi.y()).arg(data.roi.width()).arg(data.roi.height()));
    } else {
        itemR2->setText("N/A");
    }
}


void CustomPicturePanel::onTableItemClicked(QTableWidgetItem* item) {
    if (!item) return;
    int row = item->row();
    if (currentMode == CustomPictureBox::Training) {
        if (row == 0) pictureBox->selectBbox(0);
        else if (row == 1) pictureBox->selectBbox(1);
    } else if (currentMode == CustomPictureBox::Setting) {
        if (row == 2) pictureBox->selectRoi(0);
        else if (row == 3) pictureBox->selectRoi(1);
    }
}

void CustomPicturePanel::onTemplatesChanged(const QMap<int, CustomPictureBox::TemplateData>& bboxes) {
    Q_UNUSED(bboxes);
    if (currentMode == CustomPictureBox::Training)
        updateTableContents();
}

void CustomPicturePanel::onRoisChanged(const QMap<int, CustomPictureBox::TemplateData>& rois) {
    Q_UNUSED(rois);
    if (currentMode == CustomPictureBox::Setting)
        updateTableContents();
}

void CustomPicturePanel::onMeasurementChanged(const CustomPictureBox::MeasureLine &measureLine)
{
    if(measureLine.valid) {
        QString text = QString("%1 mm / px").arg(measureLine.mmPerPixels, 0, 'f', 5);
        tableWidget->item(4, 1)->setText(text);
    } else {
        tableWidget->item(4, 1)->setText("Invalid");
    }
}

void CustomPicturePanel::onRequestInputRealLength(const QPoint& dialogPos) {
    bool ok = false;
    double mm = QInputDialog::getDouble(this, tr("Input real distance (mm)"), tr("Real Distance (mm):"), 10.0, 0.01, 10000.0, 2, &ok);
    if (ok) {
        pictureBox->onInputRealLength(mm);
    }
} 
