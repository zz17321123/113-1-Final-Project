#define _USE_MATH_DEFINES
#include <math.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>
#include <string.h>

//========================[ 常數 ]========================
#define GRID_WIDTH    40          // 網格寬度
#define GRID_HEIGHT   20          // 網格高度
#define NUM_OBSTACLES 10          // 障礙物數量
#define MIN_OBS_SIZE  1           // 障礙物最小寬度/高度
#define MAX_OBS_SIZE  3           // 障礙物最大寬度/高度
#define INITIAL_MOVE_INTERVAL 100   // 初始移動間隔（毫秒）
#define MOVE_INTERVAL_INCREMENT 50  // 每次吃食物後移動間隔增加量（毫秒）
#define MAX_MOVE_INTERVAL 1000      // 最大移動間隔，防止速度過慢

//========================[ 遊戲模式 ]========================
typedef enum {
    MODE_NONE = 0,
    MODE_MENU,
    MODE_COUNTDOWN_SINGLE,
    MODE_SINGLE,
    MODE_COUNTDOWN_TWO_PLAYER,   // *** 雙人模式 ***
    MODE_TWO_PLAYER,             // *** 雙人模式 ***
    MODE_INTRO
} GameMode;

//========================[ 結構定義 ]========================
typedef struct {
    int x, y;           // 起始位置（格子座標）
    int width, height;  // 寬度和高度（格子數）
} Obstacle;

typedef struct {
    int x, y;           // 蛇的節點位置
} Point;

//========================[ 全域變數 ]========================
static GtkWidget* window = NULL;            // 主視窗
static GtkWidget* main_menu = NULL;         // 主菜單
static GtkWidget* stack = NULL;             // GtkStack

static GameMode current_mode = MODE_MENU;    // 當前模式
static gboolean game_over = FALSE;           // 遊戲結束標誌

// 單人模式相關
static GList* snake_single = NULL;            // 單人蛇的節點
static Point   food_single;                   // 單人食物位置
static int     direction_single = GDK_KEY_Right;       // 單人當前方向
static int     next_direction_single = GDK_KEY_Right;  // 單人下一方向
static int     score_single = 0;              // 單人分數
static GtkWidget* canvas_single = NULL;       // 單人繪圖區
static guint  single_timer_id = 0;            // 單人遊戲定時器ID
static guint  countdown_timer_id = 0;         // 單人倒數定時器ID
static int    current_countdown = 3;          // 單人當前倒數數字

// *** 雙人模式 ***
static GList* snake_two = NULL;               // 雙人蛇的節點
static int     direction_two = GDK_KEY_Left;       // 雙人當前方向
static int     next_direction_two = GDK_KEY_Left;  // 雙人下一方向
static int     score_two = 0;                  // 雙人分數
static GtkWidget* canvas_two = NULL;           // 雙人繪圖區
static guint  two_player_timer_id = 0;         // 雙人遊戲玩家一定時器ID
static guint  two_player_timer2_id = 0;        // 雙人遊戲玩家二定時器ID
static guint  countdown_two_player_timer_id = 0; // 雙人倒數定時器ID
static int    current_countdown_two = 3;        // 雙人當前倒數數字

// 新增雙人模式的速度控制變數
static guint player1_move_interval = INITIAL_MOVE_INTERVAL; // 玩家一的移動間隔（毫秒）
static guint player2_move_interval = INITIAL_MOVE_INTERVAL; // 玩家二的移動間隔（毫秒）
static gboolean player1_timer_active = FALSE;
static gboolean player2_timer_active = FALSE;

// 新增雙人模式的玩家狀態與生存時間
static gboolean player1_alive = TRUE;
static gboolean player2_alive = TRUE;
static time_t player1_start_time, player2_start_time;
static time_t player1_end_time, player2_end_time;
// *** 雙人模式 ***

static GList* obstacles = NULL;               // 障礙物節點

static double CELL_SIZE = 45.0;                // 單位格子大小（像素）

//========================[ 函式宣告 ]========================
static void on_single_mode_clicked(GtkButton* button, gpointer user_data);
static void on_two_player_mode_clicked(GtkButton* button, gpointer user_data); // *** 雙人模式 ***
static void on_intro_mode_clicked(GtkButton* button, gpointer user_data);
static void on_quit_clicked(GtkButton* button, gpointer user_data);
static void on_back_to_menu_clicked(GtkButton* button, gpointer user_data);
static gboolean on_key_press(GtkEventControllerKey* controller,
    guint keyval, guint keycode,
    GdkModifierType state, gpointer user_data);

static void init_single_game(void);
static void init_two_player_game(void); // *** 雙人模式 ***
static gboolean update_game_single(gpointer data);
static gboolean player1_move_cb(gpointer data); // *** 雙人模式 ***
static gboolean player2_move_cb(gpointer data); // *** 雙人模式 ***
static gboolean update_countdown_single(gpointer data);
static gboolean update_countdown_two_player(gpointer data); // *** 雙人模式 ***
static void generate_food_single(void);
static void generate_obstacles(void);
static gboolean check_collision_single(Point* head);
static gboolean check_collision_two_player(Point* head1, Point* head2); // *** 雙人模式 ***
static void show_game_over_screen_single(void);
static void show_game_over_screen_two_player(void); // *** 雙人模式 ***

static void draw_game(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data);
static void draw_single_mode(cairo_t* cr, int width, int height, double cell_size);
static void draw_two_player_mode(cairo_t* cr, int width, int height, double cell_size); // *** 雙人模式 ***
static void draw_countdown(cairo_t* cr, int width, int height, int countdown);
static void draw_snake_segment(cairo_t* cr, double x, double y, double size,
    GdkRGBA body, GdkRGBA shadow);
static void draw_food(cairo_t* cr, double x, double y, double size);
static void draw_obstacles(cairo_t* cr, double cell_size);

//========================[ 函式實作 ]========================

// 繪圖回調
static void draw_game(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data)
{
    if (current_mode == MODE_INTRO) {
        // 遊戲介紹由其他視圖處理
        return;
    }

    // *** 修正條件順序，先處理倒數模式 ***
    if (current_mode == MODE_COUNTDOWN_SINGLE || current_mode == MODE_COUNTDOWN_TWO_PLAYER) {
        if (current_mode == MODE_COUNTDOWN_SINGLE) {
            draw_countdown(cr, width, height, current_countdown);
        }
        else { // MODE_COUNTDOWN_TWO_PLAYER
            draw_countdown(cr, width, height, current_countdown_two);
        }
    }
    else if (current_mode == MODE_SINGLE) {
        draw_single_mode(cr, width, height, CELL_SIZE);
    }
    else if (current_mode == MODE_TWO_PLAYER) {
        draw_two_player_mode(cr, width, height, CELL_SIZE);
    }
}

