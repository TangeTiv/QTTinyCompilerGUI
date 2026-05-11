/****************************************************/
/* 文件: cgen.cpp                                    */
/* TINY 编译器的代码生成器实现                       */
/* （生成 TM 虚拟机的汇编代码）                      */
/* 编译器构造：原理与实践                            */
/* Kenneth C. Louden                                 */
/* C++ 重写版 — 添加了详细的中文注释                 */
/****************************************************/

#include "globals.h"
#include "symtab.h"
#include "code.h"
#include "cgen.h"

/* tmpOffset : 临时变量的内存偏移量
 * 每次存储临时变量时递减，加载时递增。
 * 用于表达式求值过程中的临时结果存储。
 */
static int tmpOffset = 0;

/* 内部递归代码生成函数的前向声明 */
static void cGen(TreeNode* tree);

/* ================================================================
 * genStmt : 在语句节点上生成代码
 *
 * 根据语句的不同类型（IfK / RepeatK / AssignK / ReadK / WriteK）
 * 生成对应的 TM 汇编指令序列。
 *
 * 参数 tree: 语句节点
 * ================================================================ */
static void genStmt(TreeNode* tree)
{
    TreeNode* p1, * p2, * p3;
    int savedLoc1, savedLoc2, currentLoc;
    int loc;

    switch (tree->kind.stmt)
    {
        /* ---- IF 语句代码生成 ---- */
        case IfK:
            if (TraceCode) emitComment("-> if");

            p1 = tree->child[0];   // 条件表达式
            p2 = tree->child[1];   // THEN 分支
            p3 = tree->child[2];   // ELSE 分支（可能为 NULL）

            /* 生成条件表达式的代码 */
            cGen(p1);

            /* 预留条件跳转指令的位置（回填地址用）*/
            savedLoc1 = emitSkip(1);
            emitComment("if: 条件跳转（条件为假时跳转到 else）");

            /* 生成 THEN 分支的代码 */
            cGen(p2);

            /* 预留无条件跳转到 END 的位置 */
            savedLoc2 = emitSkip(1);
            emitComment("if: 跳转到 end");

            /* 回填条件跳转的地址：条件为假时跳转到此处（else 分支开始）*/
            currentLoc = emitSkip(0);
            emitBackup(savedLoc1);
            emitRM_Abs("JEQ", ac, currentLoc, "if: 条件为假，跳转到 else");
            emitRestore();

            /* 生成 ELSE 分支的代码 */
            cGen(p3);

            /* 回填无条件跳转的地址：THEN 分支结束后跳转到此处（end）*/
            currentLoc = emitSkip(0);
            emitBackup(savedLoc2);
            emitRM_Abs("LDA", pc, currentLoc, "跳转到 if 语句结束");
            emitRestore();

            if (TraceCode) emitComment("<- if");
            break;

        /* ---- REPEAT 语句代码生成 ---- */
        case RepeatK:
            if (TraceCode) emitComment("-> repeat");

            p1 = tree->child[0];   // 循环体
            p2 = tree->child[1];   // 循环条件

            /* 记录循环开始位置 */
            savedLoc1 = emitSkip(0);
            emitComment("repeat: 循环体开始");

            /* 生成循环体的代码 */
            cGen(p1);

            /* 生成循环条件的代码 */
            cGen(p2);

            /* 条件为真时跳回循环体开头 */
            emitRM_Abs("JEQ", ac, savedLoc1, "repeat: 条件为真，跳回循环体");

            if (TraceCode) emitComment("<- repeat");
            break;

        /* ---- 赋值语句代码生成 ---- */
        case AssignK:
            if (TraceCode) emitComment("-> assign");

            /* 生成右侧表达式代码（结果在 ac 中）*/
            cGen(tree->child[0]);

            /* 查找变量内存位置并存储累加器的值 */
            loc = st_lookup(tree->attr.name);
            emitRM("ST", ac, loc, gp, "assign: 存储值");

            if (TraceCode) emitComment("<- assign");
            break;

        /* ---- READ 语句代码生成 ---- */
        case ReadK:
            /* IN 指令从输入读取一个整数到累加器 */
            emitRO("IN", ac, 0, 0, "读入一个整数值");

            /* 将读入的值存储到变量所在位置 */
            loc = st_lookup(tree->attr.name);
            emitRM("ST", ac, loc, gp, "read: 存储值");
            break;

        /* ---- WRITE 语句代码生成 ---- */
        case WriteK:
            /* 生成要输出的表达式的代码（结果在 ac 中）*/
            cGen(tree->child[0]);

            /* OUT 指令输出累加器中的值 */
            emitRO("OUT", ac, 0, 0, "输出 ac");
            break;

        default:
            break;
    }
}

/* ================================================================
 * genExp : 在表达式节点上生成代码
 *
 * 根据表达式的不同类型（ConstK / IdK / OpK）
 * 生成对应的 TM 汇编指令序列。
 * 所有表达式的结果都放在累加器 ac 中。
 *
 * 参数 tree: 表达式节点
 * ================================================================ */
