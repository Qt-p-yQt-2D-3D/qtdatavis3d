/****************************************************************************
**
** Copyright (C) 2014 Digia Plc
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

#include "qitemmodelbardataproxy_p.h"
#include "baritemmodelhandler_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QItemModelBarDataProxy
 * \inmodule QtDataVisualization
 * \brief Proxy class for presenting data in item models with Q3DBars.
 * \since Qt Data Visualization 1.0
 *
 * QItemModelBarDataProxy allows you to use QAbstractItemModel derived models as a data source
 * for Q3DBars. It uses the defined mappings to map data from the model to rows, columns, and
 * values of Q3DBars graph.
 *
 * The data is resolved asynchronously whenever mappings or the model changes.
 * QBarDataProxy::arrayReset() is emitted when the data has been resolved.
 *
 * There are three ways to use mappings:
 *
 * 1) If useModelCategories property is set to true, this proxy will map rows and
 *    columns of QAbstractItemModel directly to rows and columns of Q3DBars, and uses the value
 *    returned for Qt::DisplayRole as bar value by default.
 *    The value role to be used can be redefined if Qt::DisplayRole is not suitable.
 *
 * 2) For models that do not have data already neatly sorted into rows and columns, such as
 *    QAbstractListModel based models, you can define a role from the model to map for each of row,
 *    column and value.
 *
 * 3) If you do not want to include all data contained in the model, or the autogenerated rows and
 *    columns are not ordered as you wish, you can specify which rows and columns should be included
 *    and in which order by defining an explicit list of categories for either or both of rows and
 *    columns.
 *
 *    For example, assume that you have a custom QAbstractItemModel for storing various monthly values
 *    related to a business.
 *    Each item in the model has the roles "year", "month", "income", and "expenses".
 *    You could do the following to display the data in a bar graph:
 *
 *    \snippet doc_src_qtdatavisualization.cpp 3
 *
 * \sa {Qt Data Visualization Data Handling}
 */

/*!
 * \qmltype ItemModelBarDataProxy
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.0
 * \ingroup datavisualization_qml
 * \instantiates QItemModelBarDataProxy
 * \inherits BarDataProxy
 * \brief Proxy class for presenting data in item models with Bars3D.
 *
 * This type allows you to use AbstractItemModel derived models as a data source for Bars3D.
 *
 * Data is resolved asynchronously whenever the mapping or the model changes.
 * QBarDataProxy::arrayReset() is emitted when the data has been resolved.
 *
 * For more details, see QItemModelBarDataProxy documentation.
 *
 * Usage example:
 *
 * \snippet doc_src_qmldatavisualization.cpp 7
 *
 * \sa BarDataProxy, {Qt Data Visualization Data Handling}
 */

/*!
 * \qmlproperty model ItemModelBarDataProxy::itemModel
 * The item model.
 */

/*!
 * \qmlproperty string ItemModelBarDataProxy::rowRole
 * The row role of the mapping.
 */

/*!
 * \qmlproperty string ItemModelBarDataProxy::columnRole
 * The column role of the mapping.
 */

/*!
 * \qmlproperty string ItemModelBarDataProxy::valueRole
 * The value role of the mapping.
 */

/*!
 * \qmlproperty string ItemModelBarDataProxy::rotationRole
 *
 * Defines the rotation role for the mapping.
 */

/*!
 * \qmlproperty list<String> ItemModelBarDataProxy::rowCategories
 * The row categories of the mapping. Only items with row roles that are found in this list are
 * included when the data is resolved. The rows are ordered in the same order as they are in this list.
 */

/*!
 * \qmlproperty list<String> ItemModelBarDataProxy::columnCategories
 * The column categories of the mapping. Only items with column roles that are found in this list are
 * included when the data is resolved. The columns are ordered in the same order as they are in this list.
 */

/*!
 * \qmlproperty bool ItemModelBarDataProxy::useModelCategories
 * When set to true, the mapping ignores row and column roles and categories, and uses
 * the rows and columns from the model instead. Row and column headers are used for row and column
 * labels. Defaults to \c{false}.
 */

/*!
 * \qmlproperty bool ItemModelBarDataProxy::autoRowCategories
 * When set to true, the mapping ignores any explicitly set row categories
 * and overwrites them with automatically generated ones whenever the
 * data from the model is resolved. Defaults to \c{true}.
 */

/*!
 * \qmlproperty bool ItemModelBarDataProxy::autoColumnCategories
 * When set to true, the mapping ignores any explicitly set column categories
 * and overwrites them with automatically generated ones whenever the
 * data from model is resolved. Defaults to \c{true}.
 */

/*!
 * Constructs QItemModelBarDataProxy with optional \a parent.
 */
QItemModelBarDataProxy::QItemModelBarDataProxy(QObject *parent)
    : QBarDataProxy(new QItemModelBarDataProxyPrivate(this), parent)
{
    dptr()->connectItemModelHandler();
}

