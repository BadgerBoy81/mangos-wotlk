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

#include "Camera.h"
#include "Grids/GridNotifiersImpl.h"
#include "Grids/CellImpl.h"
#include "Log/Log.h"
#include "Util/Errors.h"
#include "Entities/Player.h"

Camera::Camera(Player* pl) : m_owner(*pl), m_source(pl), m_sendInProgress(false)
{
    m_source->GetViewPoint().Attach(this);
}

Camera::~Camera()
{
    // view of camera should be already reseted to owner (RemoveFromWorld -> Event_RemovedFromWorld -> ResetView)
    MANGOS_ASSERT(m_source == &m_owner);

    // for symmetry with constructor and way to make viewpoint's list empty
    m_source->GetViewPoint().Detach(this);
}

void Camera::ReceivePacket(WorldPacket const& data) const
{
    m_owner.SendDirectMessage(data);
}

void Camera::UpdateForCurrentViewPoint()
{
    m_gridRef.unlink();

    if (GridType* grid = m_source->GetViewPoint().m_grid)
        grid->AddWorldObject(this);

    m_source->GetMap()->AddUpdateCreateObject(m_source);
}

void Camera::SetView(WorldObject* obj, bool update_far_sight_field /*= true*/)
{
    MANGOS_ASSERT(obj);

    if (m_source == obj)
        return;

    if (!m_owner.IsInMap(obj))
    {
        sLog.outError("Camera::SetView, viewpoint is not in map with camera's owner");
        return;
    }

    if (!obj->isType(TypeMask(TYPEMASK_DYNAMICOBJECT | TYPEMASK_UNIT)))
    {
        sLog.outError("Camera::SetView, viewpoint type is not available for client");
        return;
    }

    // detach and deregister from active objects if there are no more reasons to be active
    m_source->GetViewPoint().Detach(this);
    if (!m_source->isActiveObject())
        m_source->GetMap()->RemoveFromActive(m_source);

    m_source = obj;

    if (!m_source->isActiveObject())
        m_source->GetMap()->AddToActive(m_source);

    m_source->GetViewPoint().Attach(this);

    if (update_far_sight_field)
        m_owner.SetGuidValue(PLAYER_FARSIGHT, (m_source == &m_owner ? ObjectGuid() : m_source->GetObjectGuid()));

    UpdateForCurrentViewPoint();
}

void Camera::Event_ViewPointVisibilityChanged()
{
    if (!m_owner.HasAtClient(m_source))
        ResetView();
}

void Camera::ResetView(bool update_far_sight_field /*= true*/)
{
    SetView(&m_owner, update_far_sight_field);
}

void Camera::Event_AddedToWorld(UpdateData* data)
{
    GridType* grid = m_source->GetViewPoint().m_grid;
    MANGOS_ASSERT(grid);
    grid->AddWorldObject(this);

    if (data)
        UpdateVisibilityForOwner(true, *data);
}

void Camera::Event_RemovedFromWorld()
{
    if (m_source == &m_owner)
    {
        m_gridRef.unlink();
        return;
    }

    ResetView();
}

void Camera::Event_Moved()
{
    m_gridRef.unlink();
    m_source->GetViewPoint().m_grid->AddWorldObject(this);
}

void Camera::UpdateVisibilityOf(WorldObject* target, UpdateData& data) const
{
    m_owner.UpdateVisibilityOf(m_source, target, data);
}

template<class T>
void Camera::UpdateVisibilityOf(T* target, UpdateData& data, WorldObjectSet& vis)
{
    m_owner.UpdateVisibilityOf<T>(m_source, target, data, vis);
}

template void Camera::UpdateVisibilityOf(Player*, UpdateData&, WorldObjectSet&);
template void Camera::UpdateVisibilityOf(Creature*, UpdateData&, WorldObjectSet&);
template void Camera::UpdateVisibilityOf(Corpse*, UpdateData&, WorldObjectSet&);
template void Camera::UpdateVisibilityOf(GameObject*, UpdateData&, WorldObjectSet&);
template void Camera::UpdateVisibilityOf(DynamicObject*, UpdateData&, WorldObjectSet&);

void Camera::UpdateVisibilityForOwner(UpdateDataMapType& update_players)
{
    UpdateDataMapType::iterator iter = update_players.find(GetOwner());

    if (iter == update_players.end())
    {
        std::pair<UpdateDataMapType::iterator, bool> p = update_players.insert(UpdateDataMapType::value_type(GetOwner(), UpdateData()));
        MANGOS_ASSERT(p.second);
        iter = p.first;
    }
    UpdateVisibilityForOwner(true, iter->second);
}

void Camera::UpdateVisibilityForOwner(bool addToWorld, UpdateData& data)
{
    m_sendInProgress = true;
    MaNGOS::VisibleNotifier notifier(*this, data, !addToWorld);
    Cell::VisitAllObjects(m_source, notifier, addToWorld ? MAX_VISIBILITY_DISTANCE : m_source->GetVisibilityData().GetVisibilityDistance(), false);
    notifier.Notify();
    m_sendInProgress = false;
}

//////////////////

ViewPoint::~ViewPoint()
{
    if (!m_cameras.empty())
    {
        sLog.outError("ViewPoint destructor called, but some cameras referenced to it");
    }
}
