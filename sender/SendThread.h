#ifndef SENDTHREAD_H
#define SENDTHREAD_H
#include <deque>
#include <map>
#include <QFile>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QImage>
#include <QWaitCondition>
#include "Ack.h"
#include "qrencode.h"

class SendThread : public QThread
{
     Q_OBJECT
public:
    SendThread(const QString& fileName
               , std::unique_ptr<IAck>&& pAckWaiter
               , int scalar=2
               , int pixelChannel=3
               , int imageCnt=1);

    void run() override;
    void stop();

signals:
    void qrReady(const QImage imgs, int seqId);
    void timeout();

private:
    void prepareCfgQRCode(int totalCnt, size_t fsize);
    void prepareQRCode( void );
    std::pair<QImage, int> prepareImages( void );

    const int mPixelChannel{3};
    const int mImageCnt{1};
    const int mQRCodeCntOneTime{1};
    const int mScalar{1};
    const int mRetryScalar{2};
    const int mRowCnt{1};
    const int mColCnt{1};
    const int mSpliterWidth{5};
    int mImageWidth{0};
    int mImageHeight{0};

    std::atomic_bool mIsRunning{true};
    bool mIsRetrying{false};

    std::unique_ptr<QFile> mInputFile;

    std::deque<std::pair<int32_t, QRcode*>> mPendingCodes;
    std::vector<std::pair<int32_t, QRcode*>> mSendingCodes;
    std::vector<std::pair<int32_t, QRcode*>> delayed;
    int mSequenceId{0};
    std::vector<char> mBuffer;

    char* mNetSeqId{nullptr};
    char* mData{nullptr};
    std::unique_ptr<IAck> mAckWaiter;
};

#endif // SENDTHREAD_H
