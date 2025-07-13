#include "custompicturebox.h"
#include <QPainterPath>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>

const QSize CustomPictureBox::FIXED_SIZE = QSize(50, 50);

CustomPictureBox::CustomPictureBox(QWidget *parent)
    : QWidget(parent),
      currentMode(Test),
      isDrawing(false),
      nextBboxId(0),
      nextRoiId(0),
      editingRoiId(-1),
      currentHandle(None),
      isEditing(false)
{
    setMouseTracking(true); // Enable mouse tracking for cursor changes
}

void CustomPictureBox::setImage(const QPixmap &pixmap)
{
    originalPixmap = pixmap;
    clearAll(); // This will also clear match results
    updateScaledPixmap();
    update(); // Schedule a repaint
}

void CustomPictureBox::setMode(Mode mode)
{
    if (currentMode != mode) {
        currentMode = mode;
        isDrawing = false;
        editingRoiId = -1;
        currentHandle = None;
        isEditing = false;
        update();
    }
}

CustomPictureBox::Mode CustomPictureBox::getMode() const {
    return currentMode;
}

const QMap<int, CustomPictureBox::TemplateData> &CustomPictureBox::getBoundingBoxes() const {
    return bboxes;
}

const QMap<int, CustomPictureBox::TemplateData> &CustomPictureBox::getRois() const
{
    return rois;
}

void CustomPictureBox::clearAll()
{
    bboxes.clear();
    rois.clear();
    clearMatchResults();
    nextBboxId = 0;
    nextRoiId = 0;
    isDrawing = false;
    editingRoiId = -1;
    currentHandle = None;
    isEditing = false;
    measureLine.valid = false;
    update();
}

void CustomPictureBox::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Fill background
    painter.fillRect(rect(), Qt::darkGray);

    if (scaledPixmap.isNull()) return;

    painter.drawPixmap(targetRect.topLeft(), scaledPixmap);

    // Draw Match Results (Rotated Rectangles) in Test and Training modes
    if (currentMode == Test || currentMode == Training) {
        painter.setPen(QPen(Qt::magenta, 3));
        for (const auto& result : matchResults) {
            if (result.size() < 6) continue;
            // Points are stored in original coordinates, scale them for drawing
            QPointF p2 = mapOriginalToWidget(result[2]);
            QPointF p3 = mapOriginalToWidget(result[3]);
            QPointF p4 = mapOriginalToWidget(result[4]);
            QPointF p5 = mapOriginalToWidget(result[5]);

            QPointF p0 = mapOriginalToWidget(result[0]);
            QPointF p1 = mapOriginalToWidget(result[1]);

            painter.drawLine(p0, p1);

            painter.drawLine(p2, p3);
            painter.drawLine(p3, p4);
            painter.drawLine(p4, p5);
            painter.drawLine(p5, p2);
        }
    }

    if (currentMode != Measure)
    {
        // Draw Templates (bboxes)
        for (const TemplateData &data : bboxes) {
            bool isSelected = (currentMode == Training && data.id == editingRoiId);
            if (currentMode == Training) {
                painter.setPen(isSelected ? QPen(Qt::cyan, 2) : QPen(Qt::green, 2));
            } else {
                painter.setPen(QPen(QColor(0, 128, 0, 128), 2)); // mờ khi không phải mode
            }
            QRectF scaledRect = getScaledRect(data.roi);
            painter.drawRect(scaledRect);
            // Draw template name
            painter.setPen(Qt::white);
            painter.drawText(scaledRect.topLeft() + QPointF(5, 15), data.name);
            if (isSelected) {
                painter.fillRect(scaledRect, QColor(0, 255, 255, 30));
                painter.drawRect(getHandleRect(scaledRect, TopLeft));
                painter.drawRect(getHandleRect(scaledRect, BottomRight));
            }
        }

        // Draw ROIs
        for (const TemplateData &data : rois) {
            QRectF scaledRoi = getScaledRect(data.roi);
            bool isSelected = (currentMode == Setting && data.id == editingRoiId);
            if (currentMode == Setting) {
                painter.setPen(isSelected ? QPen(Qt::cyan, 2) : QPen(Qt::blue, 5));
            } else {
                painter.setPen(QPen(QColor(0, 0, 128, 128), 5));
            }
            painter.drawRect(scaledRoi);
            painter.setPen(Qt::white);
            painter.drawText(scaledRoi.topLeft() + QPointF(5, 15), data.name);
            if (isSelected) {
                painter.fillRect(scaledRoi, QColor(0, 255, 255, 30));
                painter.drawRect(getHandleRect(scaledRoi, TopLeft));
                painter.drawRect(getHandleRect(scaledRoi, BottomRight));
            }
        }
    }


    // Draw the box currently being drawn
    if (isDrawing) {
        if (currentMode == Training) {
            painter.setPen(QPen(Qt::green, 2, Qt::DashLine));
        } else if (currentMode == Setting) {
            painter.setPen(QPen(Qt::red, 2, Qt::DashLine));
        }
        painter.drawRect(currentDrawingBox);
        // Draw the two diagonal lines
        painter.drawLine(currentDrawingBox.topLeft(), currentDrawingBox.bottomRight());
        painter.drawLine(currentDrawingBox.topRight(), currentDrawingBox.bottomLeft());
    }

    // Draw measure line and mm/pixels
    if (currentMode == Measure) {
        painter.setPen(QPen(Qt::yellow, 2, Qt::DashLine));

        if (isMeasuring) {
            painter.drawLine(measureStart, measureEnd);
        } else if (measureLine.valid) {
            QPointF start = mapOriginalToWidget(measureLine.originalStart);
            QPointF end = mapOriginalToWidget(measureLine.originalEnd);
            painter.drawLine(start, end);
            painter.setPen(Qt::yellow);
            painter.drawText(10, 20, QString("mm/pixel: %1").arg(measureLine.mmPerPixels, 0, 'f', 3));
        }
    }
}