/*!
 * Constructs QItemModelBarDataProxy with \a itemModel and optional \a parent. Proxy doesn't take
 * ownership of the \a itemModel, as typically item models are owned by other controls.
 */
QItemModelBarDataProxy::QItemModelBarDataProxy(const QAbstractItemModel *itemModel, QObject *parent)
    : QBarDataProxy(new QItemModelBarDataProxyPrivate(this), parent)
{
    setItemModel(itemModel);
    dptr()->connectItemModelHandler();
}

/*!
 * Constructs QItemModelBarDataProxy with \a itemModel and optional \a parent. Proxy doesn't take
 * ownership of the \a itemModel, as typically item models are owned by other controls.
 * The value role is set to \a valueRole.
 * This constructor is meant to be used with models that have data properly sorted
 * in rows and columns already, so it also sets useModelCategories property to true.
 */
QItemModelBarDataProxy::QItemModelBarDataProxy(const QAbstractItemModel *itemModel,
                                               const QString &valueRole, QObject *parent)
    : QBarDataProxy(new QItemModelBarDataProxyPrivate(this), parent)
{
    dptr()->m_itemModelHandler->setItemModel(itemModel);
    dptr()->m_valueRole = valueRole;
    dptr()->m_useModelCategories = true;
    dptr()->connectItemModelHandler();
}

/*!
 * Constructs QItemModelBarDataProxy with \a itemModel and optional \a parent. Proxy doesn't take
 * ownership of the \a itemModel, as typically item models are owned by other controls.
 * The role mappings are set with \a rowRole, \a columnRole, and \a valueRole.
 */
QItemModelBarDataProxy::QItemModelBarDataProxy(const QAbstractItemModel *itemModel,
                                               const QString &rowRole,
                                               const QString &columnRole,
                                               const QString &valueRole, QObject *parent)
    : QBarDataProxy(new QItemModelBarDataProxyPrivate(this), parent)
{
    dptr()->m_itemModelHandler->setItemModel(itemModel);
    dptr()->m_rowRole = rowRole;
    dptr()->m_columnRole = columnRole;
    dptr()->m_valueRole = valueRole;
    dptr()->connectItemModelHandler();
}

/*!
 * Constructs QItemModelBarDataProxy with \a itemModel and optional \a parent. Proxy doesn't take
 * ownership of the \a itemModel, as typically item models are owned by other controls.
 * The role mappings are set with \a rowRole, \a columnRole, \a valueRole, and \a rotationRole.
 */
QItemModelBarDataProxy::QItemModelBarDataProxy(const QAbstractItemModel *itemModel,
                                               const QString &rowRole,
                                               const QString &columnRole,
                                               const QString &valueRole,
                                               const QString &rotationRole,
                                               QObject *parent)
    : QBarDataProxy(new QItemModelBarDataProxyPrivate(this), parent)
{
    dptr()->m_itemModelHandler->setItemModel(itemModel);
    dptr()->m_rowRole = rowRole;
    dptr()->m_columnRole = columnRole;
    dptr()->m_valueRole = valueRole;
    dptr()->m_rotationRole = rotationRole;
    dptr()->connectItemModelHandler();
}

/*!
 * Constructs QItemModelBarDataProxy with \a itemModel and optional \a parent. Proxy doesn't take
 * ownership of the \a itemModel, as typically item models are owned by other controls.
 * The role mappings are set with \a rowRole, \a columnRole, and \a valueRole.
 * Row and column categories are set with \a rowCategories and \a columnCategories.
 * This constructor also sets autoRowCategories and autoColumnCategories to false.
 */
QItemModelBarDataProxy::QItemModelBarDataProxy(const QAbstractItemModel *itemModel,
                                               const QString &rowRole,
                                               const QString &columnRole,
                                               const QString &valueRole,
                                               const QStringList &rowCategories,
                                               const QStringList &columnCategories,
                                               QObject *parent)
    : QBarDataProxy(new QItemModelBarDataProxyPrivate(this), parent)
{
    dptr()->m_itemModelHandler->setItemModel(itemModel);
    dptr()->m_rowRole = rowRole;
    dptr()->m_columnRole = columnRole;
    dptr()->m_valueRole = valueRole;
    dptr()->m_rowCategories = rowCategories;
    dptr()->m_columnCategories = columnCategories;
    dptr()->m_autoRowCategories = false;
    dptr()->m_autoColumnCategories = false;
    dptr()->connectItemModelHandler();
}

/*!
 * Constructs QItemModelBarDataProxy with \a itemModel and optional \a parent. Proxy doesn't take
 * ownership of the \a itemModel, as typically item models are owned by other controls.
 * The role mappings are set with \a rowRole, \a columnRole, \a valueRole, and \a rotationRole.
 * Row and column categories are set with \a rowCategories and \a columnCategories.
 * This constructor also sets autoRowCategories and autoColumnCategories to false.
 */
