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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QtDataVis3D API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#include "qvalueaxis.h"
#include "qabstractaxis_p.h"

#ifndef QVALUEAXIS_P_H
#define QVALUEAXIS_P_H

QT_DATAVIS3D_BEGIN_NAMESPACE

class QValueAxisPrivate : public QAbstractAxisPrivate
{
    Q_OBJECT

public:
    QValueAxisPrivate(QValueAxis *q);
    virtual ~QValueAxisPrivate();

    void setRange(qreal min, qreal max);
    void setMin(qreal min);
    void setMax (qreal max);

protected:
    void recreateLabels();

    qreal m_min;
    qreal m_max;
    int m_tickCount;
    int m_subTickCount;
    bool m_autoAdjust;
    QString m_labelFormat;

private:
    QValueAxis *qptr();

    friend class QValueAxis;
};

QT_DATAVIS3D_END_NAMESPACE

#endif // QVALUEAXIS_P_H
