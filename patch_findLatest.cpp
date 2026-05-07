#include <QString>
#include <QStringList>
#include <QDebug>

QString findLatestPostId(const QStringList &postIds) {
    if (postIds.isEmpty()) {
        return QString();
    }
    QString latest = postIds.first();
    for (const auto &id : postIds) {
        if (id.length() > latest.length() || (id.length() == latest.length() && id > latest)) {
            latest = id;
        }
    }
    return latest;
}

int main() {
    qDebug() << findLatestPostId({"100", "99"});
    qDebug() << findLatestPostId({"99", "100"});
    qDebug() << findLatestPostId({"100", "100"});
    qDebug() << findLatestPostId({"1000", "999"});
    return 0;
}
