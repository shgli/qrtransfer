#include "DecodeThread.h"
#if USING_ZXING
#include <zxing/LuminanceSource.h>
#include <zxing/common/Counted.h>
#include <zxing/Reader.h>
#include <zxing/common/GlobalHistogramBinarizer.h>
#include <zxing/DecodeHints.h>
#include <zxing/MultiFormatReader.h>
#include <zxing/oned/CodaBarReader.h>
#include <zxing/oned/Code39Reader.h>
#include <zxing/oned/Code93Reader.h>
#include <zxing/oned/Code128Reader.h>
#include <zxing/MatSource.h>
#endif
#include "imgconvertors.h"
#include "opencv2/wechat_qrcode.hpp"
#include <QDir>
#include <QApplication>
static constexpr size_t MAX_BUFFER_SIZE = 2945;
static constexpr size_t SEQ_ID_SIZE = 8;
#if USING_ZXING
std::string decodeByZXing(cv::Mat& matSrc)
{
     cv::Mat matGray;
     cv::cvtColor(matSrc, matGray, cv::COLOR_BGR2GRAY);

     zxing::Ref<zxing::LuminanceSource> source(new MatSource(matGray));
     int width = source->getWidth();
     int height = source->getHeight();
     zxing::Ref<zxing::Reader> reader;
     reader.reset(new zxing::oned::Code128Reader);

     zxing::Ref<zxing::Binarizer> binarizer(new zxing::GlobalHistogramBinarizer(source));
     zxing::Ref<zxing::BinaryBitmap> bitmap(new zxing::BinaryBitmap(binarizer));
     std::string res;
     try {
         zxing::Ref<zxing::Result> result(reader->decode(bitmap, zxing::DecodeHints(zxing::DecodeHints::CODE_39_HINT)));
         res = result->getText()->getText();
     }
     catch(...)
     {
     }
     return res;
}
#endif

size_t DecodeThread::seqenceIdSize() { return SEQ_ID_SIZE; }
size_t DecodeThread::maxDataSize() { return MAX_BUFFER_SIZE; }
size_t DecodeThread::maxBufferSize() { return seqenceIdSize()+maxDataSize(); }

DecodeThread::DecodeThread(int channel, int totalCnt
                           , QSemaphore* finishNotifier
                           , char* pFile)
    :mChannel(channel)
    ,mTotalCnt(totalCnt)
    ,mFinishNotifier(finishNotifier)
    ,mOutFile(pFile)
{
    for(int i = 0; i < 256; ++i)
    {
        mColorTable.push_back(qRgb(i, i, i));
    }
}

void DecodeThread::reInitialize(int totalCnt
                  , QSemaphore* finishNotifier
                  , char* pFile)
{
    mTotalCnt = totalCnt;
    mFinishNotifier = finishNotifier;
    mOutFile = pFile;
}

void DecodeThread::setImage(QImage img)
{
    mWorkingMut.lock();
    mWorkingImage = img;
    mSyncIds.clear();
    mAreas.clear();

    mWorkingCond.notify_one();
    mWorkingMut.unlock();
}

QImage DecodeThread::getChannel(int channel, QImage img)
{
     QImage ret(img.width(), img.height(), QImage::Format_Indexed8);
     ret.setColorTable(mColorTable);
     for(int y = 0; y < img.height(); ++y)
     {
         for(int x = 0; x < img.width(); ++x)
         {
             QColor clr = img.pixelColor(x, y);
             uint idx = 0==channel? clr.red(): (1==channel? clr.green() : clr.blue());
             ret.setPixel(x, y, idx);
         }
     }

     return ret;
}

void DecodeThread::run()
{
    const std::string modelDir = QDir::currentPath().toStdString();

    // 构造（使用异常捕获构造函数是否正常）
    cv::wechat_qrcode::WeChatQRCode detector{
        modelDir + "/detect.prototxt",
        modelDir + "/detect.caffemodel",
        modelDir + "/sr.prototxt",
        modelDir + "/sr.caffemodel"
    };

    while(mIsRunning)
    {
        QImage img;
        mWorkingMut.lock();
        mWorkingCond.wait(&mWorkingMut);
        img = mWorkingImage;
        mWorkingMut.unlock();
        if(!mIsRunning)
        {
            break;
        }

        QImage pChannel = getChannel(mChannel, img);
        cv::Mat  cvImg = ASM::QImageToCvMat(pChannel);
        std::vector<cv::Mat> points;
        auto res = detector.detectAndDecode(cvImg, points);

        if(res.size() > 0)
        {
            for(auto& received : res)
            {
                char* szSyncId = received.data();
                szSyncId[SEQ_ID_SIZE-1] = '\0';

                auto syncId  = atoi(szSyncId);
                const char* data = &received[SEQ_ID_SIZE];
                if(0 != syncId || mTotalCnt == 0)
                {
                    char* dst = mOutFile+(mTotalCnt-syncId)*MAX_BUFFER_SIZE;
                    memcpy(dst, data, received.size()-SEQ_ID_SIZE);
                }
                mSyncIds.push_back(QString::number(syncId));
            }

            for(auto& p : points)
            {
                static const qreal pixelRatio = qApp->devicePixelRatio();

                int x2 = int((p.at<float>(2, 0)+pixelRatio-1.0)/pixelRatio);
                int y2 = int((p.at<float>(2, 1)+pixelRatio-1.0)/pixelRatio);

                int x0 = int(p.at<float>(0, 0)/pixelRatio);
                int y0 = int(p.at<float>(0, 1)/pixelRatio);

                QRect area(QPoint(x0, y0), QPoint(x2, y2));
                mAreas.push_back(area);
            }
        }

        mFinishNotifier->release(1);
    }
}
