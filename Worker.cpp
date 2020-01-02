#include "Worker.h"
#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QTextStream>
//枚举转数字
template <typename NumberType, typename EnumType>
static inline constexpr NumberType e2n(EnumType e)
{
    return static_cast<NumberType>(e);
}
//数字转枚举
template <typename EnumType, typename NumberType>
static inline constexpr EnumType n2e(NumberType n)
{
    return static_cast<EnumType>(n);
}
//枚举转int
template <typename EnumType>
static inline constexpr int e2i(EnumType e)
{
    return e2n<int>(e);
}
//int转枚举
template <typename EnumType>
static inline constexpr EnumType i2e(int i)
{
    return n2e(i);
}
static const QString inttegerType = "char|short|int|long|uchar|ushort|uint|ulong|int8_t|int16_t|int32_t|uint8_t|uin16_t|uint32_t";
static const QString supportTypes = QString("void|bool|%1|float|double|qreal|QString").arg(inttegerType);
static const QString nameFeature = QString("[a-zA-Z_]+[0-9a-zA-Z_]*");
static QString type2js(const QString &type){
    if (type == "bool") {
        return "boolean";
    } else if (type == "QString") {
        return "string";
    } else if (inttegerType.contains(type)) {
        return "number";
    } else if (type == "float" || type == "double" || type == "qreal ") {
        return "number";
    }
    return "string";
}
namespace Reg {
    enum class RegIndex : uint32_t {
        classNameIndex = 0,
        propertyIndex,
        privateSectionIndex,
        publicSectionIndex,
        protectedSectionIndex,
        signalsSectionIndex,
        slotsSectionIndex,
        functionIndex,

        IndexCount,
    };

    static const QRegularExpression regs[] = {
        QRegularExpression(QString(R"(\bclass\b \b(%1)\b)").arg(nameFeature)),
        QRegularExpression(R"(\bQ_PROPERTY\()"),
        QRegularExpression(R"(\bprivate\b:)"),
        QRegularExpression(R"(\bpublic\b:)"),
        QRegularExpression(R"(\bprotected\b:)"),
        QRegularExpression(R"(\b(signals|Q_SIGNALS)\b:)"),
        QRegularExpression(R"(\b(public slots|Q_SLOTS)\b:)"),
        QRegularExpression(QString(R"(\b(Q_INVOKABLE)?\b\b(const)?\s?&?&?\s?(%1)\s?&?&?\b(%2)\s?(const)?(\([^\)]*\)))").arg(supportTypes).arg(nameFeature)),
    };
    static const QRegularExpression typeNameReg(QString(R"(\b(%1) (%2)\b)").arg(supportTypes).arg(nameFeature));
    static const QRegularExpression typeNameRefReg(QString(R"(\b(const)?\s?&?&?\s?(%1)\s?&?&?(%2)\b)").arg(supportTypes).arg(nameFeature));
    static const QRegularExpression readPropertyReg(QString(R"(\bREAD (%1)\b)").arg(nameFeature));
    static const QRegularExpression writePropertyReg(QString(R"(\bWRITE (%1)\b)").arg(nameFeature));
    static const QRegularExpression signalPropertyReg(QString(R"(\bNOTIFY (%1)\b)").arg(nameFeature));
    static const QRegularExpression readonlyPropertyReg(QString(R"(\bCONSTANT\b)"));
} // namespace Reg
namespace Parser {
    enum class SectionType : uint8_t {
        s_private = 0,
        s_public,
        s_protected,
        s_signals,
        s_publicSlots,
    };
    struct Context {
        SectionType sec = SectionType::s_private;
    };
    static Context ctx;
} // namespace Parser

void Worker::work(const QString &inputFile, const QString &outputFile)
{
    QFile file(inputFile);
    if (!file.open(QFile::ReadOnly)) {
        qWarning() << inputFile << file.errorString();
        return;
    }
    QTextStream ts(&file);
    while (!ts.atEnd()) {
        parseLine(ts.readLine());
    }
    file.close();
    writeTs(outputFile);
}

void Worker::parseLine(const QString &line)
{
    for (uint32_t i = 0; i < e2n<uint32_t>(Reg::RegIndex::IndexCount); i++) {
        auto match = Reg::regs[i].match(line);
        if (match.hasMatch()) {
            switch (n2e<Reg::RegIndex>(i)) {
                case Reg::RegIndex::classNameIndex:
                    m_obj.name = match.captured(1);
                    qWarning() << "class " << m_obj.name;
                    break;
                case Reg::RegIndex::propertyIndex:
                    parseProperty(line);
                    break;
                case Reg::RegIndex::privateSectionIndex:
                    Parser::ctx.sec = Parser::SectionType::s_private;
                    break;
                case Reg::RegIndex::publicSectionIndex:
                    Parser::ctx.sec = Parser::SectionType::s_public;
                    break;
                case Reg::RegIndex::protectedSectionIndex:
                    Parser::ctx.sec = Parser::SectionType::s_protected;
                    break;
                case Reg::RegIndex::signalsSectionIndex:
                    Parser::ctx.sec = Parser::SectionType::s_signals;
                    break;
                case Reg::RegIndex::slotsSectionIndex:
                    Parser::ctx.sec = Parser::SectionType::s_publicSlots;
                    break;
                case Reg::RegIndex::functionIndex:
                    parseFunction(!match.captured(1).isEmpty(), match.captured(3), match.captured(4), match.captured(6));
                    break;
                default:
                    break;
            }
        }
    }
}

