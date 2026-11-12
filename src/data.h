// Game data, ported 1:1 from src/60-data.js of the three.js original.
#pragma once
#include "mathx.h"
enum { WARDEN, REAVER, FLETCHER, LAMPLIGHTER, APOTHECARY, RIMEBINDER, NCLS };
struct Skill { const char* id; const char* name; float cd; const char* d; bool passive; };
struct ClassDef { const char* key; const char* name; const char* role; float hp, dmg, atk, range, speed, armor; bool ranged; float slot[2]; const char* blurb; Skill skills[3]; };
inline const ClassDef CL[NCLS] = {
 {"warden","WARDEN","TANK",270,15,1.3f,1.9f,3.3f,0.28f,false,{1.9f,0.2f},"Draws the gloam onto a scarred shield.",{{"bash","SHIELD BASH",7,"staggers a target",false},{"hold","HOLD THE LINE",16,"taunts, takes less harm",false},{"riposte","RIPOSTE",4,"counter-cut after a blow",true}}},
 {"reaver","REAVER","HEAVY MELEE",195,33,1.75f,2.2f,3.2f,0.12f,false,{1.1f,-1.5f},"A greatsword and the weight to use it.",{{"cleave","CLEAVE",6,"a wide arc",false},{"sunder","SUNDER",12,"shakes the ground in a line",false},{"wind","SECOND WIND",1,"hits harder when hurt",true}}},
 {"fletcher","FLETCHER","RANGED",125,21,1.35f,13,3.4f,0.05f,true,{-1.5f,1.2f},"A longbow as tall as the archer.",{{"aimed","AIMED SHOT",6.5f,"one heavy arrow",false},{"volley","VOLLEY",11,"arrows fall on a group",false},{"pin","PINNING SHOT",9,"slows a target",false}}},
 {"lamplighter","LAMPLIGHTER","FIRE CASTER",130,19,1.6f,11,3.2f,0.06f,true,{-1.7f,-1.1f},"Carries the last lit lantern.",{{"flare","FLARE",7,"a bolt that bursts",false},{"kindle","KINDLE",14,"warm light mends the party",false},{"cinder","CINDERFALL",16,"embers rain on an area",false}}},
 {"apothecary","APOTHECARY","SUPPORT",135,13,1.8f,9,3.2f,0.06f,true,{-2.6f,0.1f},"Tonics, firepots and quick hands.",{{"tonic","TONIC",5,"heals the most hurt",false},{"firepot","FIREPOT",9,"leaves a burning patch",false},{"quick","QUICKSILVER",20,"speeds the party",false}}},
 {"rimebinder","RIMEBINDER","FROST CASTER",118,18,1.7f,11,3.2f,0.05f,true,{-1.8f,1.5f},"Binds the cold of the moor to a pale shard.",{{"lance","FROST LANCE",6,"pierces a line",false},{"hoar","HOARFROST",12,"ground that slows",false},{"shatter","SHATTER",14,"chilled foes burst",false}}}};
enum { THORNLING, PEATWIGHT, GLOAMHOUND, GLOAMWISP, RUSTBOUND, CAIRNBACK, BARROWKING, MOSSJAW, HOLLOWSTAG, EMBERHOUND, THORNBOAR, ROOTCOLOSSUS, NMON };
enum Mat { MW, MP, MC, MM, MS };  // wood peat cloth metal stone
struct MonDef { const char* name; float hp, dmg, atk, range, speed, armor, xp, cost, wind; int from; bool flank, ranged, elite, boss; int mat; int spec; float radius, hpY, scale; };
// spec: 1 sweep, 2 slam, 3 charge
inline const MonDef MON[NMON] = {
 {"Thornling",34,5,1.15f,1.3f,3.4f,0,4,3,0.42f,1,false,false,false,false,MW,0,0.28f,1.05f,0.62f},
 {"Peatwight",115,13,2.0f,1.7f,1.5f,0.05f,10,7,0.5f,1,false,false,false,false,MP,0,0.42f,1.85f,1.04f},
 {"Gloamhound",72,10,1.3f,1.6f,4.8f,0,9,7,0.45f,2,true,false,false,false,MW,0,0.4f,1.15f,1.12f},
 {"Gloamwisp",50,11,2.4f,9,1.9f,0,9,7,0.5f,3,false,true,false,false,MC,0,0.35f,2.2f,1.0f},
 {"Rustbound",240,22,2.1f,2.1f,1.75f,0.3f,24,16,0.5f,3,false,false,true,false,MM,0,0.42f,2.1f,1.1f},
 {"Cairnback",430,34,2.9f,2.6f,1.3f,0.2f,40,26,0.52f,6,false,false,true,false,MS,0,0.75f,2.7f,1.5f},
 {"THE BARROW KING",1500,30,2.3f,3.6f,1.6f,0.25f,60,0,0.5f,0,false,false,false,true,MM,1,0.8f,3.6f,1.95f},
 {"OLD MOSSJAW",1900,34,2.9f,3.9f,1.2f,0.2f,60,0,0.5f,0,false,false,false,true,MS,2,1.0f,4.4f,2.35f},
 {"THE HOLLOW STAG",1350,27,1.9f,3.4f,2.4f,0.1f,60,0,0.5f,0,false,false,false,true,MW,3,1.0f,3.6f,2.2f},
 {"Emberhound",92,13,1.25f,5.5f,4.9f,0.05f,13,9,0.44f,2,true,true,false,false,MW,0,0.42f,1.15f,1.28f},
 {"Thornboar",190,20,1.75f,2.2f,4.3f,0.22f,18,12,0.50f,3,true,false,false,false,MP,3,0.55f,1.35f,1.34f},
 {"Root Colossus",520,38,2.8f,2.8f,1.35f,0.28f,46,28,0.55f,5,false,false,true,false,MW,2,0.85f,3.1f,1.65f}};
