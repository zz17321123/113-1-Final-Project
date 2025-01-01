#define _USE_MATH_DEFINES
#include <math.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>
#include <string.h>
#include <gst/gst.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <windows.h>

//========================[ 常數 ]========================
// 定義遊戲格子的寬度與高度
#define GRID_WIDTH   40
#define GRID_HEIGHT  20
// 定義蛇移動的最大時間間隔 (毫秒)
#define MAX_INTERVAL 250 // 最大移動間隔 (毫秒)

//========================[ 遊戲模式 ]========================
// 定義遊戲的不同模式，包括無模式、主菜單、單人模式、雙人模式和遊戲介紹模式
typedef enum {
    MODE_NONE = 0,
    MODE_MENU,
    MODE_SINGLE,
    MODE_MULTI,
    MODE_INTRO      // 遊戲介紹模式
} GameMode;

//========================[ 結構定義 ]========================
// 定義障礙物的結構，包括位置和尺寸
typedef struct {
    int x, y;
    int width, height;
} Obstacle;

// 定義點的結構，用於表示蛇的節點和食物的位置
typedef struct {
    int x, y;
} Point;

//========================[ 全域變數 ]========================

// GTK相關的全域變數
static GtkWidget* window = NULL;            // 主視窗
static GtkWidget* main_menu = NULL;         // 主菜單視圖
static GtkWidget* pause_dialog = NULL;      // 暫停對話框
static GtkWidget* stack = NULL;             // GtkStack 用於管理不同的視圖

// 倒數計時相關的全域變數
static int      countdown = 3;               // 倒數計時的初始值
static gboolean show_go = FALSE;            // 是否顯示「開始」字樣
static gboolean game_started = FALSE;       // 遊戲是否已開始
static guint    countdown_timer_id = 0;      // 倒數計時器的ID

// 遊戲模式相關的全域變數
static GameMode current_mode = MODE_MENU;    // 當前的遊戲模式
static gboolean game_over = FALSE;           // 遊戲是否結束
static gboolean paused = FALSE;              // 遊戲是否暫停

// 通用障礙物的全域變數
static GList* obstacles = NULL;               // 存放所有障礙物的鏈表

//=== 單人模式相關的全域變數 ===
static GList* snake_single = NULL;            // 單人模式下蛇的節點鏈表
static Point   food_single;                   // 單人模式下食物的位置
static int     direction_single = GDK_KEY_Right;       // 單人模式下蛇的當前移動方向
static int     next_direction_single = GDK_KEY_Right;  // 單人模式下蛇的下個移動方向
static int     score_single = 0;              // 單人模式下的分數
static GtkWidget* canvas_single = NULL;       // 單人模式的繪圖區域
static int    current_interval_single = 100; // 單人模式下蛇移動的當前時間間隔
static guint  single_timer_id = 0;            // 單人模式的定時器ID
static gboolean player_single_alive = TRUE;   // 單人模式下蛇是否存活

//=== 雙人模式相關的全域變數 ===
static GList* snake1 = NULL;                  // 玩家1的蛇節點鏈表
static GList* snake2 = NULL;                  // 玩家2的蛇節點鏈表
// 玩家1使用WASD鍵，玩家2使用方向鍵
static int    direction1 = GDK_KEY_d;         // 玩家1的當前移動方向
static int    next_direction1 = GDK_KEY_d;    // 玩家1的下個移動方向
static int    direction2 = GDK_KEY_Left;      // 玩家2的當前移動方向
static int    next_direction2 = GDK_KEY_Left; // 玩家2的下個移動方向
static Point  food_multi;                      // 雙人模式下食物的位置
static int    score1 = 0;                      // 玩家1的分數
static int    score2 = 0;                      // 玩家2的分數
static gboolean player1_alive = TRUE;         // 玩家1是否存活
static gboolean player2_alive = TRUE;         // 玩家2是否存活
static int    winner = 0;                      // 贏家標識 (1: 玩家1, 2: 玩家2, 0: 平局)

// === 雙人模式相關的時間變數 ===
static time_t start_time_multi = 0;            // 雙人模式開始時間，用於計算存活秒數
static time_t dead_time_player1 = 0;           // 玩家1的存活秒數
static time_t dead_time_player2 = 0;           // 玩家2的存活秒數

static GtkWidget* canvas_multi = NULL;         // 雙人模式的繪圖區域
static int    current_interval_player1 = 100; // 玩家1的移動時間間隔
static int    current_interval_player2 = 100; // 玩家2的移動時間間隔
static guint  player1_timer_id = 0;           // 玩家1的定時器ID
static guint  player2_timer_id = 0;           // 玩家2的定時器ID

//=== 固定網格尺寸 ===
static double CELL_SIZE = 45.0;                // 單位格子的大小 (像素)

//=== 音效管理相關的結構和變數 ===
typedef struct {
    GstElement* pipeline; // GStreamer的pipeline元素
    gboolean loop;        // 是否循環播放
} SoundData;

// 全域變數來管理主選單背景音樂
static SoundData* main_menu_music = NULL;
// 全域變數來管理遊戲背景音樂
static SoundData* game_background_music = NULL;
// 全域變數來管理倒數音效
static SoundData* countdown_music = NULL;

//==============================================================
// 函式宣告
//==============================================================

/* 音效播放相關函式 */

/**
 * @brief 播放音效的函式。
 *
 * 此函式使用GStreamer播放指定的音效檔案。
 *
 * @param filename 音效檔案的路徑。
 * @param loop 是否循環播放音效。
 * @param volume_level 音量大小，範圍通常為0.0到1.0。
 * @return 返回SoundData結構的指標，用於後續控制音效。
 */
static SoundData* play_sound_effect(const char* filename, gboolean loop, float volume_level);

/**
 * @brief 當GStreamer解碼器新增pad時的回調函式。
 *
 * 此函式負責將解碼後的音頻流連接到後續的處理元件。
 *
 * @param src 來源的GstElement。
 * @param new_pad 新增加的GstPad。
 * @param data 用於連接的目標元件（通常是audioconvert）。
 */
static void on_pad_added(GstElement* src, GstPad* new_pad, gpointer data);


/* 其他函式宣告 */

/**
 * @brief 單人模式按鈕點擊事件的回調函式。
 *
 * 當玩家點擊「單人模式」按鈕時，觸發此函式以開始單人遊戲。
 *
 * @param button 被點擊的GtkButton。
 * @param user_data 用戶資料，通常為指向GtkStack的指標。
 */
static void on_single_mode_clicked(GtkButton* button, gpointer user_data);

/**
 * @brief 雙人模式按鈕點擊事件的回調函式。
 *
 * 當玩家點擊「雙人模式」按鈕時，觸發此函式以開始雙人遊戲。
 *
 * @param button 被點擊的GtkButton。
 * @param user_data 用戶資料，通常為指向GtkStack的指標。
 */
static void on_multi_mode_clicked(GtkButton* button, gpointer user_data);

/**
 * @brief 顯示主選單的函式。
 *
 * 切換到主選單視圖，讓玩家可以選擇不同的遊戲模式或退出遊戲。
 */
static void show_main_menu(void);

/**
 * @brief 返回主選單按鈕點擊事件的回調函式。
 *
 * 當玩家在遊戲中選擇返回主選單時，觸發此函式以清理遊戲資料並顯示主選單。
 *
 * @param button 被點擊的GtkButton。
 * @param user_data 用戶資料，通常為指向GtkStack的指標。
 */
static void on_back_to_menu_clicked(GtkButton* button, gpointer user_data);

/**
 * @brief 退出遊戲按鈕點擊事件的回調函式。
 *
 * 當玩家點擊「退出遊戲」按鈕時，觸發此函式以終止應用程式。
 *
 * @param button 被點擊的GtkButton。
 * @param user_data 無特定用途，可為NULL。
 */
static void on_quit_clicked(GtkButton* button, gpointer user_data);

/**
 * @brief 創建暫停對話框的函式。
 *
 * 當玩家按下ESC鍵暫停遊戲時，觸發此函式以顯示暫停選項，如繼續遊戲或返回主選單。
 */
static void create_pause_dialog(void);

/**
 * @brief 繼續遊戲按鈕點擊事件的回調函式。
 *
 * 當玩家在暫停對話框中選擇繼續遊戲時，觸發此函式以關閉對話框並恢復遊戲。
 *
 * @param btn 被點擊的GtkButton。
 * @param user_data 無特定用途，可為NULL。
 */
static void on_resume_game(GtkButton* btn, gpointer user_data);


/* 倒數計時相關函式 */

/**
 * @brief 開始倒數計時的函式。
 *
 * 初始化倒數計時的相關變數，並設置定時器以每隔一定時間更新倒數狀態。
 */
static void start_countdown(void);

/**
 * @brief 倒數計時的定時器回調函式。
 *
 * 每次定時器觸發時，更新倒數值或開始遊戲。
 *
 * @param data 無特定用途，可為NULL。
 * @return 返回TRUE以繼續定時器，返回FALSE以停止定時器。
 */
static gboolean countdown_tick(gpointer data);


/* 障礙物生成相關函式 */

/**
 * @brief 檢查障礙物生成位置是否有效的函式。
 *
 * 確保障礙物不會生成在被排除的位置（例如蛇的初始位置附近）。
 *
 * @param x 障礙物左上角的X坐標。
 * @param y 障礙物左上角的Y坐標。
 * @param w 障礙物的寬度。
 * @param h 障礙物的高度。
 * @param excluded_positions 不允許生成障礙物的位置鏈表。
 * @return 如果位置有效，返回TRUE；否則返回FALSE。
 */
static gboolean is_obstacle_valid(int x, int y, int w, int h, GList* excluded_positions);

/**
 * @brief 生成障礙物的函式。
 *
 * 在遊戲地圖上隨機生成一定數量的障礙物，並避免與排除的位置重疊。
 *
 * @param initial_positions 初始位置的鏈表，用於避免障礙物生成在這些位置。
 */
static void generate_obstacles(GList* initial_positions);


/* 單人遊戲相關函式 */

/**
 * @brief 初始化單人遊戲的函式。
 *
 * 清理之前的遊戲資料，設置蛇的初始位置和方向，生成障礙物和食物，並開始倒數計時和遊戲更新定時器。
 */
static void init_single_game(void);

/**
 * @brief 更新單人遊戲狀態的定時器回調函式。
 *
 * 控制蛇的移動、檢查碰撞、處理食物的食用以及更新遊戲畫面。
 *
 * @param data 無特定用途，可為NULL。
 * @return 返回TRUE以繼續定時器，返回FALSE以停止定時器。
 */
static gboolean update_game_single(gpointer data);

/**
 * @brief 生成單人模式下食物的位置。
 *
 * 隨機選擇一個有效的位置生成食物，確保食物不會出現在蛇或障礙物上。
 */
