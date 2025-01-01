#define _USE_MATH_DEFINES
#include <math.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

//========================[ 常數 ]========================
#define GRID_WIDTH   40
#define GRID_HEIGHT  20
#define MAX_INTERVAL 250 // 最大移動間隔 (毫秒)

//========================[ 遊戲模式 ]========================
typedef enum {
    MODE_NONE = 0,
    MODE_MENU,
    MODE_SINGLE,
    MODE_MULTI,
    MODE_INTRO
} GameMode;

//========================[ 結構定義 ]========================
typedef struct {
    int x, y;
    int width, height;
} Obstacle;

typedef struct {
    int x, y;
} Point;

//========================[ 全域變數 ]========================

// GTK相關
static GtkWidget* window = NULL;
static GtkWidget* main_menu = NULL;
static GtkWidget* pause_dialog = NULL;
static GtkWidget* stack = NULL;

// 倒數計時
static int      countdown = 3;
static gboolean show_go = FALSE;
static gboolean game_started = FALSE;
static guint    countdown_timer_id = 0;

// 遊戲模式
static GameMode current_mode = MODE_MENU;
static gboolean game_over = FALSE;
static gboolean paused = FALSE;

// 通用障礙物
static GList* obstacles = NULL;

//=== 單人模式 ===
static GList* snake_single = NULL;
static Point   food_single;
static int     direction_single = GDK_KEY_Right;
static int     next_direction_single = GDK_KEY_Right;
static int     score_single = 0;
static GtkWidget* canvas_single = NULL;
static int    current_interval_single = 100;
static guint  single_timer_id = 0;
static gboolean player_single_alive = TRUE;

//=== 雙人模式 ===
static GList* snake1 = NULL;
static GList* snake2 = NULL;
static int    direction1 = GDK_KEY_d;
static int    next_direction1 = GDK_KEY_d;
static int    direction2 = GDK_KEY_Left;
static int    next_direction2 = GDK_KEY_Left;
static Point  food_multi;
static int    score1 = 0;
static int    score2 = 0;
static gboolean player1_alive = TRUE;
static gboolean player2_alive = TRUE;
static int    winner = 0;

// 雙人模式時間
static time_t start_time_multi = 0;
static time_t dead_time_player1 = 0;
static time_t dead_time_player2 = 0;

static GtkWidget* canvas_multi = NULL;
static int    current_interval_player1 = 100;
static int    current_interval_player2 = 100;
static guint  player1_timer_id = 0;
static guint  player2_timer_id = 0;

//=== 固定網格尺寸 ===
static double CELL_SIZE = 45.0;

//==============================================================
// [蛇閃爍機制]
typedef struct {
    GList** snake_ref;
    gboolean* snake_alive;
    GList* original_snake_data;
    int flicker_count;
    int flicker_max;
    GtkWidget* draw_canvas;
} FlickerData;

/* ===== 函式宣告 ===== */

/* 倒數相關 */
static void start_countdown(void);
static gboolean countdown_tick(gpointer data);

/* 清理和暫停對話框 */
static void clear_game_data(void);
static void create_pause_dialog(void);
static void on_resume_game(GtkButton* btn, gpointer user_data);

/* 返回主選單、退出等 */
static void on_back_to_menu_clicked(GtkButton* button, gpointer user_data);
static void on_quit_clicked(GtkButton* button, gpointer user_data);

/* 單人遊戲 */
static void init_single_game(void);
static gboolean update_game_single(gpointer data);
static void generate_food_single(void);
static gboolean check_collision_single(Point* head);
static void kill_player_single(void);
static void show_game_over_screen_single(void);

/* 雙人遊戲 */
static void init_multi_game(void);
static gboolean update_player1(gpointer data);
static gboolean update_player2(gpointer data);
static void generate_food_multi(void);
static gboolean check_self_collision(Point* head, GList* snake);
static gboolean check_snake_collision(Point* head, GList* snake);
static void kill_player1(void);
static void kill_player2(void);
static void end_two_player_game(void);
static void show_game_over_screen_multi(void);

/* 繪圖 */
static void draw_game(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data);
static void draw_single_mode(cairo_t* cr, int width, int height, double cell_size);
static void draw_multi_mode(cairo_t* cr, int width, int height, double cell_size);
static void draw_snake_segment(cairo_t* cr, double x, double y, double size, GdkRGBA body, GdkRGBA shadow);
static void draw_food(cairo_t* cr, double x, double y, double size);
static void draw_wall(cairo_t* cr, double x, double y, double w, double h);

/* 鍵盤事件 */
static gboolean on_key_press(GtkEventControllerKey* controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);

/* 主選單、遊戲介紹 */
static void show_main_menu(void);
static void on_single_mode_clicked(GtkButton* button, gpointer user_data);
static void on_multi_mode_clicked(GtkButton* button, gpointer user_data);
static void on_intro_mode_clicked(GtkButton* button, gpointer user_data);

/* 障礙物相關 */
static gboolean is_obstacle_valid(int x, int y, int w, int h, GList* excluded_positions);
static void generate_obstacles(GList* initial_positions);

/* 亂數方向 */
static int get_random_direction(void);

/* 蛇閃爍 */
static gboolean do_flicker_snake(gpointer data);
static void start_flicker_snake(GList** snake_ref, gboolean* snake_alive, GtkWidget* draw_canvas);


/* -----------------------------
   [蛇閃爍機制] - 實作
   ----------------------------- */
