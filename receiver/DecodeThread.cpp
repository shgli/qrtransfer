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

DecodeThread::DecodeThread(int totalCnt
                           , QSemaphore* finishNotifier
                           , char* pFile)
    :mTotalCnt(totalCnt)
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

void DecodeThread::stop()
{
    if(mIsRunning)
    {
        {
            mIsRunning = false;
            std::unique_lock lock(mTaskMut);
            mTaskCond.notify_one();
        }

        while (isRunning())
        {
            msleep(5);
        }
    }
}

void DecodeThread::pushTask(const DecodeTask& task)
{
    std::unique_lock lock(mTaskMut);
    mPendingTasks.enqueue(task);
    mTaskCond.notify_one();
}

QImage DecodeThread::chopTaskImg(DecodeTask& task)
{
     QImage ret(task.area.width(), task.area.height(), QImage::Format_Indexed8);
     ret.setColorTable(mColorTable);
     int ty = 0;
     for(int y = task.area.top(); y <= task.area.bottom(); ++y, ty++)
     {
         int tx = 0;
         for(int x = task.area.left(); x <= task.area.right(); ++x, ++tx)
         {
             QColor clr = task.img.pixelColor(x, y);
             uint idx = 0==task.channel? clr.red(): (1==task.channel? clr.green() : clr.blue());

             ret.setPixel(tx, ty, idx);
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
        DecodeTask task;
        {
            std::unique_lock lock(mTaskMut);
            mTaskCond.wait(lock, [this](){ return !mPendingTasks.empty() || !mIsRunning; });
            if(!mIsRunning) break;

            task = mPendingTasks.dequeue();
        }

        QImage pChannel = chopTaskImg(task);
        cv::Mat  cvImg = ASM::QImageToCvMat(pChannel);
        std::vector<cv::Mat> points;
        std::vector<std::string> res;
        try {
            res = detector.detectAndDecode(cvImg, points);
        } catch (const std::exception& e) {
            qDebug() << "decode exception: channel:" << task.channel << " index:" << task.index << ", msg:" << e.what();
        }

        if(res.size() > 0)
        {
            assert(1 == res.size());
            DecodeResult ret;
            ret.index = task.index;
            std::string& received = res[0];
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
                ret.syncId = syncId;
            }

            auto& p = points[0];
            {
                static const qreal pixelRatio = qApp->devicePixelRatio();

                int x2 = int((p.at<float>(2, 0))/pixelRatio+0.5);
                int y2 = int((p.at<float>(2, 1))/pixelRatio+0.5);

                int x0 = int(p.at<float>(0, 0)/pixelRatio+0.5);
                int y0 = int(p.at<float>(0, 1)/pixelRatio+0.5);

                ret.area = QRect(QPoint(x0, y0), QPoint(x2, y2));
            }

            mResults.push_back(ret);
        }
        else
        {
            pChannel.save("fail.png");
            int i = 0;
            ++i;
        }

        mFinishNotifier->release(1);
    }
}