static void genExp(TreeNode* tree)
{
    int loc;
    TreeNode* p1, * p2;

    switch (tree->kind.exp)
    {
        /* ---- 常量表达式：加载常量值 ---- */
        case ConstK:
            if (TraceCode) emitComment("-> Const");

            /* LDC 指令将常量值加载到累加器 */
            emitRM("LDC", ac, tree->attr.val, 0, "加载常量");

            if (TraceCode) emitComment("<- Const");
            break;

        /* ---- 标识符表达式：加载变量值 ---- */
        case IdK:
            if (TraceCode) emitComment("-> Id");

            /* 查找变量内存位置，LD 指令加载到累加器 */
            loc = st_lookup(tree->attr.name);
            emitRM("LD", ac, loc, gp, "加载标识符的值");

            if (TraceCode) emitComment("<- Id");
            break;

        /* ---- 运算符表达式 ---- */
        case OpK:
            if (TraceCode) emitComment("-> Op");

            p1 = tree->child[0];   // 左操作数
            p2 = tree->child[1];   // 右操作数

            /* 生成左操作数代码（结果在 ac 中）*/
            cGen(p1);

            /* 将左操作数压栈（保存到临时区）*/
            emitRM("ST", ac, tmpOffset--, mp, "op: 左操作数压栈");

            /* 生成右操作数代码（结果在 ac 中）*/
            cGen(p2);

            /* 从栈中恢复左操作数到 ac1 */
            emitRM("LD", ac1, ++tmpOffset, mp, "op: 左操作数出栈到 ac1");

            /* 根据运算符生成对应的运算指令 */
            switch (tree->attr.op)
            {
                case PLUS:
                    emitRO("ADD", ac, ac1, ac, "op +");    // ac1 + ac → ac
                    break;
                case MINUS:
                    emitRO("SUB", ac, ac1, ac, "op -");    // ac1 - ac → ac
                    break;
                case TIMES:
                    emitRO("MUL", ac, ac1, ac, "op *");    // ac1 * ac → ac
                    break;
                case OVER:
                    emitRO("DIV", ac, ac1, ac, "op /");    // ac1 / ac → ac
                    break;
                case LT:
                    /* 比较运算：小于 */
                    emitRO("SUB", ac, ac1, ac, "op <");    // ac1 - ac
                    emitRM("JLT", ac, 2, pc, "如果为真则跳转");
                    emitRM("LDC", ac, 0, ac, "结果为假（0）");
                    emitRM("LDA", pc, 1, pc, "无条件跳转");
                    emitRM("LDC", ac, 1, ac, "结果为真（1）");
                    break;
                case EQ:
                    /* 比较运算：等于 */
                    emitRO("SUB", ac, ac1, ac, "op ==");   // ac1 - ac
                    emitRM("JEQ", ac, 2, pc, "如果为真则跳转");
                    emitRM("LDC", ac, 0, ac, "结果为假（0）");
                    emitRM("LDA", pc, 1, pc, "无条件跳转");
                    emitRM("LDC", ac, 1, ac, "结果为真（1）");
                    break;
                default:
                    emitComment("BUG: 未知运算符");
                    break;
            }

            if (TraceCode) emitComment("<- Op");
            break;

        default:
            break;
    }
}

/* ================================================================
 * cGen : 递归代码生成函数
 *
 * 遍历语法树，对每个节点根据其种类（StmtK 或 ExpK）
 * 调用对应的代码生成函数，然后递归处理兄弟节点。
 *
 * 参数 tree: 当前要生成代码的语法树节点
 * ================================================================ */
static void cGen(TreeNode* tree)
{
    if (tree != NULL)
    {
        switch (tree->nodekind)
        {
            case StmtK:
                genStmt(tree);
                break;
            case ExpK:
                genExp(tree);
                break;
            default:
                break;
        }
        cGen(tree->sibling);  // 递归处理兄弟节点
    }
}

/**********************************************/
/*     代码生成器的主函数                      */
/**********************************************/

/* ================================================================
 * codeGen : 生成完整的 TM 代码
 *
 * 代码生成流程：
 *   1. 写入文件信息注释
 *   2. 生成标准前导代码（初始化内存指针）
 *   3. 遍历语法树生成程序主体代码
 *   4. 生成 HALT 指令结束程序
 *
 * 标准前导代码：
 *   LD mp, 0(ac)    -- 从位置 0 加载最大地址到 mp
 *   ST ac, 0(ac)    -- 清除位置 0
 *
 * 参数 syntaxTree: 语法树的根节点
 * 参数 codefile  : 代码文件名（用于注释）
 * ================================================================ */
void codeGen(TreeNode* syntaxTree, const char* codefile)
{
    // 构造文件注释字符串
    std::string s = "文件: ";
    s += codefile;

    emitComment("TINY 编译到 TM 代码");
    emitComment(s.c_str());

    /* 生成标准前导代码 */
    emitComment("标准前导代码：");
    emitRM("LD", mp, 0, ac, "从位置 0 加载最大地址到 mp");
    emitRM("ST", ac, 0, ac, "清除位置 0");
    emitComment("标准前导代码结束。");

    /* 生成 TINY 程序主体的代码 */
    cGen(syntaxTree);

    /* 程序结束 */
    emitComment("程序执行结束。");
    emitRO("HALT", 0, 0, 0, "");
}
