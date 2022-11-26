#include "SendThread.h"
#include <QClipboard>
#include <QApplication>
#include <QPainter>
#include <QImage>

static constexpr size_t MAX_BUFFER_SIZE = 2945;
static constexpr size_t SEQ_ID_SIZE = 8;
SendThread::SendThread(const QString& fileName
                       , std::unique_ptr<IAck>&& pAckWaiter
                       , int scalar
                       , int pixelChannel
                       , int imageCnt)
    :mPixelChannel(pixelChannel)
    ,mImageCnt(imageCnt)
    ,mQRCodeCntOneTime(mPixelChannel*mImageCnt)
    ,mScalar(scalar)
    ,mRowCnt(1==mImageCnt?1:2)
    ,mColCnt(mImageCnt/mRowCnt+(mImageCnt%mRowCnt>0 ? 1 : 0))
{
    mAckWaiter = std::move(pAckWaiter);
    mBuffer.reserve(SEQ_ID_SIZE+MAX_BUFFER_SIZE);
    mBuffer.resize(SEQ_ID_SIZE+MAX_BUFFER_SIZE);
    mNetSeqId = mBuffer.data();
    mData = &(mBuffer[SEQ_ID_SIZE]);
    mInputFile = std::make_unique<QFile>(fileName, nullptr);
    mInputFile->open(QIODevice::ReadOnly);
}

void SendThread::stop()
{
    if(!mIsRunning)
    {
        return;
    }

    mIsRunning = false;
    mAckWaiter->stop();
    while (isRunning())
    {
        msleep(5);
    }
}

void SendThread::run()
{
    size_t fsize = mInputFile->size();
    int sendCnt = fsize/MAX_BUFFER_SIZE + ((0 == (fsize%MAX_BUFFER_SIZE)) ? 0 : 1);
    mSequenceId = sendCnt;
    int ackedCnt = 0;
    prepareCfgQRCode(sendCnt, fsize);
    while (mIsRunning && !mPendingCodes.empty())
    {
        std::pair<QImage, int> imgs = prepareImages();

        emit qrReady(imgs.first, sendCnt-ackedCnt+1);
        yieldCurrentThread();

        prepareQRCode();
        QString strAck = mAckWaiter->wait(QString::number(ackedCnt)+"|");
        if(!mIsRunning) break;

        qDebug() << "acked:" << strAck;
        QList<QString> ackList = strAck.split("|");
        if(ackList.size() >= 2)
        {
            if(0 == ackedCnt)
            {
                QRcode_free(mSendingCodes[0].second);
                ackedCnt++;
            }
            else
            {
                std::vector<std::pair<int, QRcode*>> unacked;
                int rcvAcked = ackList[0].toInt();
                assert(rcvAcked == ackedCnt);

                auto index = 0;
                for(int iAck = 1; iAck < ackList.size(); ++iAck)
                {
                    auto successIdx = ackList[iAck].toULongLong();
                    index = sizeof(successIdx) * (iAck-1);
                    while(successIdx != 0UL)
                    {
                        if(0 != (successIdx & 1UL))
                        {
                            ackedCnt++;
                            QRcode_free(mSendingCodes[index].second);
                        }
                        else
                        {
                            mPendingCodes.push_front(mSendingCodes[index]);
                        }
                        successIdx /= 2;
                        index += 1;
                    }
                }
            }
        }
    }

    if(mIsRunning)
    {
        emit qrReady(QImage(), sendCnt-ackedCnt+1);
    }
    mInputFile->close();
}

void SendThread::prepareCfgQRCode(int totalCnt, size_t fsize)
{
    snprintf(mNetSeqId, SEQ_ID_SIZE, "%07d", 0);
    mNetSeqId[SEQ_ID_SIZE-1] = '|';
    int size = snprintf(mData, MAX_BUFFER_SIZE, "%ld|%d|%d|%d|%d", fsize, totalCnt, mPixelChannel, mRowCnt, mColCnt);

    QRcode* qr = QRcode_encodeData(size+SEQ_ID_SIZE, (const unsigned char*)mBuffer.data(), 40, QR_ECLEVEL_L);
    if (qr && qr->width > 0)
    {
        mImageWidth = mColCnt * qr->width*mScalar + mSpliterWidth*mScalar*(mColCnt-1);
        mImageHeight = mRowCnt * qr->width*mScalar + mSpliterWidth*mScalar*(mRowCnt-1);
        mPendingCodes.push_back(std::make_pair(0, qr));
    }
}

