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

#include "qabstractaxis.h"
#include "qabstractaxis_p.h"

QT_DATAVIS3D_BEGIN_NAMESPACE

QAbstractAxis::QAbstractAxis(QAbstractAxisPrivate *d) :
    QObject(0),
    d_ptr(d)
{
}

QAbstractAxis::~QAbstractAxis()
{
}

QString QAbstractAxis::title() const
{
    return d_ptr->m_title;
}

QStringList QAbstractAxis::labels() const
{
    return d_ptr->m_labels;
}

QAbstractAxis::AxisOrientation QAbstractAxis::orientation() const
{
    return d_ptr->m_orientation;
}

QAbstractAxis::AxisType QAbstractAxis::type() const
{
    return d_ptr->m_type;
}

void QAbstractAxis::setTitle(QString title)
{
    if (d_ptr->m_title != title) {
        d_ptr->m_title = title;
        emit titleChanged(title);
    }
}

// QAbstractAxisPrivate

QAbstractAxisPrivate::QAbstractAxisPrivate(QAbstractAxis *q, QAbstractAxis::AxisType type)
    : QObject(0),
      q_ptr(q),
      m_orientation(QAbstractAxis::AxisOrientationNone),
      m_type(type)
{
}

QAbstractAxisPrivate::~QAbstractAxisPrivate()
{
}

void QAbstractAxisPrivate::setOrientation(QAbstractAxis::AxisOrientation orientation)
{
    if (m_orientation == QAbstractAxis::AxisOrientationNone)
        m_orientation = orientation;
    else
        Q_ASSERT("Attempted to reset axis orientation.");
}

QT_DATAVIS3D_END_NAMESPACE
