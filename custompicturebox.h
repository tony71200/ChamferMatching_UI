#ifndef CUSTOMPICTUREBOX_H
#define CUSTOMPICTUREBOX_H

#include <QObject>
#include <QWidget>
#include <QPixmap>
#include <QPainter>
#include <QMouseEvent>
#include <QRect>
#include <QMap>
#include <QDebug>

class CustomPictureBox : public QWidget
{
    Q_OBJECT
public:
    struct TemplateData {
        int id;
        QRect roi;
        QPixmap image;
        QString name;
    };

    enum Mode {
        Training, Setting, Test
    };

    enum Handle {
        None, TopLeft, BottomRight, Body
    };

    explicit CustomPictureBox(QWidget *parent = nullptr);

    void setImage(const QPixmap &pixmap);
    void setMode(Mode mode);
    Mode getMode() const;
    const QMap<int, TemplateData>& getBoundingBoxes() const;
    const QMap<int, TemplateData>& getRois() const;
    void clearAll();
    QSize getOriginalImageSize() const;
    void setRois(const QMap<int, TemplateData>& newRois);


signals:
    void templatesChanged(const QMap<int, TemplateData> &templates);
    void roisChanged(const QMap<int, TemplateData> &rois);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void updateScaledPixmap();
    QRectF getScaledRect(const QRect &rect) const;
    QRect getOriginalRect(const QRectF &rect) const;
    QRectF getHandleRect(const QRectF &rect, Handle handle) const;
    void setCursorForPosition(const QPoint &pos);
    Handle getHandleAtPosition(const QPoint &pos) const;

    Mode currentMode;
    QPixmap originalPixmap;
    QPixmap scaledPixmap;
    QRectF targetRect; // The area within the widget where the pixmap is drawn

    bool isDrawing;
    QPoint startPoint;
    QRect currentDrawingBox;

    QMap<int, TemplateData> bboxes; // For Training mode
    QMap<int, TemplateData> rois;   // For Setting mode
    int nextBboxId;
    int nextRoiId;
    static const int MAX_BBOXES = 2;
    static const int MAX_ROIS = 2;
    static const QSize FIXED_SIZE;

    // ROI Editing members
    mutable int editingRoiId;
    Handle currentHandle;
    QPoint dragStartPoint;
    QRect originalRoiRect;
    bool isEditing;

public slots:
};

#endif // CUSTOMPICTUREBOX_H