QItemModelBarDataProxy::QItemModelBarDataProxy(const QAbstractItemModel *itemModel,
                                               const QString &rowRole,
                                               const QString &columnRole,
                                               const QString &valueRole,
                                               const QString &rotationRole,
                                               const QStringList &rowCategories,
                                               const QStringList &columnCategories,
                                               QObject *parent)
    : QBarDataProxy(new QItemModelBarDataProxyPrivate(this), parent)
{
    dptr()->m_itemModelHandler->setItemModel(itemModel);
    dptr()->m_rowRole = rowRole;
    dptr()->m_columnRole = columnRole;
    dptr()->m_valueRole = valueRole;
    dptr()->m_rotationRole = rotationRole;
    dptr()->m_rowCategories = rowCategories;
    dptr()->m_columnCategories = columnCategories;
    dptr()->m_autoRowCategories = false;
    dptr()->m_autoColumnCategories = false;
    dptr()->connectItemModelHandler();
}

/*!
 * Destroys QItemModelBarDataProxy.
 */
QItemModelBarDataProxy::~QItemModelBarDataProxy()
{
}

/*!
 * \property QItemModelBarDataProxy::itemModel
 *
 * Defines item model. Does not take ownership of the model, but does connect to it to listen for
 * changes.
 */
void QItemModelBarDataProxy::setItemModel(const QAbstractItemModel *itemModel)
{
    dptr()->m_itemModelHandler->setItemModel(itemModel);
}

const QAbstractItemModel *QItemModelBarDataProxy::itemModel() const
{
    return dptrc()->m_itemModelHandler->itemModel();
}

/*!
 * \property QItemModelBarDataProxy::rowRole
 *
 * Defines the row \a role for the mapping.
 */
void QItemModelBarDataProxy::setRowRole(const QString &role)
{
    if (dptr()->m_rowRole != role) {
        dptr()->m_rowRole = role;
        emit rowRoleChanged(role);
    }
}

QString QItemModelBarDataProxy::rowRole() const
{
    return dptrc()->m_rowRole;
}

/*!
 * \property QItemModelBarDataProxy::columnRole
 *
 * Defines the column \a role for the mapping.
 */
void QItemModelBarDataProxy::setColumnRole(const QString &role)
{
    if (dptr()->m_columnRole != role) {
        dptr()->m_columnRole = role;
        emit columnRoleChanged(role);
    }
}

QString QItemModelBarDataProxy::columnRole() const
{
    return dptrc()->m_columnRole;
}

/*!
 * \property QItemModelBarDataProxy::valueRole
 *
 * Defines the value \a role for the mapping.
 */
void QItemModelBarDataProxy::setValueRole(const QString &role)
{
    if (dptr()->m_valueRole != role) {
        dptr()->m_valueRole = role;
        emit valueRoleChanged(role);
    }
}

QString QItemModelBarDataProxy::valueRole() const
{
    return dptrc()->m_valueRole;
}

/*!
 * \property QItemModelBarDataProxy::rotationRole
 *
 * Defines the rotation \a role for the mapping.
 */
void QItemModelBarDataProxy::setRotationRole(const QString &role)
{
    if (dptr()->m_rotationRole != role) {
        dptr()->m_rotationRole = role;
        emit rotationRoleChanged(role);
    }
}

QString QItemModelBarDataProxy::rotationRole() const
{
    return dptrc()->m_rotationRole;
}

/*!
 * \property QItemModelBarDataProxy::rowCategories
 *
 * Defines the row \a categories for the mapping.
 */
void QItemModelBarDataProxy::setRowCategories(const QStringList &categories)
{
    if (dptr()->m_rowCategories != categories) {
        dptr()->m_rowCategories = categories;
        emit rowCategoriesChanged();
    }
}

QStringList QItemModelBarDataProxy::rowCategories() const
{
    return dptrc()->m_rowCategories;
}

/*!
 * \property QItemModelBarDataProxy::columnCategories
 *
 * Defines the column \a categories for the mapping.
 */
void QItemModelBarDataProxy::setColumnCategories(const QStringList &categories)
{
    if (dptr()->m_columnCategories != categories) {
        dptr()->m_columnCategories = categories;
        emit columnCategoriesChanged();
    }
}

QStringList QItemModelBarDataProxy::columnCategories() const
{
    return dptrc()->m_columnCategories;
}

/*!
 * \property QItemModelBarDataProxy::useModelCategories
 *
 * When set to true, the mapping ignores row and column roles and categories, and uses
 * the rows and columns from the model instead. Defaults to \c{false}.
 */
void QItemModelBarDataProxy::setUseModelCategories(bool enable)
{
    if (dptr()->m_useModelCategories != enable) {
        dptr()->m_useModelCategories = enable;
        emit useModelCategoriesChanged(enable);
    }
}

