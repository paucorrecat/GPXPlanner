QT += core gui widgets xml charts

CONFIG += c++17
CONFIG -= app_bundle

TARGET = GPXPlanner

SOURCES += \
    main.cpp \
    MainWindow.cpp

HEADERS += \
    RiderProfile.h \
    TrackSegment.h \
    StopPoint.h \
    TimeEstimator.h \
    GPXParser.h \
    TrackPlanner.h \
    PlanSerializer.h \
    ElevationChartView.h \
    MainWindow.h \
    SettingsDialog.h \
    SrtmReader.h