void CustomPictureBox::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    updateScaledPixmap();
}

void CustomPictureBox::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !targetRect.contains(event->pos())) {
        return;
    }
    if (currentMode == Measure) {
        isMeasuring = true;
        measureStart = event->pos();
        measureEnd = event->pos();
        measureLine.originalStart = mapWidgetToOriginal(measureStart);
        measureLine.valid = false;
        update();
        return;
    }
    if (currentMode == Setting) {
        Handle handle = getHandleAtPosition(event->pos());
        if (handle != None) {
            isEditing = true;
            currentHandle = handle;
            dragStartPoint = event->pos();
            originalRoiRect = rois[editingRoiId].roi;
            update();
            return;
        }
    }
    if (currentMode == Training) {
        Handle handle = getHandleAtPosition(event->pos());
        if (handle != None) {
            isEditing = true;
            currentHandle = handle;
            dragStartPoint = event->pos();
            originalRoiRect = bboxes[editingRoiId].roi;
            update();
            return;
        }
    }
    // Deselect nếu click vùng trống
    if (editingRoiId != -1) {
        editingRoiId = -1;
        isEditing = false;
        update();
    }
    if (currentMode == Training || currentMode == Setting) {
        isDrawing = true;
        startPoint = event->pos();
        currentDrawingBox = QRect(startPoint, startPoint);
    }
}