static void generate_food_single(void);

/**
 * @brief 檢查單人模式下蛇是否發生碰撞。
 *
 * 檢查蛇頭是否與自身或障礙物發生碰撞。
 *
 * @param head 蛇頭的位置。
 * @return 如果發生碰撞，返回TRUE；否則返回FALSE。
 */
static gboolean check_collision_single(Point* head);

/**
 * @brief 顯示單人模式遊戲結束畫面的函式。
 *
 * 當蛇死亡後，顯示遊戲結束畫面並顯示最終分數。
 */
static void show_game_over_screen_single(void);

/**
 * @brief 單人模式下蛇死亡的處理函式。
 *
 * 播放死亡音效並啟動蛇的閃爍效果，提示玩家遊戲結束。
 */
static void kill_player_single(void); // 單人模式蛇死亡處理


/* 雙人遊戲相關函式 */

/**
 * @brief 初始化雙人遊戲的函式。
 *
 * 清理之前的遊戲資料，設置兩條蛇的初始位置和方向，生成障礙物和食物，並開始倒數計時和雙人遊戲更新定時器。
 */
static void init_multi_game(void);

/**
 * @brief 更新玩家1狀態的定時器回調函式。
 *
 * 控制玩家1的蛇移動、檢查碰撞、處理食物的食用以及更新遊戲畫面。
 *
 * @param data 無特定用途，可為NULL。
 * @return 返回TRUE以繼續定時器，返回FALSE以停止定時器。
 */
static gboolean update_player1(gpointer data);

/**
 * @brief 更新玩家2狀態的定時器回調函式。
 *
 * 控制玩家2的蛇移動、檢查碰撞、處理食物的食用以及更新遊戲畫面。
 *
 * @param data 無特定用途，可為NULL。
 * @return 返回TRUE以繼續定時器，返回FALSE以停止定時器。
 */
static gboolean update_player2(gpointer data);

/**
 * @brief 生成雙人模式下食物的位置。
 *
 * 隨機選擇一個有效的位置生成食物，確保食物不會出現在任何一條蛇或障礙物上。
 */
static void generate_food_multi(void);

/**
 * @brief 檢查蛇是否自撞的函式。
 *
 * 檢查指定蛇的蛇頭是否與自身的身體節點重疊。
 *
 * @param head 蛇頭的位置。
 * @param snake 蛇的節點鏈表。
 * @return 如果自撞，返回TRUE；否則返回FALSE。
 */
static gboolean check_self_collision(Point* head, GList* snake);

/**
 * @brief 檢查蛇是否撞到另一條蛇的函式。
 *
 * 檢查指定蛇的蛇頭是否與另一條蛇的任何節點重疊。
 *
 * @param head 蛇頭的位置。
 * @param snake 另一條蛇的節點鏈表。
 * @return 如果撞到另一條蛇，返回TRUE；否則返回FALSE。
 */
static gboolean check_snake_collision(Point* head, GList* snake);

/**
 * @brief 顯示雙人模式遊戲結束畫面的函式。
 *
 * 當兩條蛇都死亡後，根據分數和存活時間決定勝負，並顯示遊戲結束畫面。
 */
static void show_game_over_screen_multi(void);

/**
 * @brief 結束雙人遊戲的處理函式。
 *
 * 當兩條蛇都死亡時，結束遊戲並進行勝負判定。
 */
static void end_two_player_game(void);


/* 繪圖相關函式 */

/**
 * @brief 繪製遊戲畫面的主函式。
 *
 * 根據當前遊戲模式（單人或雙人），調用相應的繪圖函式來繪製遊戲元素。
 *
 * @param area GtkDrawingArea區域。
 * @param cr Cairo繪圖上下文。
 * @param width 畫布的寬度。
 * @param height 畫布的高度。
 * @param user_data 用戶資料，通常為NULL。
 */
static void draw_game(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data);

/**
 * @brief 繪製單人模式遊戲畫面的函式。
 *
 * 繪製單人模式下的遊戲元素，包括背景、食物、障礙物、蛇、分數和倒數計時。
 *
 * @param cr Cairo繪圖上下文。
 * @param width 畫布的寬度。
 * @param height 畫布的高度。
 * @param cell_size 每個格子的大小（像素）。
 */
static void draw_single_mode(cairo_t* cr, int width, int height, double cell_size);

/**
 * @brief 繪製雙人模式遊戲畫面的函式。
 *
 * 繪製雙人模式下的遊戲元素，包括背景、食物、障礙物、兩條蛇、分數和倒數計時。
 *
 * @param cr Cairo繪圖上下文。
 * @param width 畫布的寬度。
 * @param height 畫布的高度。
 * @param cell_size 每個格子的大小（像素）。
 */
static void draw_multi_mode(cairo_t* cr, int width, int height, double cell_size);

/**
 * @brief 繪製蛇的一個節點的函式。
 *
 * 繪製蛇的身體節點，包括陰影和本體顏色，以增強視覺效果。
 *
 * @param cr Cairo繪圖上下文。
 * @param x 節點的X坐標（像素）。
 * @param y 節點的Y坐標（像素）。
 * @param size 節點的大小（像素）。
 * @param body 蛇身體的顏色。
 * @param shadow 蛇陰影的顏色。
 */
static void draw_snake_segment(cairo_t* cr, double x, double y, double size,
    GdkRGBA body, GdkRGBA shadow);

/**
 * @brief 繪製食物的函式。
 *
 * 繪製食物的位置，包括陰影和本體顏色，以增強視覺效果。
 *
 * @param cr Cairo繪圖上下文。
 * @param x 食物的X坐標（像素）。
 * @param y 食物的Y坐標（像素）。
 * @param size 食物的大小（像素）。
 */
static void draw_food(cairo_t* cr, double x, double y, double size);

/**
 * @brief 繪製牆壁的函式。
 *
 * 繪製障礙物的位置，包括陰影和本體顏色，以增強視覺效果。
 *
 * @param cr Cairo繪圖上下文。
 * @param x 牆壁左上角的X坐標（像素）。
 * @param y 牆壁左上角的Y坐標（像素）。
 * @param w 牆壁的寬度（像素）。
 * @param h 牆壁的高度（像素）。
 */
static void draw_wall(cairo_t* cr, double x, double y, double w, double h);


/* 鍵盤按鍵處理副程式 */

/**
 * @brief 處理鍵盤按鍵事件的回調函式。
 *
 * 根據玩家的按鍵輸入來控制蛇的移動方向，並處理遊戲的暫停與繼續。
 *
 * @param controller 鍵盤事件控制器。
 * @param keyval 按鍵的GDK鍵值。
 * @param keycode 按鍵的代碼。
 * @param state 按鍵的修飾狀態。
 * @param user_data 用戶資料，通常為NULL。
 * @return 如果事件被處理，返回TRUE；否則返回FALSE。
 */
static gboolean on_key_press(GtkEventControllerKey* controller,
    guint keyval, guint keycode,
    GdkModifierType state, gpointer user_data);


/* 主選單與介紹模式按鈕相關函式 */

/**
 * @brief 遊戲介紹模式按鈕點擊事件的回調函式。
 *
 * 當玩家點擊「遊戲介紹」按鈕時，觸發此函式以顯示遊戲介紹頁面。
 *
 * @param button 被點擊的GtkButton。
 * @param user_data 用戶資料，通常為指向GtkStack的指標。
 */
static void on_intro_mode_clicked(GtkButton* button, gpointer user_data);


/* 隨機方向生成相關函式 */

/**
 * @brief 生成隨機方向的函式。
 *
 * 隨機選擇一個方向鍵（上、下、左、右）作為蛇的初始移動方向。
 *
 * @return 返回選擇的GDK鍵值，代表蛇的移動方向。
 */
static int get_random_direction(void);


//==============================================================
// [蛇閃爍機制] - 定義
//==============================================================
// 定義用於蛇閃爍效果的結構
typedef struct {
    GList** snake_ref;       // 要閃爍的蛇 (鏈表)
    gboolean* snake_alive;   // 該蛇對應的存活狀態
    GList* original_snake_data; // 原始蛇的數據
    int flicker_count;       // 閃爍計數
    int flicker_max;         // 最大閃爍次數
    GtkWidget* draw_canvas;  // 用於重繪的畫布
} FlickerData;

// 閃爍效果的回調函式
static gboolean do_flicker_snake(gpointer data);

// 啟動蛇閃爍機制
static void start_flicker_snake(GList** snake_ref, gboolean* snake_alive, GtkWidget* draw_canvas)
{
    FlickerData* fd = (FlickerData*)malloc(sizeof(FlickerData));
    fd->snake_ref = snake_ref;
    fd->snake_alive = snake_alive;
    fd->original_snake_data = *snake_ref; // 保留蛇的身體
    fd->flicker_count = 0;
    fd->flicker_max = 4;  // 閃爍次數 
    fd->draw_canvas = draw_canvas;

    // 啟動閃爍定時器，每300毫秒呼叫一次do_flicker_snake
    g_timeout_add(300, do_flicker_snake, fd);
}

// 每次呼叫 do_flicker_snake
static gboolean do_flicker_snake(gpointer data)
{
    FlickerData* fd = (FlickerData*)data;

    // 如果目前蛇存在 => 隱藏；不存在 => 還原
    if (*(fd->snake_ref)) {
        *(fd->snake_ref) = NULL;
    }
    else {
        *(fd->snake_ref) = fd->original_snake_data;
    }

    // 重繪畫布
    if (fd->draw_canvas) {
        gtk_widget_queue_draw(fd->draw_canvas);
    }

    fd->flicker_count++;
    // 閃爍到指定次數 => 真正刪除蛇並設置為死亡
    if (fd->flicker_count >= fd->flicker_max) {
        // 只設置為死亡，不釋放內存
        *(fd->snake_alive) = FALSE;

        // 將蛇從列表中移除
        if (*(fd->snake_ref)) {
            // 將蛇列表清空並釋放內存
            for (GList* iter = *(fd->snake_ref); iter; iter = iter->next) {
                free(iter->data);
            }
            g_list_free(*(fd->snake_ref));
            *(fd->snake_ref) = NULL;
        }

        free(fd);

        // 檢查是否需要結束遊戲
        if (current_mode == MODE_MULTI) {
            if (!player1_alive && !player2_alive) {
                end_two_player_game();
            }
        }
        else if (current_mode == MODE_SINGLE) { // 單人模式處理
            game_over = TRUE;

            if (single_timer_id) {
                g_source_remove(single_timer_id);
                single_timer_id = 0;
            }

            gtk_widget_queue_draw(canvas_single);
            show_game_over_screen_single();
        }

        return FALSE; // 結束定時器
    }

    return TRUE; // 繼續定時器
}

