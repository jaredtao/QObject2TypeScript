#pragma once
#include <QList>

#include <QString>
namespace Meta {
    struct Arg {
        QString type;
        QString name;
    };
    struct Property : Arg {
        QString notify;
        QString get;
        QString set;
        bool readonly = false;
    };

    struct Method {
        bool isNotifyMethod = false;
        QString name;
        QString returnType;
        QList<Arg> args;
    };
    struct MetaObj {
        QString name;
        QList<Property> properties;
        QList<Method> methods;
    };
} // namespace Meta

class Worker {
public:
    void work(const QString &inputFile, const QString &outputFile);

private:
    void parseLine(const QString &line);
    void parseProperty(const QString &line);
    void parseFunction(bool isInvok, const QString &type, const QString &name, const QString &args);

    void parseArgs(QList<Meta::Arg> &args, const QString &argsStr);
    void writeTs(const QString &outputFile);

private:
    Meta::MetaObj m_obj;
};
