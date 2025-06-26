// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Derek Lin <derekhongdalin@gmail.com>

#pragma once

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

extern "C" // due to undefined references to libosinfo content
{
#include <osinfo/osinfo.h>
}

struct GObjectDeleter { // dereference glib pointer
    void operator()(gpointer obj) const
    {
        if (obj) {
            g_object_unref(obj);
        }
    }
};

template<typename T>
using GObjectPtr = std::unique_ptr<T, GObjectDeleter>;

class OsinfoConfig : public QObject
{
    Q_OBJECT // used to be QGADGET
        QML_SINGLETON QML_ELEMENT

            Q_DISABLE_COPY_MOVE(OsinfoConfig);

public:
    explicit OsinfoConfig();
    ~OsinfoConfig();
    static OsinfoConfig *create(QQmlEngine *qmlEngine, QJSEngine *);
    static OsinfoConfig *self();

    QString getOsIdFromShortId(const QString &short_id);
    Q_INVOKABLE QString getShortIdFromId(const QString &id);
    Q_INVOKABLE QString getOsIdFromDisk(const QString &isoDiskPath);
    Q_INVOKABLE QStringList getOsVariants();
    QString getOsArchitecture(const QString &osId);

private:
    bool initOsDb();

    GObjectPtr<OsinfoLoader> m_loader;
    GObjectPtr<OsinfoDb> m_db;
};
