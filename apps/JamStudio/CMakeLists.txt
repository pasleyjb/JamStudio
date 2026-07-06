find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(JamStudio
    main.cpp
    MainWindow.cpp
    MainWindow.h
)

target_link_libraries(JamStudio
    PRIVATE
    Qt6::Widgets
)
