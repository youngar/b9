#ifndef B9_INSTRUCTIONS_HPP_
#define B9_INSTRUCTIONS_HPP_

#include <cstdint>
#include <ostream>

namespace b9 {

using RawByteCode = std::uint8_t;

enum class ByteCode : RawByteCode {

  // Generic ByteCodes

  // A special uninterpreted ByteCode placed the end of a
  // ByteCode array.
  END_SECTION = 0x0,

  // Call a Base9 function
  FUNCTION_CALL = 0x1,
  // Return from a function
  FUNCTION_RETURN = 0x2,
  // Call a native C function
  PRIMITIVE_CALL = 0x3,
  // Duplicate the top element on the stack
  DUPLICATE = 0x4,
  // Drop the top element of the stack
  DROP = 0x5,
  // Push from a local variable
  PUSH_FROM_VAR = 0x6,
  // Push into a local variable
  POP_INTO_VAR = 0x7,
  // Pushes the result of `lval + rval`, not limited to numbers
  ADD = 0x8,

  // Arithmetic bytecodes

  // Subtract two operands
  SUB = 0x9,
  // Multiply two operands
  MUL = 0xa,
  // Divide two operands
  DIV = 0xb,
  // Push a constant
  INT_PUSH_CONSTANT = 0xc,
  // Invert a boolean value, all non-zero integers are true
  NOT = 0xd,

  // Control flow bytecodes

  // Jump unconditionally by the offset
  JMP = 0xe,
  // Jump if two values are equal
  JMP_EQ_EQ = 0xf,
  // Jump if two values are not equal
  JMP_EQ_NEQ = 0x10,
  // Jump if the first value is greater than the second
  JMP_EQ_GT = 0x11,
  // Jump if the first value is greater than or equal to the second
  JMP_EQ_GE = 0x12,
  // Jump if the first value is less than to the second
  JMP_EQ_LT = 0x13,
  // Jump if the first value is less than or equal to the second
  JMP_EQ_LE = 0x14,

  // String ByteCodes

  // Push a string from this module's constant pool
  STR_PUSH_CONSTANT = 0x15,

  // Object Bytecodes

  NEW_OBJECT = 0x20,

  PUSH_FROM_OBJECT = 0x21,

  POP_INTO_OBJECT = 0x22,

  CALL_INDIRECT = 0x23,

  SYSTEM_COLLECT = 0x24,
};

inline const char *toString(ByteCode bc) {
  switch (bc) {
    case ByteCode::END_SECTION:
      return "end_section";
    case ByteCode::FUNCTION_CALL:
      return "function_call";
    case ByteCode::FUNCTION_RETURN:
      return "function_return";
    case ByteCode::PRIMITIVE_CALL:
      return "primitive_call";
    case ByteCode::DUPLICATE:
      return "duplicate";
    case ByteCode::DROP:
      return "drop";
    case ByteCode::PUSH_FROM_VAR:
      return "push_from_var";
    case ByteCode::POP_INTO_VAR:
      return "pop_into_var";
    case ByteCode::ADD:
      return "add";
    case ByteCode::SUB:
      return "sub";
    case ByteCode::MUL:
      return "mul";
    case ByteCode::DIV:
      return "div";
    case ByteCode::INT_PUSH_CONSTANT:
      return "push_constant";
    case ByteCode::NOT:
      return "not";
    case ByteCode::JMP:
      return "jmp";
    case ByteCode::JMP_EQ_EQ:
      return "jmp_eq";
    case ByteCode::JMP_EQ_NEQ:
      return "jmp_neq";
    case ByteCode::JMP_EQ_GT:
      return "jmp_gt";
    case ByteCode::JMP_EQ_GE:
      return "jmp_ge";
    case ByteCode::JMP_EQ_LT:
      return "jmp_lt";
    case ByteCode::JMP_EQ_LE:
      return "jmp_le";
    case ByteCode::STR_PUSH_CONSTANT:
      return "str_push_constant";
    case ByteCode::NEW_OBJECT:
      return "new_object";
    case ByteCode::PUSH_FROM_OBJECT:
      return "push_from_object";
    case ByteCode::POP_INTO_OBJECT:
      return "pop_into_object";
    case ByteCode::CALL_INDIRECT:
      return "call_indirect";
    case ByteCode::SYSTEM_COLLECT:
      return "system_collect";
    default:
      return "UNKNOWN_BYTECODE";
  }
}

/// Print a ByteCode
inline std::ostream &operator<<(std::ostream &out, ByteCode bc) {
  return out << toString(bc);
}

using RawInstruction = std::uint32_t;

/// The 24bit immediate encoded in an instruction. Note that parameters are
/// signed values, so special care must be taken to sign extend 24bits to 32.
using Parameter = std::int32_t;

/// A RawInstruction wrapper that will encode and decode instruction bytecodes
/// and immediate parameters. The Instruction layout is:
/// ```
/// |0000-0000 0000-0000 0000-0000 0000-0000
/// |---------| bytecode (8bits)
///           |-----------------------------| parameter (24bits)
/// ```
///
/// For many ByteCodes, the parameter is unused and left as zero.
class Instruction {
 public:
  constexpr Instruction() noexcept : raw_{0} {}

