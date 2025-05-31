#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#endif

// Structs and Enum
#define MAX_INVENTORY_SIZE 100
#define NUM_TRIGGERS 4
#define MAX_LEVEL 100

typedef enum {
    isAlive = 0,
    hasAxe = 1,
    hasKey1 = 2,
    cyclopsKilled = 3
} triggerNums;

typedef enum {
    slime = 0,
    goblin = 1,
    cyclops = 2,
    randomEnemy = -1
}enemyNum;

typedef enum {
    consumable,
    equipment,
    keyItem,
    ingredient
}itemType;

typedef enum {
    axe = 0
} equipmentNum;

typedef enum {
    key1 = 0
} keyItemNum;

typedef struct {
    char* name;
    itemType type;
    union 
    {
        struct {int amount; char* effect; } consumable;
        struct {int lvl; int durability; char* type; int hp; int atk; int def; int acc; int agility; bool isEquipped; } equipment;
        struct { } keyItem;
        struct {int amount; } ingredient;
    }data;
    
} item;

typedef struct {
    int capacity;
    int currentSize;
    item* items[MAX_INVENTORY_SIZE];
} inventory;

typedef struct {
    bool isPoisoned;
    bool isParalysed;
    bool isFrozen;
    bool isBurning;
}status;

typedef struct {
    int hp;
    int def;
    int atk;
    int acc;
    int agility;
    status status;
} stats;

typedef struct scene scene;
typedef struct {
    inventory* inv;
    bool gameTriggers[NUM_TRIGGERS];
    int lvl;
    int xp;
    stats stat;
    stats baseStats;
} player;

typedef struct scene {
    char* description;
    bool* choiceConditions;
    int* choiceIds;
    scene** nextScenePerChoice;
    int numChoices;
    int sceneNo;
    char* choices[];
} scene;

typedef struct {
    char* name;
    int dmg;
    int acc;
    int lvlReq;
    char* debuff;
} move;

typedef struct {
    char* name;
    int xpSeed;
    move moveset[4];
    int lvl;
    char* type;
    stats stat;
} enemy;

item allEquipment[] = {
    {"Axe", equipment, .data.equipment = {1, 100, "weapon", 0, 10, 0, 100, 0, false}},
};
item allKeyItem[] = {
    {"Key1", keyItem, .data.keyItem = {}},
};

enemy allEnemy[] = {
    {
        "slime", 50,
        {{"blob attack", 20, 100, 1, "NULL"}, {"electrify", 10, 70, 1, "prs"}, {"acid throw", 10, 70, 1, "psn"}, {"slimy touch", 10, 70, 0, "brn"}},
        1,
        "normal",
        {25, 15, 20, 100, 20, (status){false, false, false, false}}
    },
    {
        "goblin", 70,
        {{"bite", 20, 100, 1, "NULL"}, {"spear jab", 30, 50, 1, "NULL"}, {"enrage", 0, 100, 1, "atkBuff"}, {"posion arrow", 20, 80, 0, "psn"}},
        1,
        "normal",
        {15, 15, 25, 100, 20, (status){false, false, false, false}}
    },
    {
        "cyclops", 90,
        {{"club attack", 100, 10, 1, "NULL"}, {"body slam", 50, 30, 1, "NULL"}, {"enrage", 0, 100, 1, "atkBuff"}, {"laser eye", 0, 100, 0, "brn"}},
        1,
        "dark",
        {150, 15, 5, 100, 10, (status){false, false, false, false}}
    }
};

// Function declarations
player* createPlayer(int inventoryCapacity);
inventory* createInventory(int capacity);
void addItemToInventory(item* item, inventory* inventory);
void removeItemFromInventory(char* item, inventory* inventory);
scene* createScene(int numChoices, char* description, char** choices, bool* choiceConditions, int* choiceIds, scene** nextScenePerChoice, int sceneNo);
scene* processChoice(scene* currentScene, player* p, int choiceIndex);
void displayScene(scene* currentScene, player* p);
void updateSceneConditions(scene* currentScene, player* p);
void freeScene(scene* s);
void freePlayer(player* p);
void freeEnemy(enemy* e);
enemy* createEnemy(player* p, int enemyIndex);
void init_combat(player* p, enemy* e);
void type(const char* format, ...);
void lvlUp(player* p);

