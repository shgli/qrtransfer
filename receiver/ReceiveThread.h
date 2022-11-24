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

    void run() override;

    void stop() { mIsRunning = false; }

    int getTotalCnt() const { return mTotalCnt; }
signals:
    void acked(int ackedCnt, QString detail);

private:
    QImage grubScreen(int x0, int y0, int x2, int y2);
    bool dectQRArea(int& x0, int& y0, int& x2, int& y2, std::vector<QRect>& points);

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
