#-------------------------------------------------
#
# Project created by QtCreator 2025-06-11T16:26:16
#
#-------------------------------------------------

QT       += core gui network xml widgets
QT       += concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

#------------------------------Load OpenCv Libs--------------------------------------------------------
QT_CONFIG -= no-pkg-config
CONFIG += c++11 c++14
CONFIG += link_pkgconfig c++17
message($$PWD)

OPENCVDIR = /usr/local
OPENCVCUDADIR = /usr/local/cuda
DETECTED_OPENCVDIR = $$OPENCVDIR
win32 {
INCLUDEPATH += C:\OpenCV\qt_opencv_built\install\include
LIBS += C:\OpenCV\qt_opencv_built\install\x64\mingw\bin\libopencv_core440.dll
LIBS += C:\OpenCV\qt_opencv_built\install\x64\mingw\bin\libopencv_highgui440.dll
LIBS += C:\OpenCV\qt_opencv_built\install\x64\mingw\bin\libopencv_imgcodecs440.dll
LIBS += C:\OpenCV\qt_opencv_built\install\x64\mingw\bin\libopencv_imgproc440.dll
LIBS += C:\OpenCV\qt_opencv_built\install\x64\mingw\bin\libopencv_calib3d440.dll
LIBS += C:\OpenCV\qt_opencv_built\install\x64\mingw\bin\libopencv_video440.dll
LIBS += C:\OpenCV\qt_opencv_built\install\x64\mingw\bin\libopencv_videoio440.dll
}
else:unix{
message(Building for Linux - $$_DATE_)
LIBS += -lstdc++fs
exists(/usr/include/libzbar ) {
    LIBS += -L/usr/include/ -lzbar
}
exists($$OPENCVDIR/lib/libopencv* ) {
    message( "OpenCV" )
    DETECTED_OPENCVDIR = $$OPENCVDIR
}
else:exists($$OPENCVCUDADIR/lib/libopencv* ) {
    message( "CUDA/OpenCV" )
    DETECTED_OPENCVDIR = $$OPENCVCUDADIR
}
else {
    message( "OpenCV does not exist" )
}

LIBS += -L$$DETECTED_OPENCVDIR/lib/ -lopencv_core -lopencv_highgui -lopencv_imgcodecs -lopencv_imgproc -lopencv_calib3d -lopencv_video -lopencv_videoio \
                          -lopencv_cudacodec -lopencv_cudev -lopencv_cudaarithm -lopencv_cudabgsegm -lopencv_cudaimgproc -lopencv_cudafilters -lopencv_quality
INCLUDEPATH += $$DETECTED_OPENCVDIR/include
DEPENDPATH += $$DETECTED_OPENCVDIR/include
message( DETECTED_OPENCVDIR )
message( $$DETECTED_OPENCVDIR )
message( $$LIBS )

exists($$PWD/cub-1.7.5 ) {
    # cub version 1.7.5 for CUDA Toolkit 10.1
    message( "cub-1.7.5 for CUDA Toolkit 10.1" )
    INCLUDEPATH += $$PWD/cub-1.7.5 #/home/contrel/AlignmentSys_code/cub-1.7.5
    DEPENDPATH += $$PWD/cub-1.7.5 # /home/contrel/AlignmentSys_code/cub-1.7.5
}
else:exists($$PWD/cub-1.17.2 ) {
    message( "cub-1.17.2 for CUDA Toolkit 11.6" )
    #  cub version 1.17.2 for CUDA Toolkit 11.6
    message( "The version of CUB in your include path is not compatible with this release of Thrust." )
    INCLUDEPATH += $$PWD/cub-1.17.2
    DEPENDPATH += $$PWD/cub-1.17.2
}
else{
    message("cub libraries not found")
}

##INCLUDEPATH += $$PWD/cub-1.7.5 #/home/contrel/AlignmentSys_code/cub-1.7.5
##DEPENDPATH += $$PWD/cub-1.7.5 # /home/contrel/AlignmentSys_code/cub-1.7.5
}
#--------------------------end OpenCV------------------------------

TARGET = ChamferMatching_UI2
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
QMAKE_CXXFLAGS += -fopenmp -fopenacc
QMAKE_CFLAGS += -O3 -Ofast
QMAKE_LFLAGS += -fopenmp -lgomp
PRECOMPILED_HEADER = pch.h

SOURCES += \
        main.cpp \
        matchingui.cpp \
        custompicturebox.cpp \
    Camera/capturethreadl.cpp \
    Camera/Calibration.cpp\
    Chamfer/matchingcontainer.cpp\
    Chamfer/chamfermatch.cpp \
    globalparam.cpp \
    utils.cpp \
    FileIO/fileio.cpp \
    FileIO/inimanager.cpp \
    Log/logwriter.cpp \
    Log/timelog.cpp \
    ImageProcessing/imageprocessing.cpp\
    plcconnector.cpp \
    FileIO/fileiomanager.cpp \
    Chamfer/fastrst.cpp \
    PLCComm/PoseTransformer.cpp\
    Camera/Alignment.cpp \
    custompicturepanel.cpp \
    Utils/initsetup.cpp


