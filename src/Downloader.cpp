#include "Downloader.h"

#include "FileWriter.h"

#include <QNetworkAccessManager>
#include <QEventLoop>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtConcurrent>

void Downloader::download(const QString &url, const int CPUCount)
{
    setIsDownloading(true);

    QTime time;
    time.start();

    QNetworkAccessManager mgr;
    QNetworkRequest req;
    QEventLoop event;
    req.setUrl(url);
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    QNetworkReply *reply = mgr.head(req);
    connect(reply, &QNetworkReply::finished, &event, &QEventLoop::quit);
    event.exec();

    qlonglong length = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
    qInfo() << "Content length:" << length;

    bool supportRange = (reply->rawHeader("Accept-Ranges") == QByteArrayLiteral("bytes"));
    qInfo() << "support Range:" << supportRange;

    auto cost = time.elapsed();
    qInfo() << "get header info cost" << cost << " ms";

    QString filename = QFileInfo(url).fileName();

    if (supportRange && CPUCount > 1) {
        mulitDownload(url, length, filename, CPUCount);
    } else {
        singleDownload(url, length, filename);
    }
    setIsDownloading(false);


}

void Downloader::singleDownload(const QString &url, const qint64 length, const QString &filename)
{
    QFile file(filename);
    if (file.exists()) {
        file.remove();
    }
    if (!file.open(QFile::WriteOnly))
    {
        qWarning() << file.errorString();
        return;
    }
    QNetworkAccessManager mgr;
    QTime time;
    time.start();
    QNetworkRequest req;
    req.setUrl(url);
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    auto reply = mgr.get(req);
    connect(reply, &QNetworkReply::readyRead, this, [&](){
        file.write(reply->readAll());
    });
    QEventLoop event;
    connect(reply, &QNetworkReply::finished, &event, &QEventLoop::quit);
    event.exec();
    file.close();
    auto cost = time.elapsed();
    qWarning() << "cost" << cost;
}

void Downloader::mulitDownload(const QString &url, const qint64 length, const QString &filePath, const int CPUCount)
{
    QFile file(filePath);
    if (file.exists()) {
        file.remove();
    }
    if (!file.open(QFile::ReadWrite))
    {
        qWarning() << file.errorString();
        return;
    }
    bool ok = file.resize(length);
    qWarning() << "resize" << ok << file.size();
    ok = file.setPermissions(QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ReadGroup | QFileDevice::WriteGroup);
    qWarning() << "set permission " << ok;
    FileWriter fileWriter(&file);

    QTime time;
    time.start();
    //任务等分
    qlonglong segmentSize = length / CPUCount;
    QVector<QPair<qlonglong, qlonglong>> vec(CPUCount);
    for (int i = 0; i <CPUCount; ++i) {
        vec[i].first = i * segmentSize;
        vec[i].second = i * segmentSize + segmentSize - 1;
    }
    //余数部分加入最后一个
    vec[CPUCount -1].second += length % CPUCount;
    qWarning() << "main thread" << QThread::currentThreadId();
    auto mapCaller = [&](const QPair<qlonglong, qlonglong> &pair) ->qlonglong{
        qWarning() << "lambda start" << pair.first << pair.second << QThread::currentThreadId();
        QNetworkAccessManager mgr;
        QNetworkRequest req;
        req.setUrl(url);
        req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        req.setRawHeader("Range", QString("bytes=%1-%2").arg(pair.first).arg(pair.second).toUtf8());
        QNetworkReply *reply = mgr.get(req);
        qlonglong hasWritePos = pair.first;
        const qint64 bufSize = 1024 * 32;
        QByteArray data(bufSize, 0);
        connect(reply, &QNetworkReply::readyRead, this, [&](){
            auto d = reply->readAll();
            qint64 realSize =fileWriter.writeTo(hasWritePos, d);
            hasWritePos += realSize;
//            qint64 realSize = 0;
//            do {
//                realSize = reply->read(data.data(), bufSize);
//                if (realSize <= 0) {
//                    break;
//                }
//                hasWritePos += fileWriter.writeToWithoutMutex(hasWritePos, data.data(), realSize);
//            } while (realSize);

//            file.seek(hasWritePos);
//            file.write(d);
//            file.flush();
//            hasWritePos += d.size();

//            qint64 realSize = 0;
//            do {
//                realSize = reply->read((char *)pData + hasWritePos, bufSize);
//                if (realSize <= 0) {
//                    break;
//                }
//                hasWritePos += realSize;
//            } while (realSize);
        }, Qt::DirectConnection);
        QEventLoop event;
        connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error), reply, [&]{
            qWarning() << u8"出错" << reply->errorString();
            event.quit();
        });
        connect(reply, &QNetworkReply::finished, &event, &QEventLoop::quit);
        event.exec();
        qWarning() << "lambda " << hasWritePos - pair.first;
        return hasWritePos - pair.first;
    };

    QFuture<void> future =  QtConcurrent::map(vec, mapCaller);

    QFutureWatcher<void> futureWatcher;
    QEventLoop loop;
    connect(&futureWatcher, &QFutureWatcher<void>::finished,
            &loop, &QEventLoop::quit, Qt::QueuedConnection);
    futureWatcher.setFuture(future);
    if (!future.isFinished()) {
        qWarning() << "not finished, wait";
        loop.exec();
    }
    file.close();
    auto cost = time.elapsed();
    qWarning() << "end cost" << cost;
}
