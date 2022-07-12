#include "dstreamlight.h"

DLogs::DLogsContext DStreamLightNamespace::log_context;
DLOGS_INIT_GLOBAL_CONTEXT("DStreamLight", DStreamLightNamespace::logsInit);

void registerAll() {
    avdevice_register_all();
    avformat_network_init();
}
