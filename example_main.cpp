#include "CsvLogger.h"

#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    using namespace csvlog;

    CsvLoggerConfig config;
    config.filePath = "system_status.csv";
    config.headerText = "device=LECU01\nbuild=2026.04";
    config.intervalSec = 1;
    config.append = true;

    CsvAtomicIntItem speed("speed", 0);
    CsvAtomicIntItem temperature("temperature", 40);
    CsvStringItem mode("mode");
    mode.setValue("INIT");

    CsvLogger logger(config);
    logger.addItem(&speed, CsvItemKind::Auto);
    logger.addItem(&temperature, CsvItemKind::Auto);
    logger.addItem(&mode, CsvItemKind::Auto);

    CsvStringItem operatorNote("operator_note");
    logger.addItem(&operatorNote, CsvItemKind::Manual);

    if (!logger.start())
    {
        std::cerr << "failed to start logger\n";
        return 1;
    }

    //logger.pause();

    for (int i = 0; i < 5; ++i)
    {
        speed.setValue(i * 10);
        temperature.setValue(40 + i);
        mode.setValue((i < 2) ? "INIT" : "RUN");

        if (0 == (i%2))
        {
            operatorNote.setValue("xxxx");
            logger.write();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(700));
    }

    logger.stop();
    return 0;
}