//==============================================================
// [ 倒數機制 ] (500ms interval)
//==============================================================
// 啟動倒數計時
static void start_countdown(void)
{
    countdown = 3;            // 設定倒數初始值為3
    show_go = FALSE;          // 不顯示「開始」字樣
    game_started = FALSE;     // 遊戲尚未開始
    if (countdown_timer_id) {
        g_source_remove(countdown_timer_id); // 移除之前的定時器
        countdown_timer_id = 0;
    }

    // 播放倒數音效，設定不循環播放，音量為1.0
    countdown_music = play_sound_effect("Musics/countdown_3_to_1.mp3", FALSE, 1.0); // 不循環

    // 每940毫秒呼叫一次倒數計時器
    countdown_timer_id = g_timeout_add(940, countdown_tick, NULL);
}

// 倒數計時的回調函式
static gboolean countdown_tick(gpointer data)
{
    if (paused) {
        // 如果遊戲暫停，停止倒數計時
        return TRUE; // 繼續定時器，但不執行任何操作
    }

    if (countdown > 0) {
        countdown--; // 減少倒數值
    }
    else if (!show_go) {
        show_go = TRUE; // 顯示「開始」字樣
    }
    else {
        game_started = TRUE; // 遊戲開始
        countdown_timer_id = 0;

        // 播放遊戲背景音樂 (循環，音量一半)
        if (current_mode == MODE_SINGLE || current_mode == MODE_MULTI) {
            game_background_music = play_sound_effect("Musics/game_background.mp3", TRUE, 0.5); // 循環且音量一半
        }

        return FALSE; // 停止定時器
    }

    // 根據不同模式重繪畫面
    switch (current_mode) {
    case MODE_SINGLE:
        if (canvas_single) gtk_widget_queue_draw(canvas_single);
        break;
    case MODE_MULTI:
        if (canvas_multi) gtk_widget_queue_draw(canvas_multi);
        break;
    default:
        break;
    }
    return TRUE; // 繼續定時器
}

//==============================================================
// [共同 / 清理 / 回主選單 / 離開 / 暫停]
//==============================================================
// 清理遊戲資料，包括蛇、障礙物、分數等
static void clear_game_data(void)
{
    // 釋放「單人」蛇的記憶體
    if (snake_single) {
        for (GList* iter = snake_single; iter; iter = iter->next) {
            free(iter->data);
        }
        g_list_free(snake_single);
        snake_single = NULL;
    }
    // 釋放「雙人」蛇1的記憶體
    if (snake1) {
        for (GList* iter = snake1; iter; iter = iter->next) {
            free(iter->data);
        }
        g_list_free(snake1);
        snake1 = NULL;
    }
    // 釋放「雙人」蛇2的記憶體
    if (snake2) {
        for (GList* iter = snake2; iter; iter = iter->next) {
            free(iter->data);
        }
        g_list_free(snake2);
        snake2 = NULL;
    }
    // 釋放障礙物的記憶體
    if (obstacles) {
        for (GList* iter = obstacles; iter; iter = iter->next) {
            free(iter->data);
        }
        g_list_free(obstacles);
        obstacles = NULL;
    }

    // 重置分數
    score_single = 0;
    score1 = 0; score2 = 0;

    // 清空死亡/生存時間
    dead_time_player1 = 0;
    dead_time_player2 = 0;
    start_time_multi = 0;

    // 重置移動方向
    direction_single = GDK_KEY_Right;
    next_direction_single = GDK_KEY_Right;
    direction1 = GDK_KEY_d;
    next_direction1 = GDK_KEY_d;
    direction2 = GDK_KEY_Left;
    next_direction2 = GDK_KEY_Left;

    // 重置時間間隔
    current_interval_single = 100; // 單人模式的移動間隔
    current_interval_player1 = 100; // 玩家1的移動間隔
    current_interval_player2 = 100; // 玩家2的移動間隔

    // 移除所有定時器
    if (single_timer_id) { g_source_remove(single_timer_id);    single_timer_id = 0; }
    if (player1_timer_id) { g_source_remove(player1_timer_id);   player1_timer_id = 0; }
    if (player2_timer_id) { g_source_remove(player2_timer_id);   player2_timer_id = 0; }
    if (countdown_timer_id) { g_source_remove(countdown_timer_id); countdown_timer_id = 0; }

    // 重置單人模式蛇的生存狀態
    player_single_alive = TRUE;

    // 停止遊戲背景音樂
    if (game_background_music) {
        gst_element_set_state(game_background_music->pipeline, GST_STATE_NULL);
        gst_object_unref(game_background_music->pipeline);
        free(game_background_music);
        game_background_music = NULL;
    }

    // 停止倒數音效
    if (countdown_music) {
        gst_element_set_state(countdown_music->pipeline, GST_STATE_NULL);
        gst_object_unref(countdown_music->pipeline);
        free(countdown_music);
        countdown_music = NULL;
    }

    // 重置狀態變數
    game_started = FALSE;
    countdown = 3;
    show_go = FALSE;
}

// 返回主選單的回調函式
static void on_back_to_menu_clicked(GtkButton* button, gpointer user_data)
{
    // 播放按鈕點擊音效
    play_sound_effect("Musics/button_click.mp3", FALSE, 1.0); // 不循環

    // 清理遊戲資料
    clear_game_data();
    game_over = FALSE;
    paused = FALSE;
    current_mode = MODE_MENU;
    player1_alive = TRUE;
    player2_alive = TRUE;
    winner = 0;

    // 如果暫停對話框存在，則銷毀它
    if (pause_dialog) {
        gtk_window_destroy(GTK_WINDOW(pause_dialog));
        pause_dialog = NULL;
    }

    // 從GtkStack中移除各種遊戲相關的視圖
    GtkStack* stack_ptr = GTK_STACK(stack);
    GtkWidget* game_single = gtk_stack_get_child_by_name(stack_ptr, "game_single");
    if (game_single) gtk_stack_remove(stack_ptr, game_single);

    GtkWidget* game_multi = gtk_stack_get_child_by_name(stack_ptr, "game_multi");
    if (game_multi) gtk_stack_remove(stack_ptr, game_multi);

    GtkWidget* game_over_single = gtk_stack_get_child_by_name(stack_ptr, "game_over_single");
    if (game_over_single) gtk_stack_remove(stack_ptr, game_over_single);

    GtkWidget* game_over_multi = gtk_stack_get_child_by_name(stack_ptr, "game_over_multi");
    if (game_over_multi) gtk_stack_remove(stack_ptr, game_over_multi);

    // 播放主選單背景音樂 (循環，音量一半)
    main_menu_music = play_sound_effect("Musics/main_menu_background.mp3", TRUE, 0.5); // 循環且音量一半

    // 顯示主選單視圖
    gtk_stack_set_visible_child_name(stack_ptr, "main_menu");
}

// 停止主選單背景音樂
static void stop_main_menu_music(void)
{
    if (main_menu_music) {
        gst_element_set_state(main_menu_music->pipeline, GST_STATE_NULL);
        gst_object_unref(main_menu_music->pipeline);
        free(main_menu_music);
        main_menu_music = NULL;
    }
}

// 退出遊戲的回調函式
static void on_quit_clicked(GtkButton* button, gpointer user_data)
{
    // 播放按鈕點擊音效
    play_sound_effect("Musics/button_click.mp3", FALSE, 1.0); // 不循環

    // 獲取應用程序並退出
    GtkApplication* app = GTK_APPLICATION(gtk_window_get_application(GTK_WINDOW(window)));
    if (app) g_application_quit(G_APPLICATION(app));
}

// 顯示「暫停」對話框
static void create_pause_dialog(void)
{
    if (pause_dialog) return; // 如果已經存在暫停對話框，則不重複創建

    // 暫停遊戲背景音樂
    if (game_background_music) {
        gst_element_set_state(game_background_music->pipeline, GST_STATE_PAUSED);
    }

    // 暫停倒數音效
    if (countdown_music) {
        gst_element_set_state(countdown_music->pipeline, GST_STATE_PAUSED);
    }

    // 暫停倒數計時器
    if (countdown_timer_id) {
        g_source_remove(countdown_timer_id);
        countdown_timer_id = 0;
    }

    // 創建暫停對話框
    pause_dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(pause_dialog), "遊戲暫停");
    gtk_window_set_transient_for(GTK_WINDOW(pause_dialog), GTK_WINDOW(window));
    gtk_window_set_modal(GTK_WINDOW(pause_dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(pause_dialog), 300, 150);

    // 隱藏對話框的標題列與關閉按鈕
    gtk_window_set_decorated(GTK_WINDOW(pause_dialog), FALSE);

    // 創建垂直容器
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_window_set_child(GTK_WINDOW(pause_dialog), vbox);

    // 添加標籤
    GtkWidget* label = gtk_label_new("遊戲已暫停");
    gtk_box_append(GTK_BOX(vbox), label);

    // 添加「繼續遊戲」按鈕
    GtkWidget* resume_btn = gtk_button_new_with_label("繼續遊戲");
    gtk_box_append(GTK_BOX(vbox), resume_btn);
    g_signal_connect(resume_btn, "clicked", G_CALLBACK(on_resume_game), NULL);

    // 添加「返回主選單」按鈕
    GtkWidget* back_btn = gtk_button_new_with_label("返回主選單");
    gtk_box_append(GTK_BOX(vbox), back_btn);
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_to_menu_clicked), NULL);

    // 播放按鈕點擊音效
    play_sound_effect("Musics/button_click.mp3", FALSE, 1.0); // 不循環

    // 顯示暫停對話框
    gtk_window_present(GTK_WINDOW(pause_dialog));
}

// 恢復遊戲的回調函式
static void on_resume_game(GtkButton* btn, gpointer user_data)
{
    // 播放按鈕點擊音效
    play_sound_effect("Musics/button_click.mp3", FALSE, 1.0); // 不循環

    paused = FALSE;
    if (pause_dialog) {
        gtk_window_destroy(GTK_WINDOW(pause_dialog));
        pause_dialog = NULL;
    }

    // 恢復遊戲背景音樂
    if (game_background_music) {
        gst_element_set_state(game_background_music->pipeline, GST_STATE_PLAYING);
    }

    // 恢復倒數音效
    if (countdown_music) {
        gst_element_set_state(countdown_music->pipeline, GST_STATE_PLAYING);
    }

    // 恢復倒數計時器 (如果遊戲尚未開始)
    if (!game_started && (countdown > 0 || show_go == FALSE)) {
        countdown_timer_id = g_timeout_add(500, countdown_tick, NULL);
    }
}