// Function implementations

void lvlUp(player* p) {
    type("You leveled up!\nYou are now level %d!\n", p->lvl + 1);
    p->lvl += 1;
    p->xp -= 500;
    p->baseStats.hp += 50;
    p->baseStats.agility += 3;
    p->baseStats.def += 5;
    p->baseStats.atk += 5;
    p->stat.hp = p->baseStats.hp;
    p->stat.agility = p->baseStats.agility;
    p->stat.def = p->baseStats.def;
    p->stat.atk = p->baseStats.atk;
    type("HP: %d\n", p->baseStats.hp);
    type("Atk: %d\n", p->baseStats.atk);
    type("Def: %d\n", p->baseStats.def);
    type("Agility: %d\n", p->baseStats.agility);
}

void type(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[8096];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

#ifndef _WIN32
    // Set up non-blocking input for Unix-like systems
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
#endif

    for (int i = 0; buffer[i] != '\0'; i++) {
        putchar(buffer[i]);
        fflush(stdout);

        // Check for Enter key
#ifdef _WIN32
        if (_kbhit()) {
            int ch = _getch();
            if (ch == '\r') { // Enter key on Windows
                printf("%s", buffer + i + 1); // Print remaining buffer
                break;
            }
        }
#else
        struct pollfd fds[1];
        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;
        int ret = poll(fds, 1, 0); // Non-blocking check
        if (ret > 0 && fds[0].revents & POLLIN) {
            int ch = getchar();
            if (ch == '\n') { // Enter key on Unix
                printf("%s", buffer + i + 1); // Print remaining buffer
                break;
            }
        }
#endif

#ifdef _WIN32
        Sleep(25); // 30ms delay
#else
        usleep(25000); // 30ms delay
#endif
    }

#ifndef _WIN32
    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
}

