#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include "globals.h"
#include <iostream>



int lineno = 0;                  // 当前行号，从 0 开始
std::ifstream* source = nullptr; // 源代码输入文件流
std::ostream* listing = &std::cout;// 列表输出流（通常指向 std::cout）
std::ofstream* code = nullptr;   // TM 代码输出文件流

/* 跟踪调试标志（默认全部关闭） */
int EchoSource   = FALSE;   // 是否回显源代码
int TraceScan    = FALSE;   // 是否跟踪词法分析过程
int TraceParse   = FALSE;   // 是否打印语法树
int TraceAnalyze = FALSE;   // 是否跟踪语义分析过程
int TraceCode    = FALSE;   // 是否在代码中写入注释

int Error = FALSE;          // 错误标志，发生错误时阻止后续阶段
QStringList errorMessages;
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "TinyCompilerGUI_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    MainWindow w;
    w.show();
    return a.exec();
}