static gboolean do_flicker_snake(gpointer data)
{
    FlickerData* fd = (FlickerData*)data;
    if (*(fd->snake_ref)) {
        *(fd->snake_ref) = NULL;
    }
    else {
        *(fd->snake_ref) = fd->original_snake_data;
    }

    if (fd->draw_canvas) {
        gtk_widget_queue_draw(fd->draw_canvas);
    }

    fd->flicker_count++;
    if (fd->flicker_count >= fd->flicker_max) {
        *(fd->snake_alive) = FALSE;
        if (*(fd->snake_ref)) {
            for (GList* iter = *(fd->snake_ref); iter; iter = iter->next) {
                free(iter->data);
            }
            g_list_free(*(fd->snake_ref));
            *(fd->snake_ref) = NULL;
        }
        free(fd);

        if (current_mode == MODE_MULTI) {
            if (!player1_alive && !player2_alive) {
                end_two_player_game();
            }
        }
        else if (current_mode == MODE_SINGLE) {
            game_over = TRUE;
            if (single_timer_id) {
                g_source_remove(single_timer_id);
                single_timer_id = 0;
            }
            gtk_widget_queue_draw(canvas_single);
            show_game_over_screen_single();
        }
        return FALSE;
    }

    return TRUE;
}

static void start_flicker_snake(GList** snake_ref, gboolean* snake_alive, GtkWidget* draw_canvas)
{
    FlickerData* fd = (FlickerData*)malloc(sizeof(FlickerData));
    fd->snake_ref = snake_ref;
    fd->snake_alive = snake_alive;
    fd->original_snake_data = *snake_ref;
    fd->flicker_count = 0;
    fd->flicker_max = 4;
    fd->draw_canvas = draw_canvas;

    g_timeout_add(300, do_flicker_snake, fd);
}


/* -----------------------------
   [倒數機制]
   ----------------------------- */
static void start_countdown(void)
{
    countdown = 3;
    show_go = FALSE;
    game_started = FALSE;

    if (countdown_timer_id) {
        g_source_remove(countdown_timer_id);
        countdown_timer_id = 0;
    }
    countdown_timer_id = g_timeout_add(940, countdown_tick, NULL);
}

static gboolean countdown_tick(gpointer data)
{
    if (paused) {
        return TRUE;
    }

    if (countdown > 0) {
        countdown--;
    }
    else if (!show_go) {
        show_go = TRUE;
    }
    else {
        game_started = TRUE;
        countdown_timer_id = 0;
        return FALSE;
    }

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
    return TRUE;
}

/* -----------------------------
   [清理/返回主選單/暫停...]
   ----------------------------- */
static void clear_game_data(void)
{
    if (snake_single) {
        for (GList* iter = snake_single; iter; iter = iter->next) {
            free(iter->data);
        }
        g_list_free(snake_single);
        snake_single = NULL;
    }
    if (snake1) {
        for (GList* iter = snake1; iter; iter = iter->next) {
            free(iter->data);
        }
        g_list_free(snake1);
        snake1 = NULL;
    }
    if (snake2) {
        for (GList* iter = snake2; iter; iter = iter->next) {
            free(iter->data);
        }
        g_list_free(snake2);
        snake2 = NULL;
    }
    if (obstacles) {
        for (GList* iter = obstacles; iter; iter = iter->next) {
            free(iter->data);
        }
        g_list_free(obstacles);
        obstacles = NULL;
    }

    score_single = 0;
    score1 = 0;
    score2 = 0;
    dead_time_player1 = 0;
    dead_time_player2 = 0;
    start_time_multi = 0;

    direction_single = GDK_KEY_Right;
    next_direction_single = GDK_KEY_Right;
    direction1 = GDK_KEY_d;
    next_direction1 = GDK_KEY_d;
    direction2 = GDK_KEY_Left;
    next_direction2 = GDK_KEY_Left;

    current_interval_single = 100;
    current_interval_player1 = 100;
    current_interval_player2 = 100;

    if (single_timer_id) { g_source_remove(single_timer_id); single_timer_id = 0; }
    if (player1_timer_id) { g_source_remove(player1_timer_id); player1_timer_id = 0; }
    if (player2_timer_id) { g_source_remove(player2_timer_id); player2_timer_id = 0; }
    if (countdown_timer_id) { g_source_remove(countdown_timer_id); countdown_timer_id = 0; }

    player_single_alive = TRUE;

    game_started = FALSE;
    countdown = 3;
    show_go = FALSE;
}

static void on_back_to_menu_clicked(GtkButton* button, gpointer user_data)
{
    clear_game_data();
    game_over = FALSE;
    paused = FALSE;
    current_mode = MODE_MENU;
    player1_alive = TRUE;
    player2_alive = TRUE;
    winner = 0;

    if (pause_dialog) {
        gtk_window_destroy(GTK_WINDOW(pause_dialog));
        pause_dialog = NULL;
    }

    GtkStack* stack_ptr = GTK_STACK(stack);
    GtkWidget* game_single = gtk_stack_get_child_by_name(stack_ptr, "game_single");
    if (game_single) gtk_stack_remove(stack_ptr, game_single);

    GtkWidget* game_multi = gtk_stack_get_child_by_name(stack_ptr, "game_multi");
    if (game_multi) gtk_stack_remove(stack_ptr, game_multi);

    GtkWidget* game_over_single = gtk_stack_get_child_by_name(stack_ptr, "game_over_single");
    if (game_over_single) gtk_stack_remove(stack_ptr, game_over_single);

    GtkWidget* game_over_multi = gtk_stack_get_child_by_name(stack_ptr, "game_over_multi");
    if (game_over_multi) gtk_stack_remove(stack_ptr, game_over_multi);

    gtk_stack_set_visible_child_name(stack_ptr, "main_menu");
}