inline const int BOSS_ORDER[3] = {BARROWKING, MOSSJAW, HOLLOWSTAG};
inline const char* BOSS_TITLES[3][4] = {{"THE BARROW KING","THE KING UNDER RUST","THE CROWNLESS","LORD OF THE LONG DARK"},{"OLD MOSSJAW","THE WALKING CAIRN","GRANDFATHER STONE","THE TOR THAT WOKE"},{"THE HOLLOW STAG","THE ANTLERED GLOAM","WINTER'S HART","THE WHITE RUN"}};
struct Grade { const char* n; uint32_t c; float m; };
inline const Grade GRADES[4] = {{"WORN",0xa8a290,1},{"TEMPERED",0x7fb4d6,1.45f},{"GILDED",0xe0a33c,2.0f},{"RELIC",0xff7a3a,2.9f}};
inline const char* ITEM_WEAPON[NCLS] = {"Arming Sword","Greatsword","Longbow","Lantern Pole","Flask Belt","Shard Staff"};
inline const char* ITEM_ARMOUR[NCLS] = {"Hauberk","Mail Shirt","Padded Jack","Waxed Coat","Apron Coat","Winter Robe"};
inline const char* ITEM_TRINKET[7] = {"Wayfarer's Token","Iron Ring","Tin Whistle","Knucklebone","Sprig of Heather","Flint and Steel","Lead Charm"};
inline const char* RELIC_NAMES[12] = {"Wickfall","Gloamrend","Barrowkeeper","Ashen Vow","Moorwarden","Last Light","Peatborn","Stormcrow","Emberwake","Longnight","Cairnsong","Hearthward"};
enum { SP_HASTE, SP_CDR, SP_LEECH, SP_WARD, SP_SILVER, SP_CRIT, NSP };
inline const char* SP_TEXT[NSP] = {"+12% attack speed","skills return 12% sooner","mends 3% of damage dealt","takes 10% less harm","+20% silver found","+10% heavy blows"};
enum { C_W_HP, C_W_HOLD, C_W_BASH, C_W_RIP, C_R_CLEAVE, C_R_SUNDER, C_R_WIND, C_R_DMG, C_F_VOLLEY, C_F_AIMED, C_F_PIN, C_F_HASTE, C_L_FLARE, C_L_KINDLE, C_L_CINDER, C_L_DMG, C_A_TONIC, C_A_FIRE, C_A_QUICK, C_A_KIT, C_B_LANCE, C_B_HOAR, C_B_SHATTER, C_B_RIME, C_P_DMG, C_P_HP, C_P_LIGHT, C_P_HASTE, C_P_SILVER, C_P_FLARE, C_P_XP, NCARDS };
struct Card { int who; const char* name; const char* d; };  // who: class idx or -1 = party
inline const Card CARDS[NCARDS] = {
 {WARDEN,"OAKEN HEART","Warden: +25% life"},{WARDEN,"WALL OF IRON","Hold the Line lasts 2s longer and blocks more"},{WARDEN,"BROAD BOSS","Shield Bash strikes every foe in reach, +25% damage"},{WARDEN,"OLD SOLDIER","Riposte strikes more often and harder"},
 {REAVER,"WIDE ARC","Cleave: +30% damage and reach"},{REAVER,"EARTHSHAKER","Sunder returns 20% sooner and runs further"},{REAVER,"BLOOD UP","Second Wind wakes sooner and bites deeper"},{REAVER,"WHETTED EDGE","Reaver: +18% damage"},
 {FLETCHER,"FULL QUIVER","Volley: +3 arrows"},{FLETCHER,"HAWK EYE","Aimed Shot: +40% damage"},{FLETCHER,"BARBED HEADS","Pinned foes stay slowed longer and take +15% harm"},{FLETCHER,"STEADY HANDS","Fletcher: +15% attack speed"},
 {LAMPLIGHTER,"LINGERING FLAME","Flare leaves burning ground"},{LAMPLIGHTER,"WARM HEARTH","Kindle mends 30% more"},{LAMPLIGHTER,"EMBER STORM","Cinderfall: wider and 25% hotter"},{LAMPLIGHTER,"TRIMMED WICK","Lamplighter: +18% damage"},
 {APOTHECARY,"STRONG TONIC","Tonic mends 30% more and splashes nearby allies"},{APOTHECARY,"WILDFIRE","Firepot burns 2s longer and 25% hotter"},{APOTHECARY,"DOUBLE DOSE","Quicksilver lasts 2s longer"},{APOTHECARY,"FIELD KIT","Fallen heroes rise 25% sooner"},
 {RIMEBINDER,"SPLINTER","Frost Lance: +25% damage, chills longer"},{RIMEBINDER,"DEEP COLD","Hoarfrost: wider and slows more"},{RIMEBINDER,"BRITTLE","Shatter: +35% damage"},{RIMEBINDER,"NUMBING RIME","Chilled foes deal 10% less harm"},
 {-1,"KEEN EDGES","Whole party: +10% damage"},{-1,"HARDENED","Whole party: +12% life"},{-1,"BRIGHT LANTERN","Lantern radius +15%"},{-1,"QUICK MARCH","Whole party: +7% attack speed"},{-1,"SCAVENGERS","+25% silver found"},{-1,"FULL RESERVOIR","Lantern Flare fills 30% faster and burns 30% hotter"},{-1,"HARD LESSONS","+15% experience"}};
