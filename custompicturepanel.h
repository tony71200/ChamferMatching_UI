#ifndef CUSTOMPICTUREPANEL_H
#define CUSTOMPICTUREPANEL_H

#include <QWidget>
#include <QTableWidget>
#include "custompicturebox.h"

class CustomPicturePanel : public QWidget {
    Q_OBJECT
public:
    explicit CustomPicturePanel(QWidget *parent = nullptr);
    void setImage(const QPixmap &pixmap);
    void setMode(CustomPictureBox::Mode mode);
    CustomPictureBox* getPictureBox();
    void updateTable() {updateTableContents();}

signals:
    void templatesChanged(const QMap<int, CustomPictureBox::TemplateData> &templates);
    void roisChanged(const QMap<int, CustomPictureBox::TemplateData> &rois);

private slots:
    void onTemplatesChanged(const QMap<int, CustomPictureBox::TemplateData>& bboxes);
    void onRoisChanged(const QMap<int, CustomPictureBox::TemplateData>& rois);
    void onRequestInputRealLength(const QPoint& dialogPos);
    void onMeasurementChanged(const CustomPictureBox::MeasureLine& measureLine);
    void onTableItemClicked(QTableWidgetItem *item);
    void updateTableContents();

private:
    CustomPictureBox* pictureBox;
    QTableWidget* tableWidget;
    CustomPictureBox::Mode currentMode;

    void setupTable();
};

#endif // CUSTOMPICTUREPANEL_H 