static void on_quit_clicked(GtkButton* button, gpointer user_data)
{
    GtkApplication* app = GTK_APPLICATION(gtk_window_get_application(GTK_WINDOW(window)));
    if (app) g_application_quit(G_APPLICATION(app));
}

static void create_pause_dialog(void)
{
    if (pause_dialog) return;

    pause_dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(pause_dialog), "遊戲暫停");
    gtk_window_set_transient_for(GTK_WINDOW(pause_dialog), GTK_WINDOW(window));
    gtk_window_set_modal(GTK_WINDOW(pause_dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(pause_dialog), 300, 150);
    gtk_window_set_decorated(GTK_WINDOW(pause_dialog), FALSE);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_window_set_child(GTK_WINDOW(pause_dialog), vbox);

    GtkWidget* label = gtk_label_new("遊戲已暫停");
    gtk_box_append(GTK_BOX(vbox), label);

    GtkWidget* resume_btn = gtk_button_new_with_label("繼續遊戲");
    gtk_box_append(GTK_BOX(vbox), resume_btn);
    g_signal_connect(resume_btn, "clicked", G_CALLBACK(on_resume_game), NULL);

    GtkWidget* back_btn = gtk_button_new_with_label("返回主選單");
    gtk_box_append(GTK_BOX(vbox), back_btn);
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_to_menu_clicked), NULL);

    gtk_window_present(GTK_WINDOW(pause_dialog));
}

static void on_resume_game(GtkButton* btn, gpointer user_data)
{
    paused = FALSE;
    if (pause_dialog) {
        gtk_window_destroy(GTK_WINDOW(pause_dialog));
        pause_dialog = NULL;
    }

    if (!game_started && (countdown > 0 || show_go == FALSE)) {
        countdown_timer_id = g_timeout_add(500, countdown_tick, NULL);
    }
}

/* -----------------------------
   [障礙物]
   ----------------------------- */
static gboolean is_obstacle_valid(int x, int y, int w, int h, GList* excluded_positions)
{
    for (GList* iter = excluded_positions; iter; iter = iter->next) {
        Point* p = (Point*)iter->data;
        float obs_cx = x + (float)w / 2;
        float obs_cy = y + (float)h / 2;
        float dx = obs_cx - p->x;
        float dy = obs_cy - p->y;
        float dist2 = dx * dx + dy * dy;
        if (dist2 < 16.0f) {
            return FALSE;
        }
    }
    return TRUE;
}

static void generate_obstacles(GList* initial_positions)
{
    int obstacles_per_zone = 2;
    int zone_count = 4;

    for (int zone = 0; zone < zone_count; zone++) {
        int x_min, x_max, y_min, y_max;
        switch (zone) {
        case 0:
            x_min = 0;
            x_max = GRID_WIDTH / 2 - 1;
            y_min = 0;
            y_max = GRID_HEIGHT / 2 - 1;
            break;
        case 1:
            x_min = GRID_WIDTH / 2;
            x_max = GRID_WIDTH - 1;
            y_min = 0;
            y_max = GRID_HEIGHT / 2 - 1;
            break;
        case 2:
            x_min = 0;
            x_max = GRID_WIDTH / 2 - 1;
            y_min = GRID_HEIGHT / 2;
            y_max = GRID_HEIGHT - 1;
            break;
        default:
            x_min = GRID_WIDTH / 2;
            x_max = GRID_WIDTH - 1;
            y_min = GRID_HEIGHT / 2;
            y_max = GRID_HEIGHT - 1;
            break;
        }

        for (int i = 0; i < obstacles_per_zone; i++) {
            int attempts = 0;
            int max_attempts = 500;
            while (attempts < max_attempts) {
                int w = rand() % 3 + 1;
                int h = rand() % 3 + 1;
                if ((x_max - x_min + 1) < w || (y_max - y_min + 1) < h) {
                    attempts++;
                    continue;
                }
                int rx = x_min + rand() % ((x_max - x_min + 1) - w);
                int ry = y_min + rand() % ((y_max - y_min + 1) - h);

                if (is_obstacle_valid(rx, ry, w, h, initial_positions)) {
                    Obstacle* obs = (Obstacle*)malloc(sizeof(Obstacle));
                    obs->x = rx;
                    obs->y = ry;
                    obs->width = w;
                    obs->height = h;
                    obstacles = g_list_append(obstacles, obs);
                    break;
                }
                attempts++;
            }
        }
    }
}

/* -----------------------------
   [ 隨機方向 ]
   ----------------------------- */
static int get_random_direction(void)
{
    int directions[] = { GDK_KEY_Up, GDK_KEY_Down, GDK_KEY_Left, GDK_KEY_Right };
    int index = rand() % 4;
    return directions[index];
}

/* -----------------------------
   [ 單人模式 ]
   ----------------------------- */
