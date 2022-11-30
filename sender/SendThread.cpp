#include "SendThread.h"
#include <QClipboard>
#include <QApplication>
#include <QPainter>
#include <QImage>
#include <random>
#include <algorithm>

static constexpr qint64 MAX_BUFFER_SIZE = 2942;
static constexpr qint64 SEQ_ID_SIZE = 11;
SendThread::SendThread(const QString& fileName
                       , std::unique_ptr<IAck>&& pAckWaiter
                       , int scalar
                       , int rows
                       , int cols
                       , int pixelChannel
                       )
    :mPixelChannel(pixelChannel)
    ,mImageCnt(rows*cols)
    ,mQRCodeCntOneTime(mPixelChannel*rows*cols)
    ,mScalar(scalar)
    ,mRetryScalar(scalar+1)
    ,mRowCnt(rows)
    ,mColCnt(cols)
{
    mAckWaiter = std::move(pAckWaiter);
//    mBuffer.reserve(SEQ_ID_SIZE+MAX_BUFFER_SIZE);
//    mBuffer.resize(SEQ_ID_SIZE+MAX_BUFFER_SIZE);
    mNetSeqId = new char[SEQ_ID_SIZE];
    mInputFile = std::make_unique<QFile>(fileName, nullptr);
    mInputFile->open(QIODevice::ReadOnly);
    mData = mInputFile->map(0, mInputFile->size());
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
    qint64 fsize = mInputFile->size();
    mNextReadOffset = 0;
    qint64 recvedLen = -1;
    bool isCfgReceived = false;
    prepareCfgQRCode(fsize);
    std::random_device rd;
    while (mIsRunning && recvedLen < fsize)
    {
        QImage imgs = prepareImages(mIdentityClrs[mIdxIdentityClr%2]);
        ++mIdxIdentityClr;
        emit qrReady(imgs, fsize-recvedLen);
        yieldCurrentThread();

        prepareQRCode();
        QString strAck = mAckWaiter->wait(QString::number(recvedLen)+"|");
        if(!mIsRunning) break;

        qDebug() << "acked:" << strAck;
        QList<QString> ackList = strAck.split("|");
        if(ackList.size() >= 2)
        {
            if(!isCfgReceived)
            {
                isCfgReceived = true;
                recvedLen = 0;
                QRcode_free(mSendingCodes[0].code);
            }
            else
            {
                int rcvAcked = ackList[0].toInt();
                assert(rcvAcked == recvedLen);

                auto index = 0;
                int thisRecvCnt = 0;
                for(int iAck = 1; iAck < ackList.size(); ++iAck)
                {
                    auto successIdx = ackList[iAck].toULongLong();
                    index = 8*sizeof(successIdx) * (iAck-1);
                    while(successIdx != 0UL)
                    {
                        auto& sending = mSendingCodes[index];
                        if(0 != (successIdx & 1UL))
                        {
                            thisRecvCnt++;
                            recvedLen += sending.len;
                            //qDebug() << "acked offset:" << sending.offset << ", rcv len:" << recvedLen;
                            QRcode_free(sending.code);
                            sending.code = nullptr;
                        }

                        successIdx /= 2;
                        index += 1;
                    }
                }

                if(thisRecvCnt != mQRCodeCntOneTime)
                {
                    for(auto& sending : mSendingCodes)
                    {
                        if(nullptr != sending.code)
                        {
                            if(1) //MAX_BUFFER_SIZE == sending.len && (sending.offset+sending.len) < mInputFile->size())
                            {
                                splitQRcode(sending.offset, sending.len);
                            }
                            else
                            {
                                delayed.push_back(sending);
                            }
                        }
                    }
                }
            }
        }
    }

    if(mIsRunning)
    {
        emit qrReady(QImage(), fsize-recvedLen);
    }
    mInputFile->close();
}

void SendThread::prepareCfgQRCode(size_t fsize)
{
    char cfg[64];
    uint size = snprintf(cfg, MAX_BUFFER_SIZE, "%10d|%ld|%d|%d|%d", -1, fsize, mPixelChannel, mRowCnt, mColCnt);

    QRcode* qr = QRcode_encodeData(size, (const unsigned char*)cfg, 40, QR_ECLEVEL_L);
    if (qr && qr->width > 0)
    {
        mImageWidth = mColCnt * qr->width*mScalar + mSpliterWidth*mScalar*mColCnt;
        mImageHeight = mRowCnt * qr->width*mScalar + mSpliterWidth*mScalar*mRowCnt;
        mPendingCodes.push_back({0, size, qr});
    }
}

