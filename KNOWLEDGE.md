# CMaNGOS WotLK — Knowledge Base

Reference document for architecture, findings, and things we look up often.
Updated as we learn. If you find yourself searching for the same thing twice, put it here.

---

## Build & Run

### Build commands (Linux / Mint)

```bash
# From Build_linux/
cmake ../Source/mangos-wotlk \
  -DCMAKE_INSTALL_PREFIX=../Run_linux \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_POLICY_DEFAULT_CMP0167=OLD \
  -DBUILD_GAME_SERVER=ON \
  -DBUILD_LOGIN_SERVER=ON \
  -DBUILD_EXTRACTORS=OFF \
  -DBUILD_AHBOT=ON \
  -DBUILD_DEPRECATED_PLAYERBOT=ON \
  -DBUILD_PLAYERBOTS=OFF \
  -DBUILD_SCRIPTDEV=ON

# Build
cmake --build . -- -j$(nproc)

# Install (copies binaries + configs to Run_linux/)
cmake --install .
```

### Linux Mint / Debian library path note

On Debian-based Linux, CMake may resolve OpenSSL/Zlib search paths to `/usr/lib64` even when the libraries live in `/usr/lib/x86_64-linux-gnu`.
For VS Code build tasks, use explicit CMake overrides:

- `-DOPENSSL_ROOT_DIR=/usr`
- `-DOPENSSL_INCLUDE_DIR=/usr/include`
- `-DOPENSSL_CRYPTO_LIBRARY=/usr/lib/x86_64-linux-gnu/libcrypto.so`
- `-DOPENSSL_SSL_LIBRARY=/usr/lib/x86_64-linux-gnu/libssl.so`
- `-DZLIB_INCLUDE_DIR=/usr/include`
- `-DZLIB_LIBRARY_DEBUG=/usr/lib/x86_64-linux-gnu/libz.so`
- `-DZLIB_LIBRARY_RELEASE=/usr/lib/x86_64-linux-gnu/libz.so`

### Run the server

```bash
cd Run_linux/bin/
./mangosd -c ../etc/mangosd.conf    # world server
./realmd -c ../etc/realmd.conf      # auth/realm server
```

Auto-restart scripts (use GNU `screen`): `run-mangosd`, `run-realmd`

### VS Code debug source path mapping

If your debug session reports a source path under `/run/media/system/...` while your workspace is mounted under `/media/...`, add a `sourceMap` entry to `.vscode/launch.json`.
For example:

- `/run/media/system/small_storage/projects/badgerboy/WoW/WotLK` → `${workspaceFolder}/../..`
- `/run/media/system/small_storage/projects/badgerboy/WoW/WotLK/Source/mangos-wotlk` → `${workspaceFolder}`

### Database

- Host: `192.168.1.197:3306` (separate LAN machine)
- User: `mangos_wotlk_dev`
- Databases: `wotlkrealmd`, `wotlkmangos`, `wotlkcharacters`, `wotlklogs`

### Key directories

| Path (relative to WotLK/) | Purpose |
|---|---|
| `Source/mangos-wotlk/` | Main server source (git repo) |
| `Source/wotlk-db/` | Database scripts (git repo) |
| `Build_linux/` | Linux cmake build output |
| `Run_linux/bin/` | Server binaries |
| `Run_linux/etc/` | Runtime config files |

### Client addon workspace

- `wow-wotlk-addons/` is a second workspace root used for local WoW addon development and debugging.
- The client addon folders for `PlayerbotHandler` and `AHSeller` are symlinked into the game client addon path, so changes made here take effect immediately in the client.
- The addon code is edited in the same VS Code workspace context as the server, which makes it easier to work on bot/addon integration together.
- The addon folders themselves are not part of the `Source/mangos-wotlk` git repo; they are separate support content with their own commit history.
- Active addon work in this project currently focuses on:
  - `PlayerbotHandler` — bot-related chat commands, UI integration, and runtime telemetry.
  - `AHSeller` — auction-house helper addon used during playtesting.
- Other addon folders in `wow-wotlk-addons/` are legacy or passive support addons and are not expected to change unless they become part of active work.

### Addon workflow guidance

- You can edit addon and server code in the same workspace to keep the integration context tight.
- Use the terminal or separate git view to commit addon changes to the addon repo while keeping server changes in `Source/mangos-wotlk`.
- Keep server-side bot code changes isolated to the CMaNGOS repo unless the addon integration requires a paired change.

---

## CMaNGOS Architecture Overview

### The big picture

CMaNGOS is a server emulator for World of Warcraft. It receives packets from the WoW game client, processes game logic, and sends packets back. All game state lives server-side in MySQL + in-memory objects.

### Architecture layers

