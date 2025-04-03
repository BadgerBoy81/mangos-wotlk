/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
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

 /* ScriptData
 SDName: The_Maker
 SD%Complete: 70
 SDComment: pre-event not made
 SDCategory: Hellfire Citadel, Blood Furnace
 EndScriptData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "blood_furnace.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

enum TheMakerAction
{
	MAKER_ACTION_MAX,
};

struct boss_the_makerAI : public CombatAI
{
    boss_the_makerAI(Creature* creature) : CombatAI(creature, MAKER_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())),
        m_isRegularMode(creature->GetMap()->IsRegularDifficulty()) {}

    ScriptedInstance* m_instance;
    bool m_isRegularMode;

    void Aggro(Unit* /*who*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_THE_MAKER_EVENT, IN_PROGRESS);
    }

    void EnterEvadeMode() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_THE_MAKER_EVENT, FAIL);
    }

    void JustDied(Unit* /*who*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_THE_MAKER_EVENT, DONE);
    }
};

void AddSC_boss_the_maker()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "the_maker";
    pNewScript->GetAI = &GetNewAIInstance<boss_the_makerAI>;
    pNewScript->RegisterSelf();
}
