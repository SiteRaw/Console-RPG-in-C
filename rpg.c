#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

// Constants
#define MAP_SIZE 8
#define MAX_SPELLS 4
#define MAX_WEAPONS 5
#define MAX_ACTIONS 100 // For turn recap

// Struct definitions
typedef struct {
    char name[50];
    int range, attacks, bonus_strength, bonus_dmg;
    char special_rule[20];
} Weapon;

typedef struct {
    char name[50];
    int cost;
    char target[10];
    char effect[100];
} Spell;

typedef struct {
    char name[50];
    int movement, combat_value, strength, toughness, wounds, is_magic;
    Weapon* weapon;
    int x, y; // Position on map
    int team; // 0 for player team, 1 for enemy
    int has_moved, has_run, has_charged; // Turn state
} Unit;

// Turn recap struct
typedef struct {
    char attacker_name[50];
    char target_name[50];
    char spell_name[50];
    int action_type; // 0: combat, 1: magic, 2: shooting
    int hits, wounds, target_wounds;
    int roll, roll_needed;
} Action;

// Global variables
char map[MAP_SIZE][MAP_SIZE];
Unit* units = NULL; // Dynamic array for units
Weapon weapons[MAX_WEAPONS];
Spell spells[MAX_SPELLS];
int current_turn = 0; // 0 for player, 1 for enemy
int num_units = 0; // Track actual number of units
Action actions[MAX_ACTIONS]; // Store turn actions
int action_count = 0; // Track number of actions

// Function prototypes
void initialize_game();
void initialize_map();
void initialize_units();
void initialize_weapons();
void initialize_spells();
void display_map();
int roll_dice(int count);
void display_dice_roll(int roll);
int to_hit_roll(int combat_value, int* critical, int* roll_result);
int to_wound_roll(int attacker_strength, int defender_toughness, int* roll_result);
int perform_attack(Unit* attacker, Unit* defender, int* hits, int is_shooting);
int calculate_wounds(Unit* a, Unit* d, int hits);
void movement_phase(Unit* unit);
void magic_phase(Unit* unit);
void shooting_phase(Unit* unit);
void ai_shooting_phase(Unit* unit);
void combat_phase();
void apply_spell_effect(Unit* target, Spell* spell);
int is_adjacent(Unit* unit1, Unit* unit2);
int is_tile_occupied(int x, int y, Unit* moving_unit);
void move_unit(Unit* unit, int new_x, int new_y);
void enemy_turn();
int is_game_over();
Unit* find_closest_enemy(Unit* enemy);
void turn_recap();

// Main function
int main() {
    srand(time(NULL));
    initialize_game();
    printf("SiteRaw RPG Game\n");

    while (!is_game_over()) {
        action_count = 0; // Reset actions for recap
        printf("\nTurn %d:\n", current_turn / 2 + 1);

        // Player turn
        printf("Player's turn\n");
        for (int i = 0; i < num_units; i++) { // Player units
            if (units[i].wounds > 0 && units[i].team == 0) {
                printf("\n%s's turn:\n", units[i].name);
                units[i].has_moved = units[i].has_run = units[i].has_charged = 0;
                movement_phase(&units[i]);
                if (!units[i].has_run) {
                    magic_phase(&units[i]);
                    shooting_phase(&units[i]);
                }
            }
        }
        combat_phase();

        if (is_game_over()) break;

        // Enemy turn
        printf("Enemy's turn\n");
        enemy_turn();
        combat_phase();

        turn_recap(); // Display turn summary after both player and enemy phases
        current_turn += 2; // Increment by 2 to count a full turn
    }

    int enemy_life = 0; int player_life = 0;
    for (int i = 0; i < num_units; i++) { // Player units
        if (units[i].team == 0)
	    player_life += units[i].wounds;
	else
	    enemy_life += units[i].wounds;
    }
    printf("\nGame Over! %s wins!\n", is_game_over() && player_life >= enemy_life ? "Player" : "Enemy");
    free(units);
    return 0;
}

// Initialize game data
void initialize_game() {
    initialize_map();
    initialize_weapons();
    initialize_spells();
    initialize_units();
}