```
Main.cpp → Master::Run()
  └─ WorldRunnable (game loop thread)
       └─ World::Update(diff)           ← The heartbeat. Called every ~50ms.
            ├─ Update sessions           ← Per-player packet processing
            │    └─ WorldSession::Update()
            │         └─ packet handlers
            ├─ Update maps               ← Per-zone object updates
            │    └─ Map::Update()
            │         └─ active object update
            │              └─ Player::Update()
            │                   └─ PlayerbotAI::UpdateAI()  ← bot AI tick
            ├─ Update auctions, mail, events, weather...
            └─ Process CLI commands
```

### PlayerBot update flow

- `PlayerbotAI::UpdateAI()` is invoked from `Player::Update()` during `Map::Update()`.
- `PlayerbotMgr::UpdateAI()` is currently unused for the deprecated playerbot path in this build.
- The bot AI tick is gated by `m_ignoreAIUpdatesUntilTime` and a default 2s repeat timer.
- Main state handling in `PlayerbotAI::UpdateAI()`:
  - `BOTSTATE_LOADING`: teleport to follow target or transition to `BOTSTATE_NORMAL`.
  - `BOTSTATE_DEAD` / `BOTSTATE_DEADRELEASED`: ghost, corpse, and revive handling in `_HandleAIUpdateStateDead()`.
  - `BOTSTATE_TAME`: hunter taming logic in `_HandleAIUpdateStateTaming()`.
  - `BOTSTATE_DELAYED`: spell/craft delay processing before returning to `BOTSTATE_NORMAL`.
  - `BOTSTATE_COMBAT`: combat maneuvers via `DoNextCombatManeuver()`.
  - `BOTSTATE_LOOTING`: loot processing via `DoLoot()`.
  - `BOTSTATE_FLYING`: taxi flight handling via `DoFlight()`.
- Other key behavior:
  - non-positive spell casts are interrupted before new decisions.
  - direct master cast commands run immediately and then return.
  - class-specific idle actions are delegated to `GetClassAI()->DoNonCombatActions()`.
  - loot and GO collection are triggered from class AI flags.

### PlayerBot states

- `BOTSTATE_LOADING`
- `BOTSTATE_NORMAL`
- `BOTSTATE_COMBAT`
- `BOTSTATE_DEAD`
- `BOTSTATE_DEADRELEASED`
- `BOTSTATE_LOOTING`
- `BOTSTATE_FLYING`
- `BOTSTATE_TAME`
- `BOTSTATE_DELAYED`

### Why this matters

- The bot is not a separate scheduler; it runs inside the standard player update loop.
- Breakpoints in `Player::Update()`, `PlayerbotAI::UpdateAI()`, and `PlayerbotAI::SetState()` are the best way to trace bot behavior.

### What to map next

- `DoNextCombatManeuver()` and combat target selection
- `DoLoot()` and loot target formation
- Class-specific `DoNonCombatActions()` for role behavior

### Class AI hook mapping

- Each class AI implements:
  - `DoFirstCombatManeuver(Unit*)`
  - `DoNextCombatManeuver(Unit*)`
  - `DoNonCombatActions()`
- The base `PlayerbotClassAI` provides a no-op default and logs a warning if not overridden.
- Role-specific `DoNonCombatActions()` is used for:
  - self-buffs and stances (`Warrior`)
  - healing/resurrection/party buffs (`Priest`, `Paladin`, `Shaman`, `Druid`)
  - consumable generation and magic armor (`Mage`)
  - ranged pet / ammo upkeep (`Hunter`)
  - stealth and opener preparation (`Rogue`)
  - rune/ghoul maintenance and disease handling (`DeathKnight`)
  - curse removal, buffing, and regeneration logic (`Warlock`)
### Class combat behavior

- `DoFirstCombatManeuver()` is usually the early combat setup phase:
  - honors temporary wait conditions (`ORDERS_TEMP_WAIT_TANKAGGRO`, `ORDERS_TEMP_WAIT_OOC`)
  - then dispatches to `DoFirstCombatManeuverPVE()` or `DoFirstCombatManeuverPVP()`.
- `DoNextCombatManeuver()` is the main combat loop and often switches by scenario type.

#### Warrior
- waits for tank aggro if ordered, then starts attacking once in range.
- `DoFirstCombatManeuverPVE()` can change stance before combat: `DEFENSIVE_STANCE` for tanks, `BERSERKER_STANCE` for fury, `BATTLE_STANCE` for arms.
- it may also use `TAUNT`, `INTERCEPT`, or `CHARGE` as opening moves when appropriate.
- `DoNextCombatManeuverPVE()` uses spec-specific rotation:
  - Arms: `REND`, `MORTAL_STRIKE`, `SHATTERING_THROW`, `BLADESTORM`, `OVERPOWER`, `HEROIC_STRIKE`, `SLAM`
  - Fury: `BLOODTHIRST`, `RAMPAGE`, `WHIRLWIND`, `EXECUTE`, `BLOOD_SURGE` procs, `HEROIC_STRIKE`
  - Protection: `REVENGE`, `DEVASTATE`, `TAUNT`, `SHIELD_SLAM`, `SHIELD_BLOCK`, `CONCUSSION_BLOW`, `SPELL_REFLECTION`
