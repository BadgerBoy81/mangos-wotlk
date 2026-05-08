BOTSTATE_LOADING,           // loading state during world load
    Set when:
        - When instantiating a bot
        (Also set in HandleBotOutgoingPacket SMSG_TRANSFER_PENDING nto sure if this is actually happening at some point.)
        - Confirmed this happens when moveing from outland to azeroth (shatt -> org)
        - Confirmed when entering dungeon.
BOTSTATE_NORMAL,            // normal AI routines are processed
    Set when:
        - Logged in
        - After getting resurrected
        - After getting teleported near (HandleBotOutgoingPacket(MSG_MOVE_TELEPORT_ACK))
        - After moving world (HandleBotOutgoingPacket(SMSG_NEW_WORLD))
        - in void PlayerbotAI::DoLoot() if m_lootCurrent is empty and m_loottargets is empty
        - UpdateAI if botstate is LOADING but not being teleported
        - UpdateAI if botstate is DELAYED and m_craftSpellId = 0
        - UpdateAI if DELAYED AND (GetSpellCharges(m_CraftSpellId) == 0 || spell->CheckCast(true) != SPELL_CAST_OK)
        - UpdateAI if m_botState == BOTSTATE_FLYING
        - UpdateAI if (m_botState == BOTSTATE_DEADRELEASED) and bot revives
        - HandleTaming and tamin not possible
        - Bot gets reset command
BOTSTATE_COMBAT,            // bot is in combat
    Set wheb:
        - PlayerbotAI::Attack(Unit* forcedTarget) and botstat is not BOTSTATE_COMBAT
        - PlayerbotAI::GetDuelTarget(Unit* forcedTarget) and botstat is not BOTSTATE_COMBAT
BOTSTATE_DEAD,              // we are dead and wait for becoming ghost
BOTSTATE_DEADRELEASED,      // we released as ghost and wait to revive
BOTSTATE_LOOTING,           // looting mode, used just after combat
BOTSTATE_FLYING,            // bot is flying
BOTSTATE_TAME,              // bot hunter taming
BOTSTATE_DELAYED            // bot delay action