void CustomPictureBox::mouseMoveEvent(QMouseEvent *event)
{
    if (currentMode == Measure && isMeasuring) {
        measureEnd = event->pos();
        update();
        return;
    }
    if (currentMode == Setting && isEditing) {
        QRectF scaledOriginalRoi = getScaledRect(originalRoiRect);
        QPoint delta = event->pos() - dragStartPoint;
        QRectF newScaledRect = scaledOriginalRoi;
        if (currentHandle == TopLeft) {
            newScaledRect.setTopLeft(scaledOriginalRoi.topLeft() + delta);
        } else if (currentHandle == BottomRight) {
            newScaledRect.setBottomRight(scaledOriginalRoi.bottomRight() + delta);
        } else if (currentHandle == Body) {
            newScaledRect.translate(delta);
        }
        
        // Clamp movement/resizing within the target rect
        newScaledRect = newScaledRect.normalized();
        if(newScaledRect.left() < targetRect.left()) newScaledRect.setLeft(targetRect.left());
        if(newScaledRect.top() < targetRect.top()) newScaledRect.setTop(targetRect.top());
        if(newScaledRect.right() > targetRect.right()) newScaledRect.setRight(targetRect.right());
        if(newScaledRect.bottom() > targetRect.bottom()) newScaledRect.setBottom(targetRect.bottom());
        rois[editingRoiId].roi = getOriginalRect(newScaledRect);
        update();
    } else if (currentMode == Training && isEditing) {
        QRectF scaledOriginalRoi = getScaledRect(originalRoiRect);
        QPoint delta = event->pos() - dragStartPoint;
        QRectF newScaledRect = scaledOriginalRoi;
        if (currentHandle == TopLeft) {
            newScaledRect.setTopLeft(scaledOriginalRoi.topLeft() + delta);
        } else if (currentHandle == BottomRight) {
            newScaledRect.setBottomRight(scaledOriginalRoi.bottomRight() + delta);
        } else if (currentHandle == Body) {
            newScaledRect.translate(delta);
        }
        newScaledRect = newScaledRect.normalized();
        if(newScaledRect.left() < targetRect.left()) newScaledRect.setLeft(targetRect.left());
        if(newScaledRect.top() < targetRect.top()) newScaledRect.setTop(targetRect.top());
        if(newScaledRect.right() > targetRect.right()) newScaledRect.setRight(targetRect.right());
        if(newScaledRect.bottom() > targetRect.bottom()) newScaledRect.setBottom(targetRect.bottom());
        QRect cropped_px = getOriginalRect(newScaledRect);
        bboxes[editingRoiId].roi = cropped_px;
        bboxes[editingRoiId].image = originalPixmap.copy(cropped_px);
        update();
    } else if (isDrawing) {
        QRect constrainedRect(startPoint, event->pos());
        constrainedRect = constrainedRect.normalized();
        constrainedRect = constrainedRect.intersected(targetRect.toRect());
        currentDrawingBox = constrainedRect;
        update();
    } else if (currentMode == Setting || currentMode == Training) {
        setCursorForPosition(event->pos());
    }
}

void CustomPictureBox::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    if (currentMode == Measure && isMeasuring) {
        isMeasuring = false;
        measureLine.originalEnd = mapWidgetToOriginal(measureEnd);
        // We keep start and end in widget coords for dialog positioning only
        lastDialogPos = measureEnd;
        emit requestInputRealLength(mapToGlobal(measureEnd));
        update();
        return;
    }
    if (isEditing) {
        isEditing = false;
        currentHandle = None;
        setCursor(Qt::ArrowCursor);
        if (currentMode == Setting) emit roisChanged(rois);
        if (currentMode == Training) emit templatesChanged(bboxes);
        update();
        return;
    }
    if (!isDrawing) return;
    isDrawing = false;
    QRect finalRect = currentDrawingBox.normalized();
    if (currentMode == Training) {
        if (finalRect.width() < FIXED_SIZE.width() || finalRect.height() < FIXED_SIZE.height()) {
            QPoint centerPoint = startPoint;
            QPoint newTopLeft(centerPoint.x() - FIXED_SIZE.width() /2 ,
                              centerPoint.y() - FIXED_SIZE.height() / 2);
            finalRect = QRect(newTopLeft, FIXED_SIZE);
        }
    }
    else {
        if (finalRect.width() < 5 || finalRect.height() < 5) {
            update();
            return;
        }
    }
    QRect originalCoordRect = getOriginalRect(finalRect);
    if (originalCoordRect.isEmpty()) {
        update();
        return;
    }
    QPixmap croppedPixmap = originalPixmap.copy(originalCoordRect);
    TemplateData data = {0, originalCoordRect, croppedPixmap, ""};
    if (currentMode == Training) {
        data.id = nextBboxId;
        data.name = QString("Template %1").arg(nextBboxId + 1);
        bboxes[nextBboxId] = data;
        nextBboxId = (nextBboxId + 1) % MAX_BBOXES;
        emit templatesChanged(bboxes);
    } else if (currentMode == Setting) {
        data.id = nextRoiId;
        data.name = QString("ROI %1").arg(nextRoiId + 1);
        rois[nextRoiId] = data;
        nextRoiId = (nextRoiId + 1) % MAX_ROIS;
        emit roisChanged(rois);
    }
    update();
}

QPointF CustomPictureBox::mapWidgetToOriginal(const QPoint &widgetPoint) const
{
    if (originalPixmap.isNull() || targetRect.isEmpty()) return QPointF();

    qreal scaleX = targetRect.width() / originalPixmap.width();
    qreal scaleY = targetRect.height() / originalPixmap.height();

    qreal originalX = (widgetPoint.x() - targetRect.left()) / scaleX;
    qreal originalY = (widgetPoint.y() - targetRect.top()) / scaleY;
    return QPointF(originalX, originalY);
}

