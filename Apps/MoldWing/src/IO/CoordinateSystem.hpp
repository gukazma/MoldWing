#pragma once

#include <QString>
#include <QVector>
#include <array>
#include <vector>

namespace MoldWing {

struct CoordinateSystemInfo
{
    int epsgCode;
    QString name;
    QString description;
    bool isProjected;
};

class CoordinateSystem
{
public:
    static QVector<CoordinateSystemInfo> getCommonSystems();

    static bool transform(
        double srcX, double srcY, double srcZ,
        int srcEpsg, int dstEpsg,
        double& dstX, double& dstY, double& dstZ
    );

    static bool transformBatch(
        const std::vector<std::array<double, 3>>& srcPoints,
        int srcEpsg, int dstEpsg,
        std::vector<std::array<double, 3>>& dstPoints
    );

    static bool isValidEpsg(int epsgCode);

    static QString getSystemName(int epsgCode);

    static QString getLastError();

private:
    static QString s_lastError;
};

}
