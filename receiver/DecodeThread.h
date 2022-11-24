#ifndef DECODETHREAD_H
#define DECODETHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QSemaphore>
#include <QFile>
#include <QImage>
#include <opencv2/opencv.hpp>
class DecodeThread: public QThread
{
    Q_OBJECT

public:
   DecodeThread(int channel, int totalCnt
                , QSemaphore* finishNotifier
                , char* pFile);

   void reInitialize(int totalCnt
                     , QSemaphore* finishNotifier
                     , char* pFile);
   void setImage(QImage image);

   void run() override;

   void stop() { mIsRunning = false; }
   bool decodeSuccess() { return !mSyncIds.empty(); }

   QStringList& syncIds() { return mSyncIds; }
   std::vector<QRect>& areas() { return mAreas; }

   static size_t seqenceIdSize();
   static size_t maxDataSize();
   static size_t maxBufferSize();

private:
   QImage getChannel(int channel, QImage img);

   int mChannel;
   int mTotalCnt;
   QSemaphore* mFinishNotifier;
   char* mOutFile;

   QList<QRgb> mColorTable;
   QImage mWorkingImage;
   QMutex mWorkingMut;
   QWaitCondition mWorkingCond;
   QStringList mSyncIds;
   std::vector<QRect> mAreas;

   bool mIsRunning{true};
};

#endif // DECODETHREAD_H
