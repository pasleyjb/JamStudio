#pragma once

#include <QMainWindow>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void createMenus();
    void createToolbar();
    void createStatusBar();
    void createCentralWidget();
    void createDockWindows();
};
