#include "OSGBExporter.hpp"
#include "CoordinateSystem.hpp"
#include "Core/Logger.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <osg/Geode>
#include <osg/Group>
#include <osg/LOD>
#include <osg/PagedLOD>
#include <osg/Texture2D>
#include <osg/Material>
#include <osg/StateSet>
#include <osgDB/WriteFile>
#include <osgDB/ReadFile>
#include <osgUtil/Simplifier>
#include <osgUtil/Optimizer>

namespace MoldWing {

bool OSGBExporter::exportOSGB(
    const QString& outputDir,
    const MeshData& meshData,
    const OSGBExportOptions& options,
    const std::unordered_map<int, std::shared_ptr<TextureEditBuffer>>& editBuffers)
{
    QString tileName = options.tileName;
    if (tileName.isEmpty())
    {
        if (!meshData.sourcePath.empty())
        {
            QFileInfo fileInfo(QString::fromStdString(meshData.sourcePath));
            tileName = fileInfo.baseName();
        }
        else
        {
            tileName = "Tile_+000_+000";
        }
    }

    QString dataDir = QDir(outputDir).filePath("Data");
    QString tileDir = QDir(dataDir).filePath(tileName);

    if (!createTileDirectory(outputDir, tileName))
    {
        return false;
    }

    if (!exportTextures(tileDir, meshData, editBuffers))
    {
        MW_LOG_WARN("Failed to export some textures, continuing...");
    }

    osg::ref_ptr<osg::Node> rootNode = createTexturedGeode(meshData, tileDir, editBuffers);
    if (!rootNode.valid())
    {
        m_lastError = "Failed to create OSG geometry from mesh data";
        MW_LOG_ERROR("{}", m_lastError.toStdString());
        return false;
    }

    osgUtil::Optimizer optimizer;
    optimizer.optimize(rootNode.get());

    osg::ref_ptr<osg::Node> finalNode = rootNode;
    if (options.generateLOD && options.lodLevels > 1)
    {
        finalNode = generateLODNode(rootNode, options, tileDir, tileName);
        if (!finalNode.valid())
        {
            MW_LOG_WARN("LOD generation failed, using original mesh");
            finalNode = rootNode;
        }
    }

    QString osgbPath = QDir(tileDir).filePath(tileName + ".osgb");
    if (!osgDB::writeNodeFile(*finalNode, osgbPath.toStdString()))
    {
        m_lastError = QString("Failed to write OSGB file: %1").arg(osgbPath);
        MW_LOG_ERROR("{}", m_lastError.toStdString());
        return false;
    }

    if (!writeMetadata(outputDir, options))
    {
        MW_LOG_WARN("Failed to write metadata.xml");
    }

    MW_LOG_INFO("OSGB exported successfully: {}", osgbPath.toStdString());
    return true;
}

bool OSGBExporter::exportMultipleOSGB(
    const QString& outputDir,
    const std::vector<std::pair<const MeshData*, std::unordered_map<int, std::shared_ptr<TextureEditBuffer>>>>& meshes,
    const OSGBExportOptions& options)
{
    bool allSuccess = true;

    for (size_t i = 0; i < meshes.size(); ++i)
    {
        OSGBExportOptions tileOptions = options;

        const MeshData* mesh = meshes[i].first;
        if (!mesh->sourcePath.empty())
        {
            QFileInfo fileInfo(QString::fromStdString(mesh->sourcePath));
            tileOptions.tileName = fileInfo.baseName();
        }
        else
        {
            tileOptions.tileName = generateTileName(static_cast<int>(i));
        }

        if (!exportOSGB(outputDir, *mesh, tileOptions, meshes[i].second))
        {
            MW_LOG_ERROR("Failed to export tile: {}", tileOptions.tileName.toStdString());
            allSuccess = false;
        }
    }

    return allSuccess;
}

osg::ref_ptr<osg::Geometry> OSGBExporter::convertToOSGGeometry(const MeshData& meshData)
{
    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array(meshData.vertices.size());
    for (size_t i = 0; i < meshData.vertices.size(); ++i)
    {
        const auto& v = meshData.vertices[i];
        (*vertices)[i].set(v.position[0], v.position[1], v.position[2]);
    }
    geometry->setVertexArray(vertices.get());

    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array(meshData.vertices.size());
    for (size_t i = 0; i < meshData.vertices.size(); ++i)
    {
        const auto& v = meshData.vertices[i];
        (*normals)[i].set(v.normal[0], v.normal[1], v.normal[2]);
    }
    geometry->setNormalArray(normals.get(), osg::Array::BIND_PER_VERTEX);

    osg::ref_ptr<osg::Vec2Array> texCoords = new osg::Vec2Array(meshData.vertices.size());
    for (size_t i = 0; i < meshData.vertices.size(); ++i)
    {
        const auto& v = meshData.vertices[i];
        (*texCoords)[i].set(v.texcoord[0], v.texcoord[1]);
    }
    geometry->setTexCoordArray(0, texCoords.get());

    osg::ref_ptr<osg::DrawElementsUInt> indices =
        new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
    indices->reserve(meshData.indices.size());
    for (uint32_t idx : meshData.indices)
    {
        indices->push_back(idx);
    }
    geometry->addPrimitiveSet(indices.get());

    return geometry;
}

osg::ref_ptr<osg::Node> OSGBExporter::createTexturedGeode(
    const MeshData& meshData,
    const QString& tileDir,
    const std::unordered_map<int, std::shared_ptr<TextureEditBuffer>>& editBuffers)
{
    osg::ref_ptr<osg::Geometry> geometry = convertToOSGGeometry(meshData);
    if (!geometry.valid())
    {
        return nullptr;
    }

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geometry.get());