- uses `BERSERKER_RAGE`, shout checks, and threat-aware fallback to keep aggro.

#### Priest
- may use `PRAYER_OF_MENDING` on tanks during early PVE.
- `DoNextCombatManeuverPVE()` can cast `FEAR_WARD`, `FADE`, `INNER_FIRE`, and use `SHADOWFORM`/`VAMPIRIC_EMBRACE`.
- healer logic is strong; non-healer priests still may heal self if needed.
- can buff party with `PRAYER_OF_FORTITUDE`, `PRAYER_OF_SPIRIT`, `PRAYER_OF_SHADOW_PROTECTION`, or lower-rank equivalents.

#### Mage
- `DoFirstCombatManeuver()` is usually no-op.
- `DoNextCombatManeuverPVE()` decides ranged or melee style by range and wand availability.
- Frost spec uses `ICY_VEINS`, `ICE_BLOCK`, `ICE_BARRIER`, `DEEP_FREEZE`, `BLIZZARD`, `CONE_OF_COLD`, `FROSTBOLT`, `FROST_NOVA`, `ICE_LANCE`, and `SUMMON_WATER_ELEMENTAL`.
- Fire spec uses `FIRE_WARD`, `COMBUSTION`, `FIREBALL`, `FIRE_BLAST`, `FLAMESTRIKE`, `SCORCH`, `PYROBLAST`, `BLAST_WAVE`, `DRAGONS_BREATH`, `LIVING_BOMB`, and `FROSTFIRE_BOLT`.
- Arcane spec uses `ARCANE_POWER`, `ARCANE_MISSILES`, `ARCANE_BLAST`, `ARCANE_BARRAGE`, and will cast `SHOOT` when threat management requires it.
- threat fallback is `SHOOT`/auto-shot when the bot has aggro and no better threat control.

#### Paladin
- `DoNextCombatManeuverPVE()` checks seals and chooses healer vs melee behavior.
- uses emergency survival spells: `DIVINE_SHIELD`, `DIVINE_PROTECTION`, and `HAND_OF_PROTECTION`/`DIVINE_SACRIFICE` if necessary.
- dispels magic/disease/poison before damage rotations.
- healer paladins will heal via `FindTargetAndHeal()`, while other paladins may use `HOLY_LIGHT`, `FLASH_OF_LIGHT`, `DIVINE_STORM`, `JUDGEMENT_OF_LIGHT`, and `HAMMER_OF_WRATH`.
- it also stuns elites with `HAMMER_OF_JUSTICE` when aggroed.

#### Shaman
- `DoNextCombatManeuverPVE()` sets healers to ranged and DPS to melee based on role.
- dispels disease/poison via `DispelPlayer()`, then heals with `FindTargetAndHeal()`.
- enhancement rotation uses `STORMSTRIKE`, `FLAME_SHOCK`, `EARTH_SHOCK`, `LAVA_LASH`, and `LIGHTNING_BOLT` under `MAELSTROM_WEAPON`.
- restoration/elemental uses `FLAME_SHOCK`, `LAVA_BURST`, `LIGHTNING_BOLT`, plus totem dropping via `DropTotems()`.
- it also checks totems/shields and cooldowns with `CheckShields()` and `UseCooldowns()`.

#### Hunter
- enforces ranged style unless melee range or out of ammo.
- PVE logic heals or buffs the pet, then uses racial buffs and ranged rotation.
- ranged spells include `HUNTERS_MARK`, `RAPID_FIRE`, `MULTI_SHOT`, `ARCANE_SHOT`, `CONCUSSIVE_SHOT`, `VIPER_STING`, `SERPENT_STING`, `BLACK_ARROW`, `AIMED_SHOT`, `STEADY_SHOT`, and `KILL_SHOT`.
- if forced melee, uses `RAPTOR_STRIKE`, traps, `WING_CLIP`, and survival cooldowns.

#### Rogue
- first maneuver tries stealth and opener moves (`AMBUSH`, `CHEAP_SHOT`, `GARROTE`).
- combat sequences are divided into stealth, threat control, and normal melee.
- uses `FEINT`, `BLIND`, `VANISH`, `SHADOW_DANCE`, and energy-based finishers like `EVISCERATE`.
- also uses racial escapes and `KIDNEY_SHOT` for crowd control.

