/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDataVis3D module.
**
** $QT_BEGIN_LICENSE:LGPL$
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef Q3DMAPS_H
#define Q3DMAPS_H

#include "QtDataVis3D/qdatavis3dglobal.h"
#include "QtDataVis3D/qdatavis3namespace.h"
#include "q3dwindow.h"

#include <QFont>

class QOpenGLShaderProgram;
class QImage;
class QRect;

QTENTERPRISE_DATAVIS3D_BEGIN_NAMESPACE

class Q3DMapsPrivate;
class QDataItem;
class QDataRow;
class QDataSet;
class LabelItem;

class QTENTERPRISE_DATAVIS3D_EXPORT Q3DMaps : public Q3DWindow
{
    Q_OBJECT
    Q_PROPERTY(QString windowTitle READ windowTitle WRITE setWindowTitle)
    Q_PROPERTY(QFont font READ font WRITE setFont)
    Q_PROPERTY(float fontSize READ fontSize WRITE setFontSize)

public:
    enum AdjustmentDirection {
        AdjustHeight = 0,   // map value to y
        AdjustWidth,        // map value to x
        AdjustDepth,        // map value to z
        AdjustRadius,       // map value to x and z
        AdjustAll           // map value to all (x, y, z)
    };

public:
    explicit Q3DMaps();
    ~Q3DMaps();

    // Add data item. New data item is appended to old data.
    // ownership of data is transferred
    Q_INVOKABLE bool addDataItem(QDataItem *dataItem);

    // Add data set. New data is appended to old data.
    // ownership of data is transferred
    Q_INVOKABLE bool addData(const QVector<QDataItem*> &data);
    // ownership of data is transferred
    Q_INVOKABLE bool addData(const QDataRow &data);

    // Add data set. Old data is deleted.
    // ownership of data is transferred
    Q_INVOKABLE bool setData(const QVector<QDataItem*> &data);
    // ownership of data is transferred
    Q_INVOKABLE bool setData(QDataRow *data);

    // bar specifications; base thickness in x, y and z, enum to indicate which direction is increased with value
    // TODO: Start using thickness also in adjustment direction; use it as a relative value.
    // For example, in AdjustAll mode setting thickness to (0.1f, 1.0f, 0.5f) would apply value to
    // x at 10%, y at 100% and z at 50%. If a dimension is not included, given thickness states its absolute value.
    Q_INVOKABLE void setBarSpecs(const QVector3D &thickness = QVector3D(1.0f, 1.0f, 1.0f),
                                 AdjustmentDirection direction = AdjustHeight);

    // bar type; bars (=cubes), pyramids, cones, cylinders, balls, etc.
    Q_INVOKABLE void setBarType(BarStyle style, bool smooth = false);

    // override bar type with own mesh
    Q_INVOKABLE void setMeshFileName(const QString &objFileName);

    // Select preset camera placement
    Q_INVOKABLE void setCameraPreset(CameraPreset preset);

    // Set camera rotation if you don't want to use the presets (in horizontal (-180...180) and
    // vertical (0...90) angles and distance in percentage (10...500))
    Q_INVOKABLE void setCameraPosition(GLfloat horizontal, GLfloat vertical, GLint distance = 100);

    // Set theme (bar colors, shaders, window color, background colors, light intensity and text colors are affected)
    Q_INVOKABLE void setTheme(ColorTheme theme);

    // Set color if you don't want to use themes. Set uniform to false if you want the (height) color to change from bottom to top
    Q_INVOKABLE void setBarColor(QColor baseColor, QColor heightColor, bool uniform = true);

    // Set area specs
    Q_INVOKABLE void setAreaSpecs(const QRect &areaRect, const QImage &image);

    // Set area image
    Q_INVOKABLE void setImage(const QImage &image);

    // TODO: light placement API

    // Change selection mode; single bar, bar and row, bar and column, or all
    void setSelectionMode(SelectionMode mode);
    SelectionMode selectionMode();

    // Set window title
    void setWindowTitle(const QString &title);
    QString windowTitle();

    // Font size adjustment
    void setFontSize(float fontsize);
    float fontSize();

    // Set font
    void setFont(const QFont &font);
    QFont font();

    // Label transparency adjustment
    void setLabelTransparency(LabelTransparency transparency);
    LabelTransparency labelTransparency();

    // Adjust shadow quality
    void setShadowQuality(ShadowQuality quality);
    ShadowQuality shadowQuality();

protected:
    void initialize();
    void render();

#if defined(Q_OS_ANDROID)
    void mouseDoubleClickEvent(QMouseEvent *event);
    void touchEvent(QTouchEvent *event);
#endif
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);
    void resizeEvent(QResizeEvent *event);

private:
    void drawScene();
    QScopedPointer<Q3DMapsPrivate> d_ptr;
    Q_DISABLE_COPY(Q3DMaps);
};

QTENTERPRISE_DATAVIS3D_END_NAMESPACE

#endif
