#ifndef DECODETHREAD_H
#define DECODETHREAD_H

#include <QThread>
#include <mutex>
#include <condition_variable>
#include <QSemaphore>
#include <QFile>
#include <QImage>
#include <QQueue>
#include <opencv2/opencv.hpp>

struct DecodeTask
{
    int channel;
    int index;
    QRect area;
    QImage img;
};

struct DecodeResult
{
    int index;
    int syncId;
    QRect area;
};

class DecodeThread: public QThread
{
    Q_OBJECT

public:
   DecodeThread(int totalCnt
                , QSemaphore* finishNotifier
                , char* pFile);

   void reInitialize(int totalCnt
                     , QSemaphore* finishNotifier
                     , char* pFile);
   void pushTask(const DecodeTask& task);

   void run() override;

   void stop();
   bool decodeSuccess() { return !mResults.empty(); }

   auto& results() { return mResults; }

   static size_t seqenceIdSize();
   static size_t maxDataSize();
   static size_t maxBufferSize();

private:
   QImage chopTaskImg(DecodeTask& task);

   int mTotalCnt;
   QSemaphore* mFinishNotifier;
   char* mOutFile;

   QList<QRgb> mColorTable;
   QQueue<DecodeTask> mPendingTasks;
   std::mutex mTaskMut;
   std::condition_variable mTaskCond;

   QList<DecodeResult> mResults;

   std::atomic_bool mIsRunning{true};
};

#endif // DECODETHREAD_H