enum { CAMP_WHET, CAMP_DRESS, CAMP_OIL, CAMP_TONIC, NCAMP };
struct CampDef { const char* name; const char* d; float base; };
inline const CampDef CAMPS[NCAMP] = {{"WHETSTONE","party damage",45},{"FIELD DRESSING","party life, healing between packs",45},{"LAMP OIL","lantern radius, mending in the light",60},{"TONICS","tonics carried and their strength",70}};
enum { B_DMG, B_HP, B_SILVER, B_XP, B_START, B_APOTHECARY, B_RIMEBINDER, NBOON };
struct BoonDef { const char* name; const char* d; int cost, max; bool unlock; int cls; };
inline const BoonDef BOONS[NBOON] = {
 {"EMBER OF WRATH","+6% damage, every run",2,20,false,-1},{"EMBER OF ENDURANCE","+6% life, every run",2,20,false,-1},{"EMBER OF PLENTY","+10% silver, every run",2,10,false,-1},{"EMBER OF MEMORY","+8% experience, every run",2,10,false,-1},
 {"EMBER OF THE ROAD","begin one stage further on",4,12,false,-1},{"CALL THE APOTHECARY","unlocks the Apothecary now",5,1,true,APOTHECARY},{"CALL THE RIMEBINDER","unlocks the Rimebinder now",9,1,true,RIMEBINDER}};
inline const int UNLOCK_AT[NCLS] = {0,0,0,0,4,7};
// reaches (biomes) - ported from 30-world.js
struct Reach { const char* name; float fog[3]; float dens; uint32_t gA, gB, mud; float wet; uint32_t gr[3]; float grassH, moon, cloud; float mote[3]; float drizzle; float props[6]; };  // props: rock standing tree wall cairn fence
inline const Reach REACHES[5] = {
 {"LOW MOOR",{0.0126f,0.0189f,0.0302f},0.016f,0x232612,0x3a341c,0x18150f,0.45f,{0x3c4420,0x5f5a2c,0x8c7c48},1.0f,1.0f,0.75f,{1.0f,0.8f,0.5f},0.25f,{5,1.2f,1.4f,2.2f,0.8f,1.4f}},
 {"DROWNED FEN",{0.0109f,0.0218f,0.0231f},0.027f,0x1a2414,0x2c3018,0x141310,1.0f,{0x33421f,0x52592a,0x76703c},1.45f,0.75f,0.9f,{0.6f,0.9f,0.7f},0.6f,{2,0.4f,2.6f,0.4f,0.3f,2.2f}},
 {"BARROW DOWNS",{0.0168f,0.0210f,0.0294f},0.013f,0x26281a,0x413c26,0x201a13,0.2f,{0x424626,0x6a6436,0x9a8a58},0.75f,1.35f,0.5f,{0.8f,0.85f,1.0f},0.0f,{4,4.5f,0.5f,1.2f,3.0f,0.3f}},
 {"THORNWOOD",{0.0092f,0.0126f,0.0168f},0.024f,0x1c1c10,0x2c2616,0x17110c,0.5f,{0x2e3418,0x4a4422,0x665a30},0.9f,0.6f,0.95f,{0.9f,0.7f,0.4f},0.35f,{3,0.5f,7.5f,0.8f,0.5f,0.8f}},
 {"ASHEN TOR",{0.0231f,0.0189f,0.0176f},0.019f,0x24211d,0x383129,0x1e1a17,0.1f,{0x3a352a,0x5a5040,0x7a6a50},0.5f,1.1f,0.7f,{1.0f,0.5f,0.2f},0.0f,{9,2.0f,1.2f,0.6f,1.5f,0.2f}}};
// tuning (defaults of the original)
struct Tune { float hp = 1.3f, dmg = 1.17f, d0 = 7.5f, gap = 8, heal = 0.03f, xp = 1.4f, h0 = 1, b0 = 14, b1 = 3; };
inline const Tune TUNE;
constexpr float WALK = 1.6f; constexpr int PACKS = 3;
