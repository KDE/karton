// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Derek Lin <derekhongdalin@gmail.com>

#pragma once

#include <libvirt/libvirt.h>

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QModelIndex>
#include <QObject>
#include <QVariant>

#include "domain.h"
#include "karton.h"

class VMModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    enum Roles {
        DomainRole = Qt::UserRole
    };
    VMModel(QObject *parent = nullptr);
    ~VMModel() override;
    Q_DISABLE_COPY_MOVE(VMModel)
    static VMModel *create(QQmlEngine *qmlEngine, QJSEngine *);
    static VMModel *self();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    void updateDomains(const virDomainPtr domainPtr = nullptr);
    void refreshAllDomains();
private Q_SLOTS:
    void onDomainsChanged(const virDomainPtr domainPtr, int event, int detail);

private:
    QList<Domain *> m_datas;
    Karton *m_karton;
};