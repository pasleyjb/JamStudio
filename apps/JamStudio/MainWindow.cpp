#include "MainWindow.h"

#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QTextEdit>
#include <QToolBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("JamStudio");
    resize(1400, 900);

    createMenus();
    createToolbar();
    createCentralWidget();
    createDockWindows();
    createStatusBar();
}

void MainWindow::createMenus()
{
    menuBar()->addMenu("&File");
    menuBar()->addMenu("&Edit");
    menuBar()->addMenu("&View");
    menuBar()->addMenu("&Project");
    menuBar()->addMenu("&Audio");
    menuBar()->addMenu("&MIDI");
    menuBar()->addMenu("&Tools");
    menuBar()->addMenu("&Window");
    menuBar()->addMenu("&Help");
}

void MainWindow::createToolbar()
{
    auto *toolbar = addToolBar("Main");

    toolbar->addAction("New");
    toolbar->addAction("Open");
    toolbar->addAction("Save");

    toolbar->addSeparator();

    toolbar->addAction("Play");
    toolbar->addAction("Stop");
    toolbar->addAction("Record");
}

void MainWindow::createCentralWidget()
{
    auto *workspace = new QTextEdit(this);

    workspace->setReadOnly(true);
    workspace->setText(
        "Welcome to JamStudio\n\n"
        "This is the beginning of the JamStudio workspace.");

    setCentralWidget(workspace);
}

void MainWindow::createDockWindows()
{
    auto *projectDock = new QDockWidget("Project Explorer", this);
    projectDock->setWidget(new QLabel("No project loaded."));
    addDockWidget(Qt::LeftDockWidgetArea, projectDock);

    auto *inspectorDock = new QDockWidget("Inspector", this);
    inspectorDock->setWidget(new QLabel("Nothing selected."));
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage("Ready");
}