QPointF CustomPictureBox::mapOriginalToWidget(const QPointF &originalPoint) const
{
    if (originalPixmap.isNull() || targetRect.isEmpty()) return QPointF();

    qreal scaleX = targetRect.width() / originalPixmap.width();
    qreal scaleY = targetRect.height() / originalPixmap.height();

    qreal widgetX = originalPoint.x() * scaleX + targetRect.left();
    qreal widgetY = originalPoint.y() * scaleY + targetRect.top();

    return QPointF(widgetX, widgetY);
}


void CustomPictureBox::updateScaledPixmap()
{
    if (originalPixmap.isNull()) {
        scaledPixmap = QPixmap();
        targetRect = QRectF();
        return;
    }
    QSize widgetSize = this->size();
    scaledPixmap = originalPixmap.scaled(widgetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    int x = (widgetSize.width() - scaledPixmap.width()) / 2;
    int y = (widgetSize.height() - scaledPixmap.height()) / 2;
    targetRect = QRectF(x, y, scaledPixmap.width(), scaledPixmap.height());
}

QRectF CustomPictureBox::getScaledRect(const QRect &rect) const
{
    if (originalPixmap.isNull() || targetRect.isEmpty()) return QRectF();
    qreal scaleX = targetRect.width() / originalPixmap.width();
    qreal scaleY = targetRect.height() / originalPixmap.height();
    qreal x = targetRect.left() + rect.x() * scaleX;
    qreal y = targetRect.top() + rect.y() * scaleY;
    qreal w = rect.width() * scaleX;
    qreal h = rect.height() * scaleY;
    return QRectF(x, y, w, h);
}

QRect CustomPictureBox::getOriginalRect(const QRectF &rect) const
{
    if (originalPixmap.isNull() || targetRect.isEmpty()) return QRect();
    qreal scaleX = targetRect.width() / originalPixmap.width();
    qreal scaleY = targetRect.height() / originalPixmap.height();
    qreal x = (rect.x() - targetRect.left()) / scaleX;
    qreal y = (rect.y() - targetRect.top()) / scaleY;
    qreal w = rect.width() / scaleX;
    qreal h = rect.height() / scaleY;
    return QRect(qRound(x), qRound(y), qRound(w), qRound(h));
}

QPoint CustomPictureBox::getScaledPoint(const QPoint &point) const
{
    if (originalPixmap.isNull() || targetRect.isEmpty()) return QPoint();
    qreal scaleX = targetRect.width() / originalPixmap.width();
    qreal scaleY = targetRect.height() / originalPixmap.height();
    qreal x = point.x() * scaleX;
    qreal y = point.y() * scaleY;
    return QPoint(qRound(x), qRound(y));
}

QPoint CustomPictureBox::getOriginalPoint(const QPoint &point) const
{
    if (originalPixmap.isNull() || targetRect.isEmpty()) return QPoint();
    qreal scaleX = targetRect.width() / originalPixmap.width();
    qreal scaleY = targetRect.height() / originalPixmap.height();
    qreal x = point.x() / scaleX;
    qreal y = point.y() / scaleY;
    return QPoint(qRound(x), qRound(y));
}

QRectF CustomPictureBox::getHandleRect(const QRectF &rect, Handle handle) const
{
    const int HANDLE_SIZE = 8;
    QPointF center;
    if (handle == TopLeft) {
        center = rect.topLeft();
    } else if (handle == BottomRight) {
        center = rect.bottomRight();
    }
    return QRectF(center.x() - HANDLE_SIZE / 2, center.y() - HANDLE_SIZE / 2, HANDLE_SIZE, HANDLE_SIZE);
}

void CustomPictureBox::setCursorForPosition(const QPoint &pos)
{
    Handle handle = getHandleAtPosition(pos);
    if (handle == TopLeft || handle == BottomRight) {
        setCursor(Qt::PointingHandCursor);
    } else if (handle == Body) {
        setCursor(Qt::SizeAllCursor);
    } else {
        setCursor(Qt::CrossCursor);
    }
}

CustomPictureBox::Handle CustomPictureBox::getHandleAtPosition(const QPoint &pos) const
{
    editingRoiId = -1; // Reset before checking

    if (currentMode == Training) {
        for (const auto &bbox : bboxes) {
            QRectF scaledRect = getScaledRect(bbox.roi);
            // Check handles first
            if (getHandleRect(scaledRect, TopLeft).contains(pos)) {
                editingRoiId = bbox.id;
                return TopLeft;
            }
            if (getHandleRect(scaledRect, BottomRight).contains(pos)) {
                editingRoiId = bbox.id;
                return BottomRight;
            }
            // Then check body
            if (scaledRect.contains(pos)) {
                editingRoiId = bbox.id;
                return Body;
            }
        }
    } else if (currentMode == Setting) {
        // Check handles first, as they are on top
        for (const auto &roi : rois) {
            QRectF scaledRoi = getScaledRect(roi.roi);
            if (getHandleRect(scaledRoi, TopLeft).contains(pos)) {
                editingRoiId = roi.id;
                return TopLeft;
            }
            if (getHandleRect(scaledRoi, BottomRight).contains(pos)) {
                editingRoiId = roi.id;
                return BottomRight;
            }
            if (scaledRoi.contains(pos)) {
                editingRoiId = roi.id;
                return Body;
            }
        }
    }

    return None;
}

QSize CustomPictureBox::getOriginalImageSize() const
{
    return originalPixmap.size();
}

void CustomPictureBox::setRois(const QMap<int, TemplateData>& newRois)
{
    rois = newRois;
    // Find the max ID to correctly set the nextRoiId
    int maxId = -1;
    for(int key : rois.keys()) {
        if (key > maxId) {
            maxId = key;
        }
    }
    nextRoiId = (maxId + 1) % MAX_ROIS;
    update();
    emit roisChanged(rois);
}

void CustomPictureBox::selectBbox(int id) {
    if (bboxes.contains(id)) {
        editingRoiId = id;
        update();
    }
}
void CustomPictureBox::selectRoi(int id) {
    if (rois.contains(id)) {
        editingRoiId = id;
        update();
    }
}
void CustomPictureBox::setMeasureMode(bool enabled) {
    if (enabled) {
        setCursor(Qt::CrossCursor);
        currentMode = Measure;
    } else {
        setCursor(Qt::ArrowCursor);
        if (currentMode == Measure) currentMode = Test;
    }
    update();
}

double CustomPictureBox::getMMPerPixel() const {
    return measureLine.valid ? measureLine.mmPerPixels : 0.0;
}

void CustomPictureBox::onInputRealLength(double mm) {
    if (mm > 0.0) {
        double pixelDist = QLineF(measureLine.originalStart, measureLine.originalEnd).length();
        if (pixelDist > 0) {
            measureLine.realLengthMM = mm;
            measureLine.mmPerPixels = mm / pixelDist;
            measureLine.valid = true;
            emit measurementChanged(measureLine);
            update();
        }
    }
}

void CustomPictureBox::setMatchResults(const QVector<QVector<QPointF> > &results)
{
    matchResults = results;
    update();
}

void CustomPictureBox::clearMatchResults()
{
    if (!matchResults.isEmpty()) {
        matchResults.clear();
        update();
    }
}

void CustomPictureBox::saveResultImage(const QString &filePath)
{
    if (originalPixmap.isNull()) return;

    // To avoid data races and ensure thread safety, we copy the necessary data.
    QPixmap imageToSave = originalPixmap.copy();
    QVector<QVector<QPointF>> resultsToDraw = matchResults;

    // Run the saving operation in a separate thread
    QFuture<void> future = QtConcurrent::run([=]() mutable {
        QPainter painter(&imageToSave);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(Qt::cyan, 2)); // Use a distinct color for results

        for (const auto& result : resultsToDraw) {
            if (result.size() < 6 ) continue;
            // Points are already in the original image coordinate system
            const QPointF& p0 = result[0];
            const QPointF& p1 = result[1];
            const QPointF& p2 = result[2];
            const QPointF& p3 = result[3];
            const QPointF& p4 = result[4];
            const QPointF& p5 = result[5];

            QLineF lines[] = {
                QLineF(p2, p3),
                QLineF(p3, p4),
                QLineF(p4, p5),
                QLineF(p5, p2),
            };
            painter.drawLines(lines, 4);
        }
        imageToSave.save(filePath, "PNG");
        qDebug() << "Result image save to " << filePath;
    });
}