//==============================================================
// [障礙物]
//==============================================================
// 檢查障礙物生成的位置是否有效，不與排除的位置重疊
static gboolean is_obstacle_valid(int x, int y, int w, int h, GList* excluded_positions)
{
    // 與蛇初始位置不要太近
    for (GList* iter = excluded_positions; iter; iter = iter->next) {
        Point* p = (Point*)iter->data;
        float obs_cx = x + (float)w / 2;
        float obs_cy = y + (float)h / 2;
        float dx = obs_cx - p->x;
        float dy = obs_cy - p->y;
        float dist2 = dx * dx + dy * dy;
        if (dist2 < 16.0f) { // 距離平方小於16，表示距離小於4
            return FALSE;
        }
    }
    return TRUE;
}

// 分區生成障礙物，每個區域生成一定數量的障礙物
static void generate_obstacles(GList* initial_positions)
{
    // 不重複呼叫 srand
    // srand 已在 main 和其他地方呼叫過
    // srand((unsigned)time(NULL));

    int obstacles_per_zone = 2; // 每個區域生成的障礙物數量
    int zone_count = 4;         // 總區域數量

    for (int zone = 0; zone < zone_count; zone++) {
        int x_min, x_max, y_min, y_max;
        switch (zone) {
        case 0:
            x_min = 0;  x_max = GRID_WIDTH / 2 - 1;  y_min = 0;   y_max = GRID_HEIGHT / 2 - 1;
            break;
        case 1:
            x_min = GRID_WIDTH / 2; x_max = GRID_WIDTH - 1;  y_min = 0;   y_max = GRID_HEIGHT / 2 - 1;
            break;
        case 2:
            x_min = 0;  x_max = GRID_WIDTH / 2 - 1;  y_min = GRID_HEIGHT / 2;  y_max = GRID_HEIGHT - 1;
            break;
        default:
            x_min = GRID_WIDTH / 2; x_max = GRID_WIDTH - 1;  y_min = GRID_HEIGHT / 2;  y_max = GRID_HEIGHT - 1;
            break;
        }

        for (int i = 0; i < obstacles_per_zone; i++) {
            int attempts = 0;
            int max_attempts = 500;
            while (attempts < max_attempts) {
                int w = rand() % 3 + 1; // 隨機寬度 1~3
                int h = rand() % 3 + 1; // 隨機高度 1~3
                if ((x_max - x_min + 1) < w || (y_max - y_min + 1) < h) {
                    attempts++;
                    continue;
                }
                int rx = x_min + rand() % ((x_max - x_min + 1) - w); // 隨機X位置
                int ry = y_min + rand() % ((y_max - y_min + 1) - h); // 隨機Y位置

                // 檢查障礙物位置是否有效
                if (is_obstacle_valid(rx, ry, w, h, initial_positions)) {
                    Obstacle* obs = (Obstacle*)malloc(sizeof(Obstacle));
                    obs->x = rx;
                    obs->y = ry;
                    obs->width = w;
                    obs->height = h;
                    obstacles = g_list_append(obstacles, obs); // 添加到障礙物列表
                    break;
                }
                attempts++;
            }
        }
    }
}

//==============================================================
// [隨機方向生成函式]
//==============================================================
// 隨機選擇一個方向鍵，返回對應的GDK_KEY
static int get_random_direction(void)
{
    int directions[] = { GDK_KEY_Up, GDK_KEY_Down, GDK_KEY_Left, GDK_KEY_Right };
    int index = rand() % 4; // 隨機索引
    return directions[index];
}

//==============================================================
// [ 單人模式 ]
//==============================================================
// 初始化單人遊戲
static void init_single_game(void)
{
    clear_game_data(); // 清理之前的遊戲資料

    // 蛇初始位置設置在網格中心
    Point* h = (Point*)malloc(sizeof(Point));
    h->x = GRID_WIDTH / 2;
    h->y = GRID_HEIGHT / 2;
    snake_single = g_list_append(snake_single, h); // 添加到蛇的鏈表

    // 隨機初始方向
    direction_single = get_random_direction();
    next_direction_single = direction_single;
    score_single = 0;
    game_over = FALSE;
    paused = FALSE;

    // 收集初始位置，避免障礙物生成在蛇附近
    GList* initial_positions = NULL;
    initial_positions = g_list_append(initial_positions, h);

    generate_obstacles(initial_positions); // 生成障礙物
    generate_food_single();               // 生成食物
    start_countdown();                    // 開始倒數計時

    // 設置遊戲更新的定時器
    single_timer_id = g_timeout_add(current_interval_single, update_game_single, NULL);
    g_list_free(initial_positions); // 釋放初始位置鏈表
}

// 生成單人模式下的食物
static void generate_food_single(void)
{
    gboolean valid = FALSE;
    while (!valid) {
        food_single.x = rand() % GRID_WIDTH;  // 隨機X位置
        food_single.y = rand() % GRID_HEIGHT; // 隨機Y位置
        valid = TRUE;

        // 檢查食物是否與蛇重疊
        for (GList* iter = snake_single; iter; iter = iter->next) {
            Point* seg = (Point*)iter->data;
            if (seg->x == food_single.x && seg->y == food_single.y) {
                valid = FALSE;
                break;
            }
        }
        // 檢查食物是否與障礙物重疊
        if (valid) {
            for (GList* iter = obstacles; iter; iter = iter->next) {
                Obstacle* obs = (Obstacle*)iter->data;
                if (food_single.x >= obs->x && food_single.x < obs->x + obs->width &&
                    food_single.y >= obs->y && food_single.y < obs->y + obs->height) {
                    valid = FALSE;
                    break;
                }
            }
        }
    }
}

// 檢查單人模式下的碰撞情況，包括自撞和撞到障礙物
static gboolean check_collision_single(Point* head)
{
    // 自撞
    for (GList* iter = snake_single->next; iter; iter = iter->next) {
        Point* seg = (Point*)iter->data;
        if (seg->x == head->x && seg->y == head->y) return TRUE;
    }
    // 撞障礙物
    for (GList* iter = obstacles; iter; iter = iter->next) {
        Obstacle* obs = (Obstacle*)iter->data;
        if (head->x >= obs->x && head->x < obs->x + obs->width &&
            head->y >= obs->y && head->y < obs->y + obs->height) {
            return TRUE;
        }
    }
    return FALSE;
}

// 單人模式下蛇死亡的處理函式
static void kill_player_single(void)
{
    // 播放蛇死亡音效
    play_sound_effect("Musics/snake_die.mp3", FALSE, 1.0); // 不循環

    // 啟動蛇的閃爍效果
    start_flicker_snake(&snake_single, &player_single_alive, canvas_single);
}

// 更新單人遊戲狀態的定時器回調函式
static gboolean update_game_single(gpointer data)
{
    // 如果當前模式不是單人模式，或者遊戲已結束、暫停，則不進行更新
    if (current_mode != MODE_SINGLE || game_over || paused)
        return G_SOURCE_CONTINUE;
    if (!game_started)
        return G_SOURCE_CONTINUE;

    // 更新方向
    direction_single = next_direction_single;

    // 計算新的蛇頭位置
    Point* head = (Point*)snake_single->data;
    if (!head) { // 確保蛇頭存在
        return G_SOURCE_CONTINUE;
    }
    Point nh = { head->x, head->y };
    switch (direction_single) {
    case GDK_KEY_Up:    nh.y--; break;
    case GDK_KEY_Down:  nh.y++; break;
    case GDK_KEY_Left:  nh.x--; break;
    case GDK_KEY_Right: nh.x++; break;
    }
    // 處理邊界循環
    if (nh.x < 0) nh.x = GRID_WIDTH - 1;
    else if (nh.x >= GRID_WIDTH) nh.x = 0;
    if (nh.y < 0) nh.y = GRID_HEIGHT - 1;
    else if (nh.y >= GRID_HEIGHT) nh.y = 0;

    // 檢查碰撞
    if (check_collision_single(&nh)) {
        kill_player_single();
        if (single_timer_id) {
            g_source_remove(single_timer_id);
            single_timer_id = 0;
        }
        return G_SOURCE_CONTINUE;
    }

    // 添加新的蛇頭
    Point* new_seg = (Point*)malloc(sizeof(Point));
    *new_seg = nh;
    snake_single = g_list_prepend(snake_single, new_seg);

    // 檢查是否吃到食物
    if (nh.x == food_single.x && nh.y == food_single.y) {
        score_single++; // 增加分數
        generate_food_single(); // 生成新的食物

        // 播放吃果實的音效
        play_sound_effect("Musics/eat_fruit.mp3", FALSE, 1.0); // 不循環
    }
    else {
        // 移除蛇尾
        Point* tail = (Point*)g_list_last(snake_single)->data;
        snake_single = g_list_remove(snake_single, tail);
        free(tail);
    }

    // 重繪畫布
    gtk_widget_queue_draw(canvas_single);
    return G_SOURCE_CONTINUE;
}

// 顯示單人模式遊戲結束畫面的函式
static void show_game_over_screen_single(void)
{
    GtkStack* stack_ptr = GTK_STACK(stack);
    GtkWidget* existing_game_over_single = gtk_stack_get_child_by_name(stack_ptr, "game_over_single");
    if (existing_game_over_single) {
        gtk_stack_remove(stack_ptr, existing_game_over_single);
    }

    // 創建垂直容器
    GtkWidget* game_over_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(game_over_vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(game_over_vbox, GTK_ALIGN_CENTER);

    // 創建結束遊戲的標籤，顯示最終得分
    char buf[128];
    snprintf(buf, sizeof(buf), "遊戲結束！\n最終得分: %d", score_single);
    GtkWidget* label = gtk_label_new(buf);
    gtk_box_append(GTK_BOX(game_over_vbox), label);

    // 添加「返回主選單」按鈕
    GtkWidget* back_btn = gtk_button_new_with_label("返回主選單");
    gtk_box_append(GTK_BOX(game_over_vbox), back_btn);
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_to_menu_clicked), NULL);

    // 播放按鈕點擊音效
    play_sound_effect("Musics/button_click.mp3", FALSE, 1.0); // 不循環

    // 將結束畫面加入到GtkStack並顯示
    gtk_stack_add_named(stack_ptr, game_over_vbox, "game_over_single");
    gtk_stack_set_visible_child_name(stack_ptr, "game_over_single");
}