int SendThread::createQRcode(qint64 offset, qint64 len)
{
    static thread_local char buffer[SEQ_ID_SIZE+MAX_BUFFER_SIZE];
    static thread_local char* data = buffer+SEQ_ID_SIZE;
    snprintf(buffer, SEQ_ID_SIZE, "%010d", offset);
    buffer[SEQ_ID_SIZE-1] = '|';

    qint64 size = std::min(len, mInputFile->size()-offset);
    memcpy(data, mData+offset, size);
//    QRinput *pInput = QRinput_new2(40, QR_ECLEVEL_L);
//    QRinput_append(pInput, QR_MODE_AN, SEQ_ID_SIZE, (uchar*)mNetSeqId);
//    QRinput_append(pInput, QR_MODE_8, size, mData+offset);
//    QRcode* qr = QRcode_encodeInput(pInput);
//    QRinput_free(pInput);

    QRcode* qr = QRcode_encodeData(size+SEQ_ID_SIZE, (const unsigned char*)buffer, 40, QR_ECLEVEL_L);
    if (qr && qr->width > 0)
    {
//            mImageWidth = mColCnt * qr->width*mScalar + mSpliterWidth*mScalar*mColCnt;
//            mImageHeight = mRowCnt * qr->width*mScalar + mSpliterWidth*mScalar*mRowCnt;
        mPendingCodes.push_back({offset, size, qr});
    }
    else
    {
        assert(false);
    }

    return size;
}

void SendThread::prepareQRCode( void )
{
    while(mNextReadOffset < mInputFile->size() && mPendingCodes.size() < (size_t)mQRCodeCntOneTime)
    {
        mNextReadOffset += createQRcode(mNextReadOffset, MAX_BUFFER_SIZE);
    }

    while(mPendingCodes.size() < (size_t)mQRCodeCntOneTime && !delayed.empty())
    {
        mIsRetrying = true;
        mPendingCodes.push_back(delayed.back());
        delayed.pop_back();
    }
}

void SendThread::splitQRcode(qint64 offset, qint64 len)
{
    int len1 = len/2;
    createQRcode(offset, len1);
    createQRcode(offset+len1, len-len1);
}

QImage SendThread::prepareImages(QColor identityClr)
{
    static auto toValue = [](QRcode* qr, int y, int x)
    {
        return nullptr != qr && qr->data[y*qr->width+x] & 1 ? 0 : 255;
    };

    mSendingCodes.clear();

    int usedScalar = mIsRetrying ? mRetryScalar : mScalar;
    int borderWidth = 2*usedScalar;
    int spliterWidth = mSpliterWidth*usedScalar;
    int imageWidth = mImageWidth*usedScalar/mScalar;
    int imageHeight= mImageHeight*usedScalar/mScalar;
    QImage ret(imageWidth+borderWidth, imageHeight+borderWidth, QImage::Format_RGB32);
    QPainter painter(&ret);
    painter.fillRect(0, 0, ret.width(), ret.height(), Qt::white);//背景填充白色
    painter.fillRect(0, 0, borderWidth, imageHeight, Qt::black); //left black line
    painter.fillRect(borderWidth, imageHeight, imageWidth, borderWidth, Qt::black); //bottom black line
    painter.fillRect(0, imageHeight, borderWidth, borderWidth, identityClr);
    painter.setPen(Qt::NoPen);

    for(int iImg = 0; iImg < mImageCnt && !mPendingCodes.empty(); ++iImg)
    {
        SendingInfo first = mPendingCodes.front(); mPendingCodes.pop_front();
        mSendingCodes.push_back(first);
//        qDebug() << "sending:" << first.offset << "," << first.len;
        SendingInfo second{0, 0, nullptr};
        if(mPixelChannel >= 2 && !mPendingCodes.empty())
        {
            second = mPendingCodes.front(); mPendingCodes.pop_front();
            mSendingCodes.push_back(second);
        }
        SendingInfo third{0, 0, nullptr};
        if(mPixelChannel >= 3 && !mPendingCodes.empty())
        {
            third = mPendingCodes.front(); mPendingCodes.pop_front();
            mSendingCodes.push_back(third);
        }

        QRcode* qr = first.code;
        int iRow = iImg/mColCnt;
        int iCol = iImg%mColCnt;
        int iX = iCol*(qr->width + mSpliterWidth)*usedScalar + borderWidth + spliterWidth;
        int iY = iRow*(qr->width + mSpliterWidth)*usedScalar;
        if (qr && qr->width > 0)
        {
            for (int y = 0; y < qr->width; y++) //行
            {
                for (int x = 0; x < qr->width; x++) //列
                {
                    int red = toValue(qr, y, x);
                    int green = toValue(second.code, y, x);
                    int blue = toValue(third.code, y, x);
                    QColor clr(red, green, blue);
                    painter.setBrush(clr);

                    QRect r(iX+x * usedScalar
                            , iY+y * usedScalar
                            , usedScalar
                            , usedScalar);
                    painter.drawRect(r);
                }
            }
        }
    }

    return ret;
}
