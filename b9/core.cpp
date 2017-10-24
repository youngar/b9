#include <b9.hpp>

#include <b9/core.hpp>
#include <b9/hash.hpp>
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

void ExecutionContext::push(StackElement value) { *(stackPointer_++) = value; }

StackElement ExecutionContext::pop() { return *(--stackPointer_); }

void ExecutionContext::functionCall(Parameter value) {
  auto f = virtualMachine_->getFunction((std::size_t)value);
  auto result = interpret(value);
  push(result);
}

void ExecutionContext::primitiveCall(Parameter value) {
  PrimitiveFunction *primitive = virtualMachine_->getPrimitive(value);
  (*primitive)(this);
}

void ExecutionContext::pushFromVar(StackElement *args, Parameter offset) {
  push(args[offset]);
}

void ExecutionContext::pushIntoVar(StackElement *args, Parameter offset) {
  args[offset] = pop();
}

void ExecutionContext::drop() { pop(); }

void ExecutionContext::intPushConstant(Parameter value) { push(value); }

void ExecutionContext::strPushConstant(Parameter value) {
  push((StackElement)virtualMachine_->getString(value));
}

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

/// ExecutionContext

bool VirtualMachine::initialize() {
  if (cfg_.verbose) std::cout << "VM initializing...\n";

  if (cfg_.jit) {
    if (initializeJit()) {
      compiler_ = std::make_shared<Compiler>(this, cfg_);
      if (cfg_.verbose) std::cout << "JIT successfully initialized\n";
      return true;
    }

    if (cfg_.verbose) std::cout << "JIT failed to initialize\n";
    return false;
  }

  return true;
}

bool VirtualMachine::shutdown() {
  if (cfg_.verbose) std::cout << "VM shutting down...\n";

  if (cfg_.jit) {
    shutdownJit();
  }
  return true;
}

void VirtualMachine::load(std::shared_ptr<const Module> module) {
  module_ = module;
  compiledFunctions_.reserve(getFunctionCount());
}

/// ByteCode Interpreter

StackElement interpret_0(ExecutionContext *context, const std::size_t functionIndex) {
  return context->interpret(functionIndex);
}
StackElement interpret_1(ExecutionContext *context, const std::size_t functionIndex,
                         StackElement p1) {
  context->push(p1);
  return context->interpret(functionIndex);
}
StackElement interpret_2(ExecutionContext *context, const std::size_t functionIndex,
                         StackElement p1, StackElement p2) {
  context->push(p1);
  context->push(p2);
  return context->interpret(functionIndex);
}
StackElement interpret_3(ExecutionContext *context, const std::size_t functionIndex,
                         StackElement p1, StackElement p2, StackElement p3) {
  context->push(p1);
  context->push(p2);
  context->push(p3);
  return context->interpret(functionIndex);
}

// For primitive calls
void primitive_call(ExecutionContext *context, Parameter value) {
  context->primitiveCall(value);
}