//==============================================================
// [ 雙人模式(玩家 vs 玩家) ]
//==============================================================
// 初始化雙人遊戲
static void init_multi_game(void)
{
    clear_game_data(); // 清理之前的遊戲資料

    // 玩家1 初始位置設置在左側四分之一處
    Point* h1 = (Point*)malloc(sizeof(Point));
    h1->x = GRID_WIDTH / 4;
    h1->y = GRID_HEIGHT / 2;
    snake1 = g_list_append(snake1, h1);

    // 玩家2 初始位置設置在右側四分之三處
    Point* h2 = (Point*)malloc(sizeof(Point));
    h2->x = (GRID_WIDTH * 3) / 4;
    h2->y = GRID_HEIGHT / 2;
    snake2 = g_list_append(snake2, h2);

    // 隨機初始方向
    direction1 = get_random_direction();
    next_direction1 = direction1;
    direction2 = get_random_direction();
    next_direction2 = direction2;

    // 初始化分數和生存狀態
    score1 = 0;
    score2 = 0;
    player1_alive = TRUE;
    player2_alive = TRUE;
    winner = 0;

    // 設置雙人模式的開始時間
    start_time_multi = time(NULL);
    dead_time_player1 = 0;
    dead_time_player2 = 0;

    game_over = FALSE;
    paused = FALSE;

    // 收集初始位置，避免障礙物生成在蛇附近
    GList* initial_positions = NULL;
    initial_positions = g_list_append(initial_positions, h1);
    initial_positions = g_list_append(initial_positions, h2);

    generate_obstacles(initial_positions); // 生成障礙物
    generate_food_multi();               // 生成食物
    start_countdown();                    // 開始倒數計時

    // 設置玩家1和玩家2的獨立定時器
    player1_timer_id = g_timeout_add(current_interval_player1, update_player1, NULL);
    player2_timer_id = g_timeout_add(current_interval_player2, update_player2, NULL);

    g_list_free(initial_positions); // 釋放初始位置鏈表
}

// 生成雙人模式下的食物
static void generate_food_multi(void)
{
    gboolean valid = FALSE;
    while (!valid) {
        food_multi.x = rand() % GRID_WIDTH;  // 隨機X位置
        food_multi.y = rand() % GRID_HEIGHT; // 隨機Y位置
        valid = TRUE;
        // 檢查食物是否與玩家1的蛇重疊
        for (GList* it = snake1; it; it = it->next) {
            Point* seg = (Point*)it->data;
            if (seg->x == food_multi.x && seg->y == food_multi.y) {
                valid = FALSE;
                break;
            }
        }
        // 檢查食物是否與玩家2的蛇重疊
        for (GList* it = snake2; it; it = it->next) {
            Point* seg = (Point*)it->data;
            if (seg->x == food_multi.x && seg->y == food_multi.y) {
                valid = FALSE;
                break;
            }
        }
        // 檢查食物是否與障礙物重疊
        if (valid) {
            for (GList* iter = obstacles; iter; iter = iter->next) {
                Obstacle* obs = (Obstacle*)iter->data;
                if (food_multi.x >= obs->x && food_multi.x < obs->x + obs->width &&
                    food_multi.y >= obs->y && food_multi.y < obs->y + obs->height) {
                    valid = FALSE;
                    break;
                }
            }
        }
    }
}

// 檢查蛇是否自撞
static gboolean check_self_collision(Point* head, GList* snake)
{
    for (GList* iter = snake->next; iter; iter = iter->next) {
        Point* seg = (Point*)iter->data;
        if (seg->x == head->x && seg->y == head->y) return TRUE;
    }
    return FALSE;
}

// 檢查蛇是否撞到另一條蛇
static gboolean check_snake_collision(Point* head, GList* snake)
{
    for (GList* iter = snake; iter; iter = iter->next) {
        Point* seg = (Point*)iter->data;
        if (seg->x == head->x && seg->y == head->y) return TRUE;
    }
    return FALSE;
}

// 玩家1死亡的處理函式
static void kill_player1(void)
{
    // 記錄玩家1存活的秒數 (死亡時的時間 - 遊戲開始時間)
    if (dead_time_player1 == 0) {
        dead_time_player1 = time(NULL) - start_time_multi;
    }

    // 播放蛇死亡音效
    play_sound_effect("Musics/snake_die.mp3", FALSE, 1.0); // 不循環

    // 啟動蛇的閃爍效果
    start_flicker_snake(&snake1, &player1_alive, canvas_multi);
}

// 玩家2死亡的處理函式
static void kill_player2(void)
{
    // 記錄玩家2存活的秒數 (死亡時的時間 - 遊戲開始時間)
    if (dead_time_player2 == 0) {
        dead_time_player2 = time(NULL) - start_time_multi;
    }

    // 播放蛇死亡音效
    play_sound_effect("Musics/snake_die.mp3", FALSE, 1.0); // 不循環

    // 啟動蛇的閃爍效果
    start_flicker_snake(&snake2, &player2_alive, canvas_multi);
}

// 結束雙人遊戲的處理函式
static void end_two_player_game(void)
{
    // 若兩人都死亡，則結束遊戲並結算勝負
    if (!player1_alive && !player2_alive) {
        game_over = TRUE;

        // 根據分數決定勝利者
        if (score1 > score2) {
            winner = 1;
        }
        else if (score2 > score1) {
            winner = 2;
        }
        else {
            // 分數相同，根據存活時間決定
            if (dead_time_player1 > dead_time_player2) {
                winner = 1;
            }
            else if (dead_time_player2 > dead_time_player1) {
                winner = 2;
            }
            else {
                // 分數和存活時間都相同，判定為平局
                winner = 0; // 0 表示平局
            }
        }

        // 移除玩家1和玩家2的定時器
        if (player1_timer_id) { g_source_remove(player1_timer_id); player1_timer_id = 0; }
        if (player2_timer_id) { g_source_remove(player2_timer_id); player2_timer_id = 0; }

        // 停止遊戲背景音樂
        if (game_background_music) {
            gst_element_set_state(game_background_music->pipeline, GST_STATE_NULL);
            gst_object_unref(game_background_music->pipeline);
            free(game_background_music);
            game_background_music = NULL;
        }

        // 停止倒數音效
        if (countdown_music) {
            gst_element_set_state(countdown_music->pipeline, GST_STATE_NULL);
            gst_object_unref(countdown_music->pipeline);
            free(countdown_music);
            countdown_music = NULL;
        }

        // 重繪畫布並顯示遊戲結束畫面
        gtk_widget_queue_draw(canvas_multi);
        show_game_over_screen_multi();
    }
}

// 更新玩家1狀態的定時器回調函式
static gboolean update_player1(gpointer data)
{
    // 如果當前模式不是雙人模式，或者遊戲已結束、暫停，或者玩家1已死亡，則不進行更新
    if (current_mode != MODE_MULTI || game_over || paused || !player1_alive)
        return G_SOURCE_CONTINUE;
    if (!game_started)
        return G_SOURCE_CONTINUE;

    // 更新方向
    direction1 = next_direction1;

    // 計算新的蛇頭位置
    Point* head = (Point*)snake1->data;
    if (!head) { // 確保蛇頭存在
        return G_SOURCE_CONTINUE;
    }
    Point nh = { head->x, head->y };
    switch (direction1) {
    case GDK_KEY_Up:    nh.y--; break;
    case GDK_KEY_Down:  nh.y++; break;
    case GDK_KEY_Left:  nh.x--; break;
    case GDK_KEY_Right: nh.x++; break;
    }
    // 處理邊界循環
    if (nh.x < 0) nh.x = GRID_WIDTH - 1;
    else if (nh.x >= GRID_WIDTH) nh.x = 0;
    if (nh.y < 0) nh.y = GRID_HEIGHT - 1;
    else if (nh.y >= GRID_HEIGHT) nh.y = 0;

    // 自撞檢查
    if (check_self_collision(&nh, snake1)) {
        kill_player1();
        if (player1_timer_id) {
            g_source_remove(player1_timer_id);
            player1_timer_id = 0;
        }
        // 不立即調用 end_two_player_game，改由閃爍完成後調用
        return G_SOURCE_CONTINUE;
    }
    // 撞障礙物檢查
    for (GList* iter = obstacles; iter; iter = iter->next) {
        Obstacle* obs = (Obstacle*)iter->data;
        if (nh.x >= obs->x && nh.x < obs->x + obs->width &&
            nh.y >= obs->y && nh.y < obs->y + obs->height) {
            kill_player1();
            if (player1_timer_id) {
                g_source_remove(player1_timer_id);
                player1_timer_id = 0;
            }
            return G_SOURCE_CONTINUE;
        }
    }
    // 撞到另一條蛇的檢查
    if (player2_alive) {
        if (check_snake_collision(&nh, snake2)) {
            kill_player1();
            if (player1_timer_id) {
                g_source_remove(player1_timer_id);
                player1_timer_id = 0;
            }
            return G_SOURCE_CONTINUE;
        }
    }

    // 添加新的蛇頭
    Point* new_seg = (Point*)malloc(sizeof(Point));
    *new_seg = nh;
    snake1 = g_list_prepend(snake1, new_seg);

    // 檢查是否吃到食物
    if (nh.x == food_multi.x && nh.y == food_multi.y) {
        score1++; // 增加分數
        generate_food_multi(); // 生成新的食物

        // 播放吃果實的音效
        play_sound_effect("Musics/eat_fruit.mp3", FALSE, 1.0); // 不循環

        // 降低速度 (增加移動間隔)
        if (current_interval_player1 < MAX_INTERVAL) { // 限制最大移動間隔
            current_interval_player1 += 5;
            if (player1_timer_id) g_source_remove(player1_timer_id);
            player1_timer_id = g_timeout_add(current_interval_player1, update_player1, NULL);
        }
    }
    else {
        // 移除蛇尾
        Point* tail = (Point*)g_list_last(snake1)->data;
        snake1 = g_list_remove(snake1, tail);
        free(tail);
    }

    // 重繪畫布
    gtk_widget_queue_draw(canvas_multi);
    return G_SOURCE_CONTINUE;
}

