#include "custompicturebox.h"
#include <QPainterPath>

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
    clearAll();
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
    nextBboxId = 0;
    nextRoiId = 0;
    isDrawing = false;
    editingRoiId = -1;
    currentHandle = None;
    isEditing = false;
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

    // Draw Templates
    painter.setPen(QPen(Qt::green, 2));
    for (const TemplateData &data : bboxes) {
        painter.drawRect(getScaledRect(data.roi));
    }

    // Draw ROIs
    for (const TemplateData &data : rois) {
        QRectF scaledRoi = getScaledRect(data.roi);
        bool isSelected = (data.id == editingRoiId);

        if (isSelected) {
            painter.setPen(QPen(Qt::cyan, 2));
        } else {
            painter.setPen(QPen(Qt::blue, 5));
        }
        painter.drawRect(scaledRoi);

        // Draw ROI Name
        painter.setPen(Qt::white);
        painter.drawText(scaledRoi.topLeft() + QPointF(5, 15), data.name);

        if (isSelected) {
            // Draw overlay
            painter.fillRect(scaledRoi, QColor(0, 255, 255, 30));
            // Draw handles
            //painter.setBrush(Qt::cyan);
            painter.drawRect(getHandleRect(scaledRoi, TopLeft));
            painter.drawRect(getHandleRect(scaledRoi, BottomRight));
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

    if (currentMode == Setting) {
        //currentHandle = getHandleAtPosition(event->pos());
        //if (currentHandle != None) {
        Handle handle = getHandleAtPosition(event->pos());
        if (handle != None) {
            isEditing = true;
            currentHandle = handle;
            dragStartPoint = event->pos();
            originalRoiRect = rois[editingRoiId].roi; // editingRoiId is set by getHandleAtPosition
            update();
            return;
        }
    }

    // Deselect if clicking on an empty area
    if (editingRoiId != -1) {
        editingRoiId = -1;
        isEditing = false;
        update();
    }

    if (currentMode == Training || currentMode == Setting) {
        isDrawing = true;
        startPoint = event->pos();
        currentDrawingBox = QRect(startPoint, startPoint);
        //update();
    }
}

void CustomPictureBox::mouseMoveEvent(QMouseEvent *event)
{
//    if (currentMode == Setting && !isDrawing) {
//         setCursorForPosition(event->pos());
//    }

//    if (isDrawing) {
//        QRect constrainedRect(startPoint, event->pos());
//        constrainedRect = constrainedRect.normalized();
//        // Clamp to targetRect boundaries
//        constrainedRect = constrainedRect.intersected(targetRect.toRect());
//        currentDrawingBox = constrainedRect;
//        update();
//    } else if (currentHandle != None) { // Resizing or moving an ROI
    if (isEditing) {
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
    }
    else if (isDrawing) {
        QRect constrainedRect(startPoint, event->pos());
        constrainedRect = constrainedRect.normalized();
        constrainedRect = constrainedRect.intersected(targetRect.toRect());
        currentDrawingBox = constrainedRect;
        update();
    } else if (currentMode == Setting) {
        setCursorForPosition(event->pos());
    }
}

void CustomPictureBox::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    //if (currentHandle != None) {
    if (isEditing) {
        isEditing = false;
        currentHandle = None;
        //editingRoiId = -1; // Deselect
        setCursor(Qt::ArrowCursor);
        emit roisChanged(rois);
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
        // Ignore very small boxes (clicks)
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

    editingRoiId = -1;
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
