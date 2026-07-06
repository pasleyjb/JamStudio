#include "MainWindow.h"

#include <QLabel>
#include <QStatusBar>
#include <Qt>

MainWindow::MainWindow()
{
    setWindowTitle("JamStudio");

    resize(1200, 800);

    auto *label = new QLabel("Welcome to JamStudio");

    label->setAlignment(Qt::AlignCenter);

    setCentralWidget(label);

    statusBar()->showMessage("Ready");
}