// 更新玩家2狀態的定時器回調函式
static gboolean update_player2(gpointer data)
{
    // 如果當前模式不是雙人模式，或者遊戲已結束、暫停，或者玩家2已死亡，則不進行更新
    if (current_mode != MODE_MULTI || game_over || paused || !player2_alive)
        return G_SOURCE_CONTINUE;
    if (!game_started)
        return G_SOURCE_CONTINUE;

    // 更新方向
    direction2 = next_direction2;

    // 計算新的蛇頭位置
    Point* head = (Point*)snake2->data;
    if (!head) { // 確保蛇頭存在
        return G_SOURCE_CONTINUE;
    }
    Point nh = { head->x, head->y };
    switch (direction2) {
    case GDK_KEY_Up:    nh.y--; break;
    case GDK_KEY_Down:  nh.y++; break;
    case GDK_KEY_Left:  nh.x--; break;
    case GDK_KEY_Right: nh.x++; break;
    }
    // 處理邊界循環
    if (nh.x < 0) nh.x = GRID_WIDTH - 1;
    else if (nh.x >= GRID_WIDTH) nh.x = 0;
    if (nh.y < 0) nh.y = GRID_HEIGHT - 1;
    else if (nh.y >= GRID_HEIGHT) nh.y = 0;

    // 自撞檢查
    if (check_self_collision(&nh, snake2)) {
        kill_player2();
        if (player2_timer_id) {
            g_source_remove(player2_timer_id);
            player2_timer_id = 0;
        }
        // 不立即調用 end_two_player_game，改由閃爍完成後調用
        return G_SOURCE_CONTINUE;
    }
    // 撞障礙物檢查
    for (GList* iter = obstacles; iter; iter = iter->next) {
        Obstacle* obs = (Obstacle*)iter->data;
        if (nh.x >= obs->x && nh.x < obs->x + obs->width &&
            nh.y >= obs->y && nh.y < obs->y + obs->height) {
            kill_player2();
            if (player2_timer_id) {
                g_source_remove(player2_timer_id);
                player2_timer_id = 0;
            }
            return G_SOURCE_CONTINUE;
        }
    }
    // 撞到另一條蛇的檢查
    if (player1_alive) {
        if (check_snake_collision(&nh, snake1)) {
            kill_player2();
            if (player2_timer_id) {
                g_source_remove(player2_timer_id);
                player2_timer_id = 0;
            }
            return G_SOURCE_CONTINUE;
        }
    }

    // 添加新的蛇頭
    Point* new_seg = (Point*)malloc(sizeof(Point));
    *new_seg = nh;
    snake2 = g_list_prepend(snake2, new_seg);

    // 檢查是否吃到食物
    if (nh.x == food_multi.x && nh.y == food_multi.y) {
        score2++; // 增加分數
        generate_food_multi(); // 生成新的食物

        // 播放吃果實的音效
        play_sound_effect("Musics/eat_fruit.mp3", FALSE, 1.0); // 不循環

        // 降低速度 (增加移動間隔)
        if (current_interval_player2 < MAX_INTERVAL) { // 限制最大移動間隔
            current_interval_player2 += 5;
            if (player2_timer_id) g_source_remove(player2_timer_id);
            player2_timer_id = g_timeout_add(current_interval_player2, update_player2, NULL);
        }
    }
    else {
        // 移除蛇尾
        Point* tail = (Point*)g_list_last(snake2)->data;
        snake2 = g_list_remove(snake2, tail);
        free(tail);
    }

    // 重繪畫布
    gtk_widget_queue_draw(canvas_multi);
    return G_SOURCE_CONTINUE;
}

// 顯示雙人模式遊戲結束畫面的函式
static void show_game_over_screen_multi(void)
{
    GtkStack* stack_ptr = GTK_STACK(stack);
    GtkWidget* existing_game_over_multi = gtk_stack_get_child_by_name(stack_ptr, "game_over_multi");
    if (existing_game_over_multi) {
        gtk_stack_remove(stack_ptr, existing_game_over_multi);
    }

    // 創建垂直容器
    GtkWidget* game_over_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(game_over_vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(game_over_vbox, GTK_ALIGN_CENTER);

    // 根據勝利者設置結果描述
    const char* res = NULL;
    if (winner == 1) {
        res = "玩家1 (綠色) 獲勝！";
    }
    else if (winner == 2) {
        res = "玩家2 (橘色) 獲勝！";
    }
    else {
        // winner == 0 => 平局
        res = "平局！";
    }

    // 創建結束遊戲的標籤，顯示分數和結果
    char buf[256];
    snprintf(buf, sizeof(buf),
        "遊戲結束！\n玩家1:%d   玩家2:%d\n\n%s\n\n(玩家1存活: %ld 秒, 玩家2存活: %ld 秒)",
        score1, score2, res, (long)dead_time_player1, (long)dead_time_player2);
    GtkWidget* label = gtk_label_new(buf);
    gtk_box_append(GTK_BOX(game_over_vbox), label);

    // 添加「返回主選單」按鈕
    GtkWidget* back_btn = gtk_button_new_with_label("返回主選單");
    gtk_box_append(GTK_BOX(game_over_vbox), back_btn);
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_to_menu_clicked), NULL);

    // 播放按鈕點擊音效
    play_sound_effect("Musics/button_click.mp3", FALSE, 1.0); // 不循環

    // 將結束畫面加入到GtkStack並顯示
    gtk_stack_add_named(stack_ptr, game_over_vbox, "game_over_multi");
    gtk_stack_set_visible_child_name(stack_ptr, "game_over_multi");
}

//==============================================================
// [ 繪圖：蛇/牆/果實 ]
//==============================================================
// 繪製蛇的一個節點，包括身體和陰影
static void draw_snake_segment(cairo_t* cr, double x, double y, double size,
    GdkRGBA body, GdkRGBA shadow)
{
    // 繪製陰影
    cairo_set_source_rgba(cr, shadow.red, shadow.green, shadow.blue, shadow.alpha);
    double off = 2; // 偏移量，用於創建陰影效果
    cairo_rectangle(cr, x + off, y + off, size, size);
    cairo_fill(cr);

    // 繪製蛇本體
    cairo_set_source_rgba(cr, body.red, body.green, body.blue, body.alpha);
    cairo_rectangle(cr, x, y, size, size);
    cairo_fill(cr);
}

// 繪製食物，包括本體和陰影
static void draw_food(cairo_t* cr, double x, double y, double size)
{
    // 繪製陰影
    cairo_set_source_rgb(cr, 0.5, 0.0, 0.0); // 深紅色
    cairo_rectangle(cr, x + 2, y + 2, size, size);
    cairo_fill(cr);

    // 繪製食物本體 (紅色)
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0); // 紅色
    cairo_rectangle(cr, x, y, size, size);
    cairo_fill(cr);
}

// 繪製牆壁，包括本體和陰影
static void draw_wall(cairo_t* cr, double x, double y, double w, double h)
{
    // 繪製陰影
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2); // 灰色
    cairo_rectangle(cr, x + 2, y + 2, w, h);
    cairo_fill(cr);

    // 繪製牆本體
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5); // 較亮的灰色
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
}

//==============================================================
// [ 繪製各模式 ]
//==============================================================
// 繪製單人模式的遊戲畫面
static void draw_single_mode(cairo_t* cr, int width, int height, double cell_size)
{
    // 設置背景顏色
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); // 深灰色
    cairo_paint(cr);

    // 繪製食物
    draw_food(cr, food_single.x * cell_size, food_single.y * cell_size, cell_size);

    // 繪製障礙物
    for (GList* iter = obstacles; iter; iter = iter->next) {
        Obstacle* obs = (Obstacle*)iter->data;
        draw_wall(cr, obs->x * cell_size, obs->y * cell_size,
            obs->width * cell_size, obs->height * cell_size);
    }

    // 繪製蛇 (綠色)
    GdkRGBA body_green = { 0.0,1.0,0.0,1.0 };      // 蛇身體顏色
    GdkRGBA shadow_green = { 0.0,0.4,0.0,1.0 };    // 蛇陰影顏色
    for (GList* iter = snake_single; iter; iter = iter->next) {
        Point* seg = (Point*)iter->data;
        if (seg) { // 確保節點存在
            draw_snake_segment(cr, seg->x * cell_size, seg->y * cell_size,
                cell_size, body_green, shadow_green);
        }
    }

    // 繪製分數
    cairo_set_source_rgb(cr, 1, 1, 1); // 白色
    // 設置字體為 "Cubic 11"
    cairo_select_font_face(cr, "Cubic 11",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 16);
    char buf[64];
    snprintf(buf, sizeof(buf), "分數: %d", score_single);
    cairo_move_to(cr, 10, 25); // 設置文字位置
    cairo_show_text(cr, buf);   // 顯示文字

    // 繪製倒數
    if (!game_started) {
        cairo_set_source_rgb(cr, 1, 1, 1); // 白色
        cairo_set_font_size(cr, 40);       // 大字體
        if (countdown > 0) {
            char cbuf[8];
            snprintf(cbuf, sizeof(cbuf), "%d", countdown);
            cairo_move_to(cr, width / 2 - 20, height / 2); // 設置文字位置
            cairo_show_text(cr, cbuf);                     // 顯示倒數數字
        }
        else if (show_go && !game_started) {
            cairo_move_to(cr, width / 2 - 30, height / 2);
            cairo_show_text(cr, "開始！");                   // 顯示「開始！」字樣
        }
    }
}

// 繪製雙人模式的遊戲畫面
static void draw_multi_mode(cairo_t* cr, int width, int height, double cell_size)
{
    // 設置背景顏色
    cairo_set_source_rgb(cr, 0.12, 0.12, 0.12); // 深灰色
    cairo_paint(cr);

    // 繪製食物
    draw_food(cr, food_multi.x * cell_size, food_multi.y * cell_size, cell_size);

    // 繪製障礙物
    for (GList* it = obstacles; it; it = it->next) {
        Obstacle* obs = (Obstacle*)it->data;
        draw_wall(cr, obs->x * cell_size, obs->y * cell_size,
            obs->width * cell_size, obs->height * cell_size);
    }

    // 繪製玩家1的蛇 (綠色)
    GdkRGBA b1 = { 0.0,1.0,0.0,1.0 };           // 蛇身體顏色
    GdkRGBA s1 = { 0.0,0.4,0.0,1.0 };           // 蛇陰影顏色
    for (GList* it = snake1; it; it = it->next) {
        Point* seg = (Point*)it->data;
        if (seg) { // 確保節點存在
            draw_snake_segment(cr, seg->x * cell_size, seg->y * cell_size,
                cell_size, b1, s1);
        }
    }

    // 繪製玩家2的蛇 (橘色)
    GdkRGBA b2 = { 1.0,0.5,0.0,1.0 };           // 蛇身體顏色
    GdkRGBA s2 = { 0.4,0.2,0.0,1.0 };           // 蛇陰影顏色
    for (GList* it = snake2; it; it = it->next) {
        Point* seg = (Point*)it->data;
        if (seg) { // 確保節點存在
            draw_snake_segment(cr, seg->x * cell_size, seg->y * cell_size,
                cell_size, b2, s2);
        }
    }

    // 繪製分數
    cairo_set_source_rgb(cr, 1, 1, 1); // 白色
    // 設置字體為 "Cubic 11"
    cairo_select_font_face(cr, "Cubic 11",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 16);
    char buf[64];
    snprintf(buf, sizeof(buf), "玩家1:%d   玩家2:%d", score1, score2);
    cairo_move_to(cr, 10, 25); // 設置文字位置
    cairo_show_text(cr, buf);   // 顯示分數

    // 繪製倒數
    if (!game_started) {
        cairo_set_source_rgb(cr, 1, 1, 1); // 白色
        cairo_set_font_size(cr, 40);       // 大字體
        if (countdown > 0) {
            char cbuf[8];
            snprintf(cbuf, sizeof(cbuf), "%d", countdown);
            cairo_move_to(cr, width / 2 - 20, height / 2); // 設置文字位置
            cairo_show_text(cr, cbuf);                     // 顯示倒數數字
        }
        else if (show_go && !game_started) {
            cairo_move_to(cr, width / 2 - 30, height / 2);
            cairo_show_text(cr, "開始！");                   // 顯示「開始！」字樣
        }
    }
}

