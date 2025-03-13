/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Log/Log.h"
#include "Entities/Unit.h"
#include "MotionGenerators/MoveMap.h"
#include "MoveMapSharedDefines.h"

namespace MMAP
{
    constexpr char MAP_FILE_NAME_FORMAT[] = "mmaps/%03i.mmap";
    constexpr char TILE_FILE_NAME_FORMAT[] = "mmaps/%03i%02i%02i.mmtile";
    constexpr char TILE_ALT_FILE_NAME_FORMAT[] = "mmaps/%03i%02i%02i_%02i.mmtile";
    constexpr char GO_FILE_NAME_FORMAT[] = "mmaps/go%04i.mmtile";

    // ######################## MMapFactory ########################
    // our global singleton copy
    MMapManager* g_MMapManager = nullptr;

    // stores list of mapids which do not use pathfinding
    std::set<uint32>* g_mmapDisabledIds = nullptr;

    MMapManager* MMapFactory::createOrGetMMapManager()
    {
        if (g_MMapManager == nullptr)
            g_MMapManager = new MMapManager();

        return g_MMapManager;
    }

    void MMapFactory::preventPathfindingOnMaps(const char* ignoreMapIds)
    {
        if (!g_mmapDisabledIds)
            g_mmapDisabledIds = new std::set<uint32>();

        uint32 strLenght = strlen(ignoreMapIds) + 1;
        char* mapList = new char[strLenght];
        memcpy(mapList, ignoreMapIds, sizeof(char)*strLenght);

        char* idstr = strtok(mapList, ",");
        while (idstr)
        {
            g_mmapDisabledIds->insert(uint32(atoi(idstr)));
            idstr = strtok(nullptr, ",");
        }

        delete[] mapList;
    }

    bool MMapFactory::IsPathfindingEnabled(uint32 mapId, const Unit* unit = nullptr)
    {
        if (!createOrGetMMapManager()->IsEnabled())
            return false;

        if (unit)
        {
            Unit::MmapForcingStatus status = unit->IsIgnoringMMAP();
            switch (status)
            {
                case Unit::MmapForcingStatus::DEFAULT:
                    break;
                case Unit::MmapForcingStatus::FORCED:
                    return true;
                case Unit::MmapForcingStatus::IGNORED:
                    return false;
            }
        }

        return g_mmapDisabledIds->find(mapId) == g_mmapDisabledIds->end();
    }

    void MMapFactory::clear()
    {
        delete g_mmapDisabledIds;
        delete g_MMapManager;

        g_mmapDisabledIds = nullptr;
        g_MMapManager = nullptr;
    }

    // ######################## MMapManager ########################
    MMapManager::~MMapManager()
    {
        // by now we should not have maps loaded
        // if we had, tiles in MMapData->mmapLoadedTiles, their actual data is lost!
    }

    void MMapManager::ChangeTile(std::string const& basePath, uint32 mapId, uint32 instanceId, uint32 tileX, uint32 tileY, uint32 tileNumber)
    {
        unloadMap(mapId, instanceId, tileX, tileY);
        loadMap(basePath, mapId, instanceId, tileX, tileY, tileNumber);
    }

    bool MMapManager::loadMapData(std::string const& basePath, uint32 mapId, uint32 instanceId)
    {
        // we already have this map loaded?
        if (m_loadedMMaps.find(packInstanceId(mapId, instanceId)) != m_loadedMMaps.end())
            return true;

        // load and init dtNavMesh - read parameters from file
        uint32 pathLen = basePath.length() + strlen(MAP_FILE_NAME_FORMAT) + 1;
        char* fileName = new char[pathLen];
        snprintf(fileName, pathLen, (basePath + MAP_FILE_NAME_FORMAT).c_str(), mapId);

        FILE* file = fopen(fileName, "rb");
        if (!file)
        {
            if (MMapFactory::IsPathfindingEnabled(mapId))
                sLog.outError("MMAP:loadMapData: Error: Could not open mmap file '%s'", fileName);
            delete[] fileName;
            return false;
        }

        dtNavMeshParams params;
        fread(&params, sizeof(dtNavMeshParams), 1, file);
        fclose(file);

        dtNavMesh* mesh = dtAllocNavMesh();
        MANGOS_ASSERT(mesh);
        dtStatus dtResult = mesh->init(&params);
        if (dtStatusFailed(dtResult))
        {
            dtFreeNavMesh(mesh);
            sLog.outError("MMAP:loadMapData: Failed to initialize dtNavMesh for mmap %03u from file %s", mapId, fileName);
            delete[] fileName;
            return false;
        }

        delete[] fileName;

        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:loadMapData: Loaded %03i.mmap", mapId);

        // store inside our map list
        m_loadedMMaps.emplace(packInstanceId(mapId, instanceId), std::make_unique<MMapData>(mesh));
        return true;
    }