void init_combat(player* p, enemy* e) {
    if (!e) return;
    type("A level %d %s appears in front of you.\n", e->lvl, e->name);
    int choice;
    int damage;
    int xp;
    int moveIndex;
    int brnCounter = 0;
    int psnCounter = 0;
    int prsCounter = 0;
    bool isHit;
    while (p->stat.hp > 0 && e->stat.hp > 0) {
        type("Your HP: %d\n", p->stat.hp);
        type("%s's HP: %d\n", e->name, e->stat.hp);
        type("What do you do?\n");
        type("1. Attack\n2. Run Away\n3. Pray\n");
        if (scanf("%d", &choice) != 1) {
            type("Invalid input. Please enter a number.\n");
            while (getchar() != '\n');
            continue;
        }
        if (choice == 2) {
            if (p->stat.agility > e->stat.agility) {
                type("Ran away successfully!\n");
                break;
            } else if (p->stat.agility < e->stat.agility) {
                type("%s is too fast. Failed to run away.\n", e->name);
            } else {
                if (rand() % 2 == 0) {
                    type("Ran away successfully!\n");
                    break;
                } else {
                    type("%s is too fast. Failed to run away.\n", e->name);
                }
            }
        } else if (choice == 1) {
            if (p->stat.status.isParalysed && rand() % 2 == 0) type("You are paralyzed you can't move.\n");
            else {
                type("You attacked the %s!\n", e->name);
                damage = p->stat.atk - e->stat.def + (int)ceil(((rand() % 31) / 100.0) * p->stat.atk);
                if (damage < 0) damage = 0;
                e->stat.hp -= damage;
                type("You did %d damage!\n", damage);
            }
        }

else if (choice == 3) { // Pray
    int prayerOutcome = rand() % 2; // Randomly picks 0 (Blessing) or 1 (Curse)
    
    switch (prayerOutcome) {
        case 0: { // Blessings (50% chance)
            int grace = rand() % 100;
            
            if (grace <= 40) { // 40% chance: Cure all status effects
                p->stat.status.isBurning = false;
                p->stat.status.isPoisoned = false;
                p->stat.status.isParalysed = false;
                type("The Divine God cures your ailments!\n");
            } 
            else if (grace <= 60) { // 20% chance: Smite enemy (25% of their HP)
                int smiteDmg = e->stat.hp / 2;  //yes much needed actually
                e->stat.hp -= smiteDmg;
                type("The Divine God smites your foe for %d damage!\n", smiteDmg);
            } 
            else if (grace <= 80) { // 20% chance: Heal player (25% of max HP)
                int healAmount = p->baseStats.hp / 4;
                p->stat.hp += healAmount;
                if (p->stat.hp > p->baseStats.hp) p->stat.hp = p->baseStats.hp;
                type("The Divine God heals you for %d HP!\n", healAmount);
            } 
            else { // 20% chance: Temporary ATK boost (+10)
                p->stat.atk += 10;
                type("The Divine God blesses you with holy strength! (+10 ATK)\n");
            }
            break;
        }
        
        case 1: { // Curse (50% chance)
            int jinx = rand() % 100;
            
            if (jinx <= 40) { // 40% chance: Burn player
                p->stat.status.isBurning = true;
                type("The Divine God's holy flames are purifying you ! (You are now Burning)\n");
            } 
            else if (jinx <= 60) { // 20% chance: Lose 25% HP
                int punishDmg = p->baseStats.hp / 4;
                p->stat.hp -= punishDmg;
                type("The Divine God punishes you for %d damage!\n", punishDmg);
            } 
            else if (jinx <= 80) { // 20% chance: Paralysis
                p->stat.status.isParalysed = true;
                type("The Divine God demands repentance!\n");
            } 
            else { // 20% chance: Both lose 25% HP
                int mutualDmg = p->baseStats.hp / 4;
                p->stat.hp -= mutualDmg;
                e->stat.hp -= mutualDmg;
                type("The Divine God judges both of you! (-%d HP each)\n", mutualDmg);
            }
            break;
        }
    }
} 
        else {
            type("Invalid choice.\n");
            continue;
        }
        if (e->stat.hp > 0) {
            moveIndex = rand()%4;
            isHit = (rand()%100 < e->moveset[moveIndex].acc)? true : false;
            type("%s used %s\n", e->name, e->moveset[moveIndex].name);
            damage = e->moveset[moveIndex].dmg * (e->stat.atk)/(p->stat.def) + (int)ceil(((rand() % 31) / 100.0) * e->moveset[moveIndex].dmg);
            if (damage < 0) damage = 0;
            if(isHit) {
                if (!strcmp(e->moveset[moveIndex].debuff, "NULL")) { p->stat.hp -= damage; type("You took %d damage!\n", damage); }
                else if (!strcmp(e->moveset[moveIndex].debuff, "brn")) { p->stat.status.isBurning = true; brnCounter = 0; p->stat.hp -= damage; type("You took %d damage!\n", damage); }
                else if (!strcmp(e->moveset[moveIndex].debuff, "psn")) { p->stat.status.isPoisoned = true; psnCounter = 0; p->stat.hp -= damage; type("You took %d damage!\n", damage); }
                else if (!strcmp(e->moveset[moveIndex].debuff, "prs")) { p->stat.status.isParalysed = true; prsCounter = 0; p->stat.hp -= damage; type("You took %d damage!\n", damage); }
                else if (!strcmp(e->moveset[moveIndex].debuff, "atkBuff")) { e->stat.atk += 10; type("%s's attack went up!\n", e->name); }
            }
            else {
                type("%s missed!\n", e->name);
            }
        }
        if(p->stat.status.isBurning && brnCounter < 3) {
            type("You are burning!\n");
            damage = (int)ceil(p->baseStats.hp / 16);
            p->stat.hp-= damage;
            type("You took %d damage!\n", damage);
            brnCounter += 1;
        }
        if(p->stat.status.isPoisoned && psnCounter < 3) {
            type("You are poisoned!\n");
            damage = (int)ceil(p->baseStats.hp / 16);
            p->stat.hp-= damage;
            type("You took %d damage!\n", damage);
            psnCounter += 1;
        }
        if(p->stat.status.isParalysed && prsCounter < 3) {
            type("You are paralysed!\n");
            psnCounter += 1;
        }

        if(brnCounter == 3) p->stat.status.isBurning = false; brnCounter = 0;
        if(psnCounter == 3) p->stat.status.isPoisoned = false; psnCounter = 0;
        if(prsCounter == 3) p->stat.status.isParalysed = false; prsCounter = 0;
    }
    if (p->stat.hp <= 0) {
        type("You died!\n");
        p->gameTriggers[isAlive] = false;
    } else if (e->stat.hp <= 0) {
        type("You killed the %s!\n", e->name);
        xp = (e->xpSeed+e->xpSeed * sqrt(e->lvl / p->lvl)) * 2 * (1-(p->lvl / MAX_LEVEL));
        p->xp += xp;
        type("You gained %d XP!\n", xp);
        p->stat.status.isPoisoned = false;
        p->stat.status.isParalysed = false;
        p->stat.status.isBurning = false;
        p->stat.status.isFrozen = false;
        if(p->xp >= 500) lvlUp(p);
    }
    freeEnemy(e);
}

