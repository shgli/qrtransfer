#ifndef RECEIVETHREAD
#define RECEIVETHREAD

#include <QThread>
#include <QFile>
#include <QImage>
#include <QSemaphore>
#include "DecodeThread.h"
class ReceiveThread : public QThread
{
     Q_OBJECT

public:
    ReceiveThread(const QString& fileName);
    ~ReceiveThread();

    void run() override;

    void stop();

    qint64 getFileSize() const { return mFileSize; }
signals:
    void acked(qint64 ackedCnt, QString detail);

private:
    QImage grubScreen(QRect& area, QRect& qrCode, QPoint& offset, QPoint& idPos, QColor& idClr);

    bool dectQRArea(QRect& grubArea, QList<QRect>& points);

    std::unique_ptr<QFile> mOutputFile;
    std::atomic_bool mIsRunning{true};

    std::vector<std::unique_ptr<DecodeThread>> mDecodeThreads;
    std::unique_ptr<QSemaphore> mDecodeSep;
    std::vector<char> mCfgBuffer;

    int mRowCnt;
    int mColCnt;
    int mChannelCnt;
    qint64 mFileSize;
};


#endif // RECEIVETHREAD