static void init_single_game(void)
{
    clear_game_data();

    Point* h = (Point*)malloc(sizeof(Point));
    h->x = GRID_WIDTH / 2;
    h->y = GRID_HEIGHT / 2;
    snake_single = g_list_append(snake_single, h);

    direction_single = get_random_direction();
    next_direction_single = direction_single;
    score_single = 0;
    game_over = FALSE;
    paused = FALSE;

    GList* initial_positions = NULL;
    initial_positions = g_list_append(initial_positions, h);

    generate_obstacles(initial_positions);
    generate_food_single();
    start_countdown();

    single_timer_id = g_timeout_add(current_interval_single, update_game_single, NULL);
    g_list_free(initial_positions);
}

static void generate_food_single(void)
{
    gboolean valid = FALSE;
    while (!valid) {
        food_single.x = rand() % GRID_WIDTH;
        food_single.y = rand() % GRID_HEIGHT;
        valid = TRUE;

        for (GList* iter = snake_single; iter; iter = iter->next) {
            Point* seg = (Point*)iter->data;
            if (seg->x == food_single.x && seg->y == food_single.y) {
                valid = FALSE;
                break;
            }
        }
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

static gboolean check_collision_single(Point* head)
{
    for (GList* iter = snake_single->next; iter; iter = iter->next) {
        Point* seg = (Point*)iter->data;
        if (seg->x == head->x && seg->y == head->y) return TRUE;
    }
    for (GList* iter = obstacles; iter; iter = iter->next) {
        Obstacle* obs = (Obstacle*)iter->data;
        if (head->x >= obs->x && head->x < obs->x + obs->width &&
            head->y >= obs->y && head->y < obs->y + obs->height) {
            return TRUE;
        }
    }
    return FALSE;
}

static void kill_player_single(void)
{
    start_flicker_snake(&snake_single, &player_single_alive, canvas_single);
}

static gboolean update_game_single(gpointer data)
{
    if (current_mode != MODE_SINGLE || game_over || paused) {
        return G_SOURCE_CONTINUE;
    }
    if (!game_started) {
        return G_SOURCE_CONTINUE;
    }

    direction_single = next_direction_single;

    Point* head = (Point*)snake_single->data;
    if (!head) {
        return G_SOURCE_CONTINUE;
    }
    Point nh = { head->x, head->y };
    switch (direction_single) {
    case GDK_KEY_Up:    nh.y--; break;
    case GDK_KEY_Down:  nh.y++; break;
    case GDK_KEY_Left:  nh.x--; break;
    case GDK_KEY_Right: nh.x++; break;
    }
    if (nh.x < 0) nh.x = GRID_WIDTH - 1;
    else if (nh.x >= GRID_WIDTH) nh.x = 0;
    if (nh.y < 0) nh.y = GRID_HEIGHT - 1;
    else if (nh.y >= GRID_HEIGHT) nh.y = 0;

    if (check_collision_single(&nh)) {
        kill_player_single();
        if (single_timer_id) {
            g_source_remove(single_timer_id);
            single_timer_id = 0;
        }
        return G_SOURCE_CONTINUE;
    }

    Point* new_seg = (Point*)malloc(sizeof(Point));
    *new_seg = nh;
    snake_single = g_list_prepend(snake_single, new_seg);

    if (nh.x == food_single.x && nh.y == food_single.y) {
        score_single++;
        generate_food_single();
    }
    else {
        Point* tail = (Point*)g_list_last(snake_single)->data;
        snake_single = g_list_remove(snake_single, tail);
        free(tail);
    }

    gtk_widget_queue_draw(canvas_single);
    return G_SOURCE_CONTINUE;
}

static void show_game_over_screen_single(void)
{
    GtkStack* stack_ptr = GTK_STACK(stack);
    GtkWidget* existing_game_over_single = gtk_stack_get_child_by_name(stack_ptr, "game_over_single");
    if (existing_game_over_single) {
        gtk_stack_remove(stack_ptr, existing_game_over_single);
    }

    GtkWidget* game_over_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(game_over_vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(game_over_vbox, GTK_ALIGN_CENTER);

    char buf[128];
    snprintf(buf, sizeof(buf), "遊戲結束！\n最終得分: %d", score_single);
    GtkWidget* label = gtk_label_new(buf);
    gtk_box_append(GTK_BOX(game_over_vbox), label);

    GtkWidget* back_btn = gtk_button_new_with_label("返回主選單");
    gtk_box_append(GTK_BOX(game_over_vbox), back_btn);
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_to_menu_clicked), NULL);

    gtk_stack_add_named(stack_ptr, game_over_vbox, "game_over_single");
    gtk_stack_set_visible_child_name(stack_ptr, "game_over_single");
}

/* -----------------------------
   [ 雙人模式 ]
   ----------------------------- */
static void init_multi_game(void)
{
    clear_game_data();

    Point* h1 = (Point*)malloc(sizeof(Point));
    h1->x = GRID_WIDTH / 4;
    h1->y = GRID_HEIGHT / 2;
    snake1 = g_list_append(snake1, h1);

    Point* h2 = (Point*)malloc(sizeof(Point));
    h2->x = (GRID_WIDTH * 3) / 4;
    h2->y = GRID_HEIGHT / 2;
    snake2 = g_list_append(snake2, h2);

    direction1 = get_random_direction();
    next_direction1 = direction1;
    direction2 = get_random_direction();
    next_direction2 = direction2;

    score1 = 0;
    score2 = 0;
    player1_alive = TRUE;
    player2_alive = TRUE;
    winner = 0;

    start_time_multi = time(NULL);
    dead_time_player1 = 0;
    dead_time_player2 = 0;

    game_over = FALSE;
    paused = FALSE;

    GList* initial_positions = NULL;
    initial_positions = g_list_append(initial_positions, h1);
    initial_positions = g_list_append(initial_positions, h2);

    generate_obstacles(initial_positions);
    generate_food_multi();
    start_countdown();

    player1_timer_id = g_timeout_add(current_interval_player1, update_player1, NULL);
    player2_timer_id = g_timeout_add(current_interval_player2, update_player2, NULL);

    g_list_free(initial_positions);
}

static void generate_food_multi(void)
{
    gboolean valid = FALSE;
    while (!valid) {
        food_multi.x = rand() % GRID_WIDTH;
        food_multi.y = rand() % GRID_HEIGHT;
        valid = TRUE;

        for (GList* it = snake1; it; it = it->next) {
            Point* seg = (Point*)it->data;
            if (seg->x == food_multi.x && seg->y == food_multi.y) {
                valid = FALSE;
                break;
            }
        }
        for (GList* it = snake2; it; it = it->next) {
            Point* seg = (Point*)it->data;
            if (seg->x == food_multi.x && seg->y == food_multi.y) {
                valid = FALSE;
                break;
            }
        }
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

static gboolean check_self_collision(Point* head, GList* snake)
{
    for (GList* iter = snake->next; iter; iter = iter->next) {
        Point* seg = (Point*)iter->data;
        if (seg->x == head->x && seg->y == head->y) return TRUE;
    }
    return FALSE;
}

static gboolean check_snake_collision(Point* head, GList* snake)
{
    for (GList* iter = snake; iter; iter = iter->next) {
        Point* seg = (Point*)iter->data;
        if (seg->x == head->x && seg->y == head->y) return TRUE;
    }
    return FALSE;
}

static void kill_player1(void)
{
    if (dead_time_player1 == 0) {
        dead_time_player1 = time(NULL) - start_time_multi;
    }
    start_flicker_snake(&snake1, &player1_alive, canvas_multi);
}

static void kill_player2(void)
{
    if (dead_time_player2 == 0) {
        dead_time_player2 = time(NULL) - start_time_multi;
    }
    start_flicker_snake(&snake2, &player2_alive, canvas_multi);
}

static void end_two_player_game(void)
{
    if (!player1_alive && !player2_alive) {
        game_over = TRUE;

        if (score1 > score2) {
            winner = 1;
        }
        else if (score2 > score1) {
            winner = 2;
        }
        else {
            if (dead_time_player1 > dead_time_player2) {
                winner = 1;
            }
            else if (dead_time_player2 > dead_time_player1) {
                winner = 2;
            }
            else {
                winner = 0;
            }
        }

        if (player1_timer_id) { g_source_remove(player1_timer_id); player1_timer_id = 0; }
        if (player2_timer_id) { g_source_remove(player2_timer_id); player2_timer_id = 0; }

        gtk_widget_queue_draw(canvas_multi);
        show_game_over_screen_multi();
    }
}

static gboolean update_player1(gpointer data)
{
    if (current_mode != MODE_MULTI || game_over || paused || !player1_alive)
        return G_SOURCE_CONTINUE;
    if (!game_started)
        return G_SOURCE_CONTINUE;

    direction1 = next_direction1;

    Point* head = (Point*)snake1->data;
    if (!head) {
        return G_SOURCE_CONTINUE;
    }
    Point nh = { head->x, head->y };
    switch (direction1) {
    case GDK_KEY_Up:    nh.y--; break;
    case GDK_KEY_Down:  nh.y++; break;
    case GDK_KEY_Left:  nh.x--; break;
    case GDK_KEY_Right: nh.x++; break;
    }
    if (nh.x < 0) nh.x = GRID_WIDTH - 1;
    else if (nh.x >= GRID_WIDTH) nh.x = 0;
    if (nh.y < 0) nh.y = GRID_HEIGHT - 1;
    else if (nh.y >= GRID_HEIGHT) nh.y = 0;

    if (check_self_collision(&nh, snake1)) {
        kill_player1();
        if (player1_timer_id) {
            g_source_remove(player1_timer_id);
            player1_timer_id = 0;
        }
        return G_SOURCE_CONTINUE;
    }
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

    Point* new_seg = (Point*)malloc(sizeof(Point));
    *new_seg = nh;
    snake1 = g_list_prepend(snake1, new_seg);

    if (nh.x == food_multi.x && nh.y == food_multi.y) {
        score1++;
        generate_food_multi();

        if (current_interval_player1 < MAX_INTERVAL) {
            current_interval_player1 += 5;
            if (player1_timer_id) g_source_remove(player1_timer_id);
            player1_timer_id = g_timeout_add(current_interval_player1, update_player1, NULL);
        }
    }
    else {
        Point* tail = (Point*)g_list_last(snake1)->data;
        snake1 = g_list_remove(snake1, tail);
        free(tail);
    }

    gtk_widget_queue_draw(canvas_multi);
    return G_SOURCE_CONTINUE;
}

static gboolean update_player2(gpointer data)
{
    if (current_mode != MODE_MULTI || game_over || paused || !player2_alive)
        return G_SOURCE_CONTINUE;
    if (!game_started)
        return G_SOURCE_CONTINUE;

    direction2 = next_direction2;

    Point* head = (Point*)snake2->data;
    if (!head) {
        return G_SOURCE_CONTINUE;
    }
    Point nh = { head->x, head->y };
    switch (direction2) {
    case GDK_KEY_Up:    nh.y--; break;
    case GDK_KEY_Down:  nh.y++; break;
    case GDK_KEY_Left:  nh.x--; break;
    case GDK_KEY_Right: nh.x++; break;
    }
    if (nh.x < 0) nh.x = GRID_WIDTH - 1;
    else if (nh.x >= GRID_WIDTH) nh.x = 0;
    if (nh.y < 0) nh.y = GRID_HEIGHT - 1;
    else if (nh.y >= GRID_HEIGHT) nh.y = 0;

    if (check_self_collision(&nh, snake2)) {
        kill_player2();
        if (player2_timer_id) {
            g_source_remove(player2_timer_id);
            player2_timer_id = 0;
        }
        return G_SOURCE_CONTINUE;
    }
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

    Point* new_seg = (Point*)malloc(sizeof(Point));
    *new_seg = nh;
    snake2 = g_list_prepend(snake2, new_seg);

    if (nh.x == food_multi.x && nh.y == food_multi.y) {
        score2++;
        generate_food_multi();

        if (current_interval_player2 < MAX_INTERVAL) {
            current_interval_player2 += 5;
            if (player2_timer_id) g_source_remove(player2_timer_id);
            player2_timer_id = g_timeout_add(current_interval_player2, update_player2, NULL);
        }
    }
    else {
        Point* tail = (Point*)g_list_last(snake2)->data;
        snake2 = g_list_remove(snake2, tail);
        free(tail);
    }

    gtk_widget_queue_draw(canvas_multi);
    return G_SOURCE_CONTINUE;
}

static void show_game_over_screen_multi(void)
{
    GtkStack* stack_ptr = GTK_STACK(stack);
    GtkWidget* existing_game_over_multi = gtk_stack_get_child_by_name(stack_ptr, "game_over_multi");
    if (existing_game_over_multi) {
        gtk_stack_remove(stack_ptr, existing_game_over_multi);
    }

    GtkWidget* game_over_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(game_over_vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(game_over_vbox, GTK_ALIGN_CENTER);

    const char* res = NULL;
    if (winner == 1) {
        res = "玩家1 (綠色) 獲勝！";
    }
    else if (winner == 2) {
        res = "玩家2 (橘色) 獲勝！";
    }
    else {
        res = "平局！";
    }

    char buf[256];
    snprintf(buf, sizeof(buf),
        "遊戲結束！\n玩家1:%d   玩家2:%d\n\n%s\n\n(玩家1存活: %ld 秒, 玩家2存活: %ld 秒)",
        score1, score2, res,
        (long)dead_time_player1, (long)dead_time_player2);
    GtkWidget* label = gtk_label_new(buf);
    gtk_box_append(GTK_BOX(game_over_vbox), label);

    GtkWidget* back_btn = gtk_button_new_with_label("返回主選單");
    gtk_box_append(GTK_BOX(game_over_vbox), back_btn);
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_to_menu_clicked), NULL);

    gtk_stack_add_named(stack_ptr, game_over_vbox, "game_over_multi");
    gtk_stack_set_visible_child_name(stack_ptr, "game_over_multi");
}

/* -----------------------------
   [ 繪圖：蛇/牆/果實 ]
   ----------------------------- */
static void draw_snake_segment(cairo_t* cr, double x, double y, double size, GdkRGBA body, GdkRGBA shadow)
{
    cairo_set_source_rgba(cr, shadow.red, shadow.green, shadow.blue, shadow.alpha);
    double off = 2;
    cairo_rectangle(cr, x + off, y + off, size, size);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, body.red, body.green, body.blue, body.alpha);
    cairo_rectangle(cr, x, y, size, size);
    cairo_fill(cr);
}

static void draw_food(cairo_t* cr, double x, double y, double size)
{
    cairo_set_source_rgb(cr, 0.5, 0.0, 0.0);
    cairo_rectangle(cr, x + 2, y + 2, size, size);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
    cairo_rectangle(cr, x, y, size, size);
    cairo_fill(cr);
}

static void draw_wall(cairo_t* cr, double x, double y, double w, double h)
{
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_rectangle(cr, x + 2, y + 2, w, h);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
}

/* -----------------------------
   [ 繪製各模式 ]
   ----------------------------- */
static void draw_single_mode(cairo_t* cr, int width, int height, double cell_size)
{
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_paint(cr);

    draw_food(cr, food_single.x * cell_size, food_single.y * cell_size, cell_size);

    for (GList* iter = obstacles; iter; iter = iter->next) {
        Obstacle* obs = (Obstacle*)iter->data;
        draw_wall(cr, obs->x * cell_size, obs->y * cell_size,
            obs->width * cell_size, obs->height * cell_size);
    }

    GdkRGBA body_green = { 0.0,1.0,0.0,1.0 };
    GdkRGBA shadow_green = { 0.0,0.4,0.0,1.0 };
    for (GList* iter = snake_single; iter; iter = iter->next) {
        Point* seg = (Point*)iter->data;
        if (seg) {
            draw_snake_segment(cr, seg->x * cell_size, seg->y * cell_size,
                cell_size, body_green, shadow_green);
        }
    }

    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_select_font_face(cr, "Microsoft JhengHei",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 16);
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "分數: %d", score_single);
        cairo_move_to(cr, 10, 25);
        cairo_show_text(cr, buf);
    }

    if (!game_started) {
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_set_font_size(cr, 40);
        if (countdown > 0) {
            char cbuf[8];
            snprintf(cbuf, sizeof(cbuf), "%d", countdown);
            cairo_move_to(cr, width / 2 - 20, height / 2);
            cairo_show_text(cr, cbuf);
        }
        else if (show_go && !game_started) {
            cairo_move_to(cr, width / 2 - 30, height / 2);
            cairo_show_text(cr, "開始！");
        }
    }
}