// 繪製單人模式
static void draw_single_mode(cairo_t* cr, int width, int height, double cell_size)
{
    // 背景
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_paint(cr);

    // 食物
    draw_food(cr, food_single.x * cell_size, food_single.y * cell_size, cell_size);

    // 障礙物
    draw_obstacles(cr, cell_size);

    // 蛇
    GdkRGBA body_green = { 0.0, 1.0, 0.0, 1.0 };
    GdkRGBA shadow_green = { 0.0, 0.4, 0.0, 1.0 };
    for (GList* iter = snake_single; iter; iter = iter->next) {
        Point* seg = (Point*)iter->data;
        if (seg) {
            draw_snake_segment(cr, seg->x * cell_size, seg->y * cell_size,
                cell_size, body_green, shadow_green);
        }
    }

    // 分數
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_select_font_face(cr, "Microsoft JhengHei",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 16);
    char buf[64];
    snprintf(buf, sizeof(buf), "分數: %d", score_single);
    cairo_move_to(cr, 10, 25);
    cairo_show_text(cr, buf);
}

// *** 雙人模式 ***
// 繪製雙人模式
static void draw_two_player_mode(cairo_t* cr, int width, int height, double cell_size)
{
    // 背景
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_paint(cr);

    // 食物
    draw_food(cr, food_single.x * cell_size, food_single.y * cell_size, cell_size);

    // 障礙物
    draw_obstacles(cr, cell_size);

    // 玩家一 蛇 (綠色)
    if (player1_alive) {
        GdkRGBA body_green = { 0.0, 1.0, 0.0, 1.0 };
        GdkRGBA shadow_green = { 0.0, 0.4, 0.0, 1.0 };
        for (GList* iter = snake_single; iter; iter = iter->next) {
            Point* seg = (Point*)iter->data;
            if (seg) {
                draw_snake_segment(cr, seg->x * cell_size, seg->y * cell_size,
                    cell_size, body_green, shadow_green);
            }
        }
    }

    // 玩家二 蛇 (橘色)
    if (player2_alive) {
        GdkRGBA body_orange = { 1.0, 0.5, 0.0, 1.0 };
        GdkRGBA shadow_orange = { 0.6, 0.3, 0.0, 1.0 };
        for (GList* iter = snake_two; iter; iter = iter->next) {
            Point* seg = (Point*)iter->data;
            if (seg) {
                draw_snake_segment(cr, seg->x * cell_size, seg->y * cell_size,
                    cell_size, body_orange, shadow_orange);
            }
        }
    }

    // 分數
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_select_font_face(cr, "Microsoft JhengHei",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 16);
    char buf1[64];
    snprintf(buf1, sizeof(buf1), "玩家一分數: %d", score_single);
    cairo_move_to(cr, 10, 25);
    cairo_show_text(cr, buf1);

    char buf2[64];
    snprintf(buf2, sizeof(buf2), "玩家二分數: %d", score_two);
    cairo_move_to(cr, 10, 50);
    cairo_show_text(cr, buf2);
}
// *** 雙人模式 ***

// 繪製倒數計時
static void draw_countdown(cairo_t* cr, int width, int height, int countdown)
{
    // 背景
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_paint(cr);

    // 顯示倒數數字或 "GO!"
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_select_font_face(cr, "Microsoft JhengHei",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 100);

    char buf[16];
    if (countdown > 0) {
        snprintf(buf, sizeof(buf), "%d", countdown);
    }
    else {
        snprintf(buf, sizeof(buf), "GO!");
    }

    cairo_text_extents_t extents;
    cairo_text_extents(cr, buf, &extents);
    double x = (width - extents.width) / 2 - extents.x_bearing;
    double y = (height - extents.height) / 2 - extents.y_bearing;

    cairo_move_to(cr, x, y);
    cairo_show_text(cr, buf);
}

// 繪製蛇節點
static void draw_snake_segment(cairo_t* cr, double x, double y, double size,
    GdkRGBA body, GdkRGBA shadow)
{
    // 陰影
    cairo_set_source_rgba(cr, shadow.red, shadow.green, shadow.blue, shadow.alpha);
    double off = 2;
    cairo_rectangle(cr, x + off, y + off, size, size);
    cairo_fill(cr);

    // 本體
    cairo_set_source_rgba(cr, body.red, body.green, body.blue, body.alpha);
    cairo_rectangle(cr, x, y, size, size);
    cairo_fill(cr);
}

// 繪製食物
static void draw_food(cairo_t* cr, double x, double y, double size)
{
    // 陰影
    cairo_set_source_rgb(cr, 0.5, 0.0, 0.0);
    cairo_rectangle(cr, x + 2, y + 2, size, size);
    cairo_fill(cr);

    // 本體
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
    cairo_rectangle(cr, x, y, size, size);
    cairo_fill(cr);
}

// 繪製障礙物
static void draw_obstacles(cairo_t* cr, double cell_size)
{
    GdkRGBA obstacle_color = { 0.5, 0.5, 0.5, 1.0 }; // 灰色
    for (GList* iter = obstacles; iter; iter = iter->next) {
        Obstacle* obs = (Obstacle*)iter->data;
        if (obs) {
            cairo_set_source_rgba(cr, obstacle_color.red, obstacle_color.green, obstacle_color.blue, obstacle_color.alpha);
            cairo_rectangle(cr, obs->x * cell_size, obs->y * cell_size, obs->width * cell_size, obs->height * cell_size);
            cairo_fill(cr);
        }
    }
}