    uint32 MMapManager::packTileID(int32 x, int32 y) const
    {
        return uint32(x << 16 | y);
    }

    uint64 MMapManager::packInstanceId(uint32 mapId, uint32 instanceId) const
    {
        return (uint64(mapId) << 32) | instanceId;
    }

    bool MMapManager::IsMMapTileLoaded(uint32 mapId, uint32 instanceId, uint32 x, uint32 y) const
    {
        // get this mmap data
        auto itr = m_loadedMMaps.find(packInstanceId(mapId, instanceId));

        if (itr == m_loadedMMaps.end())
            return false;

        const auto& mmapData = itr->second;

        uint32 packedGridPos = packTileID(x, y);
        if (mmapData->mmapLoadedTiles.find(packedGridPos) != mmapData->mmapLoadedTiles.end())
            return true;

        return false;
    }

    void MMapManager::loadAllMapTiles(std::string const& basePath, uint32 mapId, uint32 instanceId)
    {
        auto itr = m_loadedMMaps.find(packInstanceId(mapId, instanceId));
        MANGOS_ASSERT(itr != m_loadedMMaps.end());
        const auto& mmapData = itr->second;

        if (mmapData->fullLoaded)
            return;

        for (const auto& entry : boost::filesystem::directory_iterator(basePath + "mmaps"))
        {
            if (entry.path().extension() == ".mmtile")
            {
                auto filename = entry.path().filename();
                auto fileNameString = filename.c_str();
                // trying to avoid string copy
                uint32 fileMapId = (fileNameString[0] - '0') * 100 + (fileNameString[1] - '0') * 10 + (fileNameString[2] - '0');
                if (fileMapId != mapId)
                    continue;

                uint32 x = (fileNameString[3] - '0') * 10 + (fileNameString[4] - '0');
                uint32 y = (fileNameString[5] - '0') * 10 + (fileNameString[6] - '0');
                uint32 packedGridPos = packTileID(x, y);
                loadMapInternal(entry.path().string().c_str(), mmapData, packedGridPos, mapId, x, y); // yes using a temporary - wchar_t on windows
            }
        }

        mmapData->fullLoaded = true;
    }

    bool MMapManager::loadMap(std::string const& basePath, uint32 mapId, uint32 instanceId, int32 x, int32 y, uint32 number)
    {
        // get this mmap data
        auto itr = m_loadedMMaps.find(packInstanceId(mapId, instanceId));
        MANGOS_ASSERT(itr != m_loadedMMaps.end()); // must not occur here as it would not be thread safe - only in loadMapData through loadMapInstance

        const auto& mmapData = itr->second;

        // check if we already have this tile loaded
        uint32 packedGridPos = packTileID(x, y);
        if (mmapData->mmapLoadedTiles.find(packedGridPos) != mmapData->mmapLoadedTiles.end())
        {
            sLog.outError("MMAP:loadMap: Asked to load already loaded navmesh tile. ");
            return false;
        }

        // load this tile :: mmaps/MMMXXYY.mmtile
        uint32 pathLen = basePath.length() + strlen(number == 0 ? TILE_FILE_NAME_FORMAT : TILE_ALT_FILE_NAME_FORMAT) + 1;
        std::unique_ptr<char[]> fileName(new char[pathLen]);
        snprintf(fileName.get(), pathLen, (basePath + (number == 0 ? TILE_FILE_NAME_FORMAT : TILE_ALT_FILE_NAME_FORMAT)).c_str(), mapId, x, y);

        return loadMapInternal(fileName.get(), mmapData, packedGridPos, mapId, x, y);
    }

