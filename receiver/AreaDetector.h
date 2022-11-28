#pragma once
#include <QImage>
#include <utility>
#include <vector>

class AreaDetector
{
public:
    AreaDetector(QColor borderClr, int minSize, int distThres);
    bool detect(QImage img, QRect& rcGrub, QRect& rcQRcode);

private:
    bool IsWhite(QColor clr);
    bool IsBorder(QColor clr);
    bool IsColor(QColor r1, QColor r2);

    int DetectVLine(QImage img, int l, int r, int w, int diff);
    int DetectHLine(QImage img, int l, int r, int w);

    bool IsCorner(const QImage& img, int x, int y, int w);

    QColor mBorderClr;
    int mMinSize;
    int mDistThreshold;

    struct SameColorInfo
    {
        int x{0};
        int len{0};
    };

    std::vector<SameColorInfo> mPattern;
};



