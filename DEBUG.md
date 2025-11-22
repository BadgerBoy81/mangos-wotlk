# Creatures
Creatures are loaded in world/world.cpp -> SetInitialWorldSettings()
starting with     sLog.outString("Loading Creature Stats...");

creature template 18688 ancient orc ancestor
static flags: 2097154
darkened spirit has the same flag
auras in creature_template_addon and creature_addon_: 32648
aura is called ancient orc invisibility but you can see orcs in graveyard, the angry ones
angry ones does not have static flags
static flags: 2097154 -> 0x20 0002 -> 0010 0000 0000 0000 0000 0010
VISIBLE_TO_GHOSTS           = 0x00200000, - NYI
NO_XP                       = 0x00000002,

unit flags ancient: 33536 -> 8300 -> 1000 0011 0000 0000
0×00000100 UNIT_FLAG_IMMUNE_TO_PLAYER
0×00000200 UNIT_FLAG_IMMUNE_TO_NPC
0×00008000 UNIT_FLAG_SWIMMING
unit flags angry: 32768 -> 0x8000 -> 1000 0000 0000 0000
0×00008000 UNIT_FLAG_SWIMMING

Spells:
The creatures above should be visible. potentially because we have commune with the ancestors aura
Aura is present in character auras
When going into area conditions for spell_area are run and nagrand is in area so should apply to whole of nagrand
spellId: 32649

Attributes: 159 383 936 -> 0x980 0180
0×08000000	SPELL_ATTR_ALLOW_WHILE_SITTING
0×01000000	SPELL_ATTR_ALLOW_WHILE_MOUNTED
0×00800000	SPELL_ATTR_ALLOW_CAST_WHILE_DEAD
0×00000100	SPELL_ATTR_DO_NOT_LOG
0×00000080	SPELL_ATTR_DO_NOT_DISPLAY
Debug test. remove the SPELL_ATTR_DO_NOT_DISPLAY making it 0x9800100 -> 159383808 this did not show icon in top right so not as expected

AttributesEx 268435488 -> 0x10000020
0×10000000	SPELL_ATTR_EX_NO_AURA_ICON here we go again
0×00000020	SPELL_ATTR_EX_ALLOW_WHILE_STEALTHED

Debug test change this to 0x20

AttributesEx2 1
0×00000001	SPELL_ATTR_EX2_ALLOW_DEAD_TARGET this sounds like something promising

AttributesEx3 1048576 -> 0x100000
0×00100000	SPELL_ATTR_EX3_ALLOW_AURA_WHILE_DEAD


Shadowmoon zealot creature template 1821
static flags 2097152 -> 0x200000