void Worker::parseProperty(const QString &line)
{
    static const QString propFeature = "Q_PROPERTY(";
    auto sline = line.simplified();
    auto start = sline.indexOf(propFeature) + propFeature.length();
    sline = sline.mid(start, sline.length() - 2);
    auto propMatch = Reg::typeNameReg.match(sline);
    if (!propMatch.hasMatch()) {
        return;
    }
    Meta::Property prop;
    prop.type = propMatch.captured(1);
    prop.name = propMatch.captured(2);
    auto readmatch = Reg::readPropertyReg.match(sline);
    if (readmatch.hasMatch()) {
        prop.get = readmatch.captured(1);
    }
    auto writeMatch = Reg::writePropertyReg.match(sline);
    if (writeMatch.hasMatch()) {
        prop.set = writeMatch.captured(1);
    }
    if (Reg::readonlyPropertyReg.match(sline).hasMatch()) {
        prop.readonly = true;
        if (!prop.set.isEmpty()) {
            prop.set.clear();
        }
    }
    auto notifyMatch = Reg::signalPropertyReg.match(sline);
    if (notifyMatch.hasMatch()) {
        prop.notify = notifyMatch.captured(1);
    }
    qWarning() << "prop:" << prop.type << prop.name << prop.get << prop.set << prop.notify << prop.readonly;
    m_obj.properties.append(prop);
}

void Worker::parseFunction(bool isInvok, const QString &type, const QString &name, const QString &args)
{
    qWarning() << "function:" << isInvok << type << name << args;
    for (auto prop : m_obj.properties) {
        if (prop.get == name || prop.set == name ||prop.notify == name) {
            return;
        }
    }
    if (Parser::ctx.sec == Parser::SectionType::s_publicSlots || (Parser::ctx.sec == Parser::SectionType::s_public && isInvok)) {
        Meta::Method method;
        method.returnType = type;
        method.name = name;
        parseArgs(method.args, args.mid(1, args.length() - 2));
        m_obj.methods.push_back(method);
    } else if (Parser::ctx.sec == Parser::SectionType::s_signals) {
        Meta::Method method;
        method.returnType = type;
        method.name = name;
        method.isNotifyMethod = true;
        parseArgs(method.args, args.mid(1, args.length() - 2));
        m_obj.methods.push_back(method);
    }
}

void Worker::parseArgs(QList<Meta::Arg> &args, const QString &argsStr)
{
    qWarning() << argsStr;
    auto list = argsStr.split(',');
    for (auto i : list) {
        auto match = Reg::typeNameRefReg.match(i);
        if (match.hasMatch()) {
            Meta::Arg arg;
            arg.type = match.captured(2);
            arg.name = match.captured(3);
            qWarning() << "    arg" << arg.type << arg.name;
            args.push_back(arg);
        }
    }
}

void Worker::writeTs(const QString &outputFile) {
    const static QString newLine = "\n";
    QFile file(outputFile);
    if (!file.open(QFile::WriteOnly)) {
        qWarning() << file.errorString();
        return;
    }
    QTextStream os(&file);
    os << "class " << m_obj.name << "{"<< newLine;
    for (auto p : m_obj.properties) {
        os << QString("   %1private _%2: %3;").arg(p.readonly ? " readonly " : " ").arg(p.name).arg(type2js(p.type)) << newLine;
    }
    for (auto p : m_obj.properties) {
        if (!p.get.isEmpty()) {
            os << QString("    get %1() { return this._%1;}").arg(p.name) << newLine;
        }
        if (!p.set.isEmpty()) {
            os << QString ("    set %1(value: %2) { this._%1 = value;}").arg(p.name).arg(type2js(p.type)) << newLine;
        }
    }
    for (auto m: m_obj.methods) {
        QString args;
        for (auto arg : m.args) {
            if (!args.isEmpty()) {
                args.append(", ");
            }
            args.append(QString("%1: %2").arg(arg.name).arg(type2js(arg.type)));
        }
        os << QString("    public %1 (%2):%3 {}").arg(m.name).arg(args).arg(m.returnType) << newLine;
    }

    os << "}" << newLine;
    file.close();
}