bool QItemModelBarDataProxy::useModelCategories() const
{
    return dptrc()->m_useModelCategories;
}

/*!
 * \property QItemModelBarDataProxy::autoRowCategories
 *
 * When set to true, the mapping ignores any explicitly set row categories
 * and overwrites them with automatically generated ones whenever the
 * data from model is resolved. Defaults to \c{true}.
 */
void QItemModelBarDataProxy::setAutoRowCategories(bool enable)
{
    if (dptr()->m_autoRowCategories != enable) {
        dptr()->m_autoRowCategories = enable;
        emit autoRowCategoriesChanged(enable);
    }
}

bool QItemModelBarDataProxy::autoRowCategories() const
{
    return dptrc()->m_autoRowCategories;
}

/*!
 * \property QItemModelBarDataProxy::autoColumnCategories
 *
 * When set to true, the mapping ignores any explicitly set column categories
 * and overwrites them with automatically generated ones whenever the
 * data from model is resolved. Defaults to \c{true}.
 */
void QItemModelBarDataProxy::setAutoColumnCategories(bool enable)
{
    if (dptr()->m_autoColumnCategories != enable) {
        dptr()->m_autoColumnCategories = enable;
        emit autoColumnCategoriesChanged(enable);
    }
}

bool QItemModelBarDataProxy::autoColumnCategories() const
{
    return dptrc()->m_autoColumnCategories;
}

/*!
 * Changes \a rowRole, \a columnRole, \a valueRole, \a rotationRole,
 * \a rowCategories and \a columnCategories to the mapping.
 */
void QItemModelBarDataProxy::remap(const QString &rowRole,
                                   const QString &columnRole,
                                   const QString &valueRole,
                                   const QString &rotationRole,
                                   const QStringList &rowCategories,
                                   const QStringList &columnCategories)
{
    setRowRole(rowRole);
    setColumnRole(columnRole);
    setValueRole(valueRole);
    setRotationRole(rotationRole);
    setRowCategories(rowCategories);
    setColumnCategories(columnCategories);
}

/*!
 * \return index of the specified \a category in row categories list.
 * If the row categories list is empty, -1 is returned.
 * \note If the automatic row categories generation is in use, this method will
 * not return a valid index before the data in the model is resolved for the first time.
 */
int QItemModelBarDataProxy::rowCategoryIndex(const QString &category)
{
    return dptr()->m_rowCategories.indexOf(category);
}

/*!
 * \return index of the specified \a category in column categories list.
 * If the category is not found, -1 is returned.
 * \note If the automatic column categories generation is in use, this method will
 * not return a valid index before the data in the model is resolved for the first time.
 */
int QItemModelBarDataProxy::columnCategoryIndex(const QString &category)
{
    return dptr()->m_columnCategories.indexOf(category);
}

/*!
 * \internal
 */
QItemModelBarDataProxyPrivate *QItemModelBarDataProxy::dptr()
{
    return static_cast<QItemModelBarDataProxyPrivate *>(d_ptr.data());
}

/*!
 * \internal
 */
const QItemModelBarDataProxyPrivate *QItemModelBarDataProxy::dptrc() const
{
    return static_cast<const QItemModelBarDataProxyPrivate *>(d_ptr.data());
}

// QItemModelBarDataProxyPrivate

QItemModelBarDataProxyPrivate::QItemModelBarDataProxyPrivate(QItemModelBarDataProxy *q)
    : QBarDataProxyPrivate(q),
      m_itemModelHandler(new BarItemModelHandler(q)),
      m_useModelCategories(false),
      m_autoRowCategories(true),
      m_autoColumnCategories(true)
{
}

QItemModelBarDataProxyPrivate::~QItemModelBarDataProxyPrivate()
{
    delete m_itemModelHandler;
}

QItemModelBarDataProxy *QItemModelBarDataProxyPrivate::qptr()
{
    return static_cast<QItemModelBarDataProxy *>(q_ptr);
}

void QItemModelBarDataProxyPrivate::connectItemModelHandler()
{
    QObject::connect(m_itemModelHandler, &BarItemModelHandler::itemModelChanged,
                     qptr(), &QItemModelBarDataProxy::itemModelChanged);
    QObject::connect(qptr(), &QItemModelBarDataProxy::rowRoleChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelBarDataProxy::columnRoleChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelBarDataProxy::valueRoleChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelBarDataProxy::rotationRoleChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelBarDataProxy::rowCategoriesChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelBarDataProxy::columnCategoriesChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelBarDataProxy::useModelCategoriesChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelBarDataProxy::autoRowCategoriesChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelBarDataProxy::autoColumnCategoriesChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
}

QT_END_NAMESPACE_DATAVISUALIZATION