void initialize_map() {
    for (int i = 0; i < MAP_SIZE; i++)
        for (int j = 0; j < MAP_SIZE; j++)
            map[i][j] = '.';
}

void initialize_weapons() {
    strcpy(weapons[0].name, "Bestial Blades");
    weapons[0].range = 1; weapons[0].attacks = 7; weapons[0].bonus_strength = 1; weapons[0].bonus_dmg = 0;
    strcpy(weapons[0].special_rule, "critical_hit");

    strcpy(weapons[1].name, "Bestial Staff");
    weapons[1].range = 1; weapons[1].attacks = 3; weapons[1].bonus_strength = 0; weapons[1].bonus_dmg = 0;
    strcpy(weapons[1].special_rule, "lifesteal");

    strcpy(weapons[2].name, "Crossbow of Death");
    weapons[2].range = 4; weapons[2].attacks = 3; weapons[2].bonus_strength = 0; weapons[2].bonus_dmg = 0;
    strcpy(weapons[2].special_rule, "none");

    strcpy(weapons[3].name, "Spectral Axe");
    weapons[3].range = 1; weapons[3].attacks = 8; weapons[3].bonus_strength = 2; weapons[3].bonus_dmg = 0;
    strcpy(weapons[3].special_rule, "death_wound");

    strcpy(weapons[4].name, "Bestial Bow");
    weapons[4].range = 3; weapons[4].attacks = 2; weapons[4].bonus_strength = 0; weapons[4].bonus_dmg = 0;
    strcpy(weapons[4].special_rule, "critical_hit");
}

void initialize_spells() {
    strcpy(spells[0].name, "Old Forest Roots"); strcpy(spells[0].target, "ally");
    spells[0].cost = 4; strcpy(spells[0].effect, "+1 toughness");

    strcpy(spells[1].name, "Twisted Instincts"); strcpy(spells[1].target, "enemy");
    spells[1].cost = 6; strcpy(spells[1].effect, "-1 to hit");

    strcpy(spells[2].name, "Gore Blades"); strcpy(spells[2].target, "ally");
    spells[2].cost = 7; strcpy(spells[2].effect, "+1 strength");

    strcpy(spells[3].name, "Bestial Rampage"); strcpy(spells[3].target, "enemy");
    spells[3].cost = 10; strcpy(spells[3].effect, "1D6 hits + move");
}

void initialize_units() {
    FILE *file = fopen("runits.csv", "r");
    if (!file) {
        printf("Error opening file.\n");
        exit(1);
    }

    // Count lines to determine number of units
    char line[200];
    num_units = -1; // Skip header
    while (fgets(line, sizeof(line), file)) num_units++;
    rewind(file);

    units = (Unit*)malloc(num_units * sizeof(Unit));
    if (!units) {
        printf("Memory allocation failed.\n");
        fclose(file);
        exit(1);
    }

    fgets(line, sizeof(line), file); // Skip header
    int i = 0;
    char weapon_name[50];
    while (fgets(line, sizeof(line), file) && i < num_units) {
        sscanf(line, "%[^,],%d,%d,%d,%d,%d,%d,%[^,],%d,%d,%d",
               units[i].name, &units[i].movement, &units[i].combat_value, &units[i].strength,
               &units[i].toughness, &units[i].wounds, &units[i].is_magic, weapon_name,
               &units[i].team, &units[i].x, &units[i].y);

        // Assign weapon pointer
        units[i].weapon = NULL;
        for (int j = 0; j < MAX_WEAPONS; j++) {
            if (strcmp(weapon_name, weapons[j].name) == 0) {
                units[i].weapon = &weapons[j];
                break;
            }
        }
        if (!units[i].weapon) {
            printf("Warning: Weapon %s not found for unit %s\n", weapon_name, units[i].name);
            units[i].weapon = &weapons[1]; // Default to Bestial Staff
        }
        units[i].has_charged = 0; // Initialize charge flag
        i++;
    }

    fclose(file);
}