enemy* createEnemy(player* p, int enemyIndex) {
    if (enemyIndex == randomEnemy) enemyIndex = rand() % 2;
    enemy* newEnemy = malloc(sizeof(enemy));
    if (!newEnemy) return NULL;
    *newEnemy = allEnemy[enemyIndex];
    newEnemy->name = strdup(newEnemy->name);
    newEnemy->type = strdup(newEnemy->type);
    newEnemy->lvl = p->lvl + (rand() % 5) - 2;
    if (newEnemy->lvl < 1) newEnemy->lvl = 1;
    newEnemy->stat.hp += newEnemy->lvl * 15;
    newEnemy->stat.def += newEnemy->lvl * 2;
    newEnemy->stat.atk += newEnemy->lvl * 3;
    newEnemy->stat.agility += newEnemy->lvl * 3;
    newEnemy->stat.status = (status){false, false, false, false};
    for (int i = 0; i < 4; i++) {
        if (newEnemy->moveset[i].name) {
            newEnemy->moveset[i].name = strdup(newEnemy->moveset[i].name);
            newEnemy->moveset[i].debuff = strdup(newEnemy->moveset[i].debuff);
        }
    }
    return newEnemy;
}

player* createPlayer(int inventoryCapacity) {
    player* p = malloc(sizeof(player));
    if (!p) return NULL;
    p->inv = createInventory(inventoryCapacity);
    if (!p->inv) {
        free(p);
        return NULL;
    }
    for (int i = 0; i < NUM_TRIGGERS; i++) {
        p->gameTriggers[i] = (i == isAlive) ? true : false;
    }
    p->lvl = 1;
    p->xp = 0;
    p->baseStats = (stats){200, 20, 30, 100, 20, (status){false, false, false, false}};
    p->stat = (stats){200, 20, 30, 100, 20, (status){false, false, false, false}};
    return p;
}

inventory* createInventory(int capacity) {
    if (capacity > MAX_INVENTORY_SIZE) {
        type("Capacity exceeds maximum inventory size (%d)\n", MAX_INVENTORY_SIZE);
        return NULL;
    }
    inventory* inv = malloc(sizeof(inventory));
    if (!inv) return NULL;
    inv->capacity = capacity;
    inv->currentSize = 0;
    for (int i = 0; i < MAX_INVENTORY_SIZE; i++) {
        inv->items[i] = NULL;
    }
    return inv;
}