static void draw_multi_mode(cairo_t* cr, int width, int height, double cell_size)
{
    cairo_set_source_rgb(cr, 0.12, 0.12, 0.12);
    cairo_paint(cr);

    draw_food(cr, food_multi.x * cell_size, food_multi.y * cell_size, cell_size);

    for (GList* it = obstacles; it; it = it->next) {
        Obstacle* obs = (Obstacle*)it->data;
        draw_wall(cr, obs->x * cell_size, obs->y * cell_size,
            obs->width * cell_size, obs->height * cell_size);
    }

    GdkRGBA b1 = { 0.0,1.0,0.0,1.0 };
    GdkRGBA s1 = { 0.0,0.4,0.0,1.0 };
    for (GList* it = snake1; it; it = it->next) {
        Point* seg = (Point*)it->data;
        if (seg) {
            draw_snake_segment(cr, seg->x * cell_size, seg->y * cell_size,
                cell_size, b1, s1);
        }
    }

    GdkRGBA b2 = { 1.0,0.5,0.0,1.0 };
    GdkRGBA s2 = { 0.4,0.2,0.0,1.0 };
    for (GList* it = snake2; it; it = it->next) {
        Point* seg = (Point*)it->data;
        if (seg) {
            draw_snake_segment(cr, seg->x * cell_size, seg->y * cell_size,
                cell_size, b2, s2);
        }
    }

    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_select_font_face(cr, "Microsoft JhengHei",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 16);
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "玩家1:%d   玩家2:%d", score1, score2);
        cairo_move_to(cr, 10, 25);
        cairo_show_text(cr, buf);
    }

    if (!game_started) {
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_set_font_size(cr, 40);
        if (countdown > 0) {
            char cbuf[8];
            snprintf(cbuf, sizeof(cbuf), "%d", countdown);
            cairo_move_to(cr, width / 2 - 20, height / 2);
            cairo_show_text(cr, cbuf);
        }
        else if (show_go && !game_started) {
            cairo_move_to(cr, width / 2 - 30, height / 2);
            cairo_show_text(cr, "開始！");
        }
    }
}