//==============================================================
// [ 繪圖區域 DrawFunc ]
//==============================================================
// GtkDrawingArea的繪圖回調函式，根據當前遊戲模式調用不同的繪圖函式
static void draw_game(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data)
{
    if (current_mode == MODE_INTRO) {
        return; // 遊戲介紹模式不需要繪製
    }

    switch (current_mode) {
    case MODE_SINGLE:
        draw_single_mode(cr, width, height, CELL_SIZE); // 繪製單人模式
        break;
    case MODE_MULTI:
        draw_multi_mode(cr, width, height, CELL_SIZE);  // 繪製雙人模式
        break;
    default:
        break;
    }
}

//==============================================================
// [ KeyPress ]
//==============================================================
// 處理鍵盤按鍵事件，用於控制蛇的移動方向和暫停遊戲
static gboolean on_key_press(GtkEventControllerKey* controller,
    guint keyval, guint keycode,
    GdkModifierType state,
    gpointer user_data)
{
    // 在遊戲開始後、非暫停、未結束時，才處理轉向
    if ((current_mode == MODE_SINGLE || current_mode == MODE_MULTI)
        && game_started && !paused && !game_over)
    {
        // 單人模式下的方向控制
        if (current_mode == MODE_SINGLE) {
            switch (keyval) {
            case GDK_KEY_Up:
                if (next_direction_single != GDK_KEY_Down)
                    next_direction_single = GDK_KEY_Up;
                break;
            case GDK_KEY_Down:
                if (next_direction_single != GDK_KEY_Up)
                    next_direction_single = GDK_KEY_Down;
                break;
            case GDK_KEY_Left:
                if (next_direction_single != GDK_KEY_Right)
                    next_direction_single = GDK_KEY_Left;
                break;
            case GDK_KEY_Right:
                if (next_direction_single != GDK_KEY_Left)
                    next_direction_single = GDK_KEY_Right;
                break;
            }
        }
        // 雙人模式下的方向控制
        else if (current_mode == MODE_MULTI) {
            // 玩家1 => WASD
            switch (keyval) {
            case GDK_KEY_w:
            case GDK_KEY_W:
                if (next_direction1 != GDK_KEY_Down)
                    next_direction1 = GDK_KEY_Up;
                break;
            case GDK_KEY_s:
            case GDK_KEY_S:
                if (next_direction1 != GDK_KEY_Up)
                    next_direction1 = GDK_KEY_Down;
                break;
            case GDK_KEY_a:
            case GDK_KEY_A:
                if (next_direction1 != GDK_KEY_Right)
                    next_direction1 = GDK_KEY_Left;
                break;
            case GDK_KEY_d:
            case GDK_KEY_D:
                if (next_direction1 != GDK_KEY_Left)
                    next_direction1 = GDK_KEY_Right;
                break;
            }
            // 玩家2 => 方向鍵
            switch (keyval) {
            case GDK_KEY_Up:
                if (next_direction2 != GDK_KEY_Down) next_direction2 = GDK_KEY_Up;
                break;
            case GDK_KEY_Down:
                if (next_direction2 != GDK_KEY_Up)   next_direction2 = GDK_KEY_Down;
                break;
            case GDK_KEY_Left:
                if (next_direction2 != GDK_KEY_Right) next_direction2 = GDK_KEY_Left;
                break;
            case GDK_KEY_Right:
                if (next_direction2 != GDK_KEY_Left)  next_direction2 = GDK_KEY_Right;
                break;
            }
        }
    }

    // ESC鍵用於暫停/繼續遊戲
    if ((current_mode == MODE_SINGLE || current_mode == MODE_MULTI)
        && !game_over)
    {
        if (keyval == GDK_KEY_Escape) {
            paused = !paused; // 切換暫停狀態
            if (paused) {
                create_pause_dialog(); // 創建暫停對話框
            }
            else if (pause_dialog) {
                gtk_window_destroy(GTK_WINDOW(pause_dialog));
                pause_dialog = NULL;

                // 恢復遊戲背景音樂
                if (game_background_music) {
                    gst_element_set_state(game_background_music->pipeline, GST_STATE_PLAYING);
                }

                // 恢復倒數音效
                if (countdown_music) {
                    gst_element_set_state(countdown_music->pipeline, GST_STATE_PLAYING);
                }

                // 恢復倒數計時器 (如果遊戲尚未開始)
                if (!game_started && (countdown > 0 || show_go == FALSE)) {
                    countdown_timer_id = g_timeout_add(500, countdown_tick, NULL);
                }
            }
            return TRUE; // 表示事件已處理
        }
    }

    return FALSE; // 事件未處理，繼續傳遞
}

//==============================================================
// [ 主選單 & 按鈕 ]
//==============================================================
// 單人模式按鈕的回調函式
static void on_single_mode_clicked(GtkButton* button, gpointer user_data)
{
    GtkStack* stack_ptr = GTK_STACK(user_data);
    current_mode = MODE_SINGLE; // 設置當前模式為單人模式

    // 播放按鈕點擊音效
    play_sound_effect("Musics/button_click.mp3", FALSE, 1.0); // 不循環

    // 停止主選單背景音樂
    stop_main_menu_music();

    // 初始化單人遊戲
    init_single_game();

    // 從GtkStack中移除之前的單人模式視圖（若存在）
    GtkWidget* existing_game_single = gtk_stack_get_child_by_name(stack_ptr, "game_single");
    if (existing_game_single) {
        gtk_stack_remove(stack_ptr, existing_game_single);
    }

    // 創建新的繪圖區域
    GtkWidget* new_canvas = gtk_drawing_area_new();
    gtk_widget_set_size_request(new_canvas, 1800, 900); // 設置畫布大小
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(new_canvas),
        draw_game, NULL, NULL); // 設置繪圖回調函式

    // 創建水平容器並添加畫布
    GtkWidget* container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(container, TRUE);
    gtk_widget_set_vexpand(container, TRUE);
    gtk_widget_set_halign(container, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(container, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(container), new_canvas);

    // 將新畫布加入到GtkStack並顯示
    gtk_stack_add_named(stack_ptr, container, "game_single");
    canvas_single = new_canvas;
    gtk_stack_set_visible_child_name(stack_ptr, "game_single");
}

// 雙人模式按鈕的回調函式
static void on_multi_mode_clicked(GtkButton* button, gpointer user_data)
{
    GtkStack* stack_ptr = GTK_STACK(user_data);
    current_mode = MODE_MULTI; // 設置當前模式為雙人模式

    // 播放按鈕點擊音效
    play_sound_effect("Musics/button_click.mp3", FALSE, 1.0); // 不循環

    // 停止主選單背景音樂
    stop_main_menu_music();

    // 初始化雙人遊戲
    init_multi_game();

    // 從GtkStack中移除之前的雙人模式視圖（若存在）
    GtkWidget* existing_game_multi = gtk_stack_get_child_by_name(stack_ptr, "game_multi");
    if (existing_game_multi) {
        gtk_stack_remove(stack_ptr, existing_game_multi);
    }

    // 創建新的繪圖區域
    GtkWidget* new_canvas = gtk_drawing_area_new();
    gtk_widget_set_size_request(new_canvas, 1800, 900); // 設置畫布大小
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(new_canvas),
        draw_game, NULL, NULL); // 設置繪圖回調函式

    // 創建水平容器並添加畫布
    GtkWidget* container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(container, TRUE);
    gtk_widget_set_vexpand(container, TRUE);
    gtk_widget_set_halign(container, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(container, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(container), new_canvas);

    // 將新畫布加入到GtkStack並顯示
    gtk_stack_add_named(stack_ptr, container, "game_multi");
    canvas_multi = new_canvas;
    gtk_stack_set_visible_child_name(stack_ptr, "game_multi");
}

// 遊戲介紹按鈕的回調函式
static void on_intro_mode_clicked(GtkButton* button, gpointer user_data)
{
    GtkStack* stack_ptr = GTK_STACK(user_data);
    current_mode = MODE_INTRO; // 設置當前模式為遊戲介紹

    // 播放按鈕點擊音效
    play_sound_effect("Musics/button_click.mp3", FALSE, 1.0); // 不循環

    // 停止主選單背景音樂
    stop_main_menu_music();

    // 顯示遊戲介紹視圖
    gtk_stack_set_visible_child_name(stack_ptr, "game_intro");
}

//==============================================================
// [ 主選單 ]
//==============================================================
// 顯示主選單的函式
static void show_main_menu(void)
{
    if (stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(stack), "main_menu"); // 設置顯示主選單
    }
}

//==============================================================
// [ GStreamer 音效播放功能 ]
//==============================================================

// Bus回調函式，用於處理GStreamer的訊息
static gboolean bus_call(GstBus* bus, GstMessage* msg, gpointer data)
{
    SoundData* sound_data = (SoundData*)data;

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS: // 當音效播放結束
        if (sound_data->loop) {
            // 如果設定為循環，則重新播放
            gst_element_seek_simple(sound_data->pipeline, GST_FORMAT_TIME,
                GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, 0);
            g_print("Looping sound: Playing in loop\n");
        }
        else {
            // 否則停止播放並釋放資源
            gst_element_set_state(sound_data->pipeline, GST_STATE_NULL);
            gst_object_unref(sound_data->pipeline);
            free(sound_data);

            // 如果是倒數音效，將全域 countdown_music 設為 NULL
            if (sound_data == countdown_music) {
                countdown_music = NULL;
            }
        }
        break;
    case GST_MESSAGE_ERROR: // 處理播放錯誤
    {
        GError* err;
        gchar* debug;

        gst_message_parse_error(msg, &err, &debug);
        g_printerr("Error: %s\n", err->message);
        g_error_free(err);
        g_free(debug);

        // 停止播放並釋放資源
        gst_element_set_state(sound_data->pipeline, GST_STATE_NULL);
        gst_object_unref(sound_data->pipeline);

        // 如果是倒數音效，將全域 countdown_music 設為 NULL
        if (sound_data == countdown_music) {
            countdown_music = NULL;
        }

        free(sound_data);
    }
    break;
    default:
        break;
    }

    return TRUE;
}