StackElement ExecutionContext::interpret(const std::size_t functionIndex) {
  auto function = virtualMachine_->getFunction(functionIndex);
  auto argsCount = function->nargs;
  auto address = virtualMachine_->getJitAddress(functionIndex);

  if (address) {
    if (cfg_.debug)
      std::cout << "Calling " << function->name.c_str() 
        << " with JIT address at " << address << std::endl;
    StackElement result = 0;
    if (cfg_.passParam) {
      switch (argsCount) {
        case 0: {
          if (cfg_.debug)
            std::cout << "calling with no args\n";
          JIT_0_args jitedcode = (JIT_0_args)address;
          result = (*jitedcode)();
        } break;
        case 1: {
          if (cfg_.debug)
            std::cout << "calling with one args\n";
          JIT_1_args jitedcode = (JIT_1_args)address;
          StackElement p1 = pop();
          result = (*jitedcode)(p1);
        } break;
        case 2: {
          if (cfg_.debug)
            std::cout << "calling with two args\n";
          JIT_2_args jitedcode = (JIT_2_args)address;
          StackElement p2 = pop();
          StackElement p1 = pop();
          result = (*jitedcode)(p1, p2);
        } break;
        case 3: {
          if (cfg_.debug)
            std::cout << "calling with three args\n";
          JIT_3_args jitedcode = (JIT_3_args)address;
          StackElement p3 = pop();
          StackElement p2 = pop();
          StackElement p1 = pop();
          result = (*jitedcode)(p1, p2, p3);
        } break;
        default:
          printf("Need to add handlers for more parameters\n");
          break;
      }
    } else {
      // Call the Jit'ed function, passing the parameters on the
      // ExecutionContext stack.
      if (cfg_.debug)
        std::cout << "parameters are on the stack\n";
      Interpret jitedcode = (Interpret)address;
      result = (*jitedcode)(this, functionIndex);
    }
    return result;
  }

  // interpret the method otherwise
  const Instruction *instructionPointer = function->address;
  StackElement *args = stackPointer_ - function->nargs;
  stackPointer_ += function->nregs;

  while (*instructionPointer != NO_MORE_BYTECODES) {
    // b9PrintStack(context);
    // std::cerr << "instruction call " << std::hex << (int)
    // ByteCodes::toByte(Instructions::getByteCode(*instructionPointer)) <<
    // std::endl;
    switch (Instructions::getByteCode(*instructionPointer)) {
      case ByteCode::INT_PUSH_CONSTANT:
        intPushConstant(Instructions::getParameter(*instructionPointer));
        break;
      case ByteCode::STR_PUSH_CONSTANT:
        strPushConstant(Instructions::getParameter(*instructionPointer));
        break;
      case ByteCode::DROP:
        drop();
        break;
      case ByteCode::INT_ADD:
        intAdd();
        break;
      case ByteCode::INT_SUB:
        intSub();
        break;
      case ByteCode::JMP:
        instructionPointer += Instructions::getParameter(*instructionPointer);
        break;
      case ByteCode::INT_JMP_EQ:
        instructionPointer +=
            intJmpEq(Instructions::getParameter(*instructionPointer));
        break;
      case ByteCode::INT_JMP_NEQ:
        instructionPointer +=
            intJmpNeq(Instructions::getParameter(*instructionPointer));
        break;
      case ByteCode::INT_JMP_GT:
        instructionPointer +=
            intJmpGt(Instructions::getParameter(*instructionPointer));
        break;
      case ByteCode::INT_JMP_GE:
        instructionPointer +=
            intJmpGe(Instructions::getParameter(*instructionPointer));
        break;
      case ByteCode::INT_JMP_LT:
        instructionPointer +=
            intJmpLt(Instructions::getParameter(*instructionPointer));
        break;
      case ByteCode::INT_JMP_LE:
        instructionPointer +=
            intJmpLe(Instructions::getParameter(*instructionPointer));
        break;
      case ByteCode::FUNCTION_CALL:
        functionCall(Instructions::getParameter(*instructionPointer));
        break;
      case ByteCode::PUSH_FROM_VAR:
        pushFromVar(args, Instructions::getParameter(*instructionPointer));
        break;
      case ByteCode::POP_INTO_VAR:
        // TODO bad name, push or pop?
        pushIntoVar(args, Instructions::getParameter(*instructionPointer));
        break;
      case ByteCode::FUNCTION_RETURN: {
        StackElement result = *(stackPointer_ - 1);
        stackPointer_ = args;
        return result;
        break;
      }
      case ByteCode::PRIMITIVE_CALL:
        primitiveCall(Instructions::getParameter(*instructionPointer));
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

void *VirtualMachine::getJitAddress(std::size_t functionIndex) {
  if (functionIndex >= compiledFunctions_.size()) {
    return nullptr;
  }
  return compiledFunctions_[functionIndex];
}

void VirtualMachine::setJitAddress(std::size_t functionIndex, void *value) {
  compiledFunctions_[functionIndex] = value;
}

PrimitiveFunction *VirtualMachine::getPrimitive(std::size_t index) {
  return module_->primitives[index];
}

const FunctionSpec *VirtualMachine::getFunction(std::size_t index) {
  return &module_->functions[index];
}

uint8_t *VirtualMachine::generateCode(const FunctionSpec &functionSpec) {
  return compiler_->generateCode(functionSpec);
}

const char *VirtualMachine::getString(int index) {
  return module_->strings[index];
}

std::size_t VirtualMachine::getFunctionCount() {
  return module_->functions.size();
}

// void removeGeneratedCode(ExecutionContext *context, int functionIndex) {
//   context->functions[functionIndex].jitAddress = 0;
//   setJitAddressSlot(context->functions[functionIndex].program, 0);
// }

// void removeAllGeneratedCode(ExecutionContext *context) {
//   int functionIndex = 0;
//   while (context->functions[functionIndex].name != NO_MORE_FUNCTIONS) {
//     removeGeneratedCode(context, functionIndex);
//     functionIndex++;
//   }
// }

void VirtualMachine::generateAllCode() {

	auto i = 0;
	for (auto &functionSpec : module_->functions) {
    if (cfg_.debug)
      std::cout << "\nJitting function: " << functionSpec.name.c_str() << std::endl;
    auto func = compiler_->generateCode(functionSpec);
	  compiledFunctions_.push_back(func);
  }
}

void ExecutionContext::reset() {
  stackPointer_ = stack_;
  programCounter_ = 0;
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
	 	std::cout << "+++++++++++++++++++++++\n";
    std::cout << "Running function: " << function->name.c_str() << " nargs: " << argsCount
              << std::endl;
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
    std::cout << "Pushing arg[" << idx << "] = " << arg << std::endl;
    executionContext_.push(arg);
  }

  // interpret or compile and run
  executionContext_.reset();

  StackElement result = executionContext_.interpret(functionIndex);

  executionContext_.reset();

  return result;
}

const char *b9ByteCodeName(ByteCode bc) {
  if (bc == ByteCode::DROP) return "DROP";
  if (bc == ByteCode::DUPLICATE) return "DUPLICATE";
  if (bc == ByteCode::FUNCTION_RETURN) return "FUNCTION_RETURN";
  if (bc == ByteCode::FUNCTION_CALL) return "FUNCTION_CALL";
  if (bc == ByteCode::PRIMITIVE_CALL) return "PRIMITIVE_CALL";
  if (bc == ByteCode::JMP) return "JMP";
  if (bc == ByteCode::PUSH_FROM_VAR) return "PUSH_FROM_VAR";
  if (bc == ByteCode::POP_INTO_VAR) return "POP_INTO_VAR";

  if (bc == ByteCode::INT_PUSH_CONSTANT) return "INT_PUSH_CONSTANT";
  if (bc == ByteCode::INT_SUB) return "INT_SUB";
  if (bc == ByteCode::INT_ADD) return "INT_ADD";
  if (bc == ByteCode::INT_JMP_EQ) return "INT_JMP_EQ";
  if (bc == ByteCode::INT_JMP_NEQ) return "INT_JMP_NEQ";
  if (bc == ByteCode::INT_JMP_GT) return "INT_JMP_GT";
  if (bc == ByteCode::INT_JMP_GE) return "INT_JMP_GE";
  if (bc == ByteCode::INT_JMP_LT) return "INT_JMP_LT";
  if (bc == ByteCode::INT_JMP_LE) return "INT_JMP_LE";

  if (bc == ByteCode::STR_PUSH_CONSTANT) return "STR_PUSH_CONSTANT";
  if (bc == ByteCode::STR_JMP_EQ) return "STR_JMP_EQ";
  if (bc == ByteCode::STR_JMP_NEQ) return "STR_JMP_NEQ";

  return "UNKNOWN BYTECODE";
}

//
// Base9 Primitives
//

extern "C" void b9_prim_print_number(ExecutionContext *context) {
  StackElement number = context->pop();
  std::cout << number << " ";
  context->push(0);
}

extern "C" void b9_prim_print_string(ExecutionContext *context) {
  char *string = (char *)keyToChar(context->pop());
  puts(string);
  context->push(0);
}

extern "C" void b9_prim_hash_table_allocate(ExecutionContext *context) {
  pHeap p = hashTable_allocate(8);
  // if (context->debug >= 1) {
  //   printf("IN hashTableAllocate %p\n", p);
  // }
  context->push((StackElement)p);
}

extern "C" void b9_prim_hash_table_put(ExecutionContext *context) {
  StackElement v = context->pop();
  StackElement k = context->pop();
  StackElement ht = context->pop();
  // if (context->debug >= 1) {
  // printf("IN hashTablePut %p %p(%s) %p(%s) \n", ht, k, k, v, v);
  // }

  context->push((StackElement)hashTable_put(context, (pHeap)ht, (hashTableKey)k,
                                            (hashTableKey)v));
}

extern "C" void b9_prim_hash_table_get(ExecutionContext *context) {
  StackElement k = context->pop();
  StackElement ht = context->pop();
  context->push(
      (StackElement)hashTable_get(context, (pHeap)ht, (hashTableKey)k));
}

}  // namespace b9