HEADERS += \
        matchingui.h \
        custompicturebox.h \
    Camera/capturethreadl.h \
    Camera/Calibration.h\
    Chamfer/matchingcontainer.h\
    Chamfer/chamfermatch.h \
    globalparams.h \
    FileIO/fileio.h \
    FileIO/inimanager.h \
    Log/logwriter.h \
    Log/timelog.h \
    ImageProcessing/imageparams.h \
    ImageProcessing/imageprocessing.h \
    Chamfer/abstractmatch.h \
    globalparms.h \
    plcconnector.h \
    FileIO/fileiomanager.h \
    Chamfer/fastrst.h \
    PLCComm/PoseTransformer.h\
    Camera/Alignment.h \
	robotposetrans.h \
    FileIO/json.hpp \
    custompicturepanel.h \
    Utils/initsetup.h


FORMS += \
        matchingui.ui

#==========================Pylon==================================================
QMAKE_LFLAGS +=$$system(/opt/pylon/bin/pylon-config --libs-rpath)

LIBS +=$$system(/opt/pylon/bin/pylon-config --libs-only-l)
LIBS +=$$system(/opt/pylon/bin/pylon-config --libs-only-L)
LIBS +=-Xlinker -E

LIBS += -lstdc++fs

#LIBS += -Xcompiler -fopenmp

INCLUDEPATH += /opt/pylon/include
INCLUDEPATH += /opt/pylon/include/GenApi


#=================================================================================

#======================CUDA MATCHING ==========================

win32 {
    CUDA_DIR = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.6"

    SYSTEM_NAME = windows11
    SYSTEM_TYPE = 64
    CUDA_ARCH = sm_86       #62
    NVCC_OPTIONS = --compiler-options -fno-strict-aliasing --use_fast_math --ptxas-options=-v

    INCLUDEPATH += $$CUDA_DIR/include
    QMAKE_LIBDIR += $$CUDA_DIR/lib

    CUDA_OBJECTS_DIR = ./
    CUDA_LIBS += $$CUDA_DIR\bin\cuda*.dll
    CUDA_LIBS += $$CUDA_DIR\bin\cublas*.dll
    message($$CUDA_LIBS)

    CUDA_INC = $$join(INCLUDEPATH,'" -I"','-I"','"')

    LIBS += $$CUDA_LIBS

    QMAKE_LFLAGS += -Wl,-rpath,$$CUDA_DIR/lib
        cuda.input = CUDA_SOURCES
        cuda.output = ${QMAKE_FILE_BASE}_cuda.o
        cuda.commands = $$CUDA_DIR/bin/nvcc $$NVCC_OPTIONS $$CUDA_INC $$NVCC_LIBS $$LIBS\
                 --machine $$SYSTEM_TYPE -arch=$$CUDA_ARCH \
                --compile -cudart static -DWIN64 -D_MBCS -Xcompiler \#"Wl,-E\"\#-rpath,libs/x86_64\" \
                -c -O3 -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_NAME}
        cuda.dependency_type = TYPE_C
        cuda.depend_commands = $$CUDA_DIR/bin/ncc -M $$CUDA_INC $$NVCC_OPTIONS ${QMAKE_FILE_NAME}
        QMAKE_EXTRA_COMPILERS += cuda
}
unix {
    #=================================================================================
    #QMAKE_EXTRA_COMPILERS += cuda

    CUDA_SDK = /usr/local/cuda/
    CUDA_DIR = /usr/local/cuda/

    SYSTEM_NAME = ubuntu
    SYSTEM_TYPE = 64
    CUDA_ARCH = sm_72       #62
    NVCC_OPTIONS = --compiler-options -fno-strict-aliasing --use_fast_math --ptxas-options=-v

    INCLUDEPATH += $$CUDA_DIR/include
    QMAKE_LIBDIR += $$CUDA_DIR/lib64

    CUDA_OBJECTS_DIR = ./
    CUDA_LIBS = -L $$CUDA_DIR/lib -lcuda -lcudart -lm -lcublas  #-fopenacc
    CUDA_INC = $$join(INCLUDEPATH,'" -I"','-I"','"')

    LIBS += $$CUDA_LIBS

    QMAKE_LFLAGS += -Wl,-rpath,$$CUDA_DIR/lib
        cuda.input = CUDA_SOURCES
        cuda.output = ${QMAKE_FILE_BASE}_cuda.o
        cuda.commands = $$CUDA_DIR/bin/nvcc $$NVCC_OPTIONS $$CUDA_INC $$NVCC_LIBS $$LIBS\
                 --machine $$SYSTEM_TYPE -arch=$$CUDA_ARCH \
                --compile -cudart static -DWIN64 -D_MBCS -Xcompiler \#"Wl,-E\"\#-rpath,libs/x86_64\" \
                -c -O3 -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_NAME}
        cuda.dependency_type = TYPE_C
        cuda.depend_commands = $$CUDA_DIR/bin/ncc -M $$CUDA_INC $$NVCC_OPTIONS ${QMAKE_FILE_NAME}
        QMAKE_EXTRA_COMPILERS += cuda
}

CUDA_SOURCES += \
    Chamfer/chamfermatch.cu \
    Chamfer/controlcudamemory.cu

DISTFILES += \
    CameraCalibration.json \
	T_base_cam.xml

win32 {
    for(FILE, DISTFILES) {
        win_copy = $$quote(cmd /c copy /Y $$PWD\\$$FILE $$OUT_PWD\\$$FILE)
        QMAKE_POST_LINK += $$win_copy $$escape_expand(\n\t)
    }
}
unix {
    for(FILE, DISTFILES) {
        cpmp = $$quote(cp $$PWD/$$FILE $$OUT_PWD/$$FILE)
        QMAKE_POST_LINK += $$cpmp $$escape_expand(\n\t)
    }
}
