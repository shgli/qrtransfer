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

    int getTotalCnt() const { return mTotalCnt; }
signals:
    void acked(int ackedCnt, QString detail);

private:
    QImage grubScreen(QRect& area, QRect& qrCode);
    bool dectQRArea(QRect& grubArea, QList<QRect>& points);

    std::unique_ptr<QFile> mOutputFile;
    std::atomic_bool mIsRunning{true};

    std::vector<std::unique_ptr<DecodeThread>> mDecodeThreads;
    std::unique_ptr<QSemaphore> mDecodeSep;
    std::vector<char> mCfgBuffer;

    int mTotalCnt;
    int mRowCnt;
    int mColCnt;
    int mChannelCnt;
};


#endif // RECEIVETHREAD