// Display the game map
void display_map() {
    printf("  ");
    for (int j = 0; j < MAP_SIZE; j++) printf("%c ", 'A' + j);
    printf("\n");
    for (int i = 0; i < MAP_SIZE; i++) {
        printf("%d ", i + 1);
        for (int j = 0; j < MAP_SIZE; j++) {
            int unit_here = -1;
            for (int k = 0; k < num_units; k++)
                if (units[k].x == i && units[k].y == j && units[k].wounds > 0)
                    unit_here = k;
            if (unit_here >= 0)
                printf("%c ", units[unit_here].name[0]);
            else
                printf(". ");
        }
        printf("\n");
    }
}

// Dice rolling
int roll_dice(int count) {
    int sum = 0;
    for (int i = 0; i < count; i++) {
        int roll = (rand() % 6) + 1;
        sum += roll;
        display_dice_roll(roll);
    }
    return sum;
}

void display_dice_roll(int roll) {
    printf("%d ", roll);
}

// Combat mechanics
int to_hit_roll(int combat_value, int* critical, int* roll_result) {
    int needed = 7 - combat_value;
    printf("To hit (need %d+): ", needed);
    int roll = roll_dice(1);
    *roll_result = roll;
    *critical = (roll == 6);
    printf("\n");
    return roll >= needed ? roll : 0;
}

int to_wound_roll(int attacker_strength, int defender_toughness, int* roll_result) {
    int needed = 4 - attacker_strength + defender_toughness;
    if (needed < 2) needed = 2;
    if (needed > 6) needed = 6;
    printf("To wound (need %d+): ", needed);
    int roll = roll_dice(1);
    *roll_result = roll;
    printf("\n");
    return roll >= needed ? roll : 0;
}

int perform_attack(Unit* attacker, Unit* defender, int* hits, int is_shooting) {
    *hits = 0;
    int hit_rolls[attacker->weapon->attacks];
    int wound_rolls[attacker->weapon->attacks];
    int critical_count = 0;

    printf("%s %s %s:\n", attacker->name, is_shooting ? "shoots" : "attacks", defender->name);
    for (int i = 0; i < attacker->weapon->attacks; i++) {
        int critical = 0;
        int hit = to_hit_roll(attacker->combat_value, &critical, &hit_rolls[i]);
        if (hit) {
            if (strcmp(attacker->weapon->special_rule, "death_wound") == 0 && critical) {
                defender->wounds -= (1 + attacker->weapon->bonus_dmg);
                printf("Death wound! %s loses 1 wound.\n", defender->name);
                continue;
            }
            if (critical) critical_count++;
            int wound_rolls_count = (critical && strcmp(attacker->weapon->special_rule, "critical_hit") == 0) ? 2 : 1;
            for (int j = 0; j < wound_rolls_count; j++) {
                if (to_wound_roll(attacker->strength + attacker->weapon->bonus_strength, defender->toughness, &wound_rolls[*hits])) {
                    (*hits)++;
                    if (critical && strcmp(attacker->weapon->special_rule, "lifesteal") == 0) {
                        attacker->wounds += 1;
                        printf("%s heals 1 wound via lifesteal!\n", attacker->name);
                    }
                }
            }
        }
    }

    // Log action for recap
    //if (*hits > 0 || critical_count > 0) {
        if (action_count < MAX_ACTIONS) {
            strcpy(actions[action_count].attacker_name, attacker->name);
            strcpy(actions[action_count].target_name, defender->name);
            actions[action_count].action_type = is_shooting ? 2 : 0; // Shooting or Combat
            actions[action_count].hits = *hits;
            actions[action_count].wounds = calculate_wounds(attacker, defender, *hits);
            actions[action_count].target_wounds = defender->wounds - actions[action_count].wounds;
            action_count++;
        }
    //}

    return *hits;
}

int calculate_wounds(Unit* a, Unit* d, int hits) {
    return (1 + a->weapon->bonus_dmg) * hits;
}

