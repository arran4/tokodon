// SPDX-FileCopyrightText: 2023 Rishi Kumar <rsi.dev17@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QObject>

class FederationInfo
{
public:
    QString id() const;
    QString domain() const;
    QDateTime createdAt() const;
    QString severity() const;
    void setSeverity(const QString &severity);
    bool rejectMedia() const;
    void setRejectMedia(const bool &rejectMedia);
    bool rejectReports() const;
    void setRejectReports(const bool &rejectReports);
    QString privateComment() const;
    void setPrivateComment(const QString &privateComment);
    QString publicComment() const;
    void setPublicComment(const QString &publicComment);
    bool obfuscate() const;
    void setObfuscate(const bool &obfuscate);

    static FederationInfo fromSourceData(const QJsonObject &doc);

private:
    QString m_id;
    QString m_domain;
    QString m_severity = "silence";
    QString m_privateComment;
    QString m_publicComment;
    QDateTime m_createdAt;
    bool m_rejectMedia = false;
    bool m_rejectReports = false;
    bool m_obfuscate = false;
};