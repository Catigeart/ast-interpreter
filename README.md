# ast-interpreter
基于clang的c语言解释器。

支持语法：
- 类型：int, char, void, 对应类型的指针
- 操作符: *, -, +, <, >, ==, =, []
- 一系列的Stmt和Expr
- 解释器直接解析的内建函数：
  - GET(), PRINT(int a), MALLOC(int a), FREE()
