/****************************************************************************
**
** Copyright (C) 2013 Digia Plc
** All rights reserved.
** For any questions to Digia, please use contact form at http://qt.digia.com
**
** This file is part of the QtDataVisualization module.
**
** Licensees holding valid Qt Enterprise licenses may use this file in
** accordance with the Qt Enterprise License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.
**
** If you have questions regarding the use of this file, please use
** contact form at http://qt.digia.com
**
****************************************************************************/

#ifndef QBARDATAITEM_H
#define QBARDATAITEM_H

#include <QtDataVisualization/qdatavisualizationenums.h>

QT_DATAVISUALIZATION_BEGIN_NAMESPACE

class QBarDataItemPrivate;

class QT_DATAVISUALIZATION_EXPORT QBarDataItem
{
public:
    QBarDataItem();
    QBarDataItem(float value);
    QBarDataItem(const QBarDataItem &other);
    ~QBarDataItem();

    QBarDataItem &operator=(const QBarDataItem &other);

    void setValue(float value) { m_value = value; }
    float value() const { return m_value; }

protected:
    virtual void createExtraData();

    QBarDataItemPrivate *d_ptr;

private:
    float m_value;
};

QT_DATAVISUALIZATION_END_NAMESPACE

#endif