  constexpr Instruction(RawInstruction raw) noexcept : raw_{raw} {}

  constexpr Instruction(ByteCode bc) noexcept
      : raw_{RawInstruction(bc) << BYTECODE_SHIFT} {}

  constexpr Instruction(ByteCode bc, Parameter p) noexcept
      : raw_{(RawInstruction(bc) << BYTECODE_SHIFT) | (p & PARAMETER_MASK)} {}

  constexpr Instruction &set(ByteCode bc, Parameter p) noexcept {
    raw_ = (RawInstruction(bc) << BYTECODE_SHIFT) | (p & PARAMETER_MASK);
    return *this;
  }

  /// Encode the bytecode
  constexpr Instruction &byteCode(ByteCode bc) noexcept {
    raw_ = (RawByteCode(bc) << BYTECODE_SHIFT) | (raw_ & PARAMETER_MASK);
    return *this;
  }

  /// Decode the bytecode
  constexpr ByteCode byteCode() const noexcept {
    return static_cast<ByteCode>(raw_ >> BYTECODE_SHIFT);
  }

  /// Encode the parameter
  constexpr Instruction &parameter(Parameter p) noexcept {
    raw_ = (raw_ & BYTECODE_MASK) | (p & PARAMETER_MASK);
    return *this;
  }

  /// Decode the parameter
  constexpr Parameter parameter() const noexcept {
    auto param = static_cast<Parameter>(raw_ & PARAMETER_MASK);
    // Sign extend when top (24th) bit is 1
    if (param & 0x0080'0000) param |= 0xFF00'0000;
    return param;
  }

  constexpr RawInstruction raw() const noexcept { return raw_; }

  constexpr bool operator==(const Instruction rhs) const {
    return raw_ == rhs.raw_;
  }

  constexpr bool operator!=(const Instruction rhs) const {
    return raw_ != rhs.raw_;
  }

 private:
  static constexpr const RawInstruction BYTECODE_SHIFT = 24;
  static constexpr const RawInstruction PARAMETER_MASK = 0x00FF'FFFF;
  static constexpr const RawInstruction BYTECODE_MASK = ~PARAMETER_MASK;

  RawInstruction raw_;
};

/// A special constant indicating the end of a sequence of instructions.
/// END_SECTION should be the last element in every functions bytecode array.
static constexpr Instruction END_SECTION{ByteCode::END_SECTION, 0};

/// Print an Instruction.
inline std::ostream &operator<<(std::ostream &out, Instruction i) {
  out << "(" << i.byteCode();

  switch (i.byteCode()) {
    // 0 parameters
    case ByteCode::END_SECTION:
    case ByteCode::DUPLICATE:
    case ByteCode::FUNCTION_RETURN:
    case ByteCode::DROP:
    case ByteCode::ADD:
    case ByteCode::SUB:
    case ByteCode::MUL:
    case ByteCode::DIV:
    case ByteCode::NOT:
    case ByteCode::NEW_OBJECT:
    case ByteCode::CALL_INDIRECT:
    case ByteCode::SYSTEM_COLLECT:
      break;
    // 1 parameter
    case ByteCode::FUNCTION_CALL:
    case ByteCode::PRIMITIVE_CALL:
    case ByteCode::JMP:
    case ByteCode::PUSH_FROM_VAR:
    case ByteCode::POP_INTO_VAR:
    case ByteCode::INT_PUSH_CONSTANT:
    case ByteCode::JMP_EQ_EQ:
    case ByteCode::JMP_EQ_NEQ:
    case ByteCode::JMP_EQ_GT:
    case ByteCode::JMP_EQ_GE:
    case ByteCode::JMP_EQ_LT:
    case ByteCode::JMP_EQ_LE:
    case ByteCode::STR_PUSH_CONSTANT:
    case ByteCode::PUSH_FROM_OBJECT:
    case ByteCode::POP_INTO_OBJECT:
    default:
      out << " " << i.parameter();
      break;
  }
  return out << ")";
}

}  // namespace b9

#endif  // B9_INSTRUCTIONS_HPP_
