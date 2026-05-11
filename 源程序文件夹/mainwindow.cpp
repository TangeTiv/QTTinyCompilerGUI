#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDir>
#include "parser.h"
#include <fstream>
#include "scanner.h"

extern std::ifstream* source; // 如果你的 globals.h 里改成了 std::ifstream，请告诉我，我给你换写法。这里假设还是 FILE*
extern int lineno;
extern int Error;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // 假设你在 Designer 中给 action 命名为 actionOpen，左侧代码框命名为 codeEditor
    connect(ui->actionOpen, &QAction::triggered, this, [=]() {
        QString fileName = QFileDialog::getOpenFileName(this, "打开 TINY 源代码", "", "TINY Files (*.tny);;All Files (*)");
        if (!fileName.isEmpty()) {
            QFile file(fileName);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                ui->codeEditor->setPlainText(QString::fromUtf8(file.readAll()));
                file.close();
            } else {
                QMessageBox::warning(this, "错误", "无法打开文件！");
            }
        }
    });

    // 保存文件的逻辑
    connect(ui->actionSave, &QAction::triggered, this, [=]() {
        QString fileName = QFileDialog::getSaveFileName(this, "保存 TINY 源代码", "", "TINY Files (*.tny);;All Files (*)");
        if (!fileName.isEmpty()) {
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << ui->codeEditor->toPlainText();
                file.close();
            }
        }
    });

    connect(ui->actionGen, &QAction::triggered, this, [=]() {
        // 1. 获取路径并保存临时文件
        QString fileName = QDir::currentPath() + "/tmp.tny";
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << ui->codeEditor->toPlainText();
            file.close();
        }

        // 2. 清理旧的文件流内存
        if (source != nullptr) {
            if (source->is_open()) source->close();
            delete source;
        }

        // 🌟 修复关键点：直接 new，绝对不要再调用 source->open()！
        source = new std::ifstream(fileName.toStdString());

        if (!source->is_open()) {
            QMessageBox::critical(this, "错误", "无法打开输入流进行语法分析！");
            return;
        }

        // 3. 编译前：重置所有状态
        lineno = 0;
        Error = FALSE;
        errorMessages.clear();
        ui->treeWidget->clear();
        ui->errorListWidget->clear();

        // 🌟 重置词法分析器，防止第二次点击报错
        resetScanner();

        // 4. 调用解析器入口
        TreeNode* root = parse();
        source->close();

        // 5. 编译后：渲染错误栏或语法树
        if (Error || !errorMessages.isEmpty()) {
            ui->treeWidget->clear();
            for (const QString& err : errorMessages) {
                QListWidgetItem *item = new QListWidgetItem(err);
                item->setForeground(Qt::red);
                ui->errorListWidget->addItem(item);
            }
        } else if (root != nullptr) {
            displayTree(root, ui->treeWidget->invisibleRootItem());
            ui->treeWidget->expandAll();
            QListWidgetItem *item = new QListWidgetItem("编译成功！语法树已生成。");
            item->setForeground(Qt::darkGreen);
            ui->errorListWidget->addItem(item);
        }
    });


}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::displayTree(TreeNode *t, QTreeWidgetItem *parentItem)
{
    if (t == nullptr) return;

    // 1. 创建当前 UI 节点，挂载到 parentItem 下
    QTreeWidgetItem *item = new QTreeWidgetItem(parentItem);
    QString text = "Unknown Node";

    // 2. 根据节点类型，设置要显示的文本 (匹配你实验三扩充的所有类型)
    if (t->nodekind == StmtK) {
        switch (t->kind.stmt) {
        case IfK:          text = "If"; break;
        case RepeatK:      text = "Repeat"; break;
        case AssignK:      text = QString("Assign to: %1").arg(t->attr.name); break;
        case ReadK:        text = QString("Read: %1").arg(t->attr.name); break;
        case WriteK:       text = "Write"; break;
        case WhileK:       text = "While"; break;
        case ForK:         text = "For"; break;
        case RegexAssignK: text = QString("Regex Assign to: %1").arg(t->attr.name); break;
        default:           text = "Unknown StmtNode"; break;
        }
    }
    else if (t->nodekind == ExpK) {
        switch (t->kind.exp) {
        case OpK:
            text = "Op: ";
            switch (t->attr.op) {
            case PLUS: text += "+"; break;
            case MINUS: text += "-"; break;
            case TIMES: text += "*"; break;
            case OVER: text += "/"; break;
            case MOD: text += "%"; break;
            case POWER: text += "^"; break;
            case LT: text += "<"; break;
            case LE: text += "<="; break;
            case GT: text += ">"; break;
            case GE: text += ">="; break;
            case EQ: text += "="; break;
            case NEQ: text += "<>"; break;
            case OR: text += "|"; break;
            case AND: text += "&"; break;
            case CLOSURE: text += "#"; break;
            case OPTIONAL: text += "?"; break;
            case INC: text += "++"; break;
            case DEC: text += "--"; break;
            default: text += "unknown_op"; break;
            }
            break;
        case ConstK:
            text = QString("Const: %1").arg(t->attr.val);
            break;
        case IdK:
            text = QString("Id: %1").arg(t->attr.name);
            break;

        default: text = "Unknown ExpNode"; break;
        }
    }

    // 设置节点的图标和文字（第 0 列）
    item->setText(0, text);

    // 3. 递归处理所有的孩子节点 (child)
    for (int i = 0; i < MAXCHILDREN; i++) {
        if (t->child[i] != nullptr) {
            // 重点：孩子节点必须作为子节点，挂载在当前 item 下面
            displayTree(t->child[i], item);
        }
    }

    // 4. 递归处理兄弟节点 (sibling)
    if (t->sibling != nullptr) {
        // 重点：兄弟节点与当前节点平级，所以挂载在 parentItem 下面！
        displayTree(t->sibling, parentItem);
    }
}
