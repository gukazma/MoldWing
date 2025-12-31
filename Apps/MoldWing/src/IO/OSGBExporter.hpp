#pragma once

#include "Core/MeshData.hpp"
#include "Texture/TextureEditBuffer.hpp"

#include <QString>
#include <memory>
#include <unordered_map>

#include <osg/ref_ptr>
#include <osg/Node>
#include <osg/Geometry>

namespace MoldWing {

struct OSGBExportOptions
{
    QString outputDirectory;
    int sourceEpsg = 0;
    int targetEpsg = 4326;
    double srsOriginX = 0.0;
    double srsOriginY = 0.0;
    double srsOriginZ = 0.0;
    bool generateLOD = true;
    int lodLevels = 3;
    float lodRatio1 = 0.5f;
    float lodRatio2 = 0.25f;
    float lodRatio3 = 0.1f;
    QString tileName;
};

class OSGBExporter
{
public:
    bool exportOSGB(
        const QString& outputDir,
        const MeshData& meshData,
        const OSGBExportOptions& options,
        const std::unordered_map<int, std::shared_ptr<TextureEditBuffer>>& editBuffers = {}
    );

    bool exportMultipleOSGB(
        const QString& outputDir,
        const std::vector<std::pair<const MeshData*, std::unordered_map<int, std::shared_ptr<TextureEditBuffer>>>>& meshes,
        const OSGBExportOptions& options
    );

    QString lastError() const { return m_lastError; }

private:
    osg::ref_ptr<osg::Geometry> convertToOSGGeometry(const MeshData& meshData);

    osg::ref_ptr<osg::Node> createTexturedGeode(
        const MeshData& meshData,
        const QString& tileDir,
        const std::unordered_map<int, std::shared_ptr<TextureEditBuffer>>& editBuffers
    );

    osg::ref_ptr<osg::Node> generateLODNode(
        osg::ref_ptr<osg::Node> fullDetailNode,
        const OSGBExportOptions& options,
        const QString& tileDir,
        const QString& tileName
    );

    bool createTileDirectory(const QString& baseDir, const QString& tileName);

    bool writeMetadata(const QString& outputDir, const OSGBExportOptions& options);

    bool exportTextures(
        const QString& tileDir,
        const MeshData& meshData,
        const std::unordered_map<int, std::shared_ptr<TextureEditBuffer>>& editBuffers
    );

    QString generateTileName(int index) const;

    QString m_lastError;
};

}