#### Warlock
- combat rotation depends strongly on spec and pet status.
- can create shards in-combat with `DRAIN_SOUL` and manage mana via `LIFE_TAP` / `DARK_PACT`.
- Affliction uses `CURSE_OF_AGONY`, `CORRUPTION`, `UNSTABLE_AFFLICTION`, `SHADOW_BOLT`.
- Demonology uses `DEMONIC_EMPOWERMENT`, `CURSE_OF_AGONY`, `CORRUPTION`, `INCINERATE`, `SHADOW_BOLT`.
- Destruction uses `CONFLAGRATE`, `CHAOS_BOLT`, `INCINERATE`, `SHADOW_BOLT`.
- PVP can open with `FEAR` then fall back to standard damage rotation.

#### Druid
- first maneuver may stealth or simply proceed.
- spec-based combat includes bear/cat DPS and spell DPS.
- bear uses `GROWL`, `FAERIE_FIRE_FERAL`, `SWIPE`, `MANGLE`, `LACERATE`, `MAUL`.
- cat uses `RIP`, `FEROCIOUS_BITE`, `SAVAGE_ROAR`, `RAKE`, `MANGLE_CAT`.
- balance uses casters like `FAERIE_FIRE`, `MOONFIRE`, `INSECT_SWARM`, `WRATH`, `STARFIRE`, and `FORCE_OF_NATURE`.
- healer mode switches to `_DoNextPVECombatManeuverHeal()` with dispels and combat resurrect logic.

#### DeathKnight
- keeps `HORN_OF_WINTER` on the master.
- uses `DEATH_GRIP` as a pull and spec-specific runic attacks.
- combat flow includes presence and rune cooldown management.

### Combat and loot flow
#### Detailed class non-combat behavior

- `Warrior`:
  - Enforces correct stance for spec.
  - Buffs master with `VIGILANCE`.
  - Uses food/drink/bandage.
  - Casts `GIFT_OF_THE_NAARU` if Draenei and low health.

- `Priest`:
  - Self-buffs with `INNER_FIRE`.
  - Dispel magic/disease.
  - Resurrect allies.
  - Heal party or self if not a healer role.
  - Group buffs: `PRAYER_OF_FORTITUDE`, `PRAYER_OF_SPIRIT`, `PRAYER_OF_SHADOW_PROTECTION` or their lower-ranked counterparts.
  - Eat/drink/bandage.

- `Mage`:
  - Removes curses from the group.
  - Ensures armor buff active (`MOLTEN_ARMOR`, `MAGE_ARMOR`, `ICE_ARMOR`, `FROST_ARMOR`).
  - Buffs party with `ARCANE_BRILLIANCE`/`ARCANE_INTELLECT`.
  - Conjures food/water if bag space allows.
  - Eat/drink/bandage.

- `Paladin`:
  - Checks and applies auras via `CheckAuras()`.
  - Uses `RIGHTEOUS_FURY` when tanking, removes it otherwise.
  - Dispels and resurrects.
  - Heals party or self.
  - Buffs group using Paladin blessing helper.
  - Eat/drink/bandage.

- `Shaman`:
  - Dispels disease/poison.
  - Resurrects allies.
  - Heals party or self.
  - Maintains weapon enchants by spec.
  - Uses `EatDrinkBandage()`.

- `Druid`:
  - Revives allies.
  - Dispels curse/poison.
  - Heals party or self.
  - Buffs group with `GIFT_OF_THE_WILD`/`MARK_OF_THE_WILD` and `THORNS`.
  - Uses `OMEN_OF_CLARITY` and `INNERVATE` for mana support.
  - Returns to proper shapeshift form after buffs.

- `Hunter`:
  - Forces ranged combat style.
  - Uses `TRUESHOT_AURA` and `ASPECT_OF_THE_HAWK`.
  - Heals/feeds pet, revives pet, or summons pet as needed.
  - Uses food/drink/bandage.

- `Rogue`:
  - Removes stealth when idle.
  - Uses food/drink/bandage.
  - Applies poisons to mainhand/offhand weapons when missing.

- `Warlock`:
  - Initializes demon spells.
  - Destroys extra soul shards when inventory is low.
  - Buffs self with `FEL_ARMOR`/`DEMON_ARMOR`/`DEMON_SKIN`.
  - Creates healthstones, soulstones, spellstones/firestones.
  - Uses `DARK_PACT` and `LIFE_TAP` for mana.
  - Heals voidwalker pet and maintains pet buffs like `SOUL_LINK` and `BLOOD_PACT`.

- `DeathKnight`:
  - Keeps `HORN_OF_WINTER` on the master.
  - Ensures the bot is standing.
  - Uses food/drink/bandage.
  - Casts `DEATH_GRIP` if available as a pull ability.

### Combat and loot flow

- Combat enters `DoNextCombatManeuver()` once the bot is in combat or has a valid target.
- `Attack()` sets `BOTSTATE_COMBAT`, clears looting, and calls `GetCombatTarget()`.
- `GetCombatTarget()` prefers:
  1. Forced target
  2. Protect target attackers when `ORDERS_PROTECT` is set
  3. Assist target attackers when `ORDERS_ASSIST` is set
  4. Any attacker found by `FindAttacker()`
