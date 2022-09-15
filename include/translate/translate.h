/**
 * @file translate.h
 * @author Charles Averill
 * @brief Function headers and definitions for translation of an AST into LLVM-IR
 * @date 10-Sep-2022
 */

#ifndef TRANSLATE_H
#define TRANSLATE_H

#include <stdio.h>
#include <stdlib.h>

#include "parse.h"
#include "translate/llvm.h"
#include "tree.h"
#include "types/number.h"

LLVMStackEntryNode* determine_binary_expression_stack_allocation(ASTNode* root);
void initialize_virtual_registers();
void add_free_virtual_register(type_register register_index);
type_register get_free_virtual_register(void);
void free_llvm_stack_entry_node_list(LLVMStackEntryNode* head);

LLVMValue ast_to_llvm(ASTNode* n);
void generate_llvm(void);

#endif /* TRANSLATE_H */
