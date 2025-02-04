/**
 * @file llvm.c
 * @author Charles Averill
 * @brief LLVM-IR emission
 * @date 12-Sep-2022
 */

#include "translate/llvm.h"
#include "data.h"
#include "translate/translate.h"
#include "utils/clang.h"
#include "utils/formatting.h"
#include "utils/logging.h"

/**
 * @brief Update loaded register linked list to include new register
 * 
 * @param reg Register to include in list
 */
void prepend_loaded(type_register reg)
{
    LLVMStackEntryNode* new = (LLVMStackEntryNode*)malloc(sizeof(LLVMStackEntryNode));
    new->reg = reg;
    new->next = NULL;

    if (loadedRegistersHead == NULL) {
        loadedRegistersHead = new;
    } else {
        new->next = loadedRegistersHead;
        loadedRegistersHead = new;
    }
}

type_register* llvm_ensure_registers_loaded(int n_registers, type_register registers[])
{
    LLVMStackEntryNode* current = loadedRegistersHead;
    bool found_registers[n_registers];

    int found = 0;
    while (current) {
        for (int i = 0; i < n_registers; i++) {
            if (!found_registers[i] && current->reg == registers[i]) {
                found_registers[i] = true;
                found++;
            }

            if (found == n_registers) {
                return NULL;
            }
        }
        current = current->next;
    }

    // Haven't loaded all of our registers yet
    type_register* loaded_registers = (type_register*)malloc(sizeof(type_register) * n_registers);
    for (int i = 0; i < n_registers; i++) {
        loaded_registers[i] = registers[i];
        if (!found_registers[i]) {
            loaded_registers[i] = get_next_local_virtual_register();
            fprintf(D_LLVM_FILE, TAB "%%%llu = load i32, i32* %%%llu, align 4" NEWLINE,
                    loaded_registers[i], registers[i]);
            prepend_loaded(loaded_registers[i]);
        }
    }

    return loaded_registers;
}

/**
 * @brief Generated program's preamble
 */
void llvm_preamble()
{
    fprintf(D_LLVM_FILE, "; ModuleID = '%s'" NEWLINE, D_INPUT_FN);

    // Target layout
    char* target_datalayout = get_target_datalayout();
    fprintf(D_LLVM_FILE, "target datalayout = \"%s\"" NEWLINE, target_datalayout);
    free(target_datalayout);
    // Target triple
    char* target_triple = get_target_triple();
    fprintf(D_LLVM_FILE, "target triple = \"%s\"" NEWLINE NEWLINE, target_triple);

    fprintf(D_LLVM_FILE, "@print_int_fstring = private unnamed_addr constant [4 x i8] "
                         "c\"%%d\\0A\\00\", align 1" NEWLINE NEWLINE);
    fprintf(D_LLVM_FILE, "; Function Attrs: noinline nounwind optnone uwtable" NEWLINE);
    fprintf(D_LLVM_FILE, "define dso_local i32 @main() #0 {" NEWLINE);
}

/**
 * @brief Generated program's postamble
 */