- The target may be cleared if it is dead, invalid, too far, or neutralized.
- When the target changes, `DoFirstCombatManeuver()` from class AI executes first-move logic.
- `DoCombatMovement()` handles melee chase, ranged chase, and stay behavior according to `m_combatStyle` and `m_movementOrder`.
- `DoLoot()` iterates `m_lootTargets`, moves to the object, and loots or skins it.
- Looting may also use lockpicking, opening spells, keys, bombs, or other skill-based actions.
- If a loot target is invalid, too far, or unlootable, it is dropped and the bot moves on.

### Threat and pull helper behavior

- `PlayerbotAI::FindAttacker()` scans `m_attackerInfo` for attackers and can filter by:
  - `AIT_VICTIMSELF` or `AIT_VICTIMNOTSELF`
  - `AIT_HIGHESTTHREAT` or `AIT_LOWESTTHREAT`
  - a specific victim unit
- This is the core helper for protect/assist target selection and threat-based decisions.
- `PlayerbotAI::GroupHoTOnTank()` loops the group and asks each bot class AI to cast a heal-over-time on the tank.
- If any bot casts a HoT, the group is told to delay updates briefly (`SetIgnoreUpdateTime(1)`).
- `PlayerbotAI::CanPull()` validates pull conditions:
  - same group as the master
  - group is idle and ready
  - the bot is a tank role
  - the bot class can actually pull via `PlayerbotClassAI::CanPull()`
- `PlayerbotAI::CastPull()` dispatches the pull action to the specific class AI (`Warrior`, `Paladin`, `Druid`, `DeathKnight`).
- `PlayerbotAI::GroupTankHoldsAggro()` checks whether the group tank still has aggro by scanning attackers not targeting the tank.

### Bot creation, login, and logout flow

- Bots are created via the player’s gossip menu using `GOSSIP_OPTION_BOT`.
- `Creature::LoadBotMenu()` builds menu items for recruiting or dismissing online characters on the same account.
- Selecting `GOSSIP_OPTION_BOT` calls `PlayerbotMgr::LoginPlayerBot()` or `LogoutPlayerBot()`.
- `LoginPlayerBot()` uses a special `HandlePlayerBotLoginCallback` path, not the normal player login path.
- On bot login, `PlayerbotMgr::OnBotLogin()`:
  - queues movement packets for the bot session
  - allocates `PlayerbotAI` for the bot
  - stores the bot in `m_playerBots`
  - ensures the bot is removed from groups if it is not with the master
  - restores leadership to the master if a bot was leader
- `LogoutPlayerBot()` and `LogoutAllBots()` mark bots for removal.
- `RemoveBots()` performs safe logout by erasing the bot from `m_playerBots`, calling `LogoutPlayer()`, and deleting the bot’s `WorldSession`.

### Bot login callback path

- `PlayerbotHolder::AddPlayerBot()` is the entry point for a bot recruitment request.
- It creates a `PlayerbotLoginQueryHolder` and delays the database query to `HandlePlayerBotLoginCallback()`.
- `HandlePlayerBotLoginCallback()` creates a new `WorldSession` for the bot with `SEC_PLAYER` and disables anticheat.
- The bot session calls `HandlePlayerLogin(lqh)` to load the bot character from DB.
- After login, control is transferred to `PlayerbotMgr::OnBotLogin()` if the bot is approved.
- Approval conditions include same account, guild bot allowance, or random-account list membership.
- If not approved, the bot is immediately logged out via `LogoutPlayerBot()`.

### Bot AI initialization

- `PlayerbotAI` is allocated for each bot inside `PlayerbotMgr::OnBotLogin()`.
- The constructor sets initial state, follow behavior, collection flags, and loads quest needs.
- `ReloadAI()` creates the class-specific AI object based on the bot’s class and spec:
  - `Priest`, `Mage`, `Warlock`, `Hunter` = ranged AI
  - `Warrior`, `Paladin`, `Rogue`, `DeathKnight` = melee AI
  - `Shaman` and `Druid` choose ranged vs melee based on spec
- `ReloadAI()` also reinitializes gathering spells and any class-specific setup.

### PlayerbotAI update loop and state machine