void addItemToInventory(item* newItem, inventory* inventory) {
    if (!inventory || !newItem) return;
    if (inventory->currentSize >= inventory->capacity) {
        type("Inventory is full\n");
        return;
    }
    item* item = malloc(sizeof(item));
    if (!item) {
        type("Failed to add item: memory allocation error\n");
        return;
    }
    item->type = newItem->type;
    item->name = strdup(newItem->name);

    switch (item->type) {
        case consumable:
            item->data.consumable.amount = newItem->data.consumable.amount;
            item->data.consumable.effect = strdup(newItem->data.consumable.effect);
            break;
        case equipment:
            item->data.equipment.lvl = newItem->data.equipment.lvl;
            item->data.equipment.durability = newItem->data.equipment.durability;
            item->data.equipment.type = strdup(newItem->data.equipment.type);
            item->data.equipment.hp = newItem->data.equipment.hp;
            item->data.equipment.atk = newItem->data.equipment.atk;
            item->data.equipment.def = newItem->data.equipment.def;
            item->data.equipment.acc = newItem->data.equipment.acc;
            item->data.equipment.agility = newItem->data.equipment.agility;
            item->data.equipment.isEquipped = newItem->data.equipment.isEquipped;
            break;
        case keyItem:
            // No specific data for key items
            break;
        case ingredient:
            item->data.ingredient.amount = newItem->data.ingredient.amount;
            break;
    }

    inventory->items[inventory->currentSize] = item;
    inventory->currentSize++;
}
void removeItemFromInventory(char* itemName, inventory* inventory) {
    if (!inventory || !itemName) return;
    if (inventory->currentSize == 0) {
        type("Inventory is Empty\n");
        return;
    }
    for (int i = 0; i < inventory->currentSize; i++) {
        if (inventory->items[i] && strcmp(inventory->items[i]->name, itemName) == 0) {
            // Free type-specific data
            switch (inventory->items[i]->type) {
                case consumable:
                    free(inventory->items[i]->data.consumable.effect);
                    break;
                case equipment:
                    free(inventory->items[i]->data.equipment.type);
                    break;
                case keyItem:
                case ingredient:
                    // No additional fields to free
                    break;
            }
            free(inventory->items[i]->name);
            free(inventory->items[i]);
            // Shift items
            for (int j = i; j < inventory->currentSize - 1; j++) {
                inventory->items[j] = inventory->items[j + 1];
            }
            inventory->items[inventory->currentSize - 1] = NULL;
            inventory->currentSize--;
            return;
        }
    }
    type("Item not in inventory\n");
}
scene* createScene(int numChoices, char* description, char** choices, bool* choiceConditions, int* choiceIds, scene** nextScenePerChoice, int sceneNo) {
    scene* newScene = malloc(sizeof(scene) + sizeof(char*) * numChoices);
    if (!newScene) return NULL;
    newScene->choiceConditions = malloc(sizeof(bool) * numChoices);
    if (!newScene->choiceConditions) {
        free(newScene);
        return NULL;
    }
    newScene->choiceIds = malloc(sizeof(int) * numChoices);
    if (!newScene->choiceIds) {
        free(newScene->choiceConditions);
        free(newScene);
        return NULL;
    }
    newScene->nextScenePerChoice = malloc(sizeof(scene*) * numChoices);
    if (!newScene->nextScenePerChoice) {
        free(newScene->choiceIds);
        free(newScene->choiceConditions);
        free(newScene);
        return NULL;
    }
    newScene->description = description ? strdup(description) : NULL;
    newScene->numChoices = numChoices;
    newScene->sceneNo = sceneNo;
    for (int i = 0; i < numChoices; i++) {
        newScene->choices[i] = choices && choices[i] ? strdup(choices[i]) : NULL;
        newScene->choiceConditions[i] = choiceConditions ? choiceConditions[i] : false;
        newScene->choiceIds[i] = choiceIds ? choiceIds[i] : (sceneNo * 100 + (i + 1));
        newScene->nextScenePerChoice[i] = nextScenePerChoice ? nextScenePerChoice[i] : NULL;
    }
    return newScene;
}