// Movement phase
void movement_phase(Unit* unit) {
    display_map();
    printf("Movement phase for %s (W: %d, Movement: %d) at %c%d\n", unit->name, unit->wounds, unit->movement, 'A' + unit->y, unit->x + 1);
    printf("Enter target position (e.g., A1) or 'S' to stay: ");
    char input[10];
    scanf("%s", input);

    if (input[0] == 'S' || input[0] == 's') {
        unit->has_moved = 1;
        return;
    }

    int new_y = toupper(input[0]) - 'A';
    int new_x = atoi(&input[1]) - 1;

    if (new_x >= 0 && new_x < MAP_SIZE && new_y >= 0 && new_y < MAP_SIZE) {
        int distance = abs(new_x - unit->x) + abs(new_y - unit->y);
        if (distance <= unit->movement && !is_tile_occupied(new_x, new_y, unit)) {
            // Check if unit is adjacent to any enemy before moving
            int was_adjacent = 0;
            for (int i = 0; i < num_units; i++) {
                if (units[i].wounds > 0 && units[i].team != unit->team && is_adjacent(unit, &units[i])) {
                    was_adjacent = 1;
                    break;
                }
            }

            // Move unit
            move_unit(unit, new_x, new_y);
            unit->has_moved = 1;

            // Check if unit is adjacent to any enemy after moving
            int is_adjacent_now = 0;
            for (int i = 0; i < num_units; i++) {
                if (units[i].wounds > 0 && units[i].team != unit->team && is_adjacent(unit, &units[i])) {
                    is_adjacent_now = 1;
                    break;
                }
            }

            // Set has_charged if unit wasn't adjacent before but is now
            if (!was_adjacent && is_adjacent_now) {
                unit->has_charged = 1;
                printf("%s has charged into combat!\n", unit->name);
            }
        } else {
            printf("Invalid move: %s\n", distance > unit->movement ? "Too far!" : "Tile occupied!");
        }
    } else {
        printf("Invalid position!\n");
    }
    display_map();
}

// Magic phase
void magic_phase(Unit* unit) {
    if (!unit->is_magic) return;
    printf("Magic phase for %s\n", unit->name);
    for (int i = 0; i < MAX_SPELLS; i++)
        printf("%d: %s (Cost: %d, Target: %s)\n", i + 1, spells[i].name, spells[i].cost, spells[i].target);
    printf("0: Skip\n");
    int choice;
    scanf("%d", &choice);
    if (choice == 0 || choice > MAX_SPELLS) return;

    Spell* spell = &spells[choice - 1];
    printf("Select target:\n");
    int valid_targets = 0;
    for (int i = 0; i < num_units; i++) {
        if (units[i].wounds > 0) {
            printf("%d: %s (Team: %s)\n", i, units[i].name, units[i].team == 0 ? "Player" : "Enemy");
            valid_targets++;
        }
    }
    if (valid_targets == 0) {
        printf("No valid targets!\n");
        return;
    }

    int target_idx;
    scanf("%d", &target_idx);
    if (target_idx < 0 || target_idx >= num_units || units[target_idx].wounds <= 0) {
        printf("Invalid target!\n");
        return;
    }
    if ((strcmp(spell->target, "ally") == 0 && units[target_idx].team != unit->team) ||
        (strcmp(spell->target, "enemy") == 0 && units[target_idx].team == unit->team)) {
        printf("Invalid target team!\n");
        return;
    }

    int roll = roll_dice(2);
    printf("Casting %s: Rolled %d (Need %d)\n", spell->name, roll, spell->cost);
    if (roll >= spell->cost) {
        apply_spell_effect(&units[target_idx], spell);
    } else {
        printf("Spell failed!\n");
    }
        // Log magic action for recap
        if (action_count < MAX_ACTIONS) {
            strcpy(actions[action_count].attacker_name, unit->name);
            strcpy(actions[action_count].target_name, units[target_idx].name);
            strcpy(actions[action_count].spell_name, spell->name);
            actions[action_count].action_type = (roll >= spell->cost) ? 1 : 3; // Magic
            actions[action_count].roll = roll;
            actions[action_count].roll_needed = spell->cost;
            action_count++;
        }
}