void llvm_postamble()
{
    fprintf(D_LLVM_FILE, TAB "ret i32 0" NEWLINE);
    fprintf(D_LLVM_FILE, "}" NEWLINE NEWLINE);
    fprintf(D_LLVM_FILE, "declare i32 @printf(i8*, ...) #1" NEWLINE NEWLINE);
    fprintf(
        D_LLVM_FILE,
        "attributes #0 = { noinline nounwind optnone uwtable \"frame-pointer\"=\"all\" "
        "\"min-legal-vector-width\"=\"0\" \"no-trapping-math\"=\"true\" "
        "\"stack-protector-buffer-size\"=\"8\" \"target-cpu\"=\"x86-64\" "
        "\"target-features\"=\"+cx8,+fxsr,+mmx,+sse,+sse2,+x87\" \"tune-cpu\"=\"generic\" }" NEWLINE
            NEWLINE);
    fprintf(
        D_LLVM_FILE,
        "attributes #1 = { \"frame-pointer\"=\"all\" \"no-trapping-math\"=\"true\" "
        "\"stack-protector-buffer-size\"=\"8\" \"target-cpu\"=\"x86-64\" "
        "\"target-features\"=\"+cx8,+fxsr,+mmx,+sse,+sse2,+x87\" \"tune-cpu\"=\"generic\" }" NEWLINE
            NEWLINE);
    fprintf(D_LLVM_FILE, "!llvm.module.flags = !{!0, !1, !2, !3, !4}" NEWLINE);
    fprintf(D_LLVM_FILE, "!llvm.ident = !{!5}" NEWLINE NEWLINE);
    fprintf(D_LLVM_FILE, "!0 = !{i32 1, !\"wchar_size\", i32 4}" NEWLINE);
    fprintf(D_LLVM_FILE, "!1 = !{i32 7, !\"PIC Level\", i32 2}" NEWLINE);
    fprintf(D_LLVM_FILE, "!2 = !{i32 7, !\"PIE Level\", i32 2}" NEWLINE);
    fprintf(D_LLVM_FILE, "!3 = !{i32 7, !\"uwtable\", i32 1}" NEWLINE);
    fprintf(D_LLVM_FILE, "!4 = !{i32 7, !\"frame-pointer\", i32 2}" NEWLINE);
    fprintf(D_LLVM_FILE, "!5 = !{!\"Ubuntu clang version 14.0.0-1ubuntu1\"}" NEWLINE);
}

/**
 * @brief Allocate space on stack for variables
 * 
 * @param stack_entries LLVMStackEntryNode pointers holding stack allocation information
 */
void llvm_stack_allocation(LLVMStackEntryNode* stack_entries)
{
    LLVMStackEntryNode* current = stack_entries;

    while (current) {
        fprintf(D_LLVM_FILE, TAB "%%%llu = alloca %s, align %d" NEWLINE, current->reg,
                numberTypeLLVMReprs[current->type], current->align_bytes);
        current = current->next;
    }
}

/**
 * @brief Generate code for binary addition
 * 
 * @param left_virtual_register Lvalue to be added
 * @param right_virtual_register Rvalue to be added
 * @return type_register Virtual register holding result
 */
static type_register llvm_add(LLVMValue left_virtual_register, LLVMValue right_virtual_register)
{
    fprintf(D_LLVM_FILE, TAB "%%%llu = add nsw i32 %%%llu, %%%llu" NEWLINE,
            get_next_local_virtual_register(), left_virtual_register.value.virtual_register_index,
            right_virtual_register.value.virtual_register_index);
    return D_LLVM_LOCAL_VIRTUAL_REGISTER_NUMBER - 1;
}

/**
 * @brief Generate code for binary subtraction
 * 
 * @param left_virtual_register Lvalue to be subtracted
 * @param right_virtual_register Rvalue to be subtracted
 * @return type_register Virtual register holding result
 */
static type_register llvm_subtract(LLVMValue left_virtual_register,
                                   LLVMValue right_virtual_register)
{
    fprintf(D_LLVM_FILE, TAB "%%%llu = sub nsw i32 %%%llu, %%%llu" NEWLINE,
            get_next_local_virtual_register(), left_virtual_register.value.virtual_register_index,
            right_virtual_register.value.virtual_register_index);
    return D_LLVM_LOCAL_VIRTUAL_REGISTER_NUMBER - 1;
}

/**
 * @brief Generate code for binary multiplication
 * 
 * @param left_virtual_register Lvalue to be multiplied
 * @param right_virtual_register Rvalue to be multiplied
 * @return type_register Virtual register holding result
 */
