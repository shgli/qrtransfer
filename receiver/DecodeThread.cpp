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
#define USING_ZBAR
static constexpr size_t SEQ_ID_SIZE = 11;
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
#elif defined(USING_ZBAR)
#include "zbar.h"
#include <iostream>

using namespace std;
using namespace zbar;  //添加zbar名称空间
using namespace cv;

std::string decodeByZBar(cv::Mat& imageGray, QRect& area)
{
    ImageScanner scanner;
    scanner.set_config(ZBAR_QRCODE, ZBAR_CFG_ENABLE, 1);
    //Mat imageGray;
    //cvtColor(image,imageGray, CV_RGB2GRAY);
    int width = imageGray.cols;
    int height = imageGray.rows;
    uchar *raw = (uchar *)imageGray.data;
    Image imageZbar(width, height, "Y800", raw, width * height);
    scanner.scan(imageZbar); //扫描条码
    Image::SymbolIterator symbol = imageZbar.symbol_begin();
    if(imageZbar.symbol_begin()==imageZbar.symbol_end())
    {
        return string();
    }

//    int left = 10000000;
//    int top = 10000000;
//    int right = 0;
//    int bottom = 0;

//    auto pt = symbol->point_begin();
//    for (; pt != symbol->point_end(); ++pt)
//    {
//        left = std::min(left, (*pt).x);
//        top = std::min(top, (*pt).y);

//        right = std::max(right, (*pt).x);
//        bottom = std::max(bottom, (*pt).y);
//    }

    imageZbar.set_data(nullptr,0);
    return symbol->get_data();
}
#else
std::string decodeByZXing(cv::Mat& image, QRect& area)
{
}

#endif

size_t DecodeThread::seqenceIdSize() { return SEQ_ID_SIZE; }

DecodeThread::DecodeThread(qint64 fileSize
                           , QSemaphore* finishNotifier
                           , char* pFile)
    :mFileSize(fileSize)
    ,mFinishNotifier(finishNotifier)
    ,mOutFile(pFile)
{
    for(int i = 0; i < 256; ++i)
    {
        mColorTable.push_back(qRgb(i, i, i));
    }
}

void DecodeThread::reInitialize(qint64 fileSize
                  , QSemaphore* finishNotifier
                  , char* pFile)
{
    mFileSize = fileSize;
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
    static auto getChannel = [](DecodeTask& t, int x, int y)
    {
        QColor clr = t.img.pixelColor(x, y);
        uint idx = 0==t.channel? clr.red(): (1==t.channel? clr.green() : clr.blue());
        return idx;
    };

    try {

    if(0 == mFileSize)
    {
        static const size_t MAX_AREA_SIZE = 2048;
        int rowValueCnt[MAX_AREA_SIZE];
        int colValueCnt[MAX_AREA_SIZE];
        memset(rowValueCnt, 0, sizeof(rowValueCnt));
        memset(colValueCnt, 0, sizeof(colValueCnt));
        for(int y = task.area.top(); y <= task.area.bottom(); ++y)
        {
            for(int x = task.area.left(); x <= task.area.right(); ++x)
            {
                if(getChannel(task, x, y) < 128)
                {
                    rowValueCnt[y-task.area.top()] += 1;
                    colValueCnt[x-task.area.left()] += 1;
                }
            }
        }

        int* minRV = nullptr, *minCV = nullptr;
        int* maxRV = nullptr, *maxCV = nullptr;

        std::tie(minRV, maxRV) = std::minmax_element(rowValueCnt, rowValueCnt+MAX_AREA_SIZE);
        std::tie(minCV, maxCV) = std::minmax_element(colValueCnt, colValueCnt+MAX_AREA_SIZE);
        auto find_first_index = [](int* cur, int* end, int inc, auto v)
        {
            for(; cur != end; cur += inc)
            {
                if(v(*cur))
                {
                    return cur;
                }
            }

            return (int*)nullptr;
        };

        int left = find_first_index(colValueCnt, colValueCnt+MAX_AREA_SIZE, 1, [maxCV](int v){ return v > (*maxCV)/5; })-colValueCnt;
        int right = find_first_index(colValueCnt+MAX_AREA_SIZE-1, colValueCnt-1, -1, [maxCV](int v){ return v > (*maxCV)/5; })-colValueCnt;
        int top = find_first_index(rowValueCnt, rowValueCnt+MAX_AREA_SIZE, 1, [maxRV](int v){ return v > (*maxRV)/5; })-rowValueCnt;
        int bottom = find_first_index(rowValueCnt+MAX_AREA_SIZE-1, rowValueCnt-1, -1, [maxRV](int v){ return v > (*maxRV)/5; })-rowValueCnt;

        task.area = QRect(task.area.left()+left-1, task.area.top()+top-1, right-left+1+2, bottom-top+1+2);
    }
    } catch (...) {
        int i = 0;
        ++i;
    }

     QImage ret(task.area.width(), task.area.height(), QImage::Format_Indexed8);
    try {
     ret.setColorTable(mColorTable);
     int ty = 0;
     for(int y = task.area.top(); y <= task.area.bottom(); ++y, ty++)
     {
         int tx = 0;
         for(int x = task.area.left(); x <= task.area.right(); ++x, ++tx)
         {
             uint idx =getChannel(task, x, y);
             ret.setPixel(tx, ty, idx);
         }
     }
    } catch (...) {
         int i = 0;
         ++i;
    }
     return ret;
}

