#include <gtk/gtk.h>

/*
 * 按鈕回呼函式：結束應用程式
 * 這邊直接呼叫 g_application_quit 來結束應用程式。
 */
static void
on_quit_clicked(GtkButton* button,
    gpointer   user_data)
{
    GApplication* app = G_APPLICATION(user_data);
    g_application_quit(app);
}

/*
 * activate 回呼函式：建立主視窗，並放置一個只有「退出遊戲」的按鈕。
 */
static void
activate(GtkApplication* app,
    gpointer        user_data)
{
    GtkWidget* window;
    GtkWidget* vbox;
    GtkWidget* quit_btn;

    /* 建立主視窗 */
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Snake Hero 蛇之英雄");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);

    /* 建立一個垂直容器，讓按鈕在視窗中置中排列 */
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);

    /* 建立「退出遊戲」按鈕，並連結回呼函式 */
    quit_btn = gtk_button_new_with_label("退出遊戲");
    g_signal_connect(quit_btn, "clicked", G_CALLBACK(on_quit_clicked), app);

    /* 將按鈕放入容器，再將容器放到視窗中 */
    gtk_box_append(GTK_BOX(vbox), quit_btn);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    /* 顯示視窗 */
    gtk_window_present(GTK_WINDOW(window));
}

/*
 * 一般的 C 語言程式進入點（main），
 * 這裡使用 GtkApplication 來管理整個應用程式生命週期。
 */
int
main(int    argc,
    char** argv)
{
    GtkApplication* app;
    int status;

    /* 建立 GtkApplication */
    app = gtk_application_new("com.example.snakehero_minimal",
        G_APPLICATION_FLAGS_NONE);

    /* 連結 "activate" 信號到 activate 函式 */
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    /* 執行應用程式主迴圈 */
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app); /* 釋放資源 */

    return status;
}
