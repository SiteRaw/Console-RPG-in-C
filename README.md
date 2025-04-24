# SiteRaw RPG in C (Console Game)
A console RPG with fully customizable units in C.

## About the Project

A Console RPG style game in C, with minimal AI/Bot control by SiteRaw Projects https://www.siteraw.com

### Rules

- units can move around the map and perform actions: charging, combat, shooting (needs ranged weapon), magic (must know spells)
- some units have access to shooting, some have access to magic, some are melee only... see the (editable) CSV file
- CSV unit file is formatted as follows: unit_name, movement, combat_value, strength, toughness, wounds, is_magic, weapon, team, x, y
- first team to get wiped out (no units left) loses
- remember to *backup* CSV file with units if you plan on modifying it

#### Advanced rules

- one turn ends when every unit has performed all their actions. Unit order is simply A, B, C, D... first unit line in CSV becomes unit A.
- Actions are (in order): movement, magic, shooting, combat resolution.
- Units that charge always go first in combat, otherwise unit order is respected.
- Shooting and magic doesn't impede combat.

**How are to hit rolls calculated?** needed_roll = 7 - attacker_combat_value;
**How are to wound rolls calculated?** needed_roll = 4 - attacker_strength + defender_toughness;
**What are critical hits?** To hit rolls of 6 produce 2 hits rather than 1 (only certain weapons).
**What is lifesteal?** To hit rolls of 6 heal 1 wound for the attacker (only certain weapons).
**How do "spells" work?** Two dice are rolled, the sum must be >= to the cost.

#### Spell List

- **Old Forest Roots**: Cost (4), Target (Ally), Effect (+1 toughness)
- **Twisted Instincts**: Cost(6), Target (Enemy), Effect (-1 to hit)
- **Gore Blades**: Cost (7), Target (Ally), Effect (+1 strength)
- **Bestial Rampage**: Cost(10), Target (Enemy), Effect (1D6 hits + move in a random direction)

### Languages

- C
- .csv

### Future updates?

- add critical spells
- enable custom weapon / spell editing
- enable editing of "constant" parameters such as MAP_SIZE through a settings file
- re-instate "running", doubling of the movement characteristic at the expense of shooting & magic, to help melee only units
- add a maximum distance to shooting (depending on the weapon)