// 鍵盤事件處理
static gboolean on_key_press(GtkEventControllerKey* controller,
    guint keyval, guint keycode,
    GdkModifierType state, gpointer user_data)
{
    if ((current_mode == MODE_SINGLE || current_mode == MODE_TWO_PLAYER) && !game_over) {
        // 控制玩家一 (單人或雙人模式)
        if (current_mode == MODE_SINGLE || current_mode == MODE_TWO_PLAYER) {
            switch (keyval) {
            case GDK_KEY_w:
            case GDK_KEY_W:
                if (next_direction_single != GDK_KEY_Down)
                    next_direction_single = GDK_KEY_Up;
                break;
            case GDK_KEY_s:
            case GDK_KEY_S:
                if (next_direction_single != GDK_KEY_Up)
                    next_direction_single = GDK_KEY_Down;
                break;
            case GDK_KEY_a:
            case GDK_KEY_A:
                if (next_direction_single != GDK_KEY_Right)
                    next_direction_single = GDK_KEY_Left;
                break;
            case GDK_KEY_d:
            case GDK_KEY_D:
                if (next_direction_single != GDK_KEY_Left)
                    next_direction_single = GDK_KEY_Right;
                break;
            }
        }

        // 控制玩家二 (僅雙人模式)
        if (current_mode == MODE_TWO_PLAYER) {
            switch (keyval) {
            case GDK_KEY_Up:
                if (next_direction_two != GDK_KEY_Down)
                    next_direction_two = GDK_KEY_Up;
                break;
            case GDK_KEY_Down:
                if (next_direction_two != GDK_KEY_Up)
                    next_direction_two = GDK_KEY_Down;
                break;
            case GDK_KEY_Left:
                if (next_direction_two != GDK_KEY_Right)
                    next_direction_two = GDK_KEY_Left;
                break;
            case GDK_KEY_Right:
                if (next_direction_two != GDK_KEY_Left)
                    next_direction_two = GDK_KEY_Right;
                break;
            }
        }
    }

    return FALSE;
}

