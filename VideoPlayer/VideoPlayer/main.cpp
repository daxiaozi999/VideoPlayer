#include "VideoPlayer.h"
#include <QtWidgets/QApplication>

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avfilter.lib")
#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "SDL3.lib")

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    VideoPlayer window;
    window.show();
    return app.exec();
}

// http://ottrrs.hl.chinamobile.com/PLTV/88888888/224/3221226016/index.m3u8