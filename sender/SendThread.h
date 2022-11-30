#ifndef SENDTHREAD_H
#define SENDTHREAD_H
#include <deque>
#include <map>
#include <tuple>
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
               , int rows=1
               , int cols=1
               , int pixelChannel=3
               );

    void run() override;
    void stop();

signals:
    void qrReady(const QImage imgs, int seqId);
    void timeout();

private:
    void prepareCfgQRCode(size_t fsize);
    void prepareQRCode( void );
    void splitQRcode(qint64 offset, qint64 len);
    int createQRcode(qint64 offset, qint64 len);
    QImage prepareImages(QColor failColor);

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

    struct SendingInfo
    {
        qint64 offset;
        qint64 len;
        QRcode* code;
    };

    std::deque<SendingInfo> mPendingCodes;
    std::vector<SendingInfo> mSendingCodes;
    std::vector<SendingInfo> delayed;
    uint mNextReadOffset{0};
    //std::vector<char> mBuffer;

    char* mNetSeqId{nullptr};
    uchar* mData{nullptr};
    uchar* mDataEnd{nullptr};
    std::unique_ptr<IAck> mAckWaiter;
    QColor mIdentityClrs[2] = {Qt::green, Qt::red};
    uint mIdxIdentityClr{0};
};

#endif // SENDTHREAD_H