// Shooting phase
void shooting_phase(Unit* unit) {
    if (unit->weapon->range <= 1) return;
    printf("Shooting phase for %s\n", unit->name);
    printf("Select target:\n");
    int valid_targets = 0;
    for (int i = 0; i < num_units; i++) {
        if (units[i].wounds > 0 && units[i].team != unit->team) {
            printf("%d: %s (Team: %s)\n", i, units[i].name, units[i].team == 0 ? "Player" : "Enemy");
            valid_targets++;
        }
    }
    if (valid_targets == 0) {
        printf("No valid targets!\n");
        return;
    }

    int target_idx;
    scanf("%d", &target_idx);
    if (target_idx < 0 || target_idx >= num_units || units[target_idx].wounds <= 0 || units[target_idx].team == unit->team) {
        printf("Invalid target!\n");
        return;
    }

    int hits;
    perform_attack(unit, &units[target_idx], &hits, 1); // is_shooting = 1
    int wounds = calculate_wounds(unit, &units[target_idx], hits);
    units[target_idx].wounds -= wounds;
    printf("%s shoots %s, deals %d wounds\n", unit->name, units[target_idx].name, wounds);
}

// AI shooting phase
void ai_shooting_phase(Unit* unit) {
    if (unit->weapon->range <= 1 || unit->has_run) return;

    Unit* target = find_closest_enemy(unit);
    if (!target) return;

    int hits;
    perform_attack(unit, target, &hits, 1); // is_shooting = 1
    int wounds = calculate_wounds(unit, target, hits);
    target->wounds -= wounds;
    printf("%s shoots %s, deals %d wounds\n", unit->name, target->name, wounds);
}

// Combat phase
void combat_phase() {
    // First pass: process charging units
    for (int i = 0; i < num_units; i++) {
        if (units[i].wounds <= 0 || !units[i].has_charged) continue;
        for (int j = 0; j < num_units; j++) {
            if (units[j].wounds <= 0 || units[j].team == units[i].team) continue;
            if (is_adjacent(&units[i], &units[j])) {
                printf("Combat between %s (charger) and %s\n", units[i].name, units[j].name);
                int attacker_hits, defender_hits;

                // Attacker (charger) goes first
                perform_attack(&units[i], &units[j], &attacker_hits, 0); // is_shooting = 0

                // Defender retaliates if alive
                if (units[j].wounds > 0) {
                    perform_attack(&units[j], &units[i], &defender_hits, 0); // is_shooting = 0
                } else {
                    defender_hits = 0;
                }

                // Resolve combat
                int attacker_dmg = calculate_wounds(&units[i], &units[j], attacker_hits);
                int defender_dmg = calculate_wounds(&units[j], &units[i], defender_hits);
                units[j].wounds -= attacker_dmg;
                units[i].wounds -= defender_dmg;
                printf("%s deals %d damage (%d), %s deals %d damage (%d)\n", units[i].name, attacker_dmg, units[j].wounds, units[j].name, defender_dmg, units[i].wounds);

                // Move charger back if defender survives
                if (units[j].wounds > 0) {
                    int dx = units[i].x > units[j].x ? 1 : (units[i].x < units[j].x ? -1 : 0);
                    int dy = units[i].y > units[j].y ? 1 : (units[i].y < units[j].y ? -1 : 0);
                    if (!is_tile_occupied(units[i].x + dx, units[i].y + dy, &units[i]))
                        move_unit(&units[i], units[i].x + dx, units[i].y + dy);
                }
            }
        }
    }

    // Second pass: process non-charging units
    for (int i = 0; i < num_units; i++) {
        if (units[i].wounds <= 0 || units[i].has_charged) continue;
        for (int j = 0; j < num_units; j++) {
            if (units[j].wounds <= 0 || units[j].team == units[i].team) continue;
            if (is_adjacent(&units[i], &units[j])) {
                printf("Combat between %s and %s\n", units[i].name, units[j].name);
                int attacker_hits, defender_hits;

                // Attacker goes first
                perform_attack(&units[i], &units[j], &attacker_hits, 0); // is_shooting = 0

                // Defender retaliates if alive
                if (units[j].wounds > 0) {
                    perform_attack(&units[j], &units[i], &defender_hits, 0); // is_shooting = 0
                } else {
                    defender_hits = 0;
                }

                // Resolve combat
                int attacker_dmg = calculate_wounds(&units[i], &units[j], attacker_hits);
                int defender_dmg = calculate_wounds(&units[j], &units[i], defender_hits);
                units[j].wounds -= attacker_dmg;
                units[i].wounds -= defender_dmg;
                printf("%s deals %d damage (%d), %s deals %d damage (%d)\n", units[i].name, attacker_dmg, units[j].wounds, units[j].name, defender_dmg, units[i].wounds);

                // Move attacker back if defender survives
                if (units[j].wounds > 0) {
                    int dx = units[i].x > units[j].x ? 1 : (units[i].x < units[j].x ? -1 : 0);
                    int dy = units[i].y > units[j].y ? 1 : (units[i].y < units[j].y ? -1 : 0);
                    if (!is_tile_occupied(units[i].x + dx, units[i].y + dy, &units[i]))
                        move_unit(&units[i], units[i].x + dx, units[i].y + dy);
                }
            }
        }
    }
}

