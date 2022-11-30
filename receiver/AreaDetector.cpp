#include <QApplication>
#include "AreaDetector.h"
AreaDetector::AreaDetector(QColor borderClr
    , int minSize
    , int distThres)
:mBorderClr(borderClr)
,mMinSize(minSize)
,mDistThreshold(distThres)
{}

bool AreaDetector::detect(QImage img, QRect& rcGrub, QRect& rcQRcode, QPoint& offset, QPoint& idClrPos, QColor& idClr)
{
    for(int y = mMinSize-1; y < img.height(); y += mMinSize)
    {
        SameColorInfo white{0, 0};
        SameColorInfo border{0, 0};
        SameColorInfo* prev{nullptr};
        for(int x = 0; x < img.width(); ++x)
        {
            QColor clr = img.pixelColor(x, y);
//            auto red1 = clr.red();
//            auto red2 = clr.red();
//            auto green1 = clr.green();
            if(IsWhite(clr))
            {
                if(prev != &white && nullptr != prev)
                {
                    white.x = x;
                    white.len = 0;
                }
                white.len++;

                if(1 == mPattern.size())
                {
                    int len = DetectVLine(img, prev->x, y+1, prev->len, 1)+1;
                    int bottom = y + len;
                    len += DetectVLine(img, prev->x, y-1, prev->len, -1);

                    if(len >= mMinSize)
                    {
                        if(IsCorner(img, prev->x, bottom, prev->len, idClr))
                        {
                            int hlen = DetectHLine(img, x, bottom, prev->len);
                            //if(std::abs(hlen-len)*50 < std::min(hlen,len))
                            {
                                static const qreal pixelRatio = qApp->devicePixelRatio();
                                static const int PIXEL_RATIO = std::ceil(pixelRatio);
                                static auto _C = [](float c)
                                {
                                  return int(c/pixelRatio+0.5);
                                };

//                                len = std::max(hlen, len);
//                                len += 2;
                                int grubLeft = prev->x-PIXEL_RATIO;
                                int grubTop = bottom-len-PIXEL_RATIO;
                                int grubWidth = hlen+prev->len+2*PIXEL_RATIO;
                                int grubHeight = len+prev->len+2*PIXEL_RATIO;
                                rcGrub = QRect(rcGrub.left()+_C(grubLeft), rcGrub.top() + _C(grubTop), _C(grubWidth), _C(grubHeight));
                                rcQRcode = QRect(x+1, grubTop+PIXEL_RATIO, hlen, len);
                                offset = QPoint(rcQRcode.left()-grubLeft, rcQRcode.top()-grubTop);

                                QRect idRect{prev->x, bottom, prev->len, prev->len};
                                idClrPos = idRect.center() - rcQRcode.bottomLeft();
                                return true;
                            }
                        }
                    }
                }

                prev = &white;
            }
            else if(IsBorder(clr))
            {
//                auto red1 = clr.red();
//                auto red2 = clr.red();
//                auto green1 = clr.green();
                if(nullptr != prev && prev != &border)
                {
                    mPattern.push_back(*prev);
                    border.len = 0;
                    border.x = x;
                }
                border.len++;
                prev = &border;
            }
            else
            {
                prev = nullptr;
                mPattern.clear();
            }
        }

        mPattern.clear();
    }

    return false;
}

bool AreaDetector::IsColor(QColor r1, QColor r2, int threshold)
{
    return (std::abs(r1.red()-r2.red()) < threshold)
            && std::abs(r1.green()-r2.green()) < threshold
            && std::abs(r1.blue()-r2.blue()) < threshold;
}

bool AreaDetector::IsWhite(QColor clr)
{
    return (std::abs(clr.red()-clr.green())-std::abs(clr.red()-clr.blue())) < 20 && IsColor(Qt::white, clr, mDistThreshold);
}

bool AreaDetector::IsBorder(QColor clr)
{
    return (std::abs(clr.red()-clr.green())-std::abs(clr.red()-clr.blue())) < 20 &&IsColor(mBorderClr, clr, mDistThreshold);
}


bool AreaDetector::IsIdentityColor(QColor clr)
{
    return AreaDetector::IsColor(clr, Qt::green, 50) || AreaDetector::IsColor(clr, Qt::red, 50);
}

bool AreaDetector::IsCorner(const QImage& img, int x, int y, int w, QColor& idClr)
{
    idClr = img.pixelColor(x, y);
    if(!IsIdentityColor(idClr))
    {
        return false;
    }

    int right = x + w - 1;
    int bottom = y + w - 1;
    for(int xx = x; xx <= right; ++xx)
    {
        for(int yy = y; yy <= bottom; ++yy)
        {
            QColor clr = img.pixelColor(xx, yy);
//            int red = clr.red();
//            int green = clr.green();
//            int blue = clr.blue();
            if(!IsColor(clr, idClr, mDistThreshold))
            {
                return false;
            }
        }
    }

    return true;
}

int AreaDetector::DetectVLine(QImage img, int x, int y, int w, int diff)
{
    int len = 0;
    for(; y >= 0 && y < img.height(); y += diff)
    {
        QColor first = img.pixelColor(x-1, y);
        QColor last = img.pixelColor(x+w, y);
        if(!IsWhite(first) || !IsWhite(last))
        {
            return len;
        }

        int xx = x+w-1;
        while(xx >= x && IsBorder(img.pixelColor(xx, y)))
        {
            --xx;
        }

        if(xx != (x-1))
        {
            return len;
        }

        ++len;
    }

    return len;
}

int AreaDetector::DetectHLine(QImage img, int x, int y, int w)
{
    int len = 0;
    for(; x < img.width(); ++x)
    {
        QColor first = img.pixelColor(x, y-1);
        QColor last = img.pixelColor(x, y+w);
        if(!IsWhite(first) || !IsWhite(last))
        {
            return len;
        }

        int yy = y+w-1;
        while(yy >= y && IsBorder(img.pixelColor(x, yy)))
        {
            --yy;
        }

        if(yy != (y-1))
        {
            return len;
        }

        len++;
    }

    return len;
}