- `PlayerbotAI::UpdateAI()` is driven by a 2-second default update timer gated by `m_ignoreAIUpdatesUntilTime`.
- Update is skipped while the bot is in bot loading teleport, currently trading, or if a spell cast is active.
- `BOTSTATE_LOADING`: teleports the bot to the follow target if too far; otherwise transitions to `BOTSTATE_NORMAL`.
- `BOTSTATE_TAME`: runs `_HandleAIUpdateStateTaming()` for hunter taming behavior.
- `BOTSTATE_DELAYED`: handles spell craft/delay actions and returns to `BOTSTATE_NORMAL` when done.
- `BOTSTATE_DEAD` / `BOTSTATE_DEADRELEASED`: handles ghost movement, corpse reclaim timing, and auto resurrection via the master.
- Mounted masters force bot dismounts in `UpdateAI()` before other actions.
- Combat is handled if the bot is in combat, has a valid target, or is in duel scenario:
  - if no mount and not channeling, `DoNextCombatManeuver()` is executed
  - combat movement is handled separately via `DoCombatMovement()` afterward
- Exiting combat with a previous combat state moves the bot into `BOTSTATE_LOOTING` and clears attacker info.
- `BOTSTATE_LOOTING` runs `DoLoot()` until no loot targets remain, then returns to `BOTSTATE_NORMAL`.
- `BOTSTATE_FLYING` calls `DoFlight()` and then returns to `BOTSTATE_NORMAL`.
- In `BOTSTATE_NORMAL`, without mount or regen, bots perform class-specific `DoNonCombatActions()`, then optionally gather loot or game objects if collection flags are set.
- If movement command is idle and not in combat, `MovementReset()` restores follow behavior.

### Master command / packet handling

- `GetCombatTarget()` always starts by calling `UpdateAttackerInfo()`.
- It can force a target from an explicit `forcedTarget` or from `ORDERS_PROTECT` on a protected unit.
- If a valid current target exists, it is retained unless it becomes neutralized or invalid.
- `ORDERS_ASSIST` makes the bot assist a target attacking an assist target with lowest threat.
- Otherwise the bot falls back to the next attacker from `FindAttacker()`.
- If the target enters a duel, the bot waits 6 seconds and ignores the duel combat.
- When target changes, the bot executes class-specific `DoFirstCombatManeuver()` before movement.

- `PlayerbotMgr::HandleMasterIncomingPacket()` intercepts many master actions and mirrors them for bots.
- Taxi packets (`CMSG_ACTIVATETAXI`, `CMSG_ACTIVATETAXIEXPRESS`) are propagated to nearby group bots via `GetTaxi()`.
- `CMSG_LOGOUT_REQUEST` logs out the bots with the master.
- `CMSG_INSPECT` causes the selected bot to send equip advice.
- Emotes like `POINT`, `STAND`, and `WAVE` are interpreted as bot commands for attack, stay, and follow.
- `PlayerbotMgr::HandleMasterOutgoingPacket()` watches selected server-to-client master packets.
- It mirrors master mount state so bots try to mount when the master mounts and dismount when the master dismounts.
- If a matching mount spell is unavailable, bots search inventory for a mount item with comparable speed.

### Command and order handling

- `PlayerbotAI::HandleCommand()` parses whispers from the master and can obey only authorized players.
- Commands include `orders`, `follow`, `stay`, `attack`, `pull`, `neutralize`, `cast`, `sell`, `buy`, `drop`, `repair`, `auction`, `mail`, `bank`, `talent`, `use`, `equip`, `find`, `get`, `collect`, `quest`, `craft`, and many more.
- `orders` handles combat orders and delay settings:
  - `orders combat tank`, `maintank`, `assist`, `heal`, `mainheal`, `notmainheal`, `protect`, `passive`, `pull`, `nodispel`, `resistfire`, `resistnature`, `resistfrost`, `resistshadow`.
  - `orders resume` restores saved combat orders from `playerbot_saved_data`.
- Combat orders are stored in `m_combatOrder` and optionally saved into `playerbot_saved_data`.
- `SetCombatOrder()` enforces mutual exclusivity between main and regular tank/heal orders.
- `ORDERS_PASSIVE` clears assist/protect targets and keeps the bot out of combat.
- `pull` command executes `CanPull()`, sets the target, casts the bot’s pull ability, and sets group `ORDERS_TEMP_WAIT_TANKAGGRO`.

- Movement orders are handled by `SetMovementOrder()`:
  - `MOVEMENT_FOLLOW` makes the bot follow the master with configurable distances.
  - `MOVEMENT_STAY` stops movement.
  - `MovementReset()` teleports the bot if it falls too far behind and uses randomized follow distance/angles.
- Follow distance may be adjusted with `follow auto`, `follow reset`, `follow far`, `follow near`, and `follow info`.
- `attack` uses the master’s selection to force the bot into combat immediately.
- `neutralize` uses class-specific `Neutralize()` logic to choose a best anti-beast/undead dispel spell.
- `cast` allows the master to command the bot to cast a specific spell on the master’s selected target.

### Combat movement and loot flow

