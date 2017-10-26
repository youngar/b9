#include <b9/hash.hpp>
#include <b9/interpreter.hpp>
#include <b9/jit.hpp>
#include <b9/loader.hpp>

#include "Jit.hpp"

#include <sys/time.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

namespace b9 {

////////////////////////////////////////
// Execution Context
////////////////////////////////////////

StackElement ExecutionContext::callFunction(std::size_t index) {
  if (cfg_.debug)
    std::cerr << "Interpreter: Calling function" << index << std::endl;

  auto jitFunction = virtualMachine_->getJitAddress(index);

  if (jitFunction) {
    return callJitFunction(index);
  } else {
    return interpretFunction(index);
  }
}

StackElement ExecutionContext::interpretFunction(std::size_t index) {
  if (cfg_.debug)
    std::cerr << "Interpreter: Interpreting function: " << index << std::endl;

  auto function = virtualMachine_->getFunction(index);

  const Instruction *instructionPointer = function->address;
  StackElement *args = stackPointer_ - function->nargs;
  stackPointer_ += function->nregs;

  while (*instructionPointer != END_SECTION) {
    switch (instructionPointer->byteCode()) {
      case ByteCode::FUNCTION_CALL: {
        auto result = callFunction(instructionPointer->parameter());
        push(result);
      } break;
      case ByteCode::FUNCTION_RETURN: {
        StackElement result = *(stackPointer_ - 1);
        stackPointer_ = args;
        return result;
        break;
      }
      case ByteCode::PRIMITIVE_CALL:
        primitiveCall(instructionPointer->parameter());
        break;
      case ByteCode::JMP:
        instructionPointer += instructionPointer->parameter();
        break;
      case ByteCode::DUPLICATE:
        // TODO
        break;
      case ByteCode::DROP:
        drop();
        break;
      case ByteCode::PUSH_FROM_VAR:
        pushFromVar(args, instructionPointer->parameter());
        break;
      case ByteCode::POP_INTO_VAR:
        // TODO bad name, push or pop?
        pushIntoVar(args, instructionPointer->parameter());
        break;
      case ByteCode::INT_ADD:
        intAdd();
        break;
      case ByteCode::INT_SUB:
        intSub();
        break;

      // CASCON2017 - Add INT_MUL and INT_DIV here

      case ByteCode::INT_PUSH_CONSTANT:
        intPushConstant(instructionPointer->parameter());
        break;
      case ByteCode::INT_JMP_EQ:
        instructionPointer += intJmpEq(instructionPointer->parameter());
        break;
      case ByteCode::INT_JMP_NEQ:
        instructionPointer += intJmpNeq(instructionPointer->parameter());
        break;
      case ByteCode::INT_JMP_GT:
        instructionPointer += intJmpGt(instructionPointer->parameter());
        break;
      case ByteCode::INT_JMP_GE:
        instructionPointer += intJmpGe(instructionPointer->parameter());
        break;
      case ByteCode::INT_JMP_LT:
        instructionPointer += intJmpLt(instructionPointer->parameter());
        break;
      case ByteCode::INT_JMP_LE:
        instructionPointer += intJmpLe(instructionPointer->parameter());
        break;
      case ByteCode::STR_PUSH_CONSTANT:
        strPushConstant(instructionPointer->parameter());
        break;
      case ByteCode::STR_JMP_EQ:
        // TODO
        break;
      case ByteCode::STR_JMP_NEQ:
        // TODO
        break;
      default:
        assert(false);
        break;
    }
    instructionPointer++;
    programCounter_++;
  }
  return *(stackPointer_ - 1);
}

StackElement ExecutionContext::callJitFunction(std::size_t index) {
  assert(cfg_.jit);

  if (cfg_.debug)
    std::cerr << "Interpreter: calling JitFunction: " << index << std::endl;

  if (cfg_.passParam) {
    if (cfg_.debug) std::cerr << "passing parameters directly" << std::endl;
    return callJitFunctionWithPassParam(index);
  } else {
    if (cfg_.debug) std::cerr << "passing parameters on the stack" << std::endl;
    auto jitFunction = virtualMachine_->getJitAddress(index);
    assert(jitFunction != nullptr);
    return jitFunction(this, index);
  }
}

StackElement ExecutionContext::callJitFunctionWithPassParam(std::size_t index) {
  // The caller has pushed all args onto the operand stack. However, the
  // jitFunction require's it's arguments to be passed in the C calling
  // convention. To do this, we pop all arguments off the stack, and return the
  // result to the interpreter.

  assert(cfg_.passParam);

  auto function = virtualMachine_->getFunction(index);
  auto jitFunction = virtualMachine_->getJitAddress(index);

  assert(jitFunction != nullptr);

  StackElement result = 0;

  switch (function->nargs) {
    case 0: {
      result = jitFunction();
    } break;
    case 1: {
      StackElement p1 = pop();
      result = jitFunction(p1);
    } break;
    case 2: {
      StackElement p2 = pop();
      StackElement p1 = pop();
      result = jitFunction(p1, p2);
    } break;
    case 3: {
      StackElement p3 = pop();
      StackElement p2 = pop();
      StackElement p1 = pop();
      result = jitFunction(p1, p2, p3);
    } break;
    case 4: {
      StackElement p4 = pop();
      StackElement p3 = pop();
      StackElement p2 = pop();
      StackElement p1 = pop();
      result = jitFunction(p1, p2, p3, p4);
    } break;
    case 5: {
      StackElement p5 = pop();
      StackElement p4 = pop();
      StackElement p3 = pop();
      StackElement p2 = pop();
      StackElement p1 = pop();
      result = jitFunction(p1, p2, p3, p4, p5);
    } break;
    case 6: {
      StackElement p6 = pop();
      StackElement p5 = pop();
      StackElement p4 = pop();
      StackElement p3 = pop();
      StackElement p2 = pop();
      StackElement p1 = pop();
      result = jitFunction(p1, p2, p3, p4, p5, p6);
    } break;
    case 7: {
      StackElement p7 = pop();
      StackElement p6 = pop();
      StackElement p5 = pop();
      StackElement p4 = pop();
      StackElement p3 = pop();
      StackElement p2 = pop();
      StackElement p1 = pop();
      result = jitFunction(p1, p2, p3, p4, p5, p6, p7);
    } break;
    default:
      throw std::runtime_error{
          "Interpreter to JIT transition failed: too many arguments"};
      break;
  }
  return result;
}

void ExecutionContext::push(StackElement value) { *(stackPointer_++) = value; }

StackElement ExecutionContext::pop() { return *(--stackPointer_); }

void ExecutionContext::functionReturn(StackElement returnVal) {
  // TODO
}

void ExecutionContext::primitiveCall(Parameter value) {
  PrimitiveFunction *primitive = virtualMachine_->getPrimitive(value);
  (*primitive)(this);
}

Parameter ExecutionContext::jmp(Parameter offset) { return offset; }

void ExecutionContext::duplicate() {
  // TODO
}

void ExecutionContext::drop() { pop(); }

void ExecutionContext::pushFromVar(StackElement *args, Parameter offset) {
  push(args[offset]);
}

void ExecutionContext::pushIntoVar(StackElement *args, Parameter offset) {
  args[offset] = pop();
}

void ExecutionContext::intAdd() {
  StackElement right = pop();
  StackElement left = pop();
  StackElement result = left + right;
  push(result);
}

void ExecutionContext::intSub() {
  StackElement right = pop();
  StackElement left = pop();
  StackElement result = left - right;
  push(result);
}

// CASCON2017 - Add intMul() and intDiv() here

void ExecutionContext::intPushConstant(Parameter value) { push(value); }

Parameter ExecutionContext::intJmpEq(Parameter delta) {
  StackElement right = pop();
  StackElement left = pop();
  if (left == right) {
    return delta;
  }
  return 0;
}

Parameter ExecutionContext::intJmpNeq(Parameter delta) {
  StackElement right = pop();
  StackElement left = pop();
  if (left != right) {
    return delta;
  }
  return 0;
}

Parameter ExecutionContext::intJmpGt(Parameter delta) {
  StackElement right = pop();
  StackElement left = pop();
  if (left > right) {
    return delta;
  }
  return 0;
}

Parameter ExecutionContext::intJmpGe(Parameter delta) {
  StackElement right = pop();
  StackElement left = pop();
  if (left >= right) {
    return delta;
  }
  return 0;
}

Parameter ExecutionContext::intJmpLt(Parameter delta) {
  StackElement right = pop();
  StackElement left = pop();
  if (left < right) {
    return delta;
  }
  return 0;
}

Parameter ExecutionContext::intJmpLe(Parameter delta) {
  StackElement right = pop();
  StackElement left = pop();
  if (left <= right) {
    return delta;
  }
  return 0;
}

void ExecutionContext::strPushConstant(Parameter value) {
  push((StackElement)virtualMachine_->getString(value));
}

/* void strJmpEq(Parameter delta);
  TODO
} */

/* void strJmpNeq(Parameter delta) {
  TODO
} */

// Context helpers and misc

void ExecutionContext::reset() {
  stackPointer_ = stack_;
  programCounter_ = 0;
}

// For primitive calls
void primitive_call(ExecutionContext *context, Parameter value) {
  context->primitiveCall(value);
}

StackElement jitToInterpreterCall(ExecutionContext *context,
                                          std::size_t value) {
                                            // std::cerr << __PRETTY_FUNCTION__;
  return context->callFunction(value);
}

////////////////////////////////////////
// Virtual Machine
////////////////////////////////////////

VirtualMachine::VirtualMachine(const Config &cfg)
    : cfg_{cfg}, executionContext_{this, cfg}, compiler_{nullptr} {
  if (cfg_.verbose) std::cout << "VM initializing..." << std::endl;

  if (cfg_.jit) {
    auto ok = initializeJit();
    if (!ok) {
      throw std::runtime_error{"Failed to init JIT"};
    }

    compiler_ = std::make_shared<Compiler>(this, cfg_);
  }
}

VirtualMachine::~VirtualMachine() noexcept {
  if (cfg_.jit) {
    shutdownJit();
  }
}

void VirtualMachine::load(std::shared_ptr<const Module> module) {
  module_ = module;
  compiledFunctions_.reserve(getFunctionCount());
}

JitFunction VirtualMachine::getJitAddress(std::size_t functionIndex) {
  if (functionIndex >= compiledFunctions_.size()) {
    return nullptr;
  }
  return compiledFunctions_[functionIndex];
}

void VirtualMachine::setJitAddress(std::size_t functionIndex,
                                   JitFunction value) {
  compiledFunctions_[functionIndex] = value;
}

PrimitiveFunction *VirtualMachine::getPrimitive(std::size_t index) {
  return module_->primitives[index];
}

const FunctionSpec *VirtualMachine::getFunction(std::size_t index) {
  return &module_->functions[index];
}

JitFunction VirtualMachine::generateCode(const std::size_t functionIndex) {
  return compiler_->generateCode(functionIndex);
}

const char *VirtualMachine::getString(int index) {
  return module_->strings[index];
}

std::size_t VirtualMachine::getFunctionCount() {
  return module_->functions.size();
}

void VirtualMachine::generateAllCode() {
  assert(cfg_.jit);
  auto functionIndex = 0;

  while (functionIndex < getFunctionCount()) {
    if (cfg_.debug)
      std::cout << "\nJitting function: " << getFunction(functionIndex)->name
                << std::endl;
    auto func = compiler_->generateCode(functionIndex);
    compiledFunctions_.push_back(func);
    ++functionIndex;
  }
}

StackElement VirtualMachine::run(const std::string &name,
                                 const std::vector<StackElement> &usrArgs) {
  return run(module_->findFunction(name), usrArgs);
}

StackElement VirtualMachine::run(const std::size_t functionIndex,
                                 const std::vector<StackElement> &usrArgs) {
  auto function = getFunction(functionIndex);
  auto argsCount = function->nargs;

  if (cfg_.verbose) {
    std::cout << "+++++++++++++++++++++++" << std::endl;
    std::cout << "Running function: " << function->name
              << " nargs: " << argsCount << std::endl;
  }

  if (argsCount != usrArgs.size()) {
    std::stringstream ss;
    ss << function->name << " - Got " << usrArgs.size()
       << " arguments, expected " << argsCount;
    std::string message = ss.str();
    throw BadFunctionCallException{message};
  }

  // push user defined arguments to send to the program
  for (std::size_t i = 0; i < argsCount; i++) {
    auto idx = argsCount - i - 1;
    auto arg = usrArgs[idx];
    executionContext_.push(arg);
  }

  StackElement result = executionContext_.callFunction(functionIndex);

  executionContext_.reset();

  return result;
}

}  // namespace b9