/* -----------------------------
   [繪圖主函式]
   ----------------------------- */
static void draw_game(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data)
{
    if (current_mode == MODE_INTRO) {
        return;
    }

    switch (current_mode) {
    case MODE_SINGLE:
        draw_single_mode(cr, width, height, CELL_SIZE);
        break;
    case MODE_MULTI:
        draw_multi_mode(cr, width, height, CELL_SIZE);
        break;
    default:
        break;
    }
}

/* -----------------------------
   [鍵盤事件]
   ----------------------------- */
static gboolean on_key_press(GtkEventControllerKey* controller,
    guint keyval, guint keycode,
    GdkModifierType state,
    gpointer user_data)
{
    if ((current_mode == MODE_SINGLE || current_mode == MODE_MULTI)
        && game_started && !paused && !game_over)
    {
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
        else if (current_mode == MODE_MULTI) {
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

    if ((current_mode == MODE_SINGLE || current_mode == MODE_MULTI) && !game_over) {
        if (keyval == GDK_KEY_Escape) {
            paused = !paused;
            if (paused) {
                create_pause_dialog();
            }
            else if (pause_dialog) {
                gtk_window_destroy(GTK_WINDOW(pause_dialog));
                pause_dialog = NULL;
                if (!game_started && (countdown > 0 || show_go == FALSE)) {
                    countdown_timer_id = g_timeout_add(500, countdown_tick, NULL);
                }
            }
            return TRUE;
        }
    }

    return FALSE;
}

/* -----------------------------
   [主選單與介紹模式]
   ----------------------------- */
static void show_main_menu(void)
{
    if (stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(stack), "main_menu");
    }
}

static void on_single_mode_clicked(GtkButton* button, gpointer user_data)
{
    GtkStack* stack_ptr = GTK_STACK(user_data);
    current_mode = MODE_SINGLE;

    init_single_game();

    GtkWidget* existing_game_single = gtk_stack_get_child_by_name(stack_ptr, "game_single");
    if (existing_game_single) {
        gtk_stack_remove(stack_ptr, existing_game_single);
    }

    GtkWidget* new_canvas = gtk_drawing_area_new();
    gtk_widget_set_size_request(new_canvas, 1800, 900);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(new_canvas), draw_game, NULL, NULL);

    GtkWidget* container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(container, TRUE);
    gtk_widget_set_vexpand(container, TRUE);
    gtk_widget_set_halign(container, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(container, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(container), new_canvas);

    gtk_stack_add_named(stack_ptr, container, "game_single");
    canvas_single = new_canvas;
    gtk_stack_set_visible_child_name(stack_ptr, "game_single");
}

static void on_multi_mode_clicked(GtkButton* button, gpointer user_data)
{
    GtkStack* stack_ptr = GTK_STACK(user_data);
    current_mode = MODE_MULTI;

    init_multi_game();

    GtkWidget* existing_game_multi = gtk_stack_get_child_by_name(stack_ptr, "game_multi");
    if (existing_game_multi) {
        gtk_stack_remove(stack_ptr, existing_game_multi);
    }

    GtkWidget* new_canvas = gtk_drawing_area_new();
    gtk_widget_set_size_request(new_canvas, 1800, 900);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(new_canvas), draw_game, NULL, NULL);

    GtkWidget* container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(container, TRUE);
    gtk_widget_set_vexpand(container, TRUE);
    gtk_widget_set_halign(container, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(container, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(container), new_canvas);

    gtk_stack_add_named(stack_ptr, container, "game_multi");
    canvas_multi = new_canvas;
    gtk_stack_set_visible_child_name(stack_ptr, "game_multi");
}

static void on_intro_mode_clicked(GtkButton* button, gpointer user_data)
{
    GtkStack* stack_ptr = GTK_STACK(user_data);
    current_mode = MODE_INTRO;
    gtk_stack_set_visible_child_name(stack_ptr, "game_intro");
}

/* -----------------------------
   [主函式 main]
   ----------------------------- */
static void app_activate(GtkApplication* app, gpointer user_data)
{
    // 創建主視窗
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Snake Hero 蛇之英雄");
    gtk_window_set_default_size(GTK_WINDOW(window), 1920, 1080);

    // CSS: 設置字體為微軟正黑體
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        "button {\n"
        "  min-width: 200px;\n"
        "  min-height: 50px;\n"
        "}\n"
        "label {\n"
        "  font-family: \"Microsoft JhengHei\";\n"
        "  font-size: 23px;\n"
        "}\n"
    );
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

    // 主選單標籤
    GtkWidget* label = gtk_label_new("選擇遊戲模式");
    gtk_box_append(GTK_BOX(main_menu), label);

    // 單人模式按鈕
    GtkWidget* single_btn = gtk_button_new_with_label("單人模式");
    gtk_box_append(GTK_BOX(main_menu), single_btn);
    g_signal_connect(single_btn, "clicked", G_CALLBACK(on_single_mode_clicked), stack);

    // 雙人模式按鈕
    GtkWidget* multi_btn = gtk_button_new_with_label("雙人模式 (玩家1 vs 玩家2)");
    gtk_box_append(GTK_BOX(main_menu), multi_btn);
    g_signal_connect(multi_btn, "clicked", G_CALLBACK(on_multi_mode_clicked), stack);

    // 遊戲介紹按鈕
    GtkWidget* intro_btn = gtk_button_new_with_label("遊戲介紹");
    gtk_box_append(GTK_BOX(main_menu), intro_btn);
    g_signal_connect(intro_btn, "clicked", G_CALLBACK(on_intro_mode_clicked), stack);

    // 退出遊戲按鈕
    GtkWidget* quit_btn = gtk_button_new_with_label("退出遊戲");
    gtk_box_append(GTK_BOX(main_menu), quit_btn);
    g_signal_connect(quit_btn, "clicked", G_CALLBACK(on_quit_clicked), NULL);

    // 加入stack
    gtk_stack_add_named(GTK_STACK(stack), main_menu, "main_menu");

    // 遊戲介紹視圖
    GtkWidget* intro_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(intro_vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(intro_vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(intro_vbox, 20);
    gtk_widget_set_margin_bottom(intro_vbox, 20);
    gtk_widget_set_margin_start(intro_vbox, 20);
    gtk_widget_set_margin_end(intro_vbox, 20);

    GtkWidget* label_intro = gtk_label_new(
        "歡迎來到 Snake Hero 蛇之英雄！\n\n"
        "【遊戲規則】\n"
        "1. 蛇會自動移動，玩家需要透過改變方向來避開障礙物和蛇身。\n"
        "2. 撞到牆壁、障礙物或自身會導致蛇死亡。\n"
        "3. 單人模式與一般貪食蛇相同，盡可能吃果實提高分數。\n"
        "4. 雙人模式以分數高者勝，同分再比存活時間。\n\n"
        "【操作說明】\n"
        "單人：方向鍵\n"
        "玩家1：WASD\n"
        "玩家2：方向鍵\n\n"
        "祝遊戲愉快！"
    );
    gtk_label_set_xalign(GTK_LABEL(label_intro), 0.0);
    gtk_label_set_wrap(GTK_LABEL(label_intro), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(label_intro), PANGO_WRAP_WORD_CHAR);
    gtk_box_append(GTK_BOX(intro_vbox), label_intro);

    GtkWidget* back_btn_intro = gtk_button_new_with_label("返回主選單");
    gtk_box_append(GTK_BOX(intro_vbox), back_btn_intro);
    g_signal_connect(back_btn_intro, "clicked", G_CALLBACK(on_back_to_menu_clicked), NULL);

    gtk_stack_add_named(GTK_STACK(stack), intro_vbox, "game_intro");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "main_menu");

    // 監聽鍵盤事件
    GtkEventController* controller = gtk_event_controller_key_new();
    g_signal_connect(controller, "key-pressed", G_CALLBACK(on_key_press), NULL);
    gtk_widget_add_controller(window, controller);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char** argv)
{
    setlocale(LC_ALL, "");
    srand((unsigned int)time(NULL));

    gtk_init();  // 初始化 GTK

    // 建立 GtkApplication
    GtkApplication* app = gtk_application_new("com.example.snakegame", G_APPLICATION_FLAGS_NONE);

    // 綁定 "activate" 到自定的函式
    g_signal_connect(app, "activate", G_CALLBACK(app_activate), NULL);

    // 執行應用程式
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