- `PlayerbotAI::DoCombatMovement()` is called after `DoFirstCombatManeuver()` on target change.
- For melee bots, it chases the target if not already in melee reach and not ordered to stay.
- For ranged bots, it chases only if it cannot reach the target with a ranged spell and is not a healer.
- Otherwise ranged bots hold position and stop movement to cast.
- `PlayerbotAI::DoLoot()` processes `m_lootTargets` and `m_lootCurrent` in FIFO order.
- It validates loot objects against distance, master distance, spawn state, and lootability.
- Creatures are looted directly or skinned if skinnable; game objects may need locks, keys, lockpicking, or skill spells.
- If the bot cannot loot a target, it clears the target, idles, and moves on.
- Looting can also queue items such as keys, bombs, or opening spells before clearing the target.
- Quest packets (`CMSG_QUESTGIVER_ACCEPT_QUEST`, `CMSG_QUESTGIVER_COMPLETE_QUEST`) are replayed for bots so they can accept/complete the same quests.
- Area trigger packets and gossip packets are forwarded to bots in range so they can trigger the same game objects or NPC interactions.
- Loot roll and gameobject use packets are used to drive bot loot logic and object interactions.

### Pain points & refactor targets

- `PlayerbotAI::UpdateAI()` is very large and mixes timer gating, state dispatch, command execution, combat, follow movement, looting, and death handling in one function.
- The default `SetIgnoreUpdateTime(2)` is a hardcoded idle throttle rather than a configurable scheduler.
- Combat logic is split across `UpdateAI()`, `DoNextCombatManeuver()`, `DoFirstCombatManeuver()`, and the class AIs, making it hard to follow the complete path.
- `GetCombatTarget()` and `DoNextCombatManeuver()` both use target state heavily, creating subtle dependencies on when `m_targetCombatGuid` is updated.
- `DoCombatMovement()` still assumes the combat loop has already set the correct combat style and target.
- `PlayerbotMgr::HandleMasterIncomingPacket()` and `HandleMasterOutgoingPacket()` are effectively command replication layers, so packet-driven state changes can occur asynchronously to the 2s update tick.
- Refactor target: extract `UpdateAI()` state handlers, keep semantics unchanged, and make the idle interval configurable.

### Practical debug hooks

- `PlayerbotAI::Attack()` and `PlayerbotAI::GetCombatTarget()` are the first decision points for combat.
- `PlayerbotAI::DoNextCombatManeuver()` is the core combat update loop.
- `PlayerbotAI::DoLoot()` is the core loot loop, and it may queue further actions via item use or spells.

### Object hierarchy

```
Object
  └─ WorldObject              (has position in the world)
       ├─ Unit                (has health, can fight)
       │    ├─ Player         (human-controlled character)
       │    └─ Creature       (NPC / mob)
       ├─ GameObject          (doors, chests, flags)
       ├─ DynamicObject       (AoE effects on ground)
       └─ Corpse
```

### Key systems

| System | Key files | What it does |
|--------|-----------|--------------|
| **World loop** | `WorldRunnable.cpp`, `World.cpp` | Drives the game tick at ~50ms intervals |
| **Sessions** | `WorldSession.cpp` | One per connected player. Handles client↔server packets (opcodes) |
| **Maps** | `Map.cpp`, `MapManager.cpp` | Each zone/instance is a Map. Updates all objects within it |
| **Spells** | `Spell.cpp`, `SpellAuras.cpp`, `SpellEffects.cpp` | Cast processing, aura application, effect resolution |
| **Movement** | `MotionGenerators/`, recastnavigation | Pathfinding via navmesh. Movement types: follow, chase, waypoint, etc. |
| **AI** | `CreatureAI.cpp`, ScriptDevAI/ | NPC behavior. Bosses have custom scripts in ScriptDevAI |
| **PlayerBot** | `PlayerBot/Base/`, `PlayerBot/AI/` | Deprecated bot system — AI-controlled Player objects |

---

## PlayerBot System

### Class hierarchy

```
PlayerbotMgr                    ← Manages all bots for one player account
  └─ PlayerbotAI                ← Core AI brain for one bot character
       └─ PlayerbotClassAI      ← Abstract interface for class-specific logic
            ├─ PlayerbotWarriorAI
            ├─ PlayerbotPaladinAI
            ├─ PlayerbotHunterAI
            ├─ PlayerbotRogueAI
            ├─ PlayerbotPriestAI
            ├─ PlayerbotShamanAI
            ├─ PlayerbotMageAI
            ├─ PlayerbotWarlockAI
            ├─ PlayerbotDruidAI
            └─ PlayerbotDeathKnightAI
```

### Bot states