static type_register llvm_multiply(LLVMValue left_virtual_register,
                                   LLVMValue right_virtual_register)
{
    fprintf(D_LLVM_FILE, TAB "%%%llu = mul nsw i32 %%%llu, %%%llu" NEWLINE,
            get_next_local_virtual_register(), left_virtual_register.value.virtual_register_index,
            right_virtual_register.value.virtual_register_index);
    return D_LLVM_LOCAL_VIRTUAL_REGISTER_NUMBER - 1;
}

/**
 * @brief Generate code for unsigned binary division
 * 
 * @param left_virtual_register Lvalue to be divided
 * @param right_virtual_register Rvalue to be divided
 * @return type_register Virtual register holding result
 */
static type_register llvm_divide(LLVMValue left_virtual_register, LLVMValue right_virtual_register)
{
    fprintf(D_LLVM_FILE, TAB "%%%llu = udiv i32 %%%llu, %%%llu" NEWLINE,
            get_next_local_virtual_register(), left_virtual_register.value.virtual_register_index,
            right_virtual_register.value.virtual_register_index);
    return D_LLVM_LOCAL_VIRTUAL_REGISTER_NUMBER - 1;
}

/**
 * @brief Generates LLVM-IR for various binary arithmetic expressions
 * 
 * @param operation Operation to perform
 * @param left_virtual_register Operand left of operation
 * @param right_virtual_register Operand right of operation
 * @return int Number of virtual register in which the result is stored
 */
LLVMValue llvm_binary_arithmetic(TokenType operation, LLVMValue left_virtual_register,
                                 LLVMValue right_virtual_register)
{
    type_register out_register;

    switch (operation) {
    case T_PLUS:
        out_register = llvm_add(left_virtual_register, right_virtual_register);
        break;
    case T_MINUS:
        out_register = llvm_subtract(left_virtual_register, right_virtual_register);
        break;
    case T_STAR:
        out_register = llvm_multiply(left_virtual_register, right_virtual_register);
        break;
    case T_SLASH:
        out_register = llvm_divide(left_virtual_register, right_virtual_register);
        break;
    case T_EXPONENT:
        fatal(RC_COMPILER_ERROR,
              "Exponent not yet supported, as libc pow only takes floating-point types");
        break;
    default:
        fatal(RC_COMPILER_ERROR,
              "llvm_binary_arithmetic receieved non-binary-arithmetic operator \"%s\"",
              tokenStrings[operation]);
    }

    prepend_loaded(out_register);

    return LLVMVALUE_VIRTUAL_REGISTER(out_register);
}

/**
 * @brief Store a constant number value into a register
 * 
 * @param value Number struct containing information about the constant
 * @return int Register number value is held in
 */
LLVMValue llvm_store_constant(Number value)
{
    type_register out_register_number = pop_stack_entry_linked_list(&freeVirtualRegistersHead);
    fprintf(D_LLVM_FILE, TAB "store %s ", numberTypeLLVMReprs[value.type]);
    fprintf(D_LLVM_FILE, numberTypeFormatStrings[value.type], value.value);
    fprintf(D_LLVM_FILE, ", %s* %%%llu, align %d" NEWLINE, numberTypeLLVMReprs[value.type],
            out_register_number, numberTypeByteSizes[value.type]);
    return LLVMVALUE_VIRTUAL_REGISTER_POINTER(out_register_number);
}

/**
 * @brief Retrieves the next valid virtual register index
 * 
 * @return type_register Index of next unused virtual register
 */
type_register get_next_local_virtual_register(void)
{
    return D_LLVM_LOCAL_VIRTUAL_REGISTER_NUMBER++;
}

/**
 * @brief Generate code to print an integer
 * 
 * @param print_vr Register holding value to print
 */
void llvm_print_int(type_register print_vr)
{
    get_next_local_virtual_register();
    fprintf(D_LLVM_FILE,
            TAB "call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x "
                "i8]* @print_int_fstring , i32 0, i32 0), i32 %%%llu)" NEWLINE,
            print_vr);
}