void DecodeThread::run()
{
#ifdef USING_WECHAT
    const std::string modelDir = QDir::currentPath().toStdString();

    // 构造（使用异常捕获构造函数是否正常）
    cv::wechat_qrcode::WeChatQRCode detector{
        modelDir + "/detect.prototxt",
        modelDir + "/detect.caffemodel",
        modelDir + "/sr.prototxt",
        modelDir + "/sr.caffemodel"
    };
#elif defined(USING_ZBAR)

#endif
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
        cv::Mat  cvImg = ASM::QImageToCvMat(pChannel, false);
#ifdef USING_WECHAT
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
            std::string& received = res[0];
#elif defined(USING_ZBAR)
        //Mat imageGray;
        //cvtColor(image,imageGray, CV_RGB2GRAY);
        int width = cvImg.cols;
        int height = cvImg.rows;
        uchar *raw = (uchar *)cvImg.data;
        Image imageZbar(width, height, "Y800", raw, width * height);
        ImageScanner scanner;
        scanner.set_config(ZBAR_QRCODE, ZBAR_CFG_ENABLE, 1);
        scanner.set_config(ZBAR_QRCODE, ZBAR_CFG_BINARY, 1);
        scanner.scan(imageZbar); //扫描条码
        Image::SymbolIterator symbol = imageZbar.symbol_begin();
        std::string received;
        if(imageZbar.symbol_begin() != imageZbar.symbol_end())
        {
            received = symbol->get_data();
        }
#endif
         if(!received.empty())
         {
            DecodeResult ret;
            ret.index = task.index;
            {
                char* szoffset = received.data();
                szoffset[SEQ_ID_SIZE-1] = '\0';

                qint64 offset  = atol(szoffset);
                const char* data = &received[SEQ_ID_SIZE];

                bool isCfg = -1 == offset;
                ret.offset = isCfg ? 0L : offset;
                if((0 == mFileSize && !isCfg) || (isCfg && 0 != mFileSize))
                {
                    ret.len = 0;
                }
                else
                {
                    char* dst = mOutFile+ret.offset ;
                    ret.len = received.size()-SEQ_ID_SIZE;
                    memcpy(dst, data, ret.len);
//                    if(ret.len >= 2942)
//                    {
//                        ret.len = 2942;
//                    }
                }
            }

//            auto& p = points[0];
//            {
//                static const qreal pixelRatio = qApp->devicePixelRatio();

//                int x2 = int((p.at<float>(2, 0))/pixelRatio+0.5);
//                int y2 = int((p.at<float>(2, 1))/pixelRatio+0.5);

//                int x0 = int(p.at<float>(0, 0)/pixelRatio+0.5);
//                int y0 = int(p.at<float>(0, 1)/pixelRatio+0.5);

//                ret.area = QRect(QPoint(x0, y0), QPoint(x2, y2));
//            }

            mResults.push_back(ret);
#ifdef USING_WECHAT
         }
#endif
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