// Apply spell effects
void apply_spell_effect(Unit* target, Spell* spell) {
    printf("%s cast on %s\n", spell->name, target->name);
    if (strcmp(spell->effect, "+1 toughness") == 0)
        target->toughness += 1;
    else if (strcmp(spell->effect, "-1 to hit") == 0)
        target->combat_value = (target->combat_value > 1 && target->combat_value < 6) ? target->combat_value - 1 : target->combat_value;
    else if (strcmp(spell->effect, "+1 strength") == 0)
        target->strength += 1;
    else if (strcmp(spell->effect, "1D6 hits + move") == 0) {
        int hits = roll_dice(1);
        target->wounds -= hits;
        int dir = roll_dice(1);
        int dx = 0, dy = 0;
        if (dir <= 3) dx = -3; // back
        else if (dir == 4) dy = -3; // left
        else if (dir == 5) dy = 3; // right
        else dx = 3; // forward
        if (!is_tile_occupied(target->x + dx, target->y + dy, target))
            move_unit(target, target->x + dx, target->y + dy);
    }
}

// Check if units are adjacent
int is_adjacent(Unit* unit1, Unit* unit2) {
    return abs(unit1->x - unit2->x) + abs(unit1->y - unit2->y) == 1;
}

// Check if tile is occupied
int is_tile_occupied(int x, int y, Unit* moving_unit) {
    if (x < 0 || x >= MAP_SIZE || y < 0 || y >= MAP_SIZE) return 1; // Out of bounds
    for (int i = 0; i < num_units; i++) {
        if (&units[i] != moving_unit && units[i].wounds > 0 && units[i].x == x && units[i].y == y)
            return 1;
    }
    return 0;
}

// Move unit on map
void move_unit(Unit* unit, int new_x, int new_y) {
    if (new_x >= 0 && new_x < MAP_SIZE && new_y >= 0 && new_y < MAP_SIZE && !is_tile_occupied(new_x, new_y, unit)) {
        unit->x = new_x;
        unit->y = new_y;
    }
}

// Find closest enemy unit
Unit* find_closest_enemy(Unit* enemy) {
    Unit* target = NULL;
    int min_distance = MAP_SIZE * 2;
    for (int i = 0; i < num_units; i++) {
        if (units[i].wounds > 0 && units[i].team != enemy->team) {
            int distance = abs(enemy->x - units[i].x) + abs(enemy->y - units[i].y);
            if (distance < min_distance) {
                min_distance = distance;
                target = &units[i];
            }
        }
    }
    return target;
}

