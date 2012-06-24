#ifndef SCRIPT_VM_H_
#define SCRIPT_VM_H_ 1

enum ScriptVMExpressionType
{
  ScriptVMExpressionU32,
  ScriptVMExpressionFloat,
  ScriptVMExpressionString,
  ScriptVMExpressionMat4x4,

  ScriptVMExpressionParen,
  ScriptVMExpressionIdentifier,
  ScriptVMExpressionStatement,
  ScriptVMExpressionAdd,
  ScriptVMExpressionSubtract,
  ScriptVMExpressionMultiply,
  ScriptVMExpressionDivide
};

#endif /* !SCRIPT_VM_H_ */