void SendThread::prepareQRCode( void )
{
    while(!mInputFile->atEnd() && mPendingCodes.size() < (size_t)mQRCodeCntOneTime)
    {
        snprintf(mNetSeqId, SEQ_ID_SIZE, "%07d", mSequenceId);
        mNetSeqId[SEQ_ID_SIZE-1] = '|';
        auto size = mInputFile->read(mData, MAX_BUFFER_SIZE);
        qDebug() << "prepare: " << mSequenceId;
        QRcode* qr = QRcode_encodeData(size+SEQ_ID_SIZE, (const unsigned char*)mBuffer.data(), 40, QR_ECLEVEL_L);
        if (qr && qr->width > 0)
        {
            mImageWidth = mColCnt * qr->width*mScalar + mSpliterWidth*mScalar*(mColCnt-1);
            mImageHeight = mRowCnt * qr->width*mScalar + mSpliterWidth*mScalar*(mRowCnt-1);
            mPendingCodes.push_back(std::make_pair(mSequenceId, qr));
            --mSequenceId;
        }
        else
        {
            assert(false);
        }


    }
}

std::pair<QImage, int> SendThread::prepareImages( void )
{
    static auto toValue = [](QRcode* qr, int y, int x)
    {
        return nullptr != qr && qr->data[y*qr->width+x] & 1 ? 0 : 255;
    };

    mSendingCodes.clear();
    int baseSeqId = 2000000000;
    QImage ret(mImageWidth, mImageHeight, QImage::Format_RGB32);
    QPainter painter(&ret);
    painter.fillRect(0, 0, mImageWidth, mImageHeight, Qt::white);//背景填充白色
    painter.setPen(Qt::NoPen);
    for(int iImg = 0; iImg < mImageCnt && !mPendingCodes.empty(); ++iImg)
    {
        std::pair<int,QRcode*> first = mPendingCodes.front(); mPendingCodes.pop_front();
        mSendingCodes.push_back(first);

        baseSeqId = std::min(baseSeqId, first.first);
        std::pair<int,QRcode*> second{0, nullptr};
        if(mPixelChannel >= 2 && !mPendingCodes.empty())
        {
            second = mPendingCodes.front(); mPendingCodes.pop_front();
            mSendingCodes.push_back(second);
            baseSeqId = std::min(baseSeqId, second.first);
        }
        std::pair<int,QRcode*> third{0, nullptr};
        if(mPixelChannel >= 3 && !mPendingCodes.empty())
        {
            third = mPendingCodes.front(); mPendingCodes.pop_front();
            mSendingCodes.push_back(third);
            baseSeqId = std::min(baseSeqId, third.first);
        }

        QRcode* qr = first.second;
        int iRow = iImg/mColCnt;
        int iCol = iImg%mColCnt;
        int iX = iCol*(qr->width*mScalar + mSpliterWidth*mScalar);
        int iY = iRow*(qr->width*mScalar + mSpliterWidth*mScalar);
        if (qr && qr->width > 0)
        {
            for (int y = 0; y < qr->width; y++) //行
            {
                for (int x = 0; x < qr->width; x++) //列
                {
                    int red = toValue(qr, y, x);
                    int green = toValue(second.second, y, x);
                    int blue = toValue(third.second, y, x);
                    QColor clr(red, green, blue);
                    painter.setBrush(clr);

                    QRect r(iX+x * mScalar
                            , iY+y * mScalar
                            , mScalar
                            , mScalar);
                    painter.drawRect(r);
                }
            }
        }
    }

    return std::make_pair(ret, baseSeqId);
}