    bool MMapManager::loadMapInternal(const char* filePath, const std::unique_ptr<MMapData>& mmapData, uint32 packedGridPos, uint32 mapId, int32 x, int32 y)
    {
        FILE* file = fopen(filePath, "rb");
        if (!file)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "ERROR: MMAP:loadMap: Could not open mmtile file '%s'", filePath);
            return false;
        }

        // read header
        MmapTileHeader fileHeader;
        fread(&fileHeader, sizeof(MmapTileHeader), 1, file);

        if (fileHeader.mmapMagic != MMAP_MAGIC)
        {
            sLog.outError("MMAP:loadMap: Bad header in mmap %s", filePath);
            fclose(file);
            return false;
        }

        if (fileHeader.mmapVersion != MMAP_VERSION)
        {
            sLog.outError("MMAP:loadMap: %s was built with generator v%i, expected v%i",
                          filePath, fileHeader.mmapVersion, MMAP_VERSION);
            fclose(file);
            return false;
        }

        unsigned char* data = (unsigned char*)dtAlloc(fileHeader.size, DT_ALLOC_PERM);
        MANGOS_ASSERT(data);

        size_t result = fread(data, fileHeader.size, 1, file);
        if (!result)
        {
            sLog.outError("MMAP:loadMap: Bad header or data in mmap %s", filePath);
            fclose(file);
            return false;
        }

        fclose(file);

        dtMeshHeader* header = (dtMeshHeader*)data;
        dtTileRef tileRef = 0;

        // memory allocated for data is now managed by detour, and will be deallocated when the tile is removed
        dtStatus dtResult = mmapData->navMesh->addTile(data, fileHeader.size, DT_TILE_FREE_DATA, 0, &tileRef);
        if (dtStatusFailed(dtResult))
        {
            sLog.outError("MMAP:loadMap: Could not load %s into navmesh", filePath);
            dtFree(data);
            return false;
        }

        mmapData->mmapLoadedTiles.insert(std::pair<uint32, dtTileRef>(packedGridPos, tileRef));
        ++m_loadedTiles;
        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:loadMap:%s: Loaded into %03i[%02i,%02i]", filePath, mapId, header->x, header->y);
        return true;
    }

    void MMapManager::loadAllGameObjectModels(std::string const& basePath, std::vector<uint32> const& displayIds)
    {
        if (!IsEnabled())
            return;

        for (uint32 displayId : displayIds)
            loadGameObject(basePath, displayId);
    }

    bool MMapManager::loadGameObject(std::string const& basePath, uint32 displayId)
    {
        // we already have this map loaded?
        if (m_loadedModels.find(displayId) != m_loadedModels.end())
            return true;

        // load and init dtNavMesh - read parameters from file
        uint32 pathLen = basePath.length() + strlen(GO_FILE_NAME_FORMAT) + 1;
        char* fileName = new char[pathLen];
        snprintf(fileName, pathLen, (basePath + GO_FILE_NAME_FORMAT).c_str(), displayId);

        FILE* file = fopen(fileName, "rb");
        if (!file)
        {
            DEBUG_LOG("MMAP:loadGameObject: Error: Could not open mmap file %s", fileName);
            delete[] fileName;
            return false;
        }

        MmapTileHeader fileHeader;
        fread(&fileHeader, sizeof(MmapTileHeader), 1, file);

        if (fileHeader.mmapMagic != MMAP_MAGIC)
        {
            sLog.outError("MMAP:loadGameObject: Bad header in mmap %s", fileName);
            fclose(file);
            return false;
        }

        if (fileHeader.mmapVersion != MMAP_VERSION)
        {
            sLog.outError("MMAP:loadGameObject: %s was built with generator v%i, expected v%i",
                fileName, fileHeader.mmapVersion, MMAP_VERSION);
            fclose(file);
            return false;
        }
        unsigned char* data = (unsigned char*)dtAlloc(fileHeader.size, DT_ALLOC_PERM);
        MANGOS_ASSERT(data);

        size_t result = fread(data, fileHeader.size, 1, file);
        if (!result)
        {
            sLog.outError("MMAP:loadGameObject: Bad header or data in mmap %s", fileName);
            fclose(file);
            return false;
        }

        fclose(file);

        dtNavMesh* mesh = dtAllocNavMesh();
        MANGOS_ASSERT(mesh);
        dtStatus r = mesh->init(data, fileHeader.size, DT_TILE_FREE_DATA);
        if (dtStatusFailed(r))
        {
            dtFreeNavMesh(mesh);
            sLog.outError("MMAP:loadGameObject: Failed to initialize dtNavMesh from file %s. Result 0x%x.", fileName, r);
            delete[] fileName;
            return false;
        }
        DETAIL_LOG("MMAP:loadGameObject: Loaded file %s [size=%u]", fileName, fileHeader.size);
        delete[] fileName;

        m_loadedModels.emplace(displayId, std::make_unique<MMapGOData>(mesh));
        return true;
    }

    bool MMapManager::loadMapInstance(std::string const& basePath, uint32 mapId, uint32 instanceId)
    {
        if (!loadMapData(basePath, mapId, instanceId))
            return false;

        auto& mmap = m_loadedMMaps[packInstanceId(mapId, instanceId)];

        // allocate mesh query
        dtNavMeshQuery* query = dtAllocNavMeshQuery();
        MANGOS_ASSERT(query);
        if (dtStatusFailed(query->init(mmap->navMesh, 1024)))
        {
            dtFreeNavMeshQuery(query);
            ERROR_DB_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:GetNavMeshQuery: Failed to initialize dtNavMeshQuery for mapId %03u instanceId %u", mapId, instanceId);
            return false;
        }

        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:GetNavMeshQuery: created dtNavMeshQuery for mapId %03u instanceId %u", mapId, instanceId);
        mmap->navMeshQuery = query;
        return true;
    }

    bool MMapManager::unloadMap(uint32 mapId, uint32 instanceId, int32 x, int32 y)
    {
        // check if we have this map loaded
        auto itr = m_loadedMMaps.find(packInstanceId(mapId, instanceId));
        if (itr == m_loadedMMaps.end())
        {
            // file may not exist, therefore not loaded
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Asked to unload not loaded navmesh map. %03u%02i%02i.mmtile", mapId, x, y);
            return false;
        }

        const auto& mmapData = (*itr).second;

        // check if we have this tile loaded
        uint32 packedGridPos = packTileID(x, y);
        if (mmapData->mmapLoadedTiles.find(packedGridPos) == mmapData->mmapLoadedTiles.end())
        {
            // file may not exist, therefore not loaded
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Asked to unload not loaded navmesh tile. %03u%02i%02i.mmtile", mapId, x, y);
            return false;
        }

        dtTileRef tileRef = mmapData->mmapLoadedTiles[packedGridPos];

        // unload, and mark as non loaded
        dtStatus dtResult = mmapData->navMesh->removeTile(tileRef, nullptr, nullptr);
        if (dtStatusFailed(dtResult))
        {
            // this is technically a memory leak
            // if the grid is later reloaded, dtNavMesh::addTile will return error but no extra memory is used
            // we cannot recover from this error - assert out
            sLog.outError("MMAP:unloadMap: Could not unload %03u%02i%02i.mmtile from navmesh", mapId, x, y);
            MANGOS_ASSERT(false);
        }
        else
        {
            mmapData->mmapLoadedTiles.erase(packedGridPos);
            --m_loadedTiles;
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Unloaded mmtile %03i[%02i,%02i] from %03i", mapId, x, y, mapId);
            return true;
        }

        return false;
    }

    bool MMapManager::unloadMap(uint32 mapId)
    {
        bool success = false;
        // unload all maps with given mapId
        for (auto itr = m_loadedMMaps.begin(); itr != m_loadedMMaps.end();)
        {
            if (itr->first != uint64(mapId) << 32)
            {
                ++itr;
                continue;
            }

            // unload all tiles from given map
            const auto& mmapData = (*itr).second;
            for (MMapTileSet::iterator i = mmapData->mmapLoadedTiles.begin(); i != mmapData->mmapLoadedTiles.end(); ++i)
            {
                uint32 x = (i->first >> 16);
                uint32 y = (i->first & 0x0000FFFF);
                dtStatus dtResult = mmapData->navMesh->removeTile(i->second, nullptr, nullptr);
                if (dtStatusFailed(dtResult))
                    sLog.outError("MMAP:unloadMap: Could not unload %03u%02i%02i.mmtile from navmesh", mapId, x, y);
                else
                {
                    --m_loadedTiles;
                    DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Unloaded mmtile %03i[%02i,%02i] from %03i", mapId, x, y, mapId);
                }
            }

            itr = m_loadedMMaps.erase(itr);
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Unloaded %03i.mmap", mapId);
            success = true;
        }

        if (!success)
            // file may not exist, therefore not loaded
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Asked to unload not loaded navmesh map %03u", mapId);

        return success;
    }

    bool MMapManager::unloadMapInstance(uint32 mapId, uint32 instanceId)
    {
        // check if we have this map loaded
        auto itr = m_loadedMMaps.find(packInstanceId(mapId, instanceId));
        if (itr == m_loadedMMaps.end())
        {
            // file may not exist, therefore not loaded
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMapInstance: Asked to unload not loaded navmesh map %03u", mapId);
            return false;
        }

        const auto& mmapData = (*itr).second;

        dtNavMeshQuery* query = mmapData->navMeshQuery;

        dtFreeNavMeshQuery(query);
        mmapData->navMeshQuery = nullptr;
        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMapInstance: Unloaded mapId %03u instanceId %u", mapId, instanceId);

        return true;
    }

    dtNavMesh const* MMapManager::GetNavMesh(uint32 mapId, uint32 instanceId)
    {
        auto itr = m_loadedMMaps.find(packInstanceId(mapId, instanceId));
        if (itr == m_loadedMMaps.end())
            return nullptr;

        return (*itr).second->navMesh;
    }

    dtNavMesh const* MMapManager::GetGONavMesh(uint32 mapId)
    {
        if (m_loadedModels.find(mapId) == m_loadedModels.end())
            return nullptr;

        return m_loadedModels[mapId]->navMesh;
    }

    dtNavMeshQuery const* MMapManager::GetNavMeshQuery(uint32 mapId, uint32 instanceId)
    {
        auto itr = m_loadedMMaps.find(packInstanceId(mapId, instanceId));
        if (itr == m_loadedMMaps.end())
            return nullptr;

        return itr->second->navMeshQuery;
    }

    dtNavMeshQuery const* MMapManager::GetModelNavMeshQuery(uint32 displayId)
    {
        if (m_loadedModels.find(displayId) == m_loadedModels.end())
            return nullptr;

        auto threadId = std::this_thread::get_id();
        const auto& mmapGOData = m_loadedModels[displayId];
        if (mmapGOData->navMeshGOQueries.find(threadId) == mmapGOData->navMeshGOQueries.end())
        {
            std::lock_guard<std::mutex> guard(m_modelsMutex);
            if (mmapGOData->navMeshGOQueries.find(threadId) == mmapGOData->navMeshGOQueries.end())
            {
                // allocate mesh query
                std::stringstream ss;
                ss << threadId;
                dtNavMeshQuery* query = dtAllocNavMeshQuery();
                MANGOS_ASSERT(query);
                if (dtStatusFailed(query->init(mmapGOData->navMesh, 2048)))
                {
                    dtFreeNavMeshQuery(query);
                    sLog.outError("MMAP:GetModelNavMeshQuery: Failed to initialize dtNavMeshQuery for displayid %03u tid %s", displayId, ss.str().data());
                    return nullptr;
                }

                DETAIL_LOG("MMAP:GetModelNavMeshQuery: created dtNavMeshQuery for displayid %03u tid %s", displayId, ss.str().data());
                mmapGOData->navMeshGOQueries.insert(std::pair<std::thread::id, dtNavMeshQuery*>(threadId, query));
            }
        }

        return mmapGOData->navMeshGOQueries[threadId];
    }
}
