#include <gtk/gtk.h>

/* 預先宣告我們需要的函式，避免順序問題 */
static void on_intro_clicked(GtkButton* button, gpointer user_data);
static void on_back_clicked(GtkButton* button, gpointer user_data);
static void on_quit_clicked(GtkButton* button, gpointer user_data);
static void activate(GtkApplication* app, gpointer user_data);

/* 全域變數：用於在不同函式間切換視圖 */
static GtkWidget* stack = NULL;

/* === 「離開遊戲」按鈕的回呼函式 === */
static void
on_quit_clicked(GtkButton* button, gpointer user_data)
{
    /* user_data 傳進來的是 GApplication */
    GApplication* app = G_APPLICATION(user_data);
    g_application_quit(app);
}

/* === 點擊「遊戲介紹」時，切換到『介紹視圖』 === */
static void
on_intro_clicked(GtkButton* button, gpointer user_data)
{
    /* 因為我們的 stack 是全域變數，所以直接使用它 */
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "intro_page");
}

/* === 「介紹視圖」中的「返回主選單」按鈕 === */
static void
on_back_clicked(GtkButton* button, gpointer user_data)
{
    /* 回到主選單 */
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "main_menu");
}

/* === GtkApplication 的 activate 回呼函式 === */
static void
activate(GtkApplication* app, gpointer user_data)
{
    GtkWidget* window;
    GtkWidget* main_menu;
    GtkWidget* intro_page;

    /* --- 產生視窗 --- */
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Snake Hero - 主選單");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);

    /* --- 建立 GtkStack 用於管理「主選單」和「遊戲介紹」頁面 --- */
    stack = gtk_stack_new();
    gtk_window_set_child(GTK_WINDOW(window), stack);

    /* ------------------------------------------------------------
     * 1. 主選單視圖 main_menu
     * ------------------------------------------------------------ */
    main_menu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    /* 設定置中對齊 */
    gtk_widget_set_halign(main_menu, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(main_menu, GTK_ALIGN_CENTER);

    /* (a) 「遊戲介紹」按鈕 */
    GtkWidget* btn_intro = gtk_button_new_with_label("遊戲介紹");
    g_signal_connect(btn_intro, "clicked", G_CALLBACK(on_intro_clicked), NULL);
    gtk_box_append(GTK_BOX(main_menu), btn_intro);

    /* (b) 「離開遊戲」按鈕 */
    GtkWidget* btn_quit = gtk_button_new_with_label("離開遊戲");
    g_signal_connect(btn_quit, "clicked", G_CALLBACK(on_quit_clicked), app);
    gtk_box_append(GTK_BOX(main_menu), btn_quit);

    /* 將 main_menu 這個容器加入到 stack */
    gtk_stack_add_named(GTK_STACK(stack), main_menu, "main_menu");

    /* ------------------------------------------------------------
     * 2. 遊戲介紹視圖 intro_page
     * ------------------------------------------------------------ */
    intro_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    /* 設定置中對齊 */
    gtk_widget_set_halign(intro_page, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(intro_page, GTK_ALIGN_CENTER);

    /* (a) 標籤：顯示遊戲介紹內容 */
    GtkWidget* label_intro = gtk_label_new(
        "這裡可以放遊戲的說明、玩法、操作方式等。\n\n"
        "1. 這是最簡單的示例，只有『主選單』與『遊戲介紹』兩個頁面。\n"
        "2. 點擊『返回主選單』按鈕，可以回到主選單視圖。\n"
        "3. 點擊『離開遊戲』按鈕，程式就會結束。"
    );
    gtk_label_set_xalign(GTK_LABEL(label_intro), 0.5);    /* 文字置中 */
    gtk_label_set_wrap(GTK_LABEL(label_intro), TRUE);     /* 允許換行 */
    gtk_box_append(GTK_BOX(intro_page), label_intro);

    /* (b) 「返回主選單」按鈕 */
    GtkWidget* btn_back = gtk_button_new_with_label("返回主選單");
    g_signal_connect(btn_back, "clicked", G_CALLBACK(on_back_clicked), NULL);
    gtk_box_append(GTK_BOX(intro_page), btn_back);

    /* 將 intro_page 這個容器加入到 stack */
    gtk_stack_add_named(GTK_STACK(stack), intro_page, "intro_page");

    /* --- 預設顯示「主選單」 --- */
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "main_menu");

    /* --- 顯示視窗 --- */
    gtk_window_present(GTK_WINDOW(window));
}

/* === 一般的 main() 作為程式入口點 === */
int
main(int argc, char** argv)
{
    GtkApplication* app;
    int status;

    /* 建立 GtkApplication */
    app = gtk_application_new("com.example.snakehero_menu",
        G_APPLICATION_FLAGS_NONE);

    /* 連結 "activate" 信號到 activate 函式 */
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    /* 執行應用程式 (主事件迴圈) */
    status = g_application_run(G_APPLICATION(app), argc, argv);

    /* 結束後釋放資源 */
    g_object_unref(app);
    return status;
}
