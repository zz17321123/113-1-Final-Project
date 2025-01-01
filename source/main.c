#include <gtk/gtk.h>

/*
 * print_hello:
 *   這是一個回呼函式(Callback Function)，當使用者按下按鈕時，會被呼叫。
 *   它會在終端機上印出 "Hello World"。
 */
static void
print_hello(GtkWidget* widget,
    gpointer   data)
{
    g_print("Hello World\n");
}

/*
 * activate:
 *   這是主視窗建立與元件初始化的函式。
 *   當 GtkApplication 收到 "activate" 訊號 (app 被啟動時) 時，會呼叫此函式。
 */
static void
activate(GtkApplication* app,
    gpointer        user_data)
{
    GtkWidget* window;  // 主視窗
    GtkWidget* button;  // 按鈕
    GtkWidget* box;     // 容器 (使用 Box 來安放按鈕)

    // 建立新視窗並加到這個 GtkApplication 裡
    window = gtk_application_window_new(app);

    // 設定視窗標題
    gtk_window_set_title(GTK_WINDOW(window), "Window");
    // 設定視窗預設大小 (寬 200, 高 200)
    gtk_window_set_default_size(GTK_WINDOW(window), 200, 200);

    // 建立一個垂直排列的 box 容器，間距設為 0
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    // 設定 box 的對齊方式，水平與垂直皆在中央
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    // 把 box 放到主視窗裡 (與 gtk_container_add 類似，新 API 為 gtk_window_set_child)
    gtk_window_set_child(GTK_WINDOW(window), box);

    // 建立一個帶有 "Hello World" 標籤的按鈕
    button = gtk_button_new_with_label("Hello World");

    /*
     * 連結按鈕事件與回呼函式：
     *   - 當按鈕被按下 (clicked) 時，會呼叫 print_hello 函式。
     */
    g_signal_connect(button, "clicked", G_CALLBACK(print_hello), NULL);

    /*
     * g_signal_connect_swapped:
     *   這段表示按鈕按下後，會呼叫 gtk_window_destroy 函式來銷毀視窗本身 (window)。
     *   swapped 的意思是將函式參數和物件交換，
     *   也就是把按鈕 callback 的第一個參數換成 window 指標，以供 gtk_window_destroy 使用。
     */
    g_signal_connect_swapped(button, "clicked", G_CALLBACK(gtk_window_destroy), window);

    // 將按鈕加入 box 容器
    gtk_box_append(GTK_BOX(box), button);

    // 顯示視窗 (替換 gtk_widget_show_all 的新函式呼叫)
    gtk_window_present(GTK_WINDOW(window));
}

/*
 * main:
 *   C 語言程式的進入點。
 *   在這裡建立 GtkApplication，並執行主迴圈 (主事件迴圈)。
 */
int
main(int    argc,
    char** argv)
{
    GtkApplication* app;
    int status;

    // 建立一個新的 GtkApplication，命名空間是 "org.gtk.example"
    app = gtk_application_new("org.gtk.example", G_APPLICATION_DEFAULT_FLAGS);

    // 連結 "activate" 訊號到 activate 函式
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    // 執行應用程式 (主事件迴圈)，並傳入命令列參數
    status = g_application_run(G_APPLICATION(app), argc, argv);

    // 釋放應用程式物件
    g_object_unref(app);

    // 回傳執行結果 (0 通常表示成功結束)
    return status;
}