scene* processChoice(scene* currentScene, player* p, int choiceIndex) {
    if (!currentScene || !p || choiceIndex < 0 || choiceIndex >= currentScene->numChoices) {
        type("Invalid choice index!\n");
        return currentScene;
    }
    if (!currentScene->choiceConditions[choiceIndex]) {
        type("Choice not available!\n");
        return currentScene;
    }
    int choiceId = currentScene->choiceIds[choiceIndex];
    int choiceNumber = choiceId % 100;
    switch (choiceId) {
        case 101: // Scene 1: Pick up axe
            if (!p->gameTriggers[hasAxe]) {
                addItemToInventory(&allEquipment[axe], p->inv);
                p->gameTriggers[hasAxe] = true;
                type("Picked up Axe!\n");
            }
            return currentScene->nextScenePerChoice[choiceNumber - 1];
        case 102: // Scene 1: Pick up key1
            if (!p->gameTriggers[hasKey1]) {
                addItemToInventory(&allKeyItem[key1], p->inv);
                p->gameTriggers[hasKey1] = true;
                type("Picked up Key1!\n");
            }
            return currentScene->nextScenePerChoice[choiceNumber - 1];
        case 103: // Scene 1: Unlock gate
            if (p->gameTriggers[hasKey1]) {
                type("Gate unlocked with Key1!\n");
                return currentScene->nextScenePerChoice[choiceNumber - 1];
            } else {
                type("Need Key1 to unlock gate.\n");
                return currentScene;
            }
        case 201: // Scene 2: Chop door
            if (p->gameTriggers[hasAxe]) {
                type("Door chopped with Axe!\n");
                return currentScene->nextScenePerChoice[choiceNumber - 1];
            } else {
                type("Need Axe to chop door.\n");
                return currentScene;
            }
        case 202: // Scene 2: Go back
            type("You go back to the dark room.\n");
            return currentScene->nextScenePerChoice[choiceNumber - 1];
        case 203: // Scene 2: Fight slime
            init_combat(p, createEnemy(p ,randomEnemy));
            if (!p->gameTriggers[isAlive]) {
                type("Game Over.\n");
                return NULL;
            }
            return currentScene;
        case 301:  //Fight Cyclops
            init_combat(p, createEnemy(p, cyclops));
            if (!p->gameTriggers[isAlive]) {
                type("Game Over.\n");
                return NULL;
            }
            p->gameTriggers[cyclopsKilled] = true;
            return currentScene;
        case 302:
            return currentScene->nextScenePerChoice[choiceNumber - 1];
            break;
        case 303:
            return currentScene->nextScenePerChoice[choiceNumber - 1];
            break;
        default:
            type("Invalid choice ID.\n");
            return currentScene;
    }
}

void displayScene(scene* currentScene, player* p) {
    if (!currentScene || !p) return;
    char temp[8096] = {0};
    int len;
    snprintf(temp, sizeof(temp), "\n%s\nWhat do you do?\n", currentScene->description);
    len = strlen(temp);
    for (int i = 0; i < currentScene->numChoices; i++) {
        if (currentScene->choiceConditions[i]) {
            char choice[512];
            snprintf(choice, sizeof(choice), "%d. %s\n", i + 1, currentScene->choices[i]);
            strncat(temp, choice, sizeof(temp)- len -1);
            len += strlen(choice);
        }
    }
    type("%s", temp, NULL);
}

void updateSceneConditions(scene* currentScene, player* p) {
    if (!currentScene || !p) return;
    for (int i = 0; i < currentScene->numChoices; i++) {
        switch (currentScene->choiceIds[i]) {
            case 101: // Pick up axe
                currentScene->choiceConditions[i] = !p->gameTriggers[hasAxe];
                break;
            case 102: // Pick up key1
                currentScene->choiceConditions[i] = !p->gameTriggers[hasKey1];
                break;
            case 103: // Unlock gate
                currentScene->choiceConditions[i] = true;
                break;
            case 201: // Chop door
                currentScene->choiceConditions[i] = true;
                break;
            case 202: // Go back
                currentScene->choiceConditions[i] = true;
                break;
            case 203: // Fight slime
                currentScene->choiceConditions[i] = true;
                break;
            case 302: // Go back to previous room
                currentScene->choiceConditions[i] = p->gameTriggers[cyclopsKilled];
                break;
            case 303: // Game End
                currentScene->choiceConditions[i] = p->gameTriggers[cyclopsKilled];
                break;
            default:
                currentScene->choiceConditions[i] = true;
                break;
        }
    }
}

void freeScene(scene* s) {
    if (s) {
        for (int i = 0; i < s->numChoices; i++) {
            free(s->choices[i]);
        }
        free(s->description);
        free(s->choiceConditions);
        free(s->choiceIds);
        free(s->nextScenePerChoice);
        free(s);
    }
}

