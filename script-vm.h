#ifndef SCRIPT_VM_H_
#define SCRIPT_VM_H_ 1

enum ScriptVMExpressionType
{
  ScriptVMExpressionU32,
  ScriptVMExpressionFloat,
  ScriptVMExpressionString,
  ScriptVMExpressionIdentifier,
  ScriptVMExpressionStatement,
  ScriptVMExpressionAdd
};
#endif /* !SCRIPT_VM_H_ */