| State | Meaning |
|-------|---------|
| `BOTSTATE_LOADING` | Loading during world load or zone transition |
| `BOTSTATE_NORMAL` | Idle — normal AI routines processed |
| `BOTSTATE_COMBAT` | In combat |
| `BOTSTATE_DEAD` | Dead, waiting to become ghost |
| `BOTSTATE_DEADRELEASED` | Released as ghost, waiting to revive |
| `BOTSTATE_LOOTING` | Looting after combat |
| `BOTSTATE_FLYING` | Flying (taxi/flight path) |
| `BOTSTATE_TAME` | Hunter taming |
| `BOTSTATE_DELAYED` | Delayed action (crafting etc.) |

### Combat orders

16 combat order types including: TANK, HEAL, ASSIST, PROTECT and group coordination flags like `ORDERS_TEMP_WAIT_TANKAGGRO`.

### Job roles (priority order)

Main Tank → Tank → Healer → Main DPS → DPS → Master → All

### Key files

All paths relative to `Source/mangos-wotlk/src/game/PlayerBot/`:

| File | What |
|------|------|
| `Base/PlayerbotAI.h` | Core header — enums, constants, class def (2300+ lines) |
| `Base/PlayerbotAI.cpp` | Core implementation — `UpdateAI()`, state machine, combat logic |
| `Base/PlayerbotMgr.h/.cpp` | Bot manager — login/logout bots, route commands |
| `Base/PlayerbotClassAI.h/.cpp` | Abstract class AI interface |
| `AI/Playerbot*AI.h/.cpp` | 10 class-specific AIs (spell selection, role behavior) |
| `config.h.in` | Compile-time config |
| `playerbot.conf.dist.in` | Runtime config template |

### Runtime config (playerbot.conf)

| Setting | Default | What |
|---------|---------|------|
| `PlayerbotAI.DisableBots` | 0 | Master switch |
| `PlayerbotAI.MaxNumBots` | 9 | Max bots per account |
| Collection flags | varies | Auto-collect combat/quest/loot items |
| `PlayerbotAI.SellGarbage` | 0 | Auto-sell gray items |

---

## Boss Encounters — How They Work

*(To be filled in Phase 2)*

### Pattern

Boss scripts live in `src/game/AI/ScriptDevAI/scripts/`. Each boss is a `CreatureAI` subclass with:
- `UpdateAI(uint32 diff)` — tick function with spell timers
- Phase transitions based on HP thresholds or timers
- Spell casts on timers or triggered by events

### Broggok (Blood Furnace)

*(To be filled when we study this encounter)*

- Location: Hellfire Citadel → Blood Furnace (TBC dungeon)
- Script path: `src/game/AI/ScriptDevAI/scripts/outland/hellfire_citadel/blood_furnace/` (to confirm)
- Key mechanics: waves of adds, poison bolt (AoE), bomb mechanic on random player

---

## Config Reference

### Useful mangosd.conf log filters

```
LogFilter_AIAndMovegens = 0    # AI decisions and movement
LogFilter_Combat = 0           # Combat calculations
LogFilter_SpellCast = 0        # Spell cast processing
LogFilter_Damage = 0           # Damage calculations
LogFilter_PlayerStats = 0      # Player stat changes
```

Set to `1` to **suppress** that log category. Set to `0` to see it.

### Quick log commands

```bash
# Follow server log, filter for playerbot
tail -f Server.log | grep -i playerbot

# Follow with multiple filters
tail -f Server.log | grep -iE "playerbot|combat|spell"
```

---

## CMake Build Flags

| Flag | Default | Notes |
|------|---------|-------|
| `BUILD_GAME_SERVER` | ON | World server (mangosd) |
| `BUILD_LOGIN_SERVER` | ON | Auth/realm server (realmd) |
| `BUILD_SCRIPTDEV` | ON | Boss/encounter scripts. Turn OFF for faster dev builds |
| `BUILD_DEPRECATED_PLAYERBOT` | OFF | **Our playerbot** — must be ON |
| `BUILD_PLAYERBOTS` | OFF | New playerbot mod — keep OFF |
| `BUILD_AHBOT` | OFF | Auction house bot |
| `BUILD_EXTRACTORS` | OFF | Map/DBC extractors |
| `PCH` | ON | Precompiled headers (speeds compile) |
| `DEBUG` | OFF | Full debug build |

### Mint-specific

- CMake 3.30+ requires: `-DCMAKE_POLICY_DEFAULT_CMP0167=OLD` (for Boost static linking)
- Build dependencies: `build-essential cmake git libboost-dev libssl-dev zlib1g-dev libmariadb-dev libreadline-dev libbz2-dev`
- Runtime dependency for the installed binary: `libmariadb3`

---

## Things We Keep Looking Up

*(Add entries here when you catch yourself searching for the same thing repeatedly)*

<!-- Example format:
### How to restart just the world server without rebuilding
Kill mangosd, re-run `./mangosd -c ../etc/mangosd.conf` from Run_linux/bin/

### Where are spell IDs defined?
DBCs (client data files) + `Spell.dbc` table. In code: `SpellEntry` struct.
-->