// Simple AI for enemy turn
void enemy_turn() {
    for (int i = 0; i < num_units; i++) {
        Unit* enemy = &units[i];
        if (enemy->wounds <= 0 || enemy->team != 1) continue;
        enemy->has_moved = enemy->has_run = enemy->has_charged = 0;

        // Move towards nearest player unit
        Unit* target = find_closest_enemy(enemy);
        if (!target) continue;

        // Check if adjacent to any enemy before moving
        int was_adjacent = 0;
        for (int j = 0; j < num_units; j++) {
            if (units[j].wounds > 0 && units[j].team != enemy->team && is_adjacent(enemy, &units[j])) {
                was_adjacent = 1;
                break;
            }
        }

        // Calculate direction and distance
        int dx = target->x - enemy->x;
        int dy = target->y - enemy->y;
        int new_x = enemy->x, new_y = enemy->y;
        int moves_left = enemy->movement;

        // Move as close as possible while respecting movement limit and collision
        while (moves_left > 0 && (dx != 0 || dy != 0)) {
            int step_x = 0, step_y = 0;
            if (abs(dx) > abs(dy)) {
                step_x = dx > 0 ? 1 : -1;
            } else if (dy != 0) {
                step_y = dy > 0 ? 1 : -1;
            } else {
                step_x = dx > 0 ? 1 : -1;
            }

            int next_x = new_x + step_x;
            int next_y = new_y + step_y;
            if (!is_tile_occupied(next_x, next_y, enemy)) {
                new_x = next_x;
                new_y = next_y;
                dx -= step_x;
                dy -= step_y;
                moves_left--;
            } else {
                // Try alternative direction if blocked
                if (step_x != 0) {
                    step_x = 0;
                    step_y = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
                } else {
                    step_y = 0;
                    step_x = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
                }
                next_x = new_x + step_x;
                next_y = new_y + step_y;
                if (!is_tile_occupied(next_x, next_y, enemy)) {
                    new_x = next_x;
                    new_y = next_y;
                    dx -= step_x;
                    dy -= step_y;
                    moves_left--;
                } else {
                    break; // No valid move available
                }
            }
        }

        // Move unit
        move_unit(enemy, new_x, new_y);
        enemy->has_moved = 1;

        // Check if adjacent to any enemy after moving
        int is_adjacent_now = 0;
        for (int j = 0; j < num_units; j++) {
            if (units[j].wounds > 0 && units[j].team != enemy->team && is_adjacent(enemy, &units[j])) {
                is_adjacent_now = 1;
                break;
            }
        }

        // Set has_charged if unit wasn't adjacent before but is now
        if (!was_adjacent && is_adjacent_now) {
            enemy->has_charged = 1;
            printf("%s has charged into combat!\n", enemy->name);
        }

        display_map();

        // Shooting phase
        ai_shooting_phase(enemy);
    }
}

// Check if game is over
int is_game_over() {
    int player_alive = 0, enemy_alive = 0;
    for (int i = 0; i < num_units; i++) {
        if (units[i].wounds > 0) {
            if (units[i].team == 0) player_alive = 1;
            else enemy_alive = 1;
        }
    }
    return !(player_alive && enemy_alive);
}

// Turn recap
void turn_recap() {
    printf("\nTurn %d Recap:\n", current_turn / 2 + 1);
    if (action_count == 0) {
        printf("No actions occurred this turn.\n");
        return;
    }
    for (int i = 0; i < action_count; i++) {
        if (actions[i].action_type == 0) { // Combat
            printf("%s attacked %s, %d hits, %d wounds, %s has %d wounds remaining.\n",
                   actions[i].attacker_name, actions[i].target_name, actions[i].hits,
                   actions[i].wounds, actions[i].target_name, actions[i].target_wounds);
        } else if (actions[i].action_type == 1) { // Magic
            printf("%s successfully (%d / %d) casted %s on %s.\n",
                   actions[i].attacker_name, actions[i].roll, actions[i].roll_needed,
                   actions[i].spell_name, actions[i].target_name);
        } else if (actions[i].action_type == 2) { // Shooting
            printf("%s shot %s, %d hits, %d wounds, %s has %d wounds remaining.\n",
                   actions[i].attacker_name, actions[i].target_name, actions[i].hits,
                   actions[i].wounds, actions[i].target_name, actions[i].target_wounds);
        } else if (actions[i].action_type == 3) { // Failed Magic
            printf("%s unsuccessfully (%d / %d) casted %s on %s.\n",
                   actions[i].attacker_name, actions[i].roll, actions[i].roll_needed,
                   actions[i].spell_name, actions[i].target_name);
        }
    }
}