// 播放音效的函式
static SoundData* play_sound_effect(const char* filename, gboolean loop, float volume_level)
{
    // 檢查音效檔案是否存在
    if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
        g_printerr("Error: 音效檔案不存在: %s\n", filename);
        return NULL;
    }

    // 創建 SoundData 結構
    SoundData* sound_data = (SoundData*)malloc(sizeof(SoundData));
    if (!sound_data) {
        g_printerr("Failed to allocate memory for SoundData.\n");
        return NULL;
    }
    sound_data->loop = loop;

    // 創建 GStreamer 的 Pipeline 元件
    GstElement* pipeline = gst_pipeline_new(NULL);
    if (!pipeline) {
        g_printerr("Failed to create GStreamer pipeline.\n");
        free(sound_data);
        return NULL;
    }
    sound_data->pipeline = pipeline;

    // 創建必要的 GStreamer 元件
    GstElement* filesrc = gst_element_factory_make("filesrc", "file-source");       // 檔案來源
    GstElement* decodebin = gst_element_factory_make("decodebin", "decoder");      // 解碼器
    GstElement* audioconvert = gst_element_factory_make("audioconvert", "converter"); // 音頻轉換器
    GstElement* audioresample = gst_element_factory_make("audioresample", "resampler"); // 音頻重採樣器
    GstElement* volume = gst_element_factory_make("volume", "volume");             // 音量控制器
    GstElement* sink = gst_element_factory_make("autoaudiosink", "audio-output");  // 音頻輸出

    if (!filesrc || !decodebin || !audioconvert || !audioresample || !volume || !sink) {
        g_printerr("Failed to create GStreamer elements.\n");
        gst_object_unref(pipeline);
        free(sound_data);
        return NULL;
    }

    // 設置檔案來源的屬性
    g_object_set(G_OBJECT(filesrc), "location", filename, NULL);

    // 設置音量
    g_object_set(G_OBJECT(volume), "volume", volume_level, NULL);

    // 將元素添加到 Pipeline 中
    gst_bin_add_many(GST_BIN(pipeline), filesrc, decodebin, audioconvert, audioresample, volume, sink, NULL);

    // 連接 filesrc 到 decodebin
    if (!gst_element_link(filesrc, decodebin)) {
        g_printerr("Failed to link filesrc to decodebin.\n");
        gst_object_unref(pipeline);
        free(sound_data);
        return NULL;
    }

    // 連接 decodebin 的 pad 到 audioconvert
    g_signal_connect(decodebin, "pad-added", G_CALLBACK(on_pad_added), audioconvert);

    // 連接 audioconvert -> audioresample -> volume -> sink
    if (!gst_element_link_many(audioconvert, audioresample, volume, sink, NULL)) {
        g_printerr("Failed to link audio elements to sink.\n");
        gst_object_unref(pipeline);
        free(sound_data);
        return NULL;
    }

    // 獲取 Bus 並添加 bus watch 以處理訊息
    GstBus* bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, bus_call, sound_data);
    gst_object_unref(bus);

    // 開始播放
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Failed to set pipeline to PLAYING for file: %s\n", filename);
        gst_object_unref(pipeline);
        free(sound_data);
        return NULL;
    }

    // 返回 SoundData 結構
    return sound_data;
}

// 解碼器 pad 新增回調，用於動態連接解碼後的音頻流
static void on_pad_added(GstElement* src, GstPad* new_pad, gpointer data)
{
    GstElement* audioconvert = (GstElement*)data; // 獲取 audioconvert 元件
    GstPad* sink_pad = gst_element_get_static_pad(audioconvert, "sink"); // 獲取 sink pad
    GstPadLinkReturn ret;

    if (gst_pad_is_linked(sink_pad)) {
        gst_object_unref(sink_pad);
        return; // 如果已經連接，則不做任何操作
    }

    // 嘗試連接新 pad 到 audioconvert 的 sink pad
    ret = gst_pad_link(new_pad, sink_pad);
    if (GST_PAD_LINK_FAILED(ret)) {
        g_printerr("Failed to link decodebin to audioconvert.\n");
    }

    gst_object_unref(sink_pad);
}

//==============================================================
// [ 程式入口點 activate & main ]
//==============================================================
// GtkApplication的activate回調函式，負責創建和初始化主視窗及其他視圖
static void activate(GtkApplication* app, gpointer user_data)
{
    // 創建主視窗
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Snake Hero 蛇之英雄");
    gtk_window_set_default_size(GTK_WINDOW(window), 1920, 1080); // 設置預設大小

    // 加載CSS樣式
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        "button {\n"
        "  min-width: 200px;\n" // 設置按鈕最小寬度
        "  min-height: 50px;\n" // 設置按鈕最小高度
        "}\n"
        "label {\n"
        "  font-family: \"Cubic 11\";\n" // 設置標籤字體
        "  font-size: 23px;\n"           // 設置標籤字體大小
        "}\n"
    );
    GdkDisplay* dsp = gdk_display_get_default();
    gtk_style_context_add_provider_for_display(
        dsp,
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);

    // 創建GtkStack，用於管理不同的視圖
    stack = gtk_stack_new();
    gtk_window_set_child(GTK_WINDOW(window), stack);

    // 創建主選單視圖
    main_menu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20); // 垂直容器，間隔20
    gtk_widget_set_halign(main_menu, GTK_ALIGN_CENTER);    // 水平居中
    gtk_widget_set_valign(main_menu, GTK_ALIGN_CENTER);    // 垂直居中
    gtk_widget_set_margin_top(main_menu, 20);              // 設置上邊距
    gtk_widget_set_margin_bottom(main_menu, 20);           // 設置下邊距
    gtk_widget_set_margin_start(main_menu, 20);            // 設置左邊距
    gtk_widget_set_margin_end(main_menu, 20);              // 設置右邊距

    // 添加主選單標籤
    GtkWidget* label = gtk_label_new("選擇遊戲模式");
    gtk_box_append(GTK_BOX(main_menu), label);

    // 添加「單人模式」按鈕
    GtkWidget* single_btn = gtk_button_new_with_label("單人模式");
    gtk_box_append(GTK_BOX(main_menu), single_btn);
    g_signal_connect(single_btn, "clicked", G_CALLBACK(on_single_mode_clicked), stack);

    // 添加「雙人模式」按鈕
    GtkWidget* multi_btn = gtk_button_new_with_label("雙人模式 (玩家1 vs 玩家2)");
    gtk_box_append(GTK_BOX(main_menu), multi_btn);
    g_signal_connect(multi_btn, "clicked", G_CALLBACK(on_multi_mode_clicked), stack);

    // 添加「遊戲介紹」按鈕
    GtkWidget* intro_btn = gtk_button_new_with_label("遊戲介紹");
    gtk_box_append(GTK_BOX(main_menu), intro_btn);
    g_signal_connect(intro_btn, "clicked", G_CALLBACK(on_intro_mode_clicked), stack);

    // 添加「退出遊戲」按鈕
    GtkWidget* quit_btn = gtk_button_new_with_label("退出遊戲");
    gtk_box_append(GTK_BOX(main_menu), quit_btn);
    g_signal_connect(quit_btn, "clicked", G_CALLBACK(on_quit_clicked), NULL);

    // 將主選單加入到GtkStack
    gtk_stack_add_named(GTK_STACK(stack), main_menu, "main_menu");

    // 創建遊戲介紹視圖
    GtkWidget* intro_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20); // 垂直容器，間隔20
    gtk_widget_set_halign(intro_vbox, GTK_ALIGN_CENTER);                // 水平居中
    gtk_widget_set_valign(intro_vbox, GTK_ALIGN_CENTER);                // 垂直居中
    gtk_widget_set_margin_top(intro_vbox, 20);                          // 設置上邊距
    gtk_widget_set_margin_bottom(intro_vbox, 20);                       // 設置下邊距
    gtk_widget_set_margin_start(intro_vbox, 20);                        // 設置左邊距
    gtk_widget_set_margin_end(intro_vbox, 20);                          // 設置右邊距

    // 添加遊戲介紹的標籤
    GtkWidget* label_intro = gtk_label_new(
        "歡迎來到 Snake Hero 蛇之英雄！\n\n"
        "【遊戲規則】\n"
        "1. 蛇會自動移動，玩家需要透過改變方向來避開障礙物和蛇身。\n"
        "2. 撞到牆壁、障礙物或自身會導致蛇死亡遊戲結束。\n"
        "3. 單人模式與一般貪食蛇玩法相同。\n"
        "4. 雙人模式吃越多果實，分數越高同時速度變慢。\n\n"
        "【操作說明】\n"
        "1. 單人模式： 玩家 (綠色蛇) 使用方向鍵控制。\n"
        "2. 雙人模式：玩家1 (綠色蛇) 使用WASD鍵控制，玩家2 (橘色蛇) 使用方向鍵控制。\n\n"
        "【獲勝條件】\n"
        "1. 單人模式：盡可能的吃果實，打破自己的最佳紀錄吧！！。\n"
        "2. 雙人模式：優先以分數高的獲勝，同分數情況下，則以存活時間久的獲勝。\n\n"

        "祝遊戲愉快！"
    );
    gtk_label_set_xalign(GTK_LABEL(label_intro), 0.0);  // 設置文字水平對齊方式
    gtk_label_set_wrap(GTK_LABEL(label_intro), TRUE);   // 啟用文字換行
    gtk_label_set_wrap_mode(GTK_LABEL(label_intro), PANGO_WRAP_WORD_CHAR); // 設置換行模式
    gtk_box_append(GTK_BOX(intro_vbox), label_intro);

    // 添加「返回主選單」按鈕
    GtkWidget* back_btn_intro = gtk_button_new_with_label("返回主選單");
    gtk_box_append(GTK_BOX(intro_vbox), back_btn_intro);
    g_signal_connect(back_btn_intro, "clicked", G_CALLBACK(on_back_to_menu_clicked), NULL);

    // 將遊戲介紹視圖加入到GtkStack
    gtk_stack_add_named(GTK_STACK(stack), intro_vbox, "game_intro");

    // 設置預設顯示為主選單
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "main_menu");

    // 監聽鍵盤事件，創建鍵盤事件控制器
    GtkEventController* controller = gtk_event_controller_key_new();
    g_signal_connect(controller, "key-pressed", G_CALLBACK(on_key_press), NULL);
    gtk_widget_add_controller(window, controller);

    // 播放主選單背景音樂 (循環，音量一半)
    main_menu_music = play_sound_effect("Musics/main_menu_background.mp3", TRUE, 0.5); // 循環且音量一半

    // 顯示主視窗
    gtk_window_present(GTK_WINDOW(window));
}

//==============================================================
// [ 程式入口點 WinMain ]
//==============================================================
// 程式的入口點，負責初始化和運行GTK應用程序
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    setlocale(LC_ALL, ""); // 設置本地化環境
    srand((unsigned int)time(NULL)); // 初始化隨機數生成器

    // 初始化GStreamer
    gst_init(NULL, NULL);

    // 創建GtkApplication
    GtkApplication* app = gtk_application_new(
        "com.example.snakegame",
        G_APPLICATION_FLAGS_NONE);

    // 連接activate信號到activate函式
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    // 運行應用程序
    int status = g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app); // 釋放應用程序對象
    return status;
}
