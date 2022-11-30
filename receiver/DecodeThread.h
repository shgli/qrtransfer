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
    uint index;
    qint64 offset;
    qint64 len;
    QRect area;
};

class DecodeThread: public QThread
{
    Q_OBJECT

public:
   DecodeThread(qint64 fileSize
                , QSemaphore* finishNotifier
                , char* pFile);

   void reInitialize(qint64 fileSize
                     , QSemaphore* finishNotifier
                     , char* pFile);
   void pushTask(const DecodeTask& task);

   void run() override;

   void stop();
   bool decodeSuccess() { return !mResults.empty(); }

   auto& results() { return mResults; }

   static size_t seqenceIdSize();

private:
   QImage chopTaskImg(DecodeTask& task);

   qint64 mFileSize;
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