// 單人模式按鈕回調
static void on_single_mode_clicked(GtkButton* button, gpointer user_data)
{
    GtkStack* stack_ptr = GTK_STACK(user_data);
    current_mode = MODE_COUNTDOWN_SINGLE;
    current_countdown = 3;

    // 初始化遊戲（蛇、食物、障礙物）
    init_single_game();

    // 移除舊視圖
    GtkWidget* existing_game_single = gtk_stack_get_child_by_name(stack_ptr, "game_single");
    if (existing_game_single) {
        gtk_stack_remove(stack_ptr, existing_game_single);
    }

    // 創建繪圖區
    GtkWidget* new_canvas = gtk_drawing_area_new();
    gtk_widget_set_size_request(new_canvas, GRID_WIDTH * CELL_SIZE, GRID_HEIGHT * CELL_SIZE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(new_canvas),
        draw_game, NULL, NULL);

    // 創建容器
    GtkWidget* container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(container, TRUE);
    gtk_widget_set_vexpand(container, TRUE);
    gtk_widget_set_halign(container, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(container, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(container), new_canvas);

    // 加入GtkStack
    gtk_stack_add_named(stack_ptr, container, "game_single");
    canvas_single = new_canvas;
    gtk_stack_set_visible_child_name(stack_ptr, "game_single");

    // 啟動倒數計時器
    countdown_timer_id = g_timeout_add_seconds(1, update_countdown_single, NULL);
}

// *** 雙人模式 ***
// 兩人模式按鈕回調
static void on_two_player_mode_clicked(GtkButton* button, gpointer user_data)
{
    GtkStack* stack_ptr = GTK_STACK(user_data);
    current_mode = MODE_COUNTDOWN_TWO_PLAYER;
    current_countdown_two = 3;

    // 初始化遊戲（兩條蛇、食物、障礙物）
    init_two_player_game();

    // 移除舊視圖
    GtkWidget* existing_game_two = gtk_stack_get_child_by_name(stack_ptr, "game_two_player");
    if (existing_game_two) {
        gtk_stack_remove(stack_ptr, existing_game_two);
    }

    // 創建繪圖區
    GtkWidget* new_canvas = gtk_drawing_area_new();
    gtk_widget_set_size_request(new_canvas, GRID_WIDTH * CELL_SIZE, GRID_HEIGHT * CELL_SIZE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(new_canvas),
        draw_game, NULL, NULL);

    // 創建容器
    GtkWidget* container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(container, TRUE);
    gtk_widget_set_vexpand(container, TRUE);
    gtk_widget_set_halign(container, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(container, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(container), new_canvas);

    // 加入GtkStack
    gtk_stack_add_named(stack_ptr, container, "game_two_player");
    canvas_two = new_canvas;
    gtk_stack_set_visible_child_name(stack_ptr, "game_two_player");

    // 初始化速度控制變數
    player1_move_interval = INITIAL_MOVE_INTERVAL;
    player2_move_interval = INITIAL_MOVE_INTERVAL;
    player1_timer_active = FALSE;
    player2_timer_active = FALSE;

    // 啟動倒數計時器
    countdown_two_player_timer_id = g_timeout_add_seconds(1, update_countdown_two_player, NULL);
}
// *** 雙人模式 ***

// 遊戲介紹按鈕回調
static void on_intro_mode_clicked(GtkButton* button, gpointer user_data)
{
    GtkStack* stack_ptr = GTK_STACK(user_data);
    current_mode = MODE_INTRO;

    // 顯示介紹視圖
    gtk_stack_set_visible_child_name(stack_ptr, "game_intro");
}

// 退出遊戲按鈕回調
static void on_quit_clicked(GtkButton* button, gpointer user_data)
{
    GtkApplication* app = GTK_APPLICATION(gtk_window_get_application(GTK_WINDOW(window)));
    if (app) g_application_quit(G_APPLICATION(app));
}

// 返回主選單按鈕回調
static void on_back_to_menu_clicked(GtkButton* button, gpointer user_data)
{
    // 清理單人蛇
    if (snake_single) {
        for (GList* iter = snake_single; iter; iter = iter->next) {
            free(iter->data);
        }
        g_list_free(snake_single);
        snake_single = NULL;
    }

    // *** 雙人模式 ***
    if (snake_two) {
        for (GList* iter = snake_two; iter; iter = iter->next) {
            free(iter->data);
        }
        g_list_free(snake_two);
        snake_two = NULL;
    }
    // *** 雙人模式 ***

    // 清理障礙物
    if (obstacles) {
        for (GList* iter = obstacles; iter; iter = iter->next) {
            free(iter->data);
        }
        g_list_free(obstacles);
        obstacles = NULL;
    }

    // 重置變數
    score_single = 0;
    direction_single = GDK_KEY_Right;
    next_direction_single = GDK_KEY_Right;
    score_two = 0; // *** 雙人模式 ***
    direction_two = GDK_KEY_Left; // *** 雙人模式 ***
    next_direction_two = GDK_KEY_Left; // *** 雙人模式 ***
    game_over = FALSE;
    current_countdown = 3;
    current_countdown_two = 3; // *** 雙人模式 ***

    // 重置速度控制變數
    player1_move_interval = INITIAL_MOVE_INTERVAL; // *** 雙人模式 ***
    player2_move_interval = INITIAL_MOVE_INTERVAL; // *** 雙人模式 ***
    player1_timer_active = FALSE; // *** 雙人模式 ***
    player2_timer_active = FALSE; // *** 雙人模式 ***

    // 重置雙人模式的玩家狀態與生存時間
    player1_alive = TRUE;
    player2_alive = TRUE;
    player1_start_time = 0;
    player2_start_time = 0;
    player1_end_time = 0;
    player2_end_time = 0;
    // *** 雙人模式 ***

    // 移除定時器
    if (single_timer_id) {
        g_source_remove(single_timer_id);
        single_timer_id = 0;
    }

    if (two_player_timer_id) { // *** 雙人模式 ***
        g_source_remove(two_player_timer_id);
        two_player_timer_id = 0;
    }

    if (two_player_timer2_id) { // *** 雙人模式 ***
        g_source_remove(two_player_timer2_id);
        two_player_timer2_id = 0;
    }

    if (countdown_timer_id) {
        g_source_remove(countdown_timer_id);
        countdown_timer_id = 0;
    }

    if (countdown_two_player_timer_id) { // *** 雙人模式 ***
        g_source_remove(countdown_two_player_timer_id);
        countdown_two_player_timer_id = 0;
    }

    // 移除視圖
    GtkStack* stack_ptr = GTK_STACK(user_data);
    GtkWidget* game_single = gtk_stack_get_child_by_name(stack_ptr, "game_single");
    if (game_single) gtk_stack_remove(stack_ptr, game_single);

    GtkWidget* game_two_player = gtk_stack_get_child_by_name(stack_ptr, "game_two_player"); // *** 雙人模式 ***
    if (game_two_player) gtk_stack_remove(stack_ptr, game_two_player);
    // *** 雙人模式 ***

    GtkWidget* game_over_single = gtk_stack_get_child_by_name(stack_ptr, "game_over_single");
    if (game_over_single) gtk_stack_remove(stack_ptr, game_over_single);

    GtkWidget* game_over_two_player = gtk_stack_get_child_by_name(stack_ptr, "game_over_two_player"); // *** 雙人模式 ***
    if (game_over_two_player) gtk_stack_remove(stack_ptr, game_over_two_player);
    // *** 雙人模式 ***

    // 顯示主選單
    gtk_stack_set_visible_child_name(stack_ptr, "main_menu");
}

// 初始化單人遊戲
static void init_single_game(void)
{
    // 清理單人蛇
    if (snake_single) {
        for (GList* iter = snake_single; iter; iter = iter->next) {
            free(iter->data);
        }
        g_list_free(snake_single);
        snake_single = NULL;
    }

    // 初始蛇
    Point* h = (Point*)malloc(sizeof(Point));
    h->x = GRID_WIDTH / 2;
    h->y = GRID_HEIGHT / 2;
    snake_single = g_list_append(snake_single, h);

    // 重置方向和分數
    direction_single = GDK_KEY_Right;
    next_direction_single = GDK_KEY_Right;
    score_single = 0;
    game_over = FALSE;

    // 生成食物和障礙物
    generate_food_single();
    generate_obstacles();
}

// 初始化雙人遊戲 *** 雙人模式 ***
static void init_two_player_game(void)
{
    // 清理兩條蛇
    if (snake_single) {
        for (GList* iter = snake_single; iter; iter = iter->next) {
            free(iter->data);
        }
        g_list_free(snake_single);
        snake_single = NULL;
    }

    if (snake_two) {
        for (GList* iter = snake_two; iter; iter = iter->next) {
            free(iter->data);
        }
        g_list_free(snake_two);
        snake_two = NULL;
    }

    // 初始玩家一蛇
    Point* h1 = (Point*)malloc(sizeof(Point));
    h1->x = GRID_WIDTH / 4;
    h1->y = GRID_HEIGHT / 2;
    snake_single = g_list_append(snake_single, h1);

    // 初始玩家二蛇
    Point* h2 = (Point*)malloc(sizeof(Point));
    h2->x = (3 * GRID_WIDTH) / 4;
    h2->y = GRID_HEIGHT / 2;
    snake_two = g_list_append(snake_two, h2);

    // 重置方向和分數
    direction_single = GDK_KEY_Right;
    next_direction_single = GDK_KEY_Right;
    direction_two = GDK_KEY_Left; // 使兩條蛇初始方向相對
    next_direction_two = GDK_KEY_Left;
    score_single = 0;
    score_two = 0;
    game_over = FALSE;

    // 重置速度控制變數
    player1_move_interval = INITIAL_MOVE_INTERVAL;
    player2_move_interval = INITIAL_MOVE_INTERVAL;
    player1_timer_active = FALSE;
    player2_timer_active = FALSE;

    // 重置雙人模式的玩家狀態與生存時間
    player1_alive = TRUE;
    player2_alive = TRUE;
    player1_start_time = time(NULL);
    player2_start_time = time(NULL);
    player1_end_time = 0;
    player2_end_time = 0;
    // *** 雙人模式 ***

    // 生成食物和障礙物
    generate_food_single();
    generate_obstacles();
}
// *** 雙人模式 ***

// 生成食物
static void generate_food_single(void)
{
    gboolean valid = FALSE;
    while (!valid) {
        food_single.x = rand() % GRID_WIDTH;
        food_single.y = rand() % GRID_HEIGHT;
        valid = TRUE;

        // 檢查是否與蛇重疊
        for (GList* iter = snake_single; iter; iter = iter->next) {
            Point* seg = (Point*)iter->data;
            if (seg->x == food_single.x && seg->y == food_single.y) {
                valid = FALSE;
                break;
            }
        }

        // *** 雙人模式 ***
        for (GList* iter = snake_two; iter; iter = iter->next) {
            Point* seg = (Point*)iter->data;
            if (seg->x == food_single.x && seg->y == food_single.y) {
                valid = FALSE;
                break;
            }
        }
        // *** 雙人模式 ***

        // 檢查是否與障礙物重疊
        if (valid) {
            for (GList* iter = obstacles; iter; iter = iter->next) {
                Obstacle* obs = (Obstacle*)iter->data;
                if (food_single.x >= obs->x && food_single.x < (obs->x + obs->width) &&
                    food_single.y >= obs->y && food_single.y < (obs->y + obs->height)) {
                    valid = FALSE;
                    break;
                }
            }
        }

        // 檢查是否與禁止區域重疊（4x4區域）
        if (valid) {
            // 對所有蛇的頭部進行檢查
            GList* snakes[] = { snake_single, snake_two };
            for (int i = 0; i < 2; i++) { // 處理單人或雙人模式
                if (snakes[i]) {
                    Point* initial_snake = (Point*)snakes[i]->data;
                    if (initial_snake) {
                        int forbidden_x_min = initial_snake->x - 2;
                        int forbidden_x_max = initial_snake->x + 1;
                        int forbidden_y_min = initial_snake->y - 2;
                        int forbidden_y_max = initial_snake->y + 1;

                        if (food_single.x >= forbidden_x_min && food_single.x <= forbidden_x_max &&
                            food_single.y >= forbidden_y_min && food_single.y <= forbidden_y_max) {
                            valid = FALSE;
                        }
                    }
                }
            }
        }
    }
}

// 生成障礙物
static void generate_obstacles(void)
{
    // 清除現有障礙物
    if (obstacles) {
        for (GList* iter = obstacles; iter; iter = iter->next) {
            free(iter->data);
        }
        g_list_free(obstacles);
        obstacles = NULL;
    }

    for (int i = 0; i < NUM_OBSTACLES; i++) {
        Obstacle* obs = (Obstacle*)malloc(sizeof(Obstacle));
        gboolean valid = FALSE;
        while (!valid) {
            obs->width = MIN_OBS_SIZE + rand() % (MAX_OBS_SIZE - MIN_OBS_SIZE + 1);
            obs->height = MIN_OBS_SIZE + rand() % (MAX_OBS_SIZE - MIN_OBS_SIZE + 1);
            obs->x = rand() % (GRID_WIDTH - obs->width);
            obs->y = rand() % (GRID_HEIGHT - obs->height);
            valid = TRUE;

            // 檢查是否與蛇重疊
            for (GList* iter = snake_single; iter; iter = iter->next) {
                Point* seg = (Point*)iter->data;
                if (seg->x >= obs->x && seg->x < (obs->x + obs->width) &&
                    seg->y >= obs->y && seg->y < (obs->y + obs->height)) {
                    valid = FALSE;
                    break;
                }
            }

            // *** 雙人模式 ***
            if (valid) {
                for (GList* iter = snake_two; iter; iter = iter->next) {
                    Point* seg = (Point*)iter->data;
                    if (seg->x >= obs->x && seg->x < (obs->x + obs->width) &&
                        seg->y >= obs->y && seg->y < (obs->y + obs->height)) {
                        valid = FALSE;
                        break;
                    }
                }
            }
            // *** 雙人模式 ***

            // 檢查是否與食物重疊
            if (valid) {
                if (food_single.x >= obs->x && food_single.x < (obs->x + obs->width) &&
                    food_single.y >= obs->y && food_single.y < (obs->y + obs->height)) {
                    valid = FALSE;
                }
            }

            // 檢查是否與其他障礙物重疊
            if (valid) {
                for (GList* iter = obstacles; iter; iter = iter->next) {
                    Obstacle* existing_obs = (Obstacle*)iter->data;
                    // 簡單的矩形重疊檢查
                    if (!(obs->x + obs->width <= existing_obs->x ||
                        existing_obs->x + existing_obs->width <= obs->x ||
                        obs->y + obs->height <= existing_obs->y ||
                        existing_obs->y + existing_obs->height <= obs->y)) {
                        valid = FALSE;
                        break;
                    }
                }
            }

            // 檢查是否與禁止生成區域重疊（4x4區域）
            if (valid) {
                // 對所有蛇的頭部進行檢查
                GList* snakes[] = { snake_single, snake_two };
                for (int i = 0; i < 2; i++) {
                    if (snakes[i]) {
                        Point* initial_snake = (Point*)snakes[i]->data;
                        if (initial_snake) {
                            int forbidden_x_min = initial_snake->x - 2;
                            int forbidden_x_max = initial_snake->x + 1;
                            int forbidden_y_min = initial_snake->y - 2;
                            int forbidden_y_max = initial_snake->y + 1;

                            // 檢查障礙物是否與禁止區域重疊
                            if (!(obs->x + obs->width - 1 < forbidden_x_min ||
                                obs->x > forbidden_x_max ||
                                obs->y + obs->height - 1 < forbidden_y_min ||
                                obs->y > forbidden_y_max)) {
                                valid = FALSE;
                            }
                        }
                    }
                }
            }
        }
        obstacles = g_list_append(obstacles, obs);
    }
}

// 碰撞檢查 (單人模式)
static gboolean check_collision_single(Point* head)
{
    // 自撞
    for (GList* iter = snake_single->next; iter; iter = iter->next) {
        Point* seg = (Point*)iter->data;
        if (seg->x == head->x && seg->y == head->y) return TRUE;
    }

    // 障礙物撞擊
    for (GList* iter = obstacles; iter; iter = iter->next) {
        Obstacle* obs = (Obstacle*)iter->data;
        if (head->x >= obs->x && head->x < (obs->x + obs->width) &&
            head->y >= obs->y && head->y < (obs->y + obs->height)) return TRUE;
    }

    return FALSE;
}

// *** 雙人模式 ***
// 碰撞檢查 (雙人模式)
static gboolean check_collision_two_player(Point* head1, Point* head2)
{
    gboolean collision = FALSE;

    if (head1 != NULL) {
        // 檢查玩家一的碰撞

        // 自撞
        for (GList* iter = snake_single->next; iter; iter = iter->next) {
            Point* seg = (Point*)iter->data;
            if (seg->x == head1->x && seg->y == head1->y) {
                collision = TRUE;
                break;
            }
        }

        // 障礙物撞擊
        if (!collision) {
            for (GList* iter = obstacles; iter; iter = iter->next) {
                Obstacle* obs = (Obstacle*)iter->data;
                if (head1->x >= obs->x && head1->x < (obs->x + obs->width) &&
                    head1->y >= obs->y && head1->y < (obs->y + obs->height)) {
                    collision = TRUE;
                    break;
                }
            }
        }

        // 撞到對方蛇的身體
        if (!collision) {
            for (GList* iter = snake_two; iter; iter = iter->next) {
                Point* seg = (Point*)iter->data;
                if (seg->x == head1->x && seg->y == head1->y) {
                    collision = TRUE;
                    break;
                }
            }
        }
    }

    if (head2 != NULL) {
        // 檢查玩家二的碰撞

        // 自撞
        for (GList* iter = snake_two->next; iter; iter = iter->next) {
            Point* seg = (Point*)iter->data;
            if (seg->x == head2->x && seg->y == head2->y) {
                collision = TRUE;
                break;
            }
        }

        // 障礙物撞擊
        if (!collision) {
            for (GList* iter = obstacles; iter; iter = iter->next) {
                Obstacle* obs = (Obstacle*)iter->data;
                if (head2->x >= obs->x && head2->x < (obs->x + obs->width) &&
                    head2->y >= obs->y && head2->y < (obs->y + obs->height)) {
                    collision = TRUE;
                    break;
                }
            }
        }

        // 撞到對方蛇的身體
        if (!collision) {
            for (GList* iter = snake_single; iter; iter = iter->next) {
                Point* seg = (Point*)iter->data;
                if (seg->x == head2->x && seg->y == head2->y) {
                    collision = TRUE;
                    break;
                }
            }
        }
    }

    // 確認兩個蛇頭是否相同
    if (head1 != NULL && head2 != NULL && head1->x == head2->x && head1->y == head2->y) {
        collision = TRUE;
    }

    return collision;
}
// *** 雙人模式 ***

// 更新遊戲狀態 (單人)
static gboolean update_game_single(gpointer data)
{
    if (current_mode != MODE_SINGLE || game_over)
        return G_SOURCE_REMOVE;

    // 更新方向
    direction_single = next_direction_single;

    // 新頭位置
    Point* head = (Point*)snake_single->data;
    if (!head) return G_SOURCE_REMOVE;
    Point nh = { head->x, head->y };
    switch (direction_single) {
    case GDK_KEY_Up:    nh.y--; break;
    case GDK_KEY_Down:  nh.y++; break;
    case GDK_KEY_Left:  nh.x--; break;
    case GDK_KEY_Right: nh.x++; break;
    }

    // 邊界循環
    if (nh.x < 0) nh.x = GRID_WIDTH - 1;
    else if (nh.x >= GRID_WIDTH) nh.x = 0;
    if (nh.y < 0) nh.y = GRID_HEIGHT - 1;
    else if (nh.y >= GRID_HEIGHT) nh.y = 0;

    // 碰撞
    if (check_collision_single(&nh)) {
        game_over = TRUE;
        gtk_widget_queue_draw(canvas_single);
        show_game_over_screen_single();
        return G_SOURCE_REMOVE;
    }

    // 添加新頭
    Point* new_seg = (Point*)malloc(sizeof(Point));
    *new_seg = nh;
    snake_single = g_list_prepend(snake_single, new_seg);

    // 吃食物
    if (nh.x == food_single.x && nh.y == food_single.y) {
        score_single++;
        generate_food_single();
    }
    else {
        // 移除尾巴
        Point* tail = (Point*)g_list_last(snake_single)->data;
        snake_single = g_list_remove(snake_single, tail);
        free(tail);
    }

    // 重繪
    gtk_widget_queue_draw(canvas_single);
    return G_SOURCE_CONTINUE;
}

// *** 雙人模式 ***
// 玩家一移動回調函式
static gboolean player1_move_cb(gpointer data)
{
    if (current_mode != MODE_TWO_PLAYER || game_over || !player1_alive)
        return G_SOURCE_REMOVE;

    // 更新方向
    direction_single = next_direction_single;

    // 新頭位置 玩家一
    Point* head1 = (Point*)snake_single->data;
    if (!head1) return G_SOURCE_REMOVE;
    Point nh1 = { head1->x, head1->y };
    switch (direction_single) {
    case GDK_KEY_Up:    nh1.y--; break;
    case GDK_KEY_Down:  nh1.y++; break;
    case GDK_KEY_Left:  nh1.x--; break;
    case GDK_KEY_Right: nh1.x++; break;
    }

    // 邊界循環 玩家一
    if (nh1.x < 0) nh1.x = GRID_WIDTH - 1;
    else if (nh1.x >= GRID_WIDTH) nh1.x = 0;
    if (nh1.y < 0) nh1.y = GRID_HEIGHT - 1;
    else if (nh1.y >= GRID_HEIGHT) nh1.y = 0;

    // 碰撞
    if (check_collision_two_player(&nh1, NULL)) {
        player1_alive = FALSE;
        player1_end_time = time(NULL);
        // 移除玩家一的 Timer
        if (two_player_timer_id) {
            g_source_remove(two_player_timer_id);
            two_player_timer_id = 0;
        }

        // 檢查是否所有玩家都已死亡
        if (!player2_alive) {
            game_over = TRUE;
            gtk_widget_queue_draw(canvas_two);
            show_game_over_screen_two_player();
            return G_SOURCE_REMOVE;
        }

        // 繪製更新
        gtk_widget_queue_draw(canvas_two);
        return G_SOURCE_CONTINUE;
    }

    // 添加新頭 玩家一
    Point* new_seg1 = (Point*)malloc(sizeof(Point));
    *new_seg1 = nh1;
    snake_single = g_list_prepend(snake_single, new_seg1);

    // 玩家一 吃食物
    if (nh1.x == food_single.x && nh1.y == food_single.y) {
        score_single++;
        generate_food_single();

        // 增加玩家一的移動間隔，使速度變慢
        if (player1_move_interval + MOVE_INTERVAL_INCREMENT <= MAX_MOVE_INTERVAL) {
            // 移除現有的 Timer
            if (two_player_timer_id) {
                g_source_remove(two_player_timer_id);
                two_player_timer_id = 0;
            }

            player1_move_interval += MOVE_INTERVAL_INCREMENT;

            // 設置新的 Timer
            two_player_timer_id = g_timeout_add(player1_move_interval, player1_move_cb, NULL);
        }
    }
    else {
        // 移除尾巴 玩家一
        Point* tail1 = (Point*)g_list_last(snake_single)->data;
        snake_single = g_list_remove(snake_single, tail1);
        free(tail1);
    }

    // 重繪
    gtk_widget_queue_draw(canvas_two);
    return G_SOURCE_CONTINUE;
}

// 玩家二移動回調函式
static gboolean player2_move_cb(gpointer data)
{
    if (current_mode != MODE_TWO_PLAYER || game_over || !player2_alive)
        return G_SOURCE_REMOVE;

    // 更新方向
    direction_two = next_direction_two;

    // 新頭位置 玩家二
    Point* head2 = (Point*)snake_two->data;
    if (!head2) return G_SOURCE_REMOVE;
    Point nh2 = { head2->x, head2->y };
    switch (direction_two) {
    case GDK_KEY_Up:    nh2.y--; break;
    case GDK_KEY_Down:  nh2.y++; break;
    case GDK_KEY_Left:  nh2.x--; break;
    case GDK_KEY_Right: nh2.x++; break;
    }

    // 邊界循環 玩家二
    if (nh2.x < 0) nh2.x = GRID_WIDTH - 1;
    else if (nh2.x >= GRID_WIDTH) nh2.x = 0;
    if (nh2.y < 0) nh2.y = GRID_HEIGHT - 1;
    else if (nh2.y >= GRID_HEIGHT) nh2.y = 0;

    // 碰撞
    if (check_collision_two_player(NULL, &nh2)) {
        player2_alive = FALSE;
        player2_end_time = time(NULL);
        // 移除玩家二的 Timer
        if (two_player_timer2_id) {
            g_source_remove(two_player_timer2_id);
            two_player_timer2_id = 0;
        }

        // 檢查是否所有玩家都已死亡
        if (!player1_alive) {
            game_over = TRUE;
            gtk_widget_queue_draw(canvas_two);
            show_game_over_screen_two_player();
            return G_SOURCE_REMOVE;
        }

        // 繪製更新
        gtk_widget_queue_draw(canvas_two);
        return G_SOURCE_CONTINUE;
    }

    // 添加新頭 玩家二
    Point* new_seg2 = (Point*)malloc(sizeof(Point));
    *new_seg2 = nh2;
    snake_two = g_list_prepend(snake_two, new_seg2);

    // 玩家二 吃食物
    if (nh2.x == food_single.x && nh2.y == food_single.y) {
        score_two++;
        generate_food_single();

        // 增加玩家二的移動間隔，使速度變慢
        if (player2_move_interval + MOVE_INTERVAL_INCREMENT <= MAX_MOVE_INTERVAL) {
            // 移除現有的 Timer
            if (two_player_timer2_id) {
                g_source_remove(two_player_timer2_id);
                two_player_timer2_id = 0;
            }

            player2_move_interval += MOVE_INTERVAL_INCREMENT;

            // 設置新的 Timer
            two_player_timer2_id = g_timeout_add(player2_move_interval, player2_move_cb, NULL);
        }
    }
    else {
        // 移除尾巴 玩家二
        Point* tail2 = (Point*)g_list_last(snake_two)->data;
        snake_two = g_list_remove(snake_two, tail2);
        free(tail2);
    }

    // 重繪
    gtk_widget_queue_draw(canvas_two);
    return G_SOURCE_CONTINUE;
}
// *** 雙人模式 ***

// 更新倒數計時 (單人)
static gboolean update_countdown_single(gpointer data)
{
    if (current_mode != MODE_COUNTDOWN_SINGLE)
        return G_SOURCE_REMOVE;

    if (current_countdown > 0) {
        current_countdown--;
        gtk_widget_queue_draw(canvas_single);
        return G_SOURCE_CONTINUE;
    }
    else {
        // 倒數結束，開始遊戲
        current_mode = MODE_SINGLE;
        countdown_timer_id = 0;
        single_timer_id = g_timeout_add(INITIAL_MOVE_INTERVAL, update_game_single, NULL);
        gtk_widget_queue_draw(canvas_single);
        return G_SOURCE_REMOVE;
    }
}

// *** 雙人模式 ***
// 更新倒數計時 (雙人模式)
static gboolean update_countdown_two_player(gpointer data)
{
    if (current_mode != MODE_COUNTDOWN_TWO_PLAYER)
        return G_SOURCE_REMOVE;

    if (current_countdown_two > 0) {
        current_countdown_two--;
        gtk_widget_queue_draw(canvas_two);
        return G_SOURCE_CONTINUE;
    }
    else {
        // 倒數結束，開始遊戲
        current_mode = MODE_TWO_PLAYER;
        countdown_two_player_timer_id = 0;

        // 設置玩家一的 Timer
        two_player_timer_id = g_timeout_add(player1_move_interval, player1_move_cb, NULL);
        player1_timer_active = TRUE;

        // 設置玩家二的 Timer
        two_player_timer2_id = g_timeout_add(player2_move_interval, player2_move_cb, NULL);
        player2_timer_active = TRUE;

        gtk_widget_queue_draw(canvas_two);
        return G_SOURCE_REMOVE;
    }
}
// *** 雙人模式 ***

// 顯示遊戲結束畫面 (單人)
static void show_game_over_screen_single(void)
{
    GtkStack* stack_ptr = GTK_STACK(stack);
    GtkWidget* existing_game_over_single = gtk_stack_get_child_by_name(stack_ptr, "game_over_single");
    if (existing_game_over_single) {
        gtk_stack_remove(stack_ptr, existing_game_over_single);
    }

    // 創建容器
    GtkWidget* game_over_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(game_over_vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(game_over_vbox, GTK_ALIGN_CENTER);

    // 標籤
    char buf[128];
    snprintf(buf, sizeof(buf), "遊戲結束!\n最終分數: %d", score_single);
    GtkWidget* label = gtk_label_new(buf);
    gtk_box_append(GTK_BOX(game_over_vbox), label);

    // 返回按鈕
    GtkWidget* back_btn = gtk_button_new_with_label("返回選單");
    gtk_box_append(GTK_BOX(game_over_vbox), back_btn);
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_to_menu_clicked), stack_ptr);

    // 加入GtkStack
    gtk_stack_add_named(stack_ptr, game_over_vbox, "game_over_single");
    gtk_stack_set_visible_child_name(stack_ptr, "game_over_single");
}

// *** 雙人模式 ***
// 顯示遊戲結束畫面 (雙人模式)
static void show_game_over_screen_two_player(void)
{
    GtkStack* stack_ptr = GTK_STACK(stack);
    GtkWidget* existing_game_over_two_player = gtk_stack_get_child_by_name(stack_ptr, "game_over_two_player");
    if (existing_game_over_two_player) {
        gtk_stack_remove(stack_ptr, existing_game_over_two_player);
    }

    // 創建容器
    GtkWidget* game_over_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(game_over_vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(game_over_vbox, GTK_ALIGN_CENTER);

    // 計算生存時間
    double player1_survival = player1_alive ? difftime(time(NULL), player1_start_time) : difftime(player1_end_time, player1_start_time);
    double player2_survival = player2_alive ? difftime(time(NULL), player2_start_time) : difftime(player2_end_time, player2_start_time);

    // 確定勝者
    const char* winner_text = "";
    if (score_single > score_two) {
        winner_text = "玩家一獲勝！";
    }
    else if (score_single < score_two) {
        winner_text = "玩家二獲勝！";
    }
    else {
        // 分數相同，依存活時間判斷
        if (player1_survival > player2_survival) {
            winner_text = "玩家一獲勝！";
        }
        else if (player1_survival < player2_survival) {
            winner_text = "玩家二獲勝！";
        }
        else {
            winner_text = "平手！";
        }
    }

    // 標籤
    char buf[256];
    snprintf(buf, sizeof(buf), "遊戲結束!\n最終分數:\n玩家一: %d\n玩家二: %d\n\n生存時間:\n玩家一: %.0f 秒\n玩家二: %.0f 秒\n\n%s",
        score_single, score_two,
        player1_survival, player2_survival,
        winner_text);
    GtkWidget* label = gtk_label_new(buf);
    gtk_box_append(GTK_BOX(game_over_vbox), label);

    // 返回按鈕
    GtkWidget* back_btn = gtk_button_new_with_label("返回選單");
    gtk_box_append(GTK_BOX(game_over_vbox), back_btn);
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_to_menu_clicked), stack_ptr);

    // 加入GtkStack
    gtk_stack_add_named(stack_ptr, game_over_vbox, "game_over_two_player");
    gtk_stack_set_visible_child_name(stack_ptr, "game_over_two_player");
}
// *** 雙人模式 ***