void freePlayer(player* p) {
    if (p) {
        if (p->inv) {
            for (int i = 0; i < p->inv->currentSize; i++) {
                if (p->inv->items[i]) {
                    switch (p->inv->items[i]->type) {
                        case consumable:
                            free(p->inv->items[i]->data.consumable.effect);
                            break;
                        case equipment:
                            free(p->inv->items[i]->data.equipment.type);
                            break;
                        case keyItem:
                        case ingredient:
                            // No additional fields to free
                            break;
                    }
                    free(p->inv->items[i]->name);
                    free(p->inv->items[i]);
                }
            }
            free(p->inv);
        }
        free(p);
    }
}

void freeEnemy(enemy* e) {
    if (e) {
        free(e->name);
        free(e->type);
        for (int i = 0; i < 4; i++) {
            free(e->moveset[i].name);
            free(e->moveset[i].debuff);
        }
        free(e);
    }
}

// Main function
int main() {
    srand(time(NULL));
    player* p = createPlayer(3);
    if (!p) return 1;

    const int numScenes = 3;
    scene* scenes[numScenes];
    scene** nextScenes[numScenes];

    char* choices1[] = {"Pick up axe", "Pick up key1", "Unlock gate", NULL};
    bool conditions1[] = {true, true, true};
    scene* nextScenes1[3] = {NULL, NULL, NULL};

    char* choices2[] = {"Chop door", "Go back to dark room", "Fight the enemy", NULL};
    bool conditions2[] = {true, true, true};
    scene* nextScenes2[3] = {NULL, NULL, NULL};
    
    char* choices3[] = {"Fight the cyclops", "Go back to previous room", "End Game",  NULL};
    bool conditions3[] = {true, true, true};
    scene* nextScenes3[3] = {NULL ,NULL, NULL};

    scenes[0] = createScene(3, "A dark room with an axe and a gate.", choices1, conditions1, NULL, nextScenes1, 1);
    scenes[1] = createScene(3, "A room with a wooden door and an enemy lurking.", choices2, conditions2, NULL, nextScenes2, 2);
    scenes[2] = createScene(3, "There is an angry Cyclops in the room", choices3, conditions3, NULL, nextScenes3, 3);
    for (int i = 0; i < numScenes; i++) {
        if (!scenes[i]) {
            for (int j = 0; j < i; j++) freeScene(scenes[j]);
            freePlayer(p);
            return 1;
        }
    }

    nextScenes[0] = nextScenes1;
    nextScenes[1] = nextScenes2;
    nextScenes[2] = nextScenes3;
    

        
    nextScenes1[0] = scenes[0];
    nextScenes1[1] = scenes[0];
    nextScenes1[2] = scenes[1];

        
    nextScenes2[0] = scenes[2];
    nextScenes2[1] = scenes[0];
    nextScenes2[2] = scenes[1];

    nextScenes3[0] = scenes[2];
    nextScenes3[1] = scenes[1];
    nextScenes3[2] = NULL;

    for (int i = 0; i < numScenes; i++) {
        for (int j = 0; j < scenes[i]->numChoices; j++) {
            scenes[i]->nextScenePerChoice[j] = nextScenes[i][j];
        }
    }

    scene* currentScene = scenes[0];
    int choice;

    while (currentScene && p->gameTriggers[isAlive]) {
        updateSceneConditions(currentScene, p);
        displayScene(currentScene, p);
        type("Enter choice (1-%d, 0 to quit): ", currentScene->numChoices);
        if (scanf("%d", &choice) != 1 || choice < 0 || choice > currentScene->numChoices) {
            type("Invalid input!\n");
            while (getchar() != '\n');
            continue;
        }
        if (choice == 0) break;
        currentScene = processChoice(currentScene, p, choice - 1);
    }

    if (!p->gameTriggers[isAlive]) {
        type("Game Over.\n");
    }

    for (int i = 0; i < numScenes; i++) {
        freeScene(scenes[i]);
    }
    freePlayer(p);
    return 0;
}