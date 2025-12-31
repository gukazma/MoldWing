#include "CoordinateSystem.hpp"
#include "Core/Logger.hpp"

#include <proj.h>

namespace MoldWing {

QString CoordinateSystem::s_lastError;

QVector<CoordinateSystemInfo> CoordinateSystem::getCommonSystems()
{
    return {
        {0,     "Local",                        "Local coordinate system (no transformation)", false},
        {4326,  "WGS84",                        "World Geodetic System 1984 (GPS)", false},
        {4490,  "CGCS2000",                     "China Geodetic Coordinate System 2000", false},
        {4547,  "CGCS2000 / 3-degree GK CM 114E", "China 2000 Gauss-Kruger Zone 38", true},
        {4548,  "CGCS2000 / 3-degree GK CM 117E", "China 2000 Gauss-Kruger Zone 39", true},
        {4549,  "CGCS2000 / 3-degree GK CM 120E", "China 2000 Gauss-Kruger Zone 40", true},
        {32649, "WGS84 / UTM zone 49N",         "UTM Zone 49 North (108°E - 114°E)", true},
        {32650, "WGS84 / UTM zone 50N",         "UTM Zone 50 North (114°E - 120°E)", true},
        {32651, "WGS84 / UTM zone 51N",         "UTM Zone 51 North (120°E - 126°E)", true},
        {3857,  "WGS84 / Pseudo-Mercator",      "Web Mercator (Google Maps, Bing)", true},
    };
}

bool CoordinateSystem::transform(
    double srcX, double srcY, double srcZ,
    int srcEpsg, int dstEpsg,
    double& dstX, double& dstY, double& dstZ)
{
    if (srcEpsg == 0 || dstEpsg == 0)
    {
        dstX = srcX;
        dstY = srcY;
        dstZ = srcZ;
        return true;
    }

    if (srcEpsg == dstEpsg)
    {
        dstX = srcX;
        dstY = srcY;
        dstZ = srcZ;
        return true;
    }

    PJ_CONTEXT* ctx = proj_context_create();
    if (!ctx)
    {
        s_lastError = "Failed to create PROJ context";
        MW_LOG_ERROR("{}", s_lastError.toStdString());
        return false;
    }

    QString srcCrs = QString("EPSG:%1").arg(srcEpsg);
    QString dstCrs = QString("EPSG:%1").arg(dstEpsg);

    PJ* P = proj_create_crs_to_crs(
        ctx,
        srcCrs.toUtf8().constData(),
        dstCrs.toUtf8().constData(),
        nullptr
    );

    if (!P)
    {
        s_lastError = QString("Failed to create transformation from EPSG:%1 to EPSG:%2")
                          .arg(srcEpsg).arg(dstEpsg);
        MW_LOG_ERROR("{}", s_lastError.toStdString());
        proj_context_destroy(ctx);
        return false;
    }

    PJ* P_normalized = proj_normalize_for_visualization(ctx, P);
    if (P_normalized)
    {
        proj_destroy(P);
        P = P_normalized;
    }

    PJ_COORD src = proj_coord(srcX, srcY, srcZ, 0);
    PJ_COORD dst = proj_trans(P, PJ_FWD, src);

    if (dst.xyz.x == HUGE_VAL)
    {
        s_lastError = QString("Coordinate transformation failed for point (%1, %2, %3)")
                          .arg(srcX).arg(srcY).arg(srcZ);
        MW_LOG_ERROR("{}", s_lastError.toStdString());
        proj_destroy(P);
        proj_context_destroy(ctx);
        return false;
    }

    dstX = dst.xyz.x;
    dstY = dst.xyz.y;
    dstZ = dst.xyz.z;

    proj_destroy(P);
    proj_context_destroy(ctx);

    return true;
}

bool CoordinateSystem::transformBatch(
    const std::vector<std::array<double, 3>>& srcPoints,
    int srcEpsg, int dstEpsg,
    std::vector<std::array<double, 3>>& dstPoints)
{
    dstPoints.clear();
    dstPoints.reserve(srcPoints.size());

    if (srcEpsg == 0 || dstEpsg == 0 || srcEpsg == dstEpsg)
    {
        dstPoints = srcPoints;
        return true;
    }

    PJ_CONTEXT* ctx = proj_context_create();
    if (!ctx)
    {
        s_lastError = "Failed to create PROJ context";
        return false;
    }

    QString srcCrs = QString("EPSG:%1").arg(srcEpsg);
    QString dstCrs = QString("EPSG:%1").arg(dstEpsg);

    PJ* P = proj_create_crs_to_crs(
        ctx,
        srcCrs.toUtf8().constData(),
        dstCrs.toUtf8().constData(),
        nullptr
    );

    if (!P)
    {
        s_lastError = QString("Failed to create transformation from EPSG:%1 to EPSG:%2")
                          .arg(srcEpsg).arg(dstEpsg);
        proj_context_destroy(ctx);
        return false;
    }

    PJ* P_normalized = proj_normalize_for_visualization(ctx, P);
    if (P_normalized)
    {
        proj_destroy(P);
        P = P_normalized;
    }

    for (const auto& srcPt : srcPoints)
    {
        PJ_COORD src = proj_coord(srcPt[0], srcPt[1], srcPt[2], 0);
        PJ_COORD dst = proj_trans(P, PJ_FWD, src);

        if (dst.xyz.x == HUGE_VAL)
        {
            s_lastError = QString("Batch transformation failed at point (%1, %2, %3)")
                              .arg(srcPt[0]).arg(srcPt[1]).arg(srcPt[2]);
            proj_destroy(P);
            proj_context_destroy(ctx);
            return false;
        }

        dstPoints.push_back({dst.xyz.x, dst.xyz.y, dst.xyz.z});
    }

    proj_destroy(P);
    proj_context_destroy(ctx);

    return true;
}

bool CoordinateSystem::isValidEpsg(int epsgCode)
{
    if (epsgCode == 0)
        return true;

    PJ_CONTEXT* ctx = proj_context_create();
    if (!ctx)
        return false;

    QString crs = QString("EPSG:%1").arg(epsgCode);
    PJ* P = proj_create(ctx, crs.toUtf8().constData());

    bool valid = (P != nullptr);

    if (P)
        proj_destroy(P);
    proj_context_destroy(ctx);

    return valid;
}

QString CoordinateSystem::getSystemName(int epsgCode)
{
    auto systems = getCommonSystems();
    for (const auto& sys : systems)
    {
        if (sys.epsgCode == epsgCode)
            return sys.name;
    }

    if (isValidEpsg(epsgCode))
        return QString("EPSG:%1").arg(epsgCode);

    return "Unknown";
}

QString CoordinateSystem::getLastError()
{
    return s_lastError;
}

}