// 激活回調
static void activate(GtkApplication* app, gpointer user_data)
{
    // 創建視窗
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Snake Hero");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    // CSS
    GtkCssProvider* provider = gtk_css_provider_new();
    const gchar* css_data =
        "button {\n"
        "  min-width: 200px;\n"
        "  min-height: 50px;\n"
        "}\n"
        "label {\n"
        "  font-family: \"Microsoft JhengHei\";\n"
        "  font-size: 23px;\n"
        "}\n";
    gtk_css_provider_load_from_string(provider, css_data);
    GdkDisplay* dsp = gdk_display_get_default();
    gtk_style_context_add_provider_for_display(
        dsp,
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);

    // GtkStack
    stack = gtk_stack_new();
    gtk_window_set_child(GTK_WINDOW(window), stack);

    // 主選單
    main_menu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(main_menu, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(main_menu, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(main_menu, 20);
    gtk_widget_set_margin_bottom(main_menu, 20);
    gtk_widget_set_margin_start(main_menu, 20);
    gtk_widget_set_margin_end(main_menu, 20);

    // 標籤
    GtkWidget* label = gtk_label_new("選擇遊戲模式");
    gtk_box_append(GTK_BOX(main_menu), label);

    // 單人模式
    GtkWidget* single_btn = gtk_button_new_with_label("單人模式");
    gtk_box_append(GTK_BOX(main_menu), single_btn);
    g_signal_connect(single_btn, "clicked", G_CALLBACK(on_single_mode_clicked), stack);

    // *** 雙人模式 ***
    // 兩人模式
    GtkWidget* two_player_btn = gtk_button_new_with_label("雙人模式");
    gtk_box_append(GTK_BOX(main_menu), two_player_btn);
    g_signal_connect(two_player_btn, "clicked", G_CALLBACK(on_two_player_mode_clicked), stack);
    // *** 雙人模式 ***

    // 遊戲介紹
    GtkWidget* intro_btn = gtk_button_new_with_label("遊戲介紹");
    gtk_box_append(GTK_BOX(main_menu), intro_btn);
    g_signal_connect(intro_btn, "clicked", G_CALLBACK(on_intro_mode_clicked), stack);

    // 退出
    GtkWidget* quit_btn = gtk_button_new_with_label("退出遊戲");
    gtk_box_append(GTK_BOX(main_menu), quit_btn);
    g_signal_connect(quit_btn, "clicked", G_CALLBACK(on_quit_clicked), NULL);

    // 加入GtkStack
    gtk_stack_add_named(GTK_STACK(stack), main_menu, "main_menu");

    // 遊戲介紹視圖
    GtkWidget* intro_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(intro_vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(intro_vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(intro_vbox, 20);
    gtk_widget_set_margin_bottom(intro_vbox, 20);
    gtk_widget_set_margin_start(intro_vbox, 20);
    gtk_widget_set_margin_end(intro_vbox, 20);

    // 介紹文字
    GtkWidget* label_intro = gtk_label_new(
        "歡迎來到 Snake Hero!\n\n"
        "【規則】\n"
        "1. 蛇會自動移動。改變方向以避開自身。\n"
        "2. 碰到牆壁或自身會結束遊戲。\n"
        "3. 單人模式目標是盡可能多地吃食物以增加分數。\n"
        "4. 雙人模式允許兩位玩家同時競爭。\n\n"
        "【控制方式】\n"
        "玩家一: 使用 W/A/S/D 鍵控制蛇的方向。\n"
        "玩家二: 使用方向鍵控制蛇的方向。\n\n"
        "享受遊戲吧!"
    );
    gtk_label_set_xalign(GTK_LABEL(label_intro), 0.0);
    gtk_label_set_wrap(GTK_LABEL(label_intro), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(label_intro), PANGO_WRAP_WORD_CHAR);
    gtk_box_append(GTK_BOX(intro_vbox), label_intro);

    // 返回按鈕
    GtkWidget* back_btn_intro = gtk_button_new_with_label("返回選單");
    gtk_box_append(GTK_BOX(intro_vbox), back_btn_intro);
    g_signal_connect(back_btn_intro, "clicked", G_CALLBACK(on_back_to_menu_clicked), stack);

    // 加入GtkStack
    gtk_stack_add_named(GTK_STACK(stack), intro_vbox, "game_intro");

    // 顯示主選單
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "main_menu");

    // 鍵盤事件
    GtkEventController* controller = gtk_event_controller_key_new();
    g_signal_connect(controller, "key-pressed", G_CALLBACK(on_key_press), NULL);
    gtk_widget_add_controller(window, controller);

    // 顯示視窗
    gtk_window_present(GTK_WINDOW(window));
}

// 主函式
int main(int argc, char** argv) {
    setlocale(LC_ALL, "");
    srand((unsigned int)time(NULL));

    GtkApplication* app = gtk_application_new(
        "com.example.snakegame",
        G_APPLICATION_FLAGS_NONE
    );

    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
