#include "Worker.h"
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName("QObject2TS");
    QCoreApplication::setOrganizationName("jaredtao");
    QCoreApplication::setOrganizationDomain("https://jaredtao.github.io");
    QCoreApplication::setApplicationVersion("1.0.0");

    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("convert QObject to typescript");
    parser.addPositionalArgument("input", "input file path");
    parser.addPositionalArgument("output", "output file path");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.process(a);

    QString inputFile;
    QString outputFile;

    if (parser.positionalArguments().size() < 1) {
        parser.showHelp();
    }
    inputFile = parser.positionalArguments().at(0);
    if (parser.positionalArguments().size() > 1) {
        parser.positionalArguments().at(1);
    } else {
        QFileInfo info(inputFile);
        outputFile = info.absolutePath() + "/" + info.baseName() + ".ts";
    }

    Worker worker;
    worker.work(inputFile, outputFile);

    return 0;
}
