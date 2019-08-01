#pragma once
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
class FileWriter {
public:
    FileWriter (QFile *file) : m_pFile(file)
    {
        Q_ASSERT(file);
    }
    qint64 writeTo(qint64 offset, const QByteArray &data)
    {
        QMutexLocker locker(&m_mutex);
        if (!m_pFile->seek(offset))
        {
            return 0;
        }
        return m_pFile->write(data);
    }
    qint64 writeToWithoutMutex(qint64 offset, const QByteArray &data)
    {
        if (!m_pFile->seek(offset))
        {
            return 0;
        }
        return m_pFile->write(data);
    }
    qint64 writeTo(qint64 offset, const char * const data, qint64 length)
    {
        QMutexLocker locker(&m_mutex);
        if (!m_pFile->seek(offset))
        {
            return 0;
        }
        return m_pFile->write(data, length);
    }
    qint64 writeToWithoutMutex(qint64 offset, const char * const data, qint64 length)
    {
        if (!m_pFile->seek(offset))
        {
            return 0;
        }
        return m_pFile->write(data, length);
    }
private:
    QFile *m_pFile;
    QMutex m_mutex;
};
