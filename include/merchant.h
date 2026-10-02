#ifndef MERCHANT_H
#define MERCHANT_H

#include "types.h"
#include "item.h"

// game/merchant.cpp: the merchant's stock / weapon-tune tables and the shop price logic.

// Stock table entry (8 bytes); tables end with id 0xFFFF.
struct StockEntry {
    u16 id;       // 0x00  item id
    s16 num;      // 0x02  pieces in stock; -1 = unlimited (1000), -2 = never for sale
    u8 isNew;     // 0x04  added since the last shop visit
    u8 pad_5[3];
};

// Weapon tune table entry (8 bytes); tables end with id 0xFFFF.
struct LevelEntry {
    u16 id;       // 0x00  weapon item id
    u8 lv[4];     // 0x02  max tune level per type (fire, magazine, speed, exclusive)
    u8 isNew;     // 0x06
    u8 pad_7;
};

struct STOCK_INFO {
    StockEntry e[64];  // 0x200
};

struct LEVEL_INFO {
    LevelEntry e[32];  // 0x100
};

// Per-merchant persistent data (saved with the game).
struct MERCHANT_DATA {
    STOCK_INFO stock;  // 0x000
    LEVEL_INFO level;  // 0x200
    s8 friendship;          // 0x300  0..100
    u8 study_num;
    s8 reduction_ratio;       // 0x302  percent off the selling price
    u8 bonus_flag;
};                     // 0x304

// Price table entry (6 bytes: sell / exercise / item price tables); tables end with id 0xFFFF.
struct PRICE_INFO {
    u16 id;       // 0x00
    u16 price;    // 0x02  price / 10
    u8 unit;      // 0x04  pieces per purchase
    u8 pad_5;
};

// Weapon tune price table entry (0x2A bytes): price / 10 per level (index lv - 2).
struct LEVEL_PRICE {
    u16 id;       // 0x00
    s16 power[7];  // 0x02  firepower levels 2.. (levelupPrice type 0)
    s16 speed[3];  // 0x10  firing speed levels (type 1) (PS2 speed; was `mag`)
    s16 reload[3]; // 0x16  reload speed levels (type 2) (PS2 reload; was `speed`)
    s16 bullet[7]; // 0x1C  capacity levels (type 3) (PS2 bullet; was `ex`)
};

// Merchant personality constants (merchant_info_A).
struct MERCHANT_INFO {
    u32 id;
    s8 shift_Discount;
    s8 shift_Recommend;
    s8 shift_Bonus;
    s8 shift_Buyup;   // 0x07  favor change per purchase
    s32 threshold;   // 0x08  sell points from which sellFavorBig applies
    u8 sellFavorBig;   // 0x0C
    u8 sellFavor;      // 0x0D
    u8 m_off_ratio_first;
    u8 m_off_ratio_good;
    u8 m_off_ratio_normal;
    u8 m_off_ratio_bad;
    u8 m_win_ratio_good;
    u8 m_win_ratio_normal;
    u8 m_win_ratio_bad;
};

// The merchant selected for the current room (merchantChar).
class MerchantCharacter {
public:
    MERCHANT_INFO* m_p_info;      // 0x00
    MERCHANT_DATA* m_p_data;      // 0x04
    PRICE_INFO* m_p_sell;       // 0x08
    PRICE_INFO* m_p_exer;       // 0x0C
    LEVEL_PRICE* m_p_lvup;      // 0x10

    MerchantCharacter() {}
    ~MerchantCharacter() {}
    void setChar(MERCHANT_INFO* info, MERCHANT_DATA* data, PRICE_INFO* sell, PRICE_INFO* exer, LEVEL_PRICE* lvup);
};                           // 0x14

// Shop session: a working copy of the merchant data plus the item lists shown in the shop.
class Merchant {
private:
    MERCHANT_INFO* m_p_info;      // 0x000
    PRICE_INFO* m_p_sell;       // 0x004  selling price table
    PRICE_INFO* m_p_exer;       // 0x008  exercise (buy-up) price table
    LEVEL_PRICE* m_p_lvup;      // 0x00C  weapon tune price table
    STOCK_INFO m_stock;        // 0x010
public:
    LEVEL_INFO level;        // 0x210
private:
    s8 m_friendship;                // 0x310
    u8 m_study_num;
    s8 m_reduction_ratio;             // 0x312
    u8 m_bonus_flag;
    u8 m_exer_tbl_num;          // 0x314
    u8 m_sell_tbl_num;           // 0x315
public:
    u8 exerciseList[0xFF];   // 0x316  cItemMgr slot indexes of the items the player can sell
    u8 sellingList[0xFF];    // 0x415  sellPrice indexes of the items for sale

    Merchant(MerchantCharacter* c);
    void save(MERCHANT_DATA* p_data);
    void load(MERCHANT_DATA* p_data);
    StockEntry* stockPtr(u16 id);
    void stockAdd(u16 id, int num);
    void stockSub(u16 id, int num);
    int stockNum(u16 id);
    int stockNew(u16 id);
    int stockNew();
    LevelEntry* levelPtr(u16 id);
    int levelNew(u16 id);
    int levelNew();
    s8 levelMax(u16 id, int type);
    int stockSpecial(ITEM_ID id);
    int specialTunable(cItem* p_item);
    int specialTuned(cItem* p_item);
    int tunable(cItem* p_item);
    void makeList();
    int makeSellingList();
    u8 sellingItemNum();
    PRICE_INFO* sellingItemNo(int no);
    PRICE_INFO* sellingItemId(u16 id);
    int makeExerciseList();
    u8 exerciseItemNum();
    cItem* exerciseItemPtr(int no);
    PRICE_INFO* exerciseItemNo(int no);
    PRICE_INFO* exerciseItemId(u16 id);
    int buyupPrice(u16 id, int num);
    int buyupPrice(cItem* item, int num);
    int buyup(cItem* p_item, int num, int* pocket);
    int sellPrice(u16 id, int num);
    int sellUnit(u16 id);
    int sell(u16 id, int num, int* pocket);
    int levelupItemNum();
    LevelEntry* levelupItemNo(int no);
    cItem* levelupItemPtr(int no);
    LEVEL_PRICE* levelupItemPrice(u16 id);
    int levelupPrice(u16 id, int type, int lv);
    int levelupPrice(cItem* item, int type, int lv);
};                           // 0x514

extern MerchantCharacter merchantChar;
extern MERCHANT_DATA merchantData[1];
extern StockEntry stock_1st_mission[];
extern StockEntry stock_2st_first[];
extern MERCHANT_INFO merchant_info_A;
extern LEVEL_PRICE level_price[];
extern PRICE_INFO g_item_price_tbl[];
// Per-room stock / level tables the room scripts add (r11c, r200).
extern StockEntry stock_r11c[];
extern StockEntry stock_r11c_after_event[];
extern LevelEntry level_r200[];
extern LevelEntry level_null[];

void merchant_stage1_full();
void merchant_stage2_full();
void merchant_stage3_full();
void MerchantGameInit();
void Merchant2ndRoundInit();
void MerchantRoomInit();
int MerchantDataSize();
void MerchantDataSave(void* dst);
void MerchantDataLoad(void* src);
void stockDataInit(MERCHANT_DATA* p_data);
void add_stock(StockEntry* dst, StockEntry* src);
void stockDataAdd(MERCHANT_DATA* d, StockEntry* tbl);
void levelDataInit(MERCHANT_DATA* p_data);
void levelDataAdd(MERCHANT_DATA* d, LevelEntry* tbl);
int checkSellingItem(ITEM_ID id);
int checkExerciseItem(ITEM_ID id);

#endif