    if (!meshData.textures.empty())
    {
        osg::ref_ptr<osg::StateSet> stateSet = geode->getOrCreateStateSet();

        QString texturePath;
        if (!meshData.textures.empty() && meshData.textures[0])
        {
            QString texFileName = QFileInfo(meshData.textures[0]->filePath).fileName();
            texturePath = QDir(tileDir).filePath(texFileName);
        }

        if (!texturePath.isEmpty() && QFile::exists(texturePath))
        {
            osg::ref_ptr<osg::Image> image = osgDB::readImageFile(texturePath.toStdString());
            if (image.valid())
            {
                osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D();
                texture->setImage(image.get());
                texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
                texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
                texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
                texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);

                stateSet->setTextureAttributeAndModes(0, texture.get(), osg::StateAttribute::ON);
            }
        }

        osg::ref_ptr<osg::Material> material = new osg::Material();
        material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
        material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.2f, 0.2f, 0.2f, 1.0f));
        stateSet->setAttributeAndModes(material.get(), osg::StateAttribute::ON);
    }

    return geode;
}

osg::ref_ptr<osg::Node> OSGBExporter::generateLODNode(
    osg::ref_ptr<osg::Node> fullDetailNode,
    const OSGBExportOptions& options,
    const QString& tileDir,
    const QString& tileName)
{
    osg::ref_ptr<osg::LOD> lodNode = new osg::LOD();
    lodNode->setRangeMode(osg::LOD::DISTANCE_FROM_EYE_POINT);

    lodNode->addChild(fullDetailNode.get(), 0.0f, 500.0f);

    float ratios[] = {options.lodRatio1, options.lodRatio2, options.lodRatio3};
    float ranges[] = {500.0f, 1000.0f, 2000.0f, 5000.0f};

    for (int level = 1; level < options.lodLevels && level < 4; ++level)
    {
        osg::ref_ptr<osg::Node> simplified =
            dynamic_cast<osg::Node*>(fullDetailNode->clone(osg::CopyOp::DEEP_COPY_ALL));

        if (simplified.valid())
        {
            osgUtil::Simplifier simplifier;
            simplifier.setSampleRatio(ratios[level - 1]);
            simplified->accept(simplifier);

            lodNode->addChild(simplified.get(), ranges[level - 1], ranges[level]);

            QString lodFileName = QString("%1_L%2_%3.osgb")
                                      .arg(tileName)
                                      .arg(16 + level)
                                      .arg(0);
            QString lodPath = QDir(tileDir).filePath(lodFileName);
            osgDB::writeNodeFile(*simplified, lodPath.toStdString());

            MW_LOG_DEBUG("Generated LOD level {}: {}", level, lodFileName.toStdString());
        }
    }

    return lodNode;
}

