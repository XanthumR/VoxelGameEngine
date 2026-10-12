#include "Simulation/Simulation.h"

#include "Simulation/BuildingLook.h"
#include "Simulation/SaveGame.h"

Simulation::Simulation(TerrainGenerator& terrain) : m_Islands(terrain), m_Trees(terrain), m_Ships(terrain) {
    m_Ships.SetTrade(&m_Traders, &m_Treasury);
}

void Simulation::LiftBuilding(GameObjectId id) {
    if (!m_Objects.IsAlive(id)) return;
    const VoxelAnchorComponent& anchor = m_Objects.Anchor(id);
    m_Occupancy.Release(glm::ivec2(ColumnToTile(anchor.origin.x), ColumnToTile(anchor.origin.z)), anchor.footprint / TILE_SIZE, id);
    MarkBuildingsChanged();
}

void Simulation::PlaceLiftedBuilding(GameObjectId id, glm::ivec2 minTile, uint8_t rotation) {
    if (!m_Objects.IsAlive(id)) return;
    BuildingComponent& building = m_Objects.Building(id);
    glm::ivec2 tiles = FootprintTiles(BUILDING_TYPES[building.type], rotation);
    building.rotation = rotation;
    VoxelAnchorComponent& anchor = m_Objects.Anchor(id);
    anchor.origin = glm::ivec3(minTile.x * TILE_SIZE, BUILD_GROUND_Y, minTile.y * TILE_SIZE);
    anchor.footprint = tiles * TILE_SIZE;
    m_Occupancy.Occupy(minTile, tiles, id);
    if (BUILDING_TYPES[building.type].role == BuildingRole::Producer) ProductionSystem::RecallCart(m_Objects.Production(id));
    MarkBuildingsChanged();
}

void Simulation::FixedUpdate(float tickSeconds) {
    // Warehouse reach and building connections, when roads or buildings changed
    m_Logistics.Update(m_Objects, m_Roads, m_BuildingsRevision);
    // Residents: consumption, growth, upgrades
    m_Population.Update(m_Objects, m_Economy, m_TickCount);
    // Felled trees grow back
    m_Trees.Update(m_TickCount, m_Occupancy, m_Roads);
    // Producers: workforce, location, cycles. Their location factors follow buildings, roads and trees.
    uint32_t worldRevision = m_BuildingsRevision * 2654435761u + m_Roads.Revision() * 40503u + m_Trees.Revision();
    m_Production.Update(m_Objects, m_Economy, m_Islands, m_Occupancy, m_Roads, m_Trees, worldRevision, m_TickCount);
    // Taxes in, upkeep out
    m_Treasury.Update(m_Objects);
    // Traders fill their stock again; ships sail on
    m_Traders.Update(m_TickCount);
    m_Ships.Update(m_Objects, m_Occupancy, m_Economy);
    // The stock trends of the islands' goods
    m_Economy.RecordTrends(m_TickCount);
    // What the player is told about
    m_Notifications.Update(m_Objects, m_Economy, m_Ships, m_Treasury, m_TickCount);

    m_TickCount++;
    m_SimulationSeconds += tickSeconds;
}

void Simulation::Save(SaveWriter& out) const {
    out.Write(m_TickCount);
    out.Write(m_SimulationSeconds);
    m_Islands.Save(out);
    m_Objects.Save(out);
    m_Roads.Save(out);
    m_Economy.Save(out);
    m_Population.Save(out);
    m_Trees.Save(out);
    m_Treasury.Save(out);
    m_Traders.Save(out);
    m_Ships.Save(out);
    m_Notifications.Save(out);
}

bool Simulation::Load(SaveReader& in) {
    in.Read(m_TickCount);
    in.Read(m_SimulationSeconds);
    m_Islands.Load(in);
    m_Objects.Load(in);
    m_Roads.Load(in);
    m_Economy.Load(in);
    m_Population.Load(in);
    m_Trees.Load(in);
    m_Treasury.Load(in);
    m_Traders.Load(in);
    m_Ships.Load(in);
    m_Notifications.Load(in, m_Ships);

    // Derived: who stands where, and (on the next tick) the roads' reach and the connections
    m_Occupancy = OccupancyGrid();
    for (uint32_t slot = 0; slot < m_Objects.SlotCount(); slot++) {
        GameObjectId id = m_Objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        const VoxelAnchorComponent& anchor = m_Objects.Anchor(id);
        m_Occupancy.Occupy(glm::ivec2(ColumnToTile(anchor.origin.x), ColumnToTile(anchor.origin.z)), anchor.footprint / TILE_SIZE, id);
    }
    MarkBuildingsChanged();
    return in.Ok();
}