bool OSGBExporter::createTileDirectory(const QString& baseDir, const QString& tileName)
{
    QDir dir(baseDir);

    if (!dir.exists())
    {
        if (!dir.mkpath("."))
        {
            m_lastError = QString("Failed to create output directory: %1").arg(baseDir);
            MW_LOG_ERROR("{}", m_lastError.toStdString());
            return false;
        }
    }

    QString dataDir = dir.filePath("Data");
    if (!QDir(dataDir).exists())
    {
        if (!QDir().mkpath(dataDir))
        {
            m_lastError = QString("Failed to create Data directory: %1").arg(dataDir);
            MW_LOG_ERROR("{}", m_lastError.toStdString());
            return false;
        }
    }

    QString tileDir = QDir(dataDir).filePath(tileName);
    if (!QDir(tileDir).exists())
    {
        if (!QDir().mkpath(tileDir))
        {
            m_lastError = QString("Failed to create tile directory: %1").arg(tileDir);
            MW_LOG_ERROR("{}", m_lastError.toStdString());
            return false;
        }
    }

    return true;
}

bool OSGBExporter::writeMetadata(const QString& outputDir, const OSGBExportOptions& options)
{
    QString metadataPath = QDir(outputDir).filePath("metadata.xml");
    QFile file(metadataPath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        m_lastError = QString("Failed to open metadata file for writing: %1").arg(metadataPath);
        MW_LOG_ERROR("{}", m_lastError.toStdString());
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    out << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    out << "<ModelMetadata version=\"1\">\n";

    if (options.targetEpsg > 0)
    {
        out << "  <SRS>EPSG:" << options.targetEpsg << "</SRS>\n";
    }
    else
    {
        out << "  <SRS>LOCAL_CS[\"Local Coordinates\"]</SRS>\n";
    }

    out << QString("  <SRSOrigin>%1,%2,%3</SRSOrigin>\n")
               .arg(options.srsOriginX, 0, 'f', 6)
               .arg(options.srsOriginY, 0, 'f', 6)
               .arg(options.srsOriginZ, 0, 'f', 6);

    out << "  <Texture>\n";
    out << "    <ColorSource>Visible</ColorSource>\n";
    out << "  </Texture>\n";

    out << "</ModelMetadata>\n";

    file.close();

    MW_LOG_INFO("Metadata written to: {}", metadataPath.toStdString());
    return true;
}

bool OSGBExporter::exportTextures(
    const QString& tileDir,
    const MeshData& meshData,
    const std::unordered_map<int, std::shared_ptr<TextureEditBuffer>>& editBuffers)
{
    bool allSuccess = true;

    for (size_t i = 0; i < meshData.textures.size(); ++i)
    {
        const auto& texture = meshData.textures[i];
        if (!texture)
            continue;

        QString srcPath = texture->filePath;
        QString fileName = QFileInfo(srcPath).fileName();
        QString dstPath = QDir(tileDir).filePath(fileName);

        auto editIt = editBuffers.find(static_cast<int>(i));
        if (editIt != editBuffers.end() && editIt->second && editIt->second->isValid())
        {
            if (!editIt->second->save(dstPath))
            {
                MW_LOG_WARN("Failed to save edited texture: {}", dstPath.toStdString());
                allSuccess = false;
            }
            else
            {
                MW_LOG_DEBUG("Exported edited texture: {}", dstPath.toStdString());
            }
        }
        else if (QFile::exists(srcPath))
        {
            if (QFile::exists(dstPath))
                QFile::remove(dstPath);

            if (!QFile::copy(srcPath, dstPath))
            {
                MW_LOG_WARN("Failed to copy texture: {} -> {}", srcPath.toStdString(), dstPath.toStdString());
                allSuccess = false;
            }
            else
            {
                MW_LOG_DEBUG("Copied texture: {}", dstPath.toStdString());
            }
        }
        else if (!texture->image.isNull())
        {
            if (!texture->image.save(dstPath))
            {
                MW_LOG_WARN("Failed to save texture from memory: {}", dstPath.toStdString());
                allSuccess = false;
            }
            else
            {
                MW_LOG_DEBUG("Saved texture from memory: {}", dstPath.toStdString());
            }
        }
    }

    return allSuccess;
}

QString OSGBExporter::generateTileName(int index) const
{
    int row = index / 100;
    int col = index % 100;
    return QString("Tile_%1%2_%3%4")
        .arg(row >= 0 ? "+" : "-")
        .arg(qAbs(row), 3, 10, QChar('0'))
        .arg(col >= 0 ? "+" : "-")
        .arg(qAbs(col), 3, 10, QChar('0'));
}

